#!/usr/bin/env python3
"""Exact Query/Retrieve results and failure paths against independent peers (#1319)."""
import argparse
import fnmatch
import json
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess
import sys
import time
import warnings

import pydicom
from pydicom.datadict import dictionary_VR, tag_for_keyword
from pydicom.dataelem import RawDataElement
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.multival import MultiValue
from pydicom.tag import Tag
from pydicom.uid import ExplicitVRLittleEndian

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
warnings.filterwarnings('ignore', category=UserWarning)
from qr_matrix import (ALL_PATIENTS, FIND_REFUSALS, IDENTITY, INSTANCES, MODELS, PATIENT_BY_KEY, PATIENTS, REFERENCE_DIFFERENCES,
                       RESPONSE_EXTRAS, RETRIEVE_CASES, RETRIEVE_REFUSALS, RETURN_KEYS, SERIES, SERIES_BY_KEY, STUDIES, STUDY_BY_KEY,
                       requirement_cases,
                       UNSUPPORTED_KEYS, CT, FindCase, case_id, find_cases, instance_uid, modalities_in_study, series_uid,
                       study_uid)
from run import Validation, require
from support import declared_support

QR_PORT, SERVER_PORT, REFERENCE_PORT, DESTINATION_PORT, CLOSED_PORT = 22620, 22621, 22622, 22623, 22624
QR_AE, SERVER_AE, REFERENCE_AE, DESTINATION_AE, SCU_AE = 'PACS_QR', 'PACS_SERVER', 'REFERENCE', 'DEST_SCP', 'QR_SCU'
TEXT_VRS = {'PN', 'LO', 'SH', 'LT', 'ST', 'UT', 'UC'}
SUCCESS, CANCEL, WARNING, DATASET_MISMATCH = 0x0000, 0xFE00, 0xB000, 0xA900
OUT_OF_RESOURCES_SUBOPS, DESTINATION_UNKNOWN = 0xA702, 0xA801
ANY_EXIT = tuple(range(256))


def encode_text(value, charset):
    """PS3.5 bytes from Python codecs, independent of any DICOM writer."""
    if charset in (None, 'ISO_IR 6'):
        return value.encode('ascii')
    if charset == 'ISO_IR 192':
        return value.encode('utf-8')
    if charset == 'ISO_IR 100':
        return value.encode('latin1')
    if charset == r'\ISO 2022 IR 87':
        # Designation does not survive a component delimiter (PS3.5 6.1.2.5.3).
        return b'^'.join(part.encode('iso2022_jp') for part in value.split('^'))
    if charset == r'\ISO 2022 IR 149':
        return b'^'.join(b'\x1b$)C'+part.encode('euc_kr') for part in value.split('^'))
    raise ValueError(charset)


def set_value(ds, keyword, value, charset=None):
    tag = tag_for_keyword(keyword)
    vr = dictionary_VR(tag)
    if vr not in TEXT_VRS or value is None or value.isascii():
        setattr(ds, keyword, value)
        return
    raw = encode_text(value, charset)
    raw += b' ' if len(raw) % 2 else b''
    ds[tag] = RawDataElement(Tag(tag), vr, len(raw), raw, 0, False, True)


def set_charset(ds, charset):
    if charset and charset != 'ISO_IR 6':
        ds.SpecificCharacterSet = ['', charset[1:]] if charset.startswith('\\') else charset


def write_part10(ds, path, sop_class, sop_instance):
    meta = FileMetaDataset()
    meta.MediaStorageSOPClassUID = sop_class
    meta.MediaStorageSOPInstanceUID = sop_instance
    meta.TransferSyntaxUID = ExplicitVRLittleEndian
    ds.file_meta = meta
    ds.is_little_endian, ds.is_implicit_VR = True, False
    ds.save_as(path, write_like_original=False)


def is_failure(status):
    return status is not None and (0xA000 <= status <= 0xAFFF or 0xC000 <= status <= 0xCFFF)


def pixels_of(key):
    """Distinct bytes per object, so a swapped or reused object cannot pass."""
    index = [row.key for row in INSTANCES].index(key)
    return bytes([index+1, index+2, index+3, index+4])


def fixture_values(level, key):
    """The literal value each return key must carry for one fixture entity."""
    if level == 'PATIENT':
        row = PATIENT_BY_KEY[key]
        return dict(PatientID=row.id, PatientName=row.name, PatientBirthDate=row.birth, PatientSex=row.sex)
    if level == 'STUDY':
        row = STUDY_BY_KEY[key]
        return dict(StudyInstanceUID=study_uid(key), StudyDate=row.date, StudyTime=row.time,
                    AccessionNumber=row.accession, StudyID=row.study_id, StudyDescription=row.description,
                    ReferringPhysicianName=row.referring, ModalitiesInStudy='\\'.join(modalities_in_study(key)))
    if level == 'SERIES':
        row = SERIES_BY_KEY[key]
        return dict(SeriesInstanceUID=series_uid(key), Modality=row.modality, SeriesNumber=str(row.number),
                    SeriesDescription=row.description)
    row = next(item for item in INSTANCES if item.key == key)
    return dict(SOPInstanceUID=instance_uid(key), SOPClassUID=row.sop_class, InstanceNumber=str(row.number))


def response_value(ds, keyword):
    value = ds.get(keyword)
    if value is None:
        return ''
    if isinstance(value, MultiValue):
        return '\\'.join(sorted(str(item) for item in value))
    return str(value).strip()


QUERY_SCU_FLAGS = {'PatientID': '--patient-id', 'PatientName': '--patient-name', 'PatientBirthDate': '--patient-birth-date',
                   'PatientSex': '--patient-sex', 'StudyDate': '--study-date', 'StudyTime': '--study-time',
                   'AccessionNumber': '--accession-number', 'StudyInstanceUID': '--study-uid', 'StudyID': '--study-id',
                   'StudyDescription': '--study-description', 'Modality': '--modality', 'SeriesInstanceUID': '--series-uid',
                   'SOPInstanceUID': '--sop-instance-uid', 'SpecificCharacterSet': ''}
QUERY_SCU_IDENTITY = {'PATIENT': 'patientId', 'STUDY': 'studyInstanceUid', 'SERIES': 'seriesInstanceUid', 'IMAGE': 'sopInstanceUid'}
RETRIEVE_SCU_FLAGS = {'PatientID': '--patient-id', 'StudyInstanceUID': '--study-uid', 'SeriesInstanceUID': '--series-uid',
                      'SOPInstanceUID': '--sop-instance-uid'}


def tag_text(keyword):
    tag = Tag(tag_for_keyword(keyword))
    return f'{tag.group:04x},{tag.element:04x}'


def clear_files(directory):
    for path in directory.iterdir():
        if path.is_file():
            path.unlink()


def protocol_verdict(row, detail):
    require(row['status'] == 'passed', detail)
    return detail


IDENTITY_TO_KEY = {
    'PATIENT': {row.id: row.key for row in PATIENTS},
    'STUDY': {study_uid(row.key): row.key for row in STUDIES},
    'SERIES': {series_uid(row.key): row.key for row in SERIES},
    'IMAGE': {instance_uid(row.key): row.key for row in INSTANCES},
}


class QrMatrix(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.sources = self.output/'sources'
        self.queries = self.output/'queries'
        self.counter = 0

    # Fixtures -------------------------------------------------------------------------------

    def build_sources(self):
        self.sources.mkdir()
        self.queries.mkdir()
        for instance in INSTANCES:
            series = SERIES_BY_KEY[instance.series]
            study = STUDY_BY_KEY[series.study]
            patient = PATIENT_BY_KEY[study.patient]
            ds = Dataset()
            set_charset(ds, patient.charset)
            ds.SOPClassUID = instance.sop_class
            ds.SOPInstanceUID = instance_uid(instance.key)
            ds.StudyDate, ds.StudyTime = study.date, study.time
            set_value(ds, 'AccessionNumber', study.accession, patient.charset)
            ds.Modality = series.modality
            set_value(ds, 'ReferringPhysicianName', study.referring, patient.charset)
            set_value(ds, 'StudyDescription', study.description, patient.charset)
            set_value(ds, 'SeriesDescription', series.description, patient.charset)
            set_value(ds, 'PatientName', patient.name, patient.charset)
            ds.PatientID = patient.id
            ds.PatientBirthDate, ds.PatientSex = patient.birth, patient.sex
            ds.StudyInstanceUID = study_uid(study.key)
            ds.SeriesInstanceUID = series_uid(series.key)
            ds.StudyID = study.study_id
            ds.SeriesNumber = series.number
            ds.InstanceNumber = instance.number
            ds.SamplesPerPixel, ds.PhotometricInterpretation = 1, 'MONOCHROME2'
            ds.Rows, ds.Columns = 2, 2
            ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 8, 8, 7, 0
            ds.PixelData = pixels_of(instance.key)
            write_part10(ds, self.sources/f'{instance.key}.dcm', instance.sop_class, instance_uid(instance.key))
        return f'{len(INSTANCES)} objects, {len(PATIENTS)} patients, {len(STUDIES)} studies, {len(SERIES)} series'

    def query_file(self, level, keys):
        ds = Dataset()
        charset = keys.get('SpecificCharacterSet')
        set_charset(ds, charset)
        if level is not None:
            ds.QueryRetrieveLevel = level
        for keyword in (*keys, *RETURN_KEYS.get(level, ())):
            if keyword not in ('SpecificCharacterSet',) and keyword not in ds:
                set_value(ds, keyword, keys.get(keyword, ''), charset)
        self.counter += 1
        path = self.queries/f'query-{self.counter:04}.dcm'
        write_part10(ds, path, '1.2.840.10008.5.1.4.1.2.2.1', f'1.2.3.1319.9.{self.counter}')
        return path

    # C-FIND against an SCP ------------------------------------------------------------------

    def run_findscu(self, port, ae, model, query):
        directory = self.output/'responses'/self.current
        directory.mkdir(parents=True)
        self.command(['findscu', '-d', '-to', '10', '-ta', '10', '-td', '30', MODELS[model]['flag'],
                      '-aet', SCU_AE, '-aec', ae, '-X', '-od', directory, '127.0.0.1', port, query], timeout=60)
        # DCMTK logs to stderr, which command() keeps only in this case's log.
        trace = (self.output/f'{self.current}.log').read_text(errors='replace')
        statuses = [int(value, 16) for value in re.findall(r'DIMSE Status\s*:\s*0x([0-9a-fA-F]{4})', trace)]
        require(statuses, 'findscu reported no DIMSE status')
        responses = [pydicom.dcmread(path, force=True) for path in sorted(directory.iterdir())]
        return statuses, responses

    def check_response(self, row, ds, ae):
        level = row.level
        identity = response_value(ds, IDENTITY[level])
        key = IDENTITY_TO_KEY[level].get(identity)
        require(key is not None, f'Response names unknown entity {identity!r}')
        present = {element.keyword for element in ds}
        requested = {*row.keys, *RETURN_KEYS[level]}-UNSUPPORTED_KEYS
        missing = requested-present-{'SpecificCharacterSet'}
        extra = present-requested-RESPONSE_EXTRAS
        require(not missing, f'{key}: response lacks requested keys {sorted(missing)}')
        require(not extra, f'{key}: response has attributes not in the request {sorted(extra)}')
        require(response_value(ds, 'QueryRetrieveLevel') == level, f'{key}: Query/Retrieve Level not echoed')
        require(response_value(ds, 'RetrieveAETitle') == ae, f'{key}: Retrieve AE Title is not this SCP')
        for keyword, expected in fixture_values(level, key).items():
            actual = response_value(ds, keyword)
            require(actual == expected, f'{key}: {keyword} is {actual!r}, expected {expected!r}')
        return key

    def find_case(self, port, ae, row):
        statuses, responses = self.run_findscu(port, ae, row.model, self.query_file(row.level, row.keys))
        keys = [self.check_response(row, ds, ae) for ds in responses]
        require(sorted(keys) == sorted(row.expected), f'Result set {sorted(keys)}, expected {sorted(row.expected)}')
        require(statuses[-1] == SUCCESS, f'Final status {statuses[-1]:04X}')
        pending = 0xFF01 if set(row.keys) & UNSUPPORTED_KEYS else 0xFF00
        require(all(status == pending for status in statuses[:-1]), f'Pending statuses {set(statuses[:-1])}, expected {pending:04X}')
        return f'{len(keys)} exact matches, final status 0000'

    def refusal_case(self, port, ae, row):
        statuses, responses = self.run_findscu(port, ae, row.model, self.query_file(row.level, row.keys))
        require(not responses, f'{len(responses)} identifiers returned for an invalid request')
        require(statuses == [DATASET_MISMATCH], f'Statuses {[f"{status:04X}" for status in statuses]}, expected [A900]')
        return 'status A900 with no identifier'

    def find_matrix(self, server, port, ae):
        prefix = f'qr.find.{server}'
        for row in find_cases():
            self.case(case_id(prefix, row), lambda row=row: self.find_case(port, ae, row),
                      requires=[f'qr.seed.{server}'])
        for row in FIND_REFUSALS:
            self.case(f'{prefix}.refuse.{row.name}', lambda row=row: self.refusal_case(port, ae, row),
                      requires=[f'qr.seed.{server}'])
        # Runs last: every hostile key above must have left the index usable and whole.
        after = FindCase('still-whole-after-hostile-keys', 'patient-root', 'PATIENT', {}, ALL_PATIENTS)
        self.case(case_id(prefix, after), lambda: self.find_case(port, ae, after), requires=[f'qr.seed.{server}'])

    def qr_server_args(self, root, sources=None, peer=None):
        peer = peer or f'{DESTINATION_AE}:127.0.0.1:{DESTINATION_PORT}'
        return [self.bin/'qr_scp', QR_PORT, QR_AE, '--storage-dir', sources or self.sources, '--index-db', root/'index.db',
                '--peer', peer]

    def pacs_server_args(self, root):
        args = [self.bin/'pacs_server', '--port', SERVER_PORT, '--ae-title', SERVER_AE, '--storage-dir', root/'archive',
                '--db-path', root/'index.db']
        # Older builds lack the option; they then fail the C-MOVE cases with A801 instead of not starting.
        help_text = subprocess.run([str(self.bin/'pacs_server'), '--help'], capture_output=True, text=True).stdout
        return [*args, '--peer', f'{DESTINATION_AE}:127.0.0.1:{DESTINATION_PORT}'] if '--peer' in help_text else args

    def seed_qr(self, root, sources=None, count=None):
        count = len(INSTANCES) if count is None else count
        output = self.command([*self.qr_server_args(root, sources), '--scan-only'], timeout=60)
        require(f'Scan complete: {count} files indexed\n' in output, 'qr_scp did not index every object')
        return f'{count} objects indexed by the public scan'

    def seed_pacs_server(self):
        self.command(['storescu', '-nh', '-aet', SCU_AE, '-aec', SERVER_AE, '+sd', '-R', '127.0.0.1', SERVER_PORT, self.sources],
                     timeout=120)
        return f'{len(INSTANCES)} objects delivered over C-STORE'

    def scp_find(self):
        root = self.output/'scp-qr-find'
        root.mkdir()
        self.case('qr.seed.qr-scp', lambda: self.seed_qr(root), requires=['qr.sources'])
        with self.server(self.qr_server_args(root), QR_PORT, log_name='qr-scp-find') as process:
            self.find_matrix('qr-scp', QR_PORT, QR_AE)
        self.case('qr.find.qr-scp.exit', lambda: self.server_exit(process, 'qr_scp'))
        root = self.output/'scp-server-find'
        (root/'archive').mkdir(parents=True)
        with self.server(self.pacs_server_args(root), SERVER_PORT, log_name='pacs-server-find') as process:
            self.case('qr.seed.pacs-server', self.seed_pacs_server, requires=['qr.sources'])
            self.find_matrix('pacs-server', SERVER_PORT, SERVER_AE)
        self.case('qr.find.pacs-server.exit', lambda: self.server_exit(process, 'pacs_server'))

    def server_exit(self, process, name):
        require(process.returncode == 0, f'{name} exit {process.returncode}')
        return 'exit 0 after the matrix'

    # Reference cross-check: the expectations must hold on an independent archive ------------

    def reference_archive(self, name='reference'):
        root = self.output/name
        (root/'db').mkdir(parents=True)
        config = root/'dcmqrscp.cfg'
        config.write_text(f"""NetworkTCPPort = {REFERENCE_PORT}
MaxPDUSize = 16384
MaxAssociations = 16
HostTable BEGIN
destination = ({DESTINATION_AE}, 127.0.0.1, {DESTINATION_PORT})
HostTable END
VendorTable BEGIN
VendorTable END
AETable BEGIN
{REFERENCE_AE} {root/'db'} RW (500, 1024mb) ANY
AETable END
""")
        self.command(['dcmqridx', root/'db', *sorted(self.sources.iterdir())], timeout=60)
        return config

    def reference_server(self, config, log_name='reference-archive'):
        return self.server(['dcmqrscp', '-d', '-c', config], REFERENCE_PORT, log_name=log_name)

    def check_difference(self, case, observed):
        reason = REFERENCE_DIFFERENCES.get(case)
        require(reason is None or observed is not None, f'Listed difference no longer differs: {case}')
        require(reason is not None or observed is None, f'Reference disagrees with the expectation: {observed}')
        return f'differs as declared ({observed}): {reason}' if reason else 'reference agrees with the expectation'

    def reference_find(self, row):
        statuses, responses = self.run_findscu(REFERENCE_PORT, REFERENCE_AE, row.model, self.query_file(row.level, row.keys))
        keys = sorted(IDENTITY_TO_KEY[row.level].get(response_value(ds, IDENTITY[row.level]), '?') for ds in responses)
        observed = None
        if statuses[-1] != SUCCESS or keys != sorted(row.expected):
            observed = f'status {statuses[-1]:04X}, keys {keys}'
        return self.check_difference(case_id('find', row), observed)

    def reference_find_refusal(self, row):
        statuses, responses = self.run_findscu(REFERENCE_PORT, REFERENCE_AE, row.model, self.query_file(row.level, row.keys))
        # Table C.4-1 offers A900 and Cxxx for this; dcmqrscp uses C000 where PACS uses A900.
        observed = None if len(statuses) == 1 and is_failure(statuses[0]) and not responses else \
            f'statuses {[f"{status:04X}" for status in statuses]}, {len(responses)} identifiers'
        return self.check_difference(f'find.refuse.{row.name}', observed)

    def reference_retrieve(self, operation, row):
        rows, received, _ = self.run_retrieve(operation, REFERENCE_PORT, REFERENCE_AE, row, self.retrieve_file(row))
        final = rows[-1] if rows else dict(status=None)
        observed = None
        if final['status'] != SUCCESS or sorted(received) != sorted(row.expected):
            observed = f'final {final}, received {sorted(received)}'
        return self.check_difference(f'{operation}.{row.model}.{(row.level or "none").lower()}.{row.name}', observed)

    def reference_retrieve_refusal(self, operation, row):
        rows, received, _ = self.run_retrieve(operation, REFERENCE_PORT, REFERENCE_AE, row, self.retrieve_file(row))
        final = rows[-1] if rows else dict(status=None)
        observed = None if is_failure(final['status']) and not received else f'final {final}, received {sorted(received)}'
        return self.check_difference(f'{operation}.refuse.{row.name}', observed)

    def reference_group(self):
        config = self.reference_archive()
        self.destination = self.output/'reference-destination'
        self.destination.mkdir()
        with self.server(['storescp', '-aet', DESTINATION_AE, '-od', self.destination, '-fe', '.dcm', DESTINATION_PORT],
                         DESTINATION_PORT, log_name='reference-destination'), self.reference_server(config):
            for row in find_cases():
                self.case(case_id('qr.reference.find', row), lambda row=row: self.reference_find(row), requires=['qr.sources'])
            for row in FIND_REFUSALS:
                self.case(f'qr.reference.find.refuse.{row.name}', lambda row=row: self.reference_find_refusal(row),
                          requires=['qr.sources'])
            for operation in ('get', 'move'):
                for row in RETRIEVE_CASES:
                    self.case(case_id(f'qr.reference.{operation}', row),
                              lambda operation=operation, row=row: self.reference_retrieve(operation, row),
                              requires=['qr.sources'])
                for row in RETRIEVE_REFUSALS:
                    self.case(f'qr.reference.{operation}.refuse.{row.name}',
                              lambda operation=operation, row=row: self.reference_retrieve_refusal(operation, row),
                              requires=['qr.sources'])

    # C-GET and C-MOVE against an SCP --------------------------------------------------------

    def retrieve_file(self, row):
        ds = Dataset()
        if row.level is not None:
            ds.QueryRetrieveLevel = row.level
        for keyword, value in row.keys.items():
            setattr(ds, keyword, value)
        self.counter += 1
        path = self.queries/f'retrieve-{self.counter:04}.dcm'
        write_part10(ds, path, '1.2.840.10008.5.1.4.1.2.2.3', f'1.2.3.1319.8.{self.counter}')
        return path

    def dimse_responses(self, kind):
        """Every response of this command in the case trace, as DCMTK itself reported it."""
        trace = (self.output/f'{self.current}.log').read_text(errors='replace')
        rows = []
        for block in re.split(r'=+ INCOMING DIMSE MESSAGE =+', trace)[1:]:
            head = block.split('END DIMSE MESSAGE')[0]
            if not re.search(rf'Message Type\s*:\s*{kind} RSP', head):
                continue
            row = dict(status=int(re.search(r'DIMSE Status\s*:\s*0x([0-9a-fA-F]{4})', head).group(1), 16),
                       data_set=bool(re.search(r'Data Set\s*:\s*present', head)))
            for label in ('Remaining', 'Completed', 'Failed', 'Warning'):
                found = re.search(label+r' Suboperations\s*:\s*(\d+)', head)
                if found:
                    row[label.lower()] = int(found.group(1))
            rows.append(row)
        failed = re.findall(r'\(0008,0058\) UI \[([^\]]*)\]', trace)
        return rows, failed[-1].split('\\') if failed else []

    def run_retrieve(self, operation, port, ae, row, query, directory=None):
        """Run one retrieve with a real peer. Returns its responses and what the receiver holds."""
        if operation == 'get':
            directory = directory or self.output/'received'/self.current
            directory.mkdir(parents=True)
            self.command(['getscu', '-d', '-to', '10', '-ta', '10', '-td', '30', MODELS[row.model]['flag'], '-aet', SCU_AE,
                          '-aec', ae, '-od', directory, '127.0.0.1', port, query], timeout=120, allowed=ANY_EXIT)
        else:
            directory = directory or self.destination
            for path in directory.iterdir():
                if path.is_file():
                    path.unlink()
            self.command(['movescu', '-d', '-to', '10', '-ta', '10', '-td', '30', MODELS[row.model]['flag'], '-aet', SCU_AE,
                          '-aec', ae, '-aem', DESTINATION_AE, '127.0.0.1', port, query], timeout=120, allowed=ANY_EXIT)
        rows, failed = self.dimse_responses('C-GET' if operation == 'get' else 'C-MOVE')
        return rows, self.received_objects(directory), failed

    def received_objects(self, directory):
        """Object bytes the receiver holds, checked against the source they must have come from."""
        by_uid = {instance_uid(row.key): row.key for row in INSTANCES}
        keys = []
        for path in sorted(directory.rglob('*')):
            if not path.is_file():
                continue
            ds = pydicom.dcmread(path)
            key = by_uid.get(str(ds.SOPInstanceUID))
            require(key is not None, f'Receiver holds unknown object {ds.SOPInstanceUID}')
            require(ds.PixelData == pixels_of(key), f'{key}: pixel data changed')
            keys.append(key)
        return keys

    def check_counters(self, rows, total, completed, failed, warning, status):
        require(rows, 'No retrieve response')
        final = rows[-1]
        require(final['status'] == status, f'Final status {final["status"]:04X}, expected {status:04X}')
        got = tuple(final.get(name, 0) for name in ('completed', 'failed', 'warning'))
        require(got == (completed, failed, warning), f'Final counters {got}, expected {(completed, failed, warning)}')
        if status != CANCEL:
            require('remaining' not in final, 'Remaining count in a terminal non-cancel response')
        for row in rows[:-1]:
            require(row['status'] == 0xFF00, f'Non-final response with status {row["status"]:04X}')
            count = sum(row.get(name, 0) for name in ('remaining', 'completed', 'failed', 'warning'))
            require(count == total, f'Pending counters total {count}, expected {total}')

    def retrieve_case(self, operation, port, ae, row):
        rows, received, failed = self.run_retrieve(operation, port, ae, row, self.retrieve_file(row))
        require(sorted(received) == sorted(row.expected), f'Receiver holds {sorted(received)}, expected {sorted(row.expected)}')
        count = len(row.expected)
        self.check_counters(rows, count, count, 0, 0, SUCCESS)
        require(not failed, f'Failed SOP Instance UID List on success: {failed}')
        return f'{count} exact objects, final 0000, counters ({count}, 0, 0)'

    def retrieve_refusal(self, operation, port, ae, row):
        rows, received, _ = self.run_retrieve(operation, port, ae, row, self.retrieve_file(row))
        require(not received, f'Objects delivered for an invalid request: {sorted(received)}')
        require(len(rows) == 1 and rows[0]['status'] == DATASET_MISMATCH, f'Responses {rows}, expected one A900')
        return 'status A900, nothing delivered'

    def retrieve_matrix(self, server, port, ae, operations):
        for operation in operations:
            prefix = f'qr.{operation}.{server}'
            for row in RETRIEVE_CASES:
                self.case(case_id(prefix, row), lambda operation=operation, row=row: self.retrieve_case(operation, port, ae, row),
                          requires=[f'qr.seed.retrieve.{server}'])
            for row in RETRIEVE_REFUSALS:
                self.case(f'{prefix}.refuse.{row.name}',
                          lambda operation=operation, row=row: self.retrieve_refusal(operation, port, ae, row),
                          requires=[f'qr.seed.retrieve.{server}'])

    def scp_retrieve(self):
        self.destination = self.output/'destination'
        self.destination.mkdir()
        with self.server(['storescp', '-aet', DESTINATION_AE, '-od', self.destination, '-fe', '.dcm', DESTINATION_PORT],
                         DESTINATION_PORT, log_name='destination'):
            root = self.output/'scp-qr-retrieve'
            root.mkdir()
            self.case('qr.seed.retrieve.qr-scp', lambda: self.seed_qr(root), requires=['qr.sources'])
            with self.server(self.qr_server_args(root), QR_PORT, log_name='qr-scp-retrieve') as process:
                self.retrieve_matrix('qr-scp', QR_PORT, QR_AE, ('get', 'move'))
            self.case('qr.retrieve.qr-scp.exit', lambda: self.server_exit(process, 'qr_scp'))
            root = self.output/'scp-server-retrieve'
            (root/'archive').mkdir(parents=True)
            with self.server(self.pacs_server_args(root), SERVER_PORT, log_name='pacs-server-retrieve') as process:
                self.case('qr.seed.retrieve.pacs-server', self.seed_pacs_server, requires=['qr.sources'])
                self.retrieve_matrix('pacs-server', SERVER_PORT, SERVER_AE, ('get', 'move'))
            self.case('qr.retrieve.pacs-server.exit', lambda: self.server_exit(process, 'pacs_server'))

    # Failure paths with real peers ----------------------------------------------------------

    def fresh_archive(self, name):
        """A private copy, because these cases damage the archive or its index."""
        root = self.output/'faults'/name
        sources = root/'sources'
        sources.mkdir(parents=True)
        for path in self.sources.iterdir():
            shutil.copy(path, sources/path.name)
        self.seed_qr(root, sources)
        return root, sources

    def fault_case(self, name, operation, damage, expected, failed, status):
        def action():
            root, sources = self.fresh_archive(f'{name}-{operation}')
            damage(sources)
            destination = root/'received'
            destination.mkdir()
            row = next(item for item in RETRIEVE_CASES if item.name == 'study' and item.model == 'study-root')
            with self.server(self.qr_server_args(root, sources), QR_PORT, log_name=f'{self.current}-pacs') as process:
                if operation == 'move':
                    with self.server(['storescp', '-aet', DESTINATION_AE, '-od', destination, '-fe', '.dcm', DESTINATION_PORT],
                                     DESTINATION_PORT, log_name=f'{self.current}-destination'):
                        rows, received, listed = self.run_retrieve('move', QR_PORT, QR_AE, row, self.retrieve_file(row), destination)
                else:
                    rows, received, listed = self.run_retrieve('get', QR_PORT, QR_AE, row, self.retrieve_file(row), destination/'get')
            self.server_exit(process, 'qr_scp')
            require(sorted(received) == sorted(expected), f'Receiver holds {sorted(received)}, expected {sorted(expected)}')
            self.check_counters(rows, len(expected)+len(failed), len(expected), len(failed), 0, status)
            uids = sorted(instance_uid(key) for key in failed)
            if operation == 'move':
                require(sorted(listed) == uids, f'Failed SOP Instance UID List {sorted(listed)}, expected {uids}')
            else:
                # getscu reports that the list came but not what it holds; the protocol case
                # get-names-unreadable-object reads the exact list for C-GET.
                require(rows[-1]['data_set'], 'Final response has no Failed SOP Instance UID List')
            return f'{len(expected)} delivered, {len(failed)} failed by name, final {status:04X}'
        self.case(f'qr.fault.{name}.{operation}', action, requires=['qr.sources'])

    def move_destination_fault(self, name, peer, accepted_sops, expected, failed, status):
        def action():
            root, sources = self.fresh_archive(name)
            destination = root/'received'
            destination.mkdir()
            row = next(item for item in RETRIEVE_CASES if item.name == 'study' and item.model == 'study-root')
            config = root/'accept.cfg'
            config.write_text('\n'.join(['[[TransferSyntaxes]]', '[Syntaxes]', 'TransferSyntax1 = 1.2.840.10008.1.2.1',
                                         'TransferSyntax2 = 1.2.840.10008.1.2', '[[PresentationContexts]]', '[Contexts]',
                                         *[f'PresentationContext{i} = {uid}\\Syntaxes' for i, uid in enumerate(accepted_sops, 1)],
                                         '[[Profiles]]', '[Default]', 'PresentationContexts = Contexts'])+'\n')
            with self.server(['storescp', '-aet', DESTINATION_AE, '-xf', config, 'Default', '-od', destination, '-fe', '.dcm',
                              DESTINATION_PORT], DESTINATION_PORT, log_name=f'{self.current}-destination'):
                with self.server(self.qr_server_args(root, sources, peer), QR_PORT, log_name=f'{self.current}-pacs') as process:
                    rows, received, listed = self.run_retrieve('move', QR_PORT, QR_AE, row, self.retrieve_file(row), destination)
            self.server_exit(process, 'qr_scp')
            require(sorted(received) == sorted(expected), f'Receiver holds {sorted(received)}, expected {sorted(expected)}')
            self.check_counters(rows, len(expected)+len(failed), len(expected), len(failed), 0, status)
            uids = sorted(instance_uid(key) for key in failed)
            require(sorted(listed) == uids, f'Failed SOP Instance UID List {sorted(listed)}, expected {uids}')
            return f'{len(expected)} delivered, {len(failed)} failed by name, final {status:04X}'
        self.case(f'qr.fault.{name}', action, requires=['qr.sources'])

    def unknown_destination(self):
        root, sources = self.fresh_archive('unknown-destination')
        row = next(item for item in RETRIEVE_CASES if item.name == 'study' and item.model == 'study-root')
        with self.server(self.qr_server_args(root, sources, f'OTHER_SCP:127.0.0.1:{DESTINATION_PORT}'), QR_PORT,
                         log_name=f'{self.current}-pacs') as process:
            self.destination = root/'received'
            self.destination.mkdir()
            rows, received, _ = self.run_retrieve('move', QR_PORT, QR_AE, row, self.retrieve_file(row))
        self.server_exit(process, 'qr_scp')
        require(not received, 'Objects delivered to an unknown destination')
        require(len(rows) == 1 and rows[0]['status'] == DESTINATION_UNKNOWN, f'Responses {rows}, expected one A801')
        return 'status A801, nothing delivered'

    def database_failure(self, table, request):
        """A failing index must be reported as a failure, never as a search with no results."""
        def action():
            root, sources = self.fresh_archive(f'database-{table}')
            with self.server(self.qr_server_args(root, sources), QR_PORT, log_name=f'{self.current}-pacs') as process:
                with sqlite3.connect(root/'index.db') as connection:
                    connection.execute(f'ALTER TABLE {table} RENAME TO {table}_gone')
                return request(root)
        return action

    def failing_find(self, root):
        statuses, responses = self.run_findscu(QR_PORT, QR_AE, 'study-root', self.query_file('STUDY', {}))
        require(not responses, f'{len(responses)} identifiers returned from a failing index')
        require(len(statuses) == 1 and 0xC000 <= statuses[0] <= 0xCFFF, f'Statuses {[f"{s:04X}" for s in statuses]}, expected Cxxx')
        return f'status {statuses[0]:04X}, no identifiers'

    def failing_retrieve(self, root):
        row = next(item for item in RETRIEVE_CASES if item.name == 'study' and item.model == 'study-root')
        rows, received, _ = self.run_retrieve('get', QR_PORT, QR_AE, row, self.retrieve_file(row), root/'received')
        require(not received, 'Objects delivered from a failing index')
        require(len(rows) == 1 and 0xC000 <= rows[0]['status'] <= 0xCFFF, f'Responses {rows}, expected one Cxxx')
        return f'status {rows[0]["status"]:04X}, nothing delivered'

    def faults(self):
        def remove(*keys):
            def damage(sources):
                for key in keys:
                    (sources/f'{key}.dcm').unlink()
            return damage

        def corrupt(sources):
            (sources/'I1a2.dcm').write_bytes(b'not a DICOM file')

        all_of_study = ['I1a1', 'I1a2', 'I1b1', 'I1c1']
        for operation in ('get', 'move'):
            self.fault_case('file-missing', operation, remove('I1a2'), ['I1a1', 'I1b1', 'I1c1'], ['I1a2'], WARNING)
            self.fault_case('file-corrupt', operation, corrupt, ['I1a1', 'I1b1', 'I1c1'], ['I1a2'], WARNING)
            self.fault_case('all-files-missing', operation, remove(*all_of_study), [], all_of_study, OUT_OF_RESOURCES_SUBOPS)
        self.case('qr.fault.unknown-destination', self.unknown_destination, requires=['qr.sources'])
        self.move_destination_fault('destination-refuses-a-class', None, [CT], ['I1a1', 'I1a2', 'I1b1'], ['I1c1'], WARNING)
        self.move_destination_fault('destination-down', f'{DESTINATION_AE}:127.0.0.1:{CLOSED_PORT}', [CT], [], all_of_study,
                                    OUT_OF_RESOURCES_SUBOPS)
        self.case('qr.fault.database-failure.find', self.database_failure('studies', self.failing_find), requires=['qr.sources'])
        self.case('qr.fault.database-failure.retrieve', self.database_failure('instances', self.failing_retrieve),
                  requires=['qr.sources'])

    # PACS as the SCU against the independent archive ----------------------------------------

    def find_scu_case(self, row):
        results = self.output/'scu-results'/f'{self.current}.json'
        results.parent.mkdir(exist_ok=True)
        args = [self.bin/'find_scu', '127.0.0.1', REFERENCE_PORT, '-aec', REFERENCE_AE, '-aet', SCU_AE,
                '-P' if row.model == 'patient-root' else '-S', '-L', row.level, '-o', 'json', '--output-file', results, '-q']
        for keyword, value in row.keys.items():
            if keyword != 'SpecificCharacterSet':
                args += ['-k', f'{tag_text(keyword)}={value}']
        # Documented: exit 1 means failed or no results, and either way nothing was found.
        self.command(args, timeout=60, allowed=(0,) if row.expected else (1,))
        rows = json.loads(results.read_bytes().decode('utf-8'))
        tag = tag_text(IDENTITY[row.level]).replace(',', '').upper()
        keys = sorted(IDENTITY_TO_KEY[row.level].get(item.get(tag, '').strip(), '?') for item in rows)
        require(keys == sorted(row.expected), f'Result set {keys}, expected {sorted(row.expected)}')
        return f'{len(keys)} exact matches through find_scu'

    def query_scu_case(self, row):
        args = [self.bin/'query_scu', '127.0.0.1', REFERENCE_PORT, REFERENCE_AE, '--calling-ae', SCU_AE, '--model',
                'patient' if row.model == 'patient-root' else 'study', '--level', row.level, '--format', 'json']
        for keyword, value in row.keys.items():
            if keyword != 'SpecificCharacterSet':
                args += [QUERY_SCU_FLAGS[keyword], value]
        document = json.loads(self.command(args, timeout=60, allowed=(0,) if row.expected else (1,)))
        field = QUERY_SCU_IDENTITY[row.level]
        keys = sorted(IDENTITY_TO_KEY[row.level].get(item.get(field, '').strip(), '?') for item in document['results'])
        require(keys == sorted(row.expected), f'Result set {keys}, expected {sorted(row.expected)}')
        return f'{len(keys)} exact matches through query_scu'

    def retrieve_tool_args(self, tool, row, operation, directory):
        model = row.model == 'patient-root'
        if tool == 'retrieve_scu':
            args = [self.bin/tool, '127.0.0.1', REFERENCE_PORT, REFERENCE_AE, '--calling-ae', SCU_AE, '--no-progress',
                    '--model', 'patient' if model else 'study', '--level', row.level, '--mode', operation,
                    '--timeout', 30]
            args += ['--dest-ae', DESTINATION_AE] if operation == 'move' else ['-o', directory]
            for keyword, value in row.keys.items():
                args += [RETRIEVE_SCU_FLAGS[keyword], value]
            return args
        args = [self.bin/tool, '127.0.0.1', REFERENCE_PORT, '-aec', REFERENCE_AE, '-aet', SCU_AE, '-P' if model else '-S',
                '-L', row.level, '--no-progress', '-q']
        args += ['-aem', DESTINATION_AE] if operation == 'move' else ['-od', directory]
        for keyword, value in row.keys.items():
            args += ['-k', f'{tag_text(keyword)}={value}']
        return args

    def retrieve_tool_case(self, tool, operation, row):
        directory = self.output/'scu-received'/self.current if operation == 'get' else self.destination
        if operation == 'get':
            directory.mkdir(parents=True)
        else:
            clear_files(directory)
        self.command(self.retrieve_tool_args(tool, row, operation, directory), timeout=90)
        received = self.received_objects(directory)
        require(sorted(received) == sorted(row.expected), f'Receiver holds {sorted(received)}, expected {sorted(row.expected)}')
        return f'exit 0, {len(received)} exact objects'

    def scu_must_fail(self, args, reason, allowed=None):
        allowed = allowed if allowed is not None else tuple(range(1, 256))
        started = time.monotonic()
        self.command(args, timeout=90, allowed=allowed)
        elapsed = time.monotonic()-started
        require(elapsed < 75, f'{reason}: took {elapsed:.0f} s')
        return f'{reason}: non-zero exit in {elapsed:.1f} s'

    def scu_find_failures(self):
        common = ['-aet', SCU_AE, '-S', '-L', 'PATIENT', '-k', '0010,0020=QR-P1', '-o', 'json', '-q']
        for tool, args, reason in (
                ('find_scu', [self.bin/'find_scu', '127.0.0.1', REFERENCE_PORT, '-aec', REFERENCE_AE, *common], 'refused level'),
                ('query_scu', [self.bin/'query_scu', '127.0.0.1', REFERENCE_PORT, REFERENCE_AE, '--calling-ae', SCU_AE,
                               '--model', 'study', '--level', 'PATIENT', '--patient-id', 'QR-P1', '--format', 'json'], 'refused level'),
                ('find_scu.wrong-ae', [self.bin/'find_scu', '127.0.0.1', REFERENCE_PORT, '-aec', 'NOT_THIS_AE', *common], 'wrong AE'),
                ('query_scu.wrong-ae', [self.bin/'query_scu', '127.0.0.1', REFERENCE_PORT, 'NOT_THIS_AE', '--calling-ae', SCU_AE,
                                        '--model', 'study', '--level', 'STUDY', '--format', 'json'], 'wrong AE'),
                ('find_scu.peer-down', [self.bin/'find_scu', '127.0.0.1', CLOSED_PORT, '-aec', REFERENCE_AE, *common], 'peer down')):
            self.case(f'qr.scu.fail.{tool}', lambda args=args, reason=reason: self.scu_must_fail(args, reason),
                      requires=['qr.sources'])

    def scu_retrieve_failures(self):
        row = next(item for item in RETRIEVE_CASES if item.name == 'study' and item.model == 'study-root')
        for tool in ('get_scu', 'move_scu', 'retrieve_scu'):
            operation = 'move' if tool == 'move_scu' else 'get'
            directory = self.output/'scu-received'/f'{tool}-failures'

            def arguments(tool=tool, operation=operation, directory=directory, row=row):
                return self.retrieve_tool_args(tool, row, operation, directory)

            def wrong_ae(arguments=arguments):
                args = [str(item) for item in arguments()]
                args[args.index(REFERENCE_AE)] = 'NOT_THIS_AE'
                return self.scu_must_fail(args, 'wrong AE')

            def peer_down(arguments=arguments):
                args = [str(item) for item in arguments()]
                args[args.index(str(REFERENCE_PORT))] = str(CLOSED_PORT)
                return self.scu_must_fail(args, 'peer down')

            def refused_level(tool=tool, operation=operation, directory=directory, row=row):
                bad = row._replace(level='PATIENT')
                return self.scu_must_fail(self.retrieve_tool_args(tool, bad, operation, directory), 'refused or empty level',
                                          allowed=tuple(range(0, 256)))

            self.case(f'qr.scu.fail.{tool}.wrong-ae', wrong_ae, requires=['qr.sources'])
            self.case(f'qr.scu.fail.{tool}.peer-down', peer_down, requires=['qr.sources'])

        def unknown_destination(tool, args):
            return self.scu_must_fail(args, 'unknown destination')

        move_scu = [self.bin/'move_scu', '127.0.0.1', REFERENCE_PORT, '-aec', REFERENCE_AE, '-aet', SCU_AE, '-S', '-L', 'STUDY',
                    '--no-progress', '-q', '-aem', 'NOT_A_PEER', '-k', f'0020,000d={row.keys["StudyInstanceUID"]}']
        retrieve_scu = [self.bin/'retrieve_scu', '127.0.0.1', REFERENCE_PORT, REFERENCE_AE, '--calling-ae', SCU_AE, '--no-progress',
                        '--mode', 'move', '--dest-ae', 'NOT_A_PEER', '--level', 'STUDY', '--study-uid', row.keys['StudyInstanceUID'],
                        '--timeout', 30]
        self.case('qr.scu.fail.move_scu.unknown-destination', lambda: unknown_destination('move_scu', move_scu), requires=['qr.sources'])
        self.case('qr.scu.fail.retrieve_scu.unknown-destination', lambda: unknown_destination('retrieve_scu', retrieve_scu),
                  requires=['qr.sources'])

    def scu_partial_failure(self, tool):
        """The independent archive reports a partial failure; the tool must say so, not hang or claim success."""
        root = self.output/f'scu-partial-{tool}'
        config_path = self.reference_archive(f'reference-partial-{tool}')
        destination = root/'received'
        destination.mkdir(parents=True)
        accept = root/'accept.cfg'
        accept.write_text('\n'.join(['[[TransferSyntaxes]]', '[Syntaxes]', 'TransferSyntax1 = 1.2.840.10008.1.2.1',
                                     'TransferSyntax2 = 1.2.840.10008.1.2', '[[PresentationContexts]]', '[Contexts]',
                                     f'PresentationContext1 = {CT}\\Syntaxes', '[[Profiles]]', '[Default]',
                                     'PresentationContexts = Contexts'])+'\n')
        row = next(item for item in RETRIEVE_CASES if item.name == 'study' and item.model == 'study-root')
        with self.server(['storescp', '-aet', DESTINATION_AE, '-xf', accept, 'Default', '-od', destination, '-fe', '.dcm',
                          DESTINATION_PORT], DESTINATION_PORT, log_name=f'{self.current}-destination'):
            with self.reference_server(config_path, log_name=f'{self.current}-reference'):
                self.command(self.retrieve_tool_args(tool, row, 'move', destination), timeout=45, allowed=(1,))
        received = self.received_objects(destination)
        require(sorted(received) == ['I1a1', 'I1a2', 'I1b1'], f'Receiver holds {sorted(received)}')
        return 'exit 1 on Warning B000, three objects delivered, no hang'

    def scu_group(self):
        config = self.reference_archive('reference-scu')
        self.destination = self.output/'scu-destination'
        self.destination.mkdir()
        find_rows = [row for row in find_cases() if case_id('find', row) not in REFERENCE_DIFFERENCES
                     and row.keys.get('SpecificCharacterSet') in (None, 'ISO_IR 192')]
        with self.server(['storescp', '-aet', DESTINATION_AE, '-od', self.destination, '-fe', '.dcm', DESTINATION_PORT],
                         DESTINATION_PORT, log_name='scu-destination'), self.reference_server(config, 'scu-reference'):
            for row in find_rows:
                if row.model == 'patient-study-only':
                    continue  # Declared SCP only: neither tool can propose the retired class.
                self.case(case_id('qr.scu.find_scu', row), lambda row=row: self.find_scu_case(row), requires=['qr.sources'])
                if set(row.keys)-{'SpecificCharacterSet'} <= set(QUERY_SCU_FLAGS):
                    self.case(case_id('qr.scu.query_scu', row), lambda row=row: self.query_scu_case(row), requires=['qr.sources'])
            for tool, operations in (('get_scu', ('get',)), ('move_scu', ('move',)), ('retrieve_scu', ('get', 'move'))):
                for operation in operations:
                    for row in RETRIEVE_CASES:
                        if tool == 'retrieve_scu' and not set(row.keys) <= set(RETRIEVE_SCU_FLAGS):
                            continue
                        if f'{operation}.{row.model}.{row.level.lower()}.{row.name}' in REFERENCE_DIFFERENCES:
                            continue
                        self.case(case_id(f'qr.scu.{tool}.{operation}', row),
                                  lambda tool=tool, operation=operation, row=row: self.retrieve_tool_case(tool, operation, row),
                                  requires=['qr.sources'])
            self.scu_find_failures()
            self.scu_retrieve_failures()
        for tool in ('move_scu', 'retrieve_scu'):
            self.case(f'qr.scu.fail.{tool}.partial', lambda tool=tool: self.scu_partial_failure(tool), requires=['qr.sources'])

    # C-CANCEL from independent tools --------------------------------------------------------

    def bulk_patients(self, root, count):
        self.seed_qr(root, root/'sources', 0)
        with sqlite3.connect(root/'index.db') as connection:
            connection.executemany('INSERT INTO patients (patient_id, patient_name, birth_date, sex) VALUES (?, ?, ?, ?)',
                                   ((f'BULK{i:06}', f'BULK^{i:06}', '19700101', 'O') for i in range(count)))

    def find_cancel(self):
        root = self.output/'cancel-find'
        (root/'sources').mkdir(parents=True)
        count = 60000
        self.bulk_patients(root, count)
        query = self.query_file('PATIENT', {'PatientID': 'BULK*'})
        directory = self.output/'responses'/self.current
        directory.mkdir(parents=True)
        with self.server(self.qr_server_args(root, root/'sources'), QR_PORT, log_name=f'{self.current}-pacs') as process:
            started = time.monotonic()
            self.command(['findscu', '-d', '--cancel', '1', '-to', '10', '-ta', '10', '-td', '60', '-P', '-aet', SCU_AE, '-aec', QR_AE,
                          '-X', '-od', directory, '127.0.0.1', QR_PORT, query], timeout=120)
            elapsed = time.monotonic()-started
        self.server_exit(process, 'qr_scp')
        rows, _ = self.dimse_responses('C-FIND')
        require(rows and rows[-1]['status'] == CANCEL, f'Final status {rows[-1]["status"] if rows else None}, expected FE00')
        delivered = len(rows)-1
        require(delivered < count, f'All {count} matches arrived despite C-CANCEL')
        files = sum(1 for path in directory.iterdir() if path.is_file())
        require(files == delivered, f'{files} response files for {delivered} pending responses')
        return f'findscu --cancel 1: final FE00 after {delivered} of {count} matches in {elapsed:.2f} s'

    def move_cancel(self):
        root = self.output/'cancel-move'
        sources = root/'sources'
        sources.mkdir(parents=True)
        count = 300
        for index in range(count):
            ds = Dataset()
            ds.SOPClassUID, ds.SOPInstanceUID = CT, f'1.2.3.1319.55.1.{index+1}'
            ds.StudyInstanceUID, ds.SeriesInstanceUID = '1.2.3.1319.55', '1.2.3.1319.55.1'
            ds.PatientID, ds.PatientName, ds.Modality = 'CANCEL', 'CANCEL^MOVE', 'CT'
            ds.StudyDate, ds.StudyTime, ds.AccessionNumber, ds.StudyID = '20260101', '120000', '', ''
            ds.SeriesNumber, ds.InstanceNumber = 1, index+1
            ds.Rows, ds.Columns, ds.SamplesPerPixel, ds.PhotometricInterpretation = 2, 2, 1, 'MONOCHROME2'
            ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 8, 8, 7, 0
            ds.PixelData = bytes([1, 2, 3, 4])
            write_part10(ds, sources/f'{index}.dcm', CT, ds.SOPInstanceUID)
        self.seed_qr(root, sources, count)
        destination = root/'received'
        destination.mkdir()
        query = self.query_file('STUDY', {'StudyInstanceUID': '1.2.3.1319.55'})
        with self.server(['storescp', '-aet', DESTINATION_AE, '-od', destination, '-fe', '.dcm', DESTINATION_PORT],
                         DESTINATION_PORT, log_name=f'{self.current}-destination'):
            with self.server(self.qr_server_args(root, sources), QR_PORT, log_name=f'{self.current}-pacs') as process:
                self.command(['movescu', '-d', '--cancel', '1', '-to', '10', '-ta', '10', '-td', '60', '-S', '-aet', SCU_AE, '-aec', QR_AE,
                              '-aem', DESTINATION_AE, '127.0.0.1', QR_PORT, query], timeout=120)
                rows, failed = self.dimse_responses('C-MOVE')
                require(rows and rows[-1]['status'] == CANCEL, f'Final {rows[-1] if rows else None}, expected FE00')
                final = rows[-1]
                time.sleep(1)
                first = sum(1 for path in destination.iterdir() if path.is_file())
                time.sleep(2)
                second = sum(1 for path in destination.iterdir() if path.is_file())
            self.server_exit(process, 'qr_scp')
        require(first == second, f'Objects kept arriving after the final response: {first} then {second}')
        completed, lost = final.get('completed', 0), final.get('failed', 0)
        require(final.get('remaining', 0) > 0, f'Cancel left nothing remaining: {final}')
        require(completed+lost+final.get('warning', 0)+final['remaining'] == count, f'Counters do not total {count}: {final}')
        # A store in flight when the cancel took effect may have landed although it counts as failed.
        require(completed <= second <= completed+lost, f'Receiver holds {second}, counters say {final}')
        require(len(failed) == lost, f'{len(failed)} failed UIDs for {lost} failed sub-operations')
        return f'movescu --cancel 1: FE00, receiver steady at {second}, counters {final}'

    def cancel_group(self):
        self.case('qr.cancel.find', self.find_cancel, requires=['qr.sources'])
        self.case('qr.cancel.move', self.move_cancel, requires=['qr.sources'])

    # Verification, the other half of this job's declared contract ---------------------------

    def echo_case(self, args, expect_refused):
        allowed = tuple(range(1, 256)) if expect_refused else (0,)
        self.command(args, timeout=30, allowed=allowed)
        return 'refused with a non-zero exit' if expect_refused else 'C-ECHO completed, exit 0'

    def verification_group(self):
        accept = self.output/'verification-accept.cfg'
        accept.write_text('\n'.join(['[[TransferSyntaxes]]', '[Syntaxes]', 'TransferSyntax1 = 1.2.840.10008.1.2.1',
                                     '[[PresentationContexts]]', '[Contexts]', f'PresentationContext1 = {CT}\\Syntaxes',
                                     '[[Profiles]]', '[Default]', 'PresentationContexts = Contexts'])+'\n')
        root = self.output/'verification-qr'
        root.mkdir()
        (root/'sources').mkdir()
        self.seed_qr(root, root/'sources', 0)
        echo_scu = [self.bin/'echo_scu', '-aet', SCU_AE]
        with self.server(self.qr_server_args(root, root/'sources'), QR_PORT, log_name='verification-qr') as process:
            self.case('qr.verification.scp.qr-scp', lambda: self.echo_case(
                ['echoscu', '-aet', SCU_AE, '-aec', QR_AE, '127.0.0.1', QR_PORT], False))
            self.case('qr.verification.scp-refused.wrong-ae', lambda: self.echo_case(
                ['echoscu', '-aet', SCU_AE, '-aec', 'NOT_THIS_AE', '127.0.0.1', QR_PORT], True))
        self.case('qr.verification.scp.qr-scp.exit', lambda: self.server_exit(process, 'qr_scp'))
        server_root = self.output/'verification-server'
        (server_root/'archive').mkdir(parents=True)
        with self.server(self.pacs_server_args(server_root), SERVER_PORT, log_name='verification-server') as process:
            self.case('qr.verification.scp.pacs-server', lambda: self.echo_case(
                ['echoscu', '-aet', SCU_AE, '-aec', SERVER_AE, '127.0.0.1', SERVER_PORT], False))
        self.case('qr.verification.scp.pacs-server.exit', lambda: self.server_exit(process, 'pacs_server'))
        with self.server(['storescp', '-aet', DESTINATION_AE, '-od', self.output, DESTINATION_PORT], DESTINATION_PORT,
                         log_name='verification-peer'):
            self.case('qr.verification.scu.positive', lambda: self.echo_case(
                [*echo_scu, '-aec', DESTINATION_AE, '127.0.0.1', DESTINATION_PORT], False))
        with self.server(['storescp', '-aet', DESTINATION_AE, '-xf', accept, 'Default', '-od', self.output, DESTINATION_PORT],
                         DESTINATION_PORT, log_name='verification-peer-without-echo'):
            self.case('qr.verification.scu-refused.no-context', lambda: self.echo_case(
                [*echo_scu, '-aec', DESTINATION_AE, '127.0.0.1', DESTINATION_PORT], True))
        self.case('qr.verification.scu-refused.peer-down', lambda: self.echo_case(
            [*echo_scu, '-aec', DESTINATION_AE, '127.0.0.1', CLOSED_PORT], True))

    # Protocol probes with a controlled peer -------------------------------------------------

    def protocol_probe(self, label, script, *arguments, fixture=True):
        directory = self.output/'protocol'/label
        directory.parent.mkdir(exist_ok=True)
        args = ['python3', '-B', Path(__file__).with_name(script), '--output', directory, *arguments]
        if fixture:
            args += ['--fixture', self.sources/'I1a1.dcm']
        self.command(args, timeout=540, allowed=ANY_EXIT)
        summary = directory/'summary.json'
        if not summary.exists():
            trace = (self.output/f'{self.current}.log').read_text(errors='replace')
            require('\nEXIT 0\n' in trace, f'{script} failed; see {self.current}.log')
            return f'{script} completed with exit 0'
        rows = json.loads(summary.read_text())
        failed = [row for row in rows if row['status'] != 'passed']
        self.protocol_rows[label] = rows
        require(not failed, f'{len(failed)} of {len(rows)} failed: '+', '.join(row['case'] for row in failed))
        return f'{len(rows)} cases passed'

    def protocol_group(self):
        self.protocol_rows = {}
        for label, script, arguments, fixture in (
                ('find-scp', 'qr_protocol_probe.py', ('--binary', self.bin/'qr_scp'), False),
                ('get-scp', 'get_scp_protocol_probe.py', ('--binary', self.bin/'qr_scp'), True),
                ('move-scp', 'move_scp_protocol_probe.py', ('--binary', self.bin/'qr_scp'), True),
                ('retrieve-scu', 'get_scu_protocol_probe.py', ('--binary', self.bin/'retrieve_scu'), True)):
            self.case(f'qr.protocol.{label}', lambda label=label, script=script, arguments=arguments, fixture=fixture:
                      self.protocol_probe(label, script, *arguments, fixture=fixture), requires=['qr.sources'])
            for row in self.protocol_rows.get(label, []):
                detail = row.get('detail', row.get('error', ''))
                self.case(f'qr.protocol.{label}.{row["case"]}', lambda row=row, detail=detail: protocol_verdict(row, detail),
                          requires=['qr.sources'])

    # Report ---------------------------------------------------------------------------------

    def requirement_results(self):
        """Each owned requirement with the cases behind it. No case means not evaluated, never passed."""
        status = {row['id']: row['status'] for row in self.results}
        results = {}
        declared = declared_support(Path(__file__).resolve().parents[3])
        for requirement, patterns in requirement_cases(declared).items():
            matched = [case for case in status if any(fnmatch.fnmatchcase(case, pattern) for pattern in patterns)]
            results[requirement] = dict(patterns=patterns, cases=len(matched),
                                        passed=sum(status[case] == 'passed' for case in matched),
                                        status='unexecuted' if not matched else
                                        'passed' if all(status[case] == 'passed' for case in matched) else 'failed')
        return results

    def run_all(self, groups):
        self.case('qr.sources', self.build_sources, phase='fixture_preparation')
        if 'scp-find' in groups:
            self.scp_find()
        if 'reference' in groups:
            self.reference_group()
        if 'scp-retrieve' in groups:
            self.scp_retrieve()
        if 'faults' in groups:
            self.faults()
        if 'scu' in groups:
            self.scu_group()
        if 'cancel' in groups:
            self.cancel_group()
        if 'protocol' in groups:
            self.protocol_group()
        if 'verification' in groups:
            self.verification_group()
        self.final = True
        self.save()
        counts = {status: sum(1 for row in self.results if row['status'] == status)
                  for status in ('passed', 'failed', 'blocked')}
        (self.output/'qr-summary.json').write_text(json.dumps(dict(
            counts=counts, requirement_results=self.requirement_results(),
            reference_differences=sorted(REFERENCE_DIFFERENCES),
            failures=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'failed'],
            blocked=[row['id'] for row in self.results if row['status'] == 'blocked']), indent=1)+'\n')
        shutil.rmtree(self.queries, ignore_errors=True)
        print(json.dumps(counts))
        return int(counts['failed'] > 0 or counts['blocked'] > 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--source-digest', required=True)
    parser.add_argument('--groups', default='scp-find,reference,scp-retrieve,faults,scu,cancel,protocol,verification')
    args = parser.parse_args()
    args.profile = 'uncompressed'
    return QrMatrix(args).run_all(set(args.groups.split(',')))


if __name__ == '__main__':
    raise SystemExit(main())
