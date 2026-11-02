#!/usr/bin/env python3
"""DIMSE-N services against peers that share no PACS code (#1320).

Every peer runs as its own process. The `peers` group runs first with no PACS process at all: each pair
of tools and each expected table must work between independent implementations before any PACS result
counts, and a product case whose prerequisite failed is reported as blocked.
"""
import argparse
import base64
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sqlite3
import subprocess
import sys
import threading
import time
import warnings

import pydicom
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.uid import ExplicitVRLittleEndian

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
warnings.filterwarnings('ignore', category=UserWarning)
from dimse_n import (COLOR_IMAGE_BOX, COLOR_META, FILM_BOX, FILM_SESSION, GRAY_IMAGE_BOX, GRAY_META, GRAY_PIXELS, MPPS, MPPS_REFUSALS,
                     MWL, MWL_ITEMS, MWL_QUERIES, PERFORMED_REQUIRED, PRINTER, PRINTER_INSTANCE, SECONDARY_CAPTURE, SESSION,
                     STGCMT_REFUSALS, STORAGE_COMMITMENT, STORAGE_COMMITMENT_INSTANCE, SUCCESS, UNRECOGNIZED_OPERATION, UPS_EVENT,
                     UPS_FILTERED_GLOBAL, UPS_GLOBAL, UPS_OPERATIONS, UPS_PULL, UPS_PUSH, UPS_QUERY, UPS_SET_CASES, UPS_STATES, UPS_TABLE,
                     UPS_WATCH, change_state, commitment_request, film_box_values, image_box_values, mpps_completion, mpps_create,
                     mpps_set, n_create_ups, n_get_ups, n_set_ups, print_step, request_cancel, requirement_cases, ups_cancellation,
                     ups_create, ups_event_step, ups_performed, ups_setup, watch_action)
from run import Validation, require
from support import declared_support

PACS_PORT, MPPS_TOOL_PORT, MWL_TOOL_PORT = 23200, 23201, 23202
PEER_MPPS_PORT, PEER_MWL_PORT, PEER_QR_PORT, PEER_UPS_PORT, PEER_PRINT_PORT = 23210, 23211, 23212, 23213, 23214
STGCMT_LISTEN_PORT, RECEIVER_PORT, RECEIVER2_PORT, SCU_LISTEN_PORT, LATE_PORT, PRINT_SCRIPTED_PORT, DEAD_PORT = 23220, 23221, 23222, 23223, 23224, 23225, 23229
PACS_AE, PEER_AE, SCU_AE = 'PACS_N', 'PEER_N', 'SCRIPTED_SCU'
STGCMT_AE, RECEIVER_AE, RECEIVER2_AE, UNREACHABLE_AE, SCU_LISTEN_AE = 'STGSCU', 'WATCHER', 'WATCHER2', 'UNREACHABLE', 'DRIVER_SCU'
UPS_CONTEXTS = (UPS_PUSH, UPS_PULL, UPS_WATCH, UPS_QUERY)
PRINT_CONTEXTS = (GRAY_META, COLOR_META, PRINTER)
IMAGE_STUDY, IMAGE_SERIES, IMAGE_ACCESSION = '1.2.826.0.1.3680043.8.498.1320.200', '1.2.826.0.1.3680043.8.498.1320.300', 'A1'
COLOR_PIXELS = bytes(range(12))
RSP = re.compile(r'>> \d+:([NC]-[A-Z-]+-RSP)\[[^\]\n]*?status=([0-9a-fA-F]+)H')
OBJECTS = {name: f'1.2.826.0.1.3680043.8.498.1320.100.{number}' for number, name in enumerate(('kept', 'second', 'never-stored', 'vanishes'), 1)}
# dcm4che dcmqrscp ends the association on a commitment request with no Referenced SOP Sequence or Transaction
# UID, and dcmprscp and PS3.4 differ here and there, so these lists are what each reference really answers;
# PACS is held to PS3.4 and PS3.7. Each difference is a recorded fact, not a relaxation of the PACS expectation.
REFERENCE_STGCMT_REFUSALS = {'wrong-action-type': '0123'}


def rsp_statuses(text):
    return [(match[1], match[2].upper().zfill(4)) for match in RSP.finditer(text)]


def write_image(path, instance, patient='N^OBJECT'):
    ds = Dataset()
    ds.SOPClassUID, ds.SOPInstanceUID = SECONDARY_CAPTURE, instance
    ds.StudyInstanceUID, ds.SeriesInstanceUID = IMAGE_STUDY, IMAGE_SERIES
    ds.PatientName, ds.PatientID, ds.StudyDate, ds.StudyTime = patient, 'NOBJ', '20261102', '101010'
    ds.StudyID, ds.AccessionNumber, ds.Modality, ds.SeriesNumber, ds.InstanceNumber = 'S1', IMAGE_ACCESSION, 'OT', '1', '1'
    ds.ConversionType = 'WSD'
    ds.Rows, ds.Columns, ds.BitsAllocated, ds.BitsStored, ds.HighBit = 4, 4, 8, 8, 7
    ds.SamplesPerPixel, ds.PixelRepresentation, ds.PhotometricInterpretation = 1, 0, 'MONOCHROME2'
    ds.PixelData = GRAY_PIXELS
    meta = FileMetaDataset()
    meta.MediaStorageSOPClassUID, meta.MediaStorageSOPInstanceUID = ds.SOPClassUID, ds.SOPInstanceUID
    meta.TransferSyntaxUID = ExplicitVRLittleEndian
    ds.file_meta = meta
    ds.is_little_endian, ds.is_implicit_VR = True, False
    ds.save_as(path, write_like_original=False)


def write_color_image(path, instance):
    write_image(path, instance)
    ds = pydicom.dcmread(path)
    ds.Rows, ds.Columns, ds.SamplesPerPixel, ds.PlanarConfiguration, ds.PhotometricInterpretation = 2, 2, 3, 0, 'RGB'
    ds.PixelData = COLOR_PIXELS
    ds.save_as(path, write_like_original=False)


class Target:
    def __init__(self, name, port, ae):
        self.name, self.port, self.ae = name, port, ae


class DimseN(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.peers = Path(args.peers_dir)
        self.dcm4che = self.peers/'dcm4che-5.35.2/bin'
        self.scripts = Path(__file__).resolve().parents[2]/'peers/dimse-n'
        os.environ['PYTHONPATH'] = f'{self.peers/"python"}:{self.scripts}'
        self.work = self.output/'work'
        self.work.mkdir()
        self.images = self.work/'images'
        self.sequence = 0
        self.uid_base = f'2.25.1320{int(time.time())}'

    # -- plumbing

    def uid(self, *parts):
        self.sequence += 1
        return '.'.join([self.uid_base, str(self.sequence), *map(str, parts)])

    def scripted(self, target, steps, contexts, calling=SCU_AE, **extra):
        self.sequence += 1
        spec, result = self.work/f'{self.sequence}-spec.json', self.work/f'{self.sequence}-result.json'
        spec.write_text(json.dumps(dict(host='127.0.0.1', port=target.port, called_ae=target.ae, calling_ae=calling,
                                        contexts=list(contexts), steps=steps, **extra), indent=1))
        self.command(['python3', self.scripts/'scripted_scu.py', spec, result], timeout=120)
        return json.loads(result.read_text())

    def tool(self, args, timeout=60, allowed=(0,)):
        """Run a tool and return stdout and stderr; dcm4che logs its DIMSE statuses, and its exit does not state them."""
        log = self.output/f'{self.current}.log'
        before = log.stat().st_size if log.exists() else 0
        self.command(args, timeout=timeout, allowed=allowed)
        with log.open(errors='replace') as handle:
            handle.seek(before)
            return handle.read()

    @staticmethod
    def statuses(result):
        return [row['status'] for row in result['steps']]

    def established(self, result):
        require(result['established'], f'association not established (rejected={result["rejected"]}, aborted={result["aborted"]})')
        return result

    @staticmethod
    def records(path):
        path = Path(path)
        return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []

    def events(self, path, count=None, timeout=12):
        """Events a receiver process wrote; waits for `count` of them, then a moment longer to catch extras."""
        deadline = time.monotonic()+timeout
        rows = []
        while time.monotonic() < deadline:
            rows = [row for row in self.records(path) if row.get('kind') == 'event']
            if count is not None and len(rows) >= count:
                time.sleep(.3)
                return [row for row in self.records(path) if row.get('kind') == 'event']
            time.sleep(.1)
        return rows

    def receiver(self, ae, port, record, *extra, log=None):
        return self.server(['python3', self.scripts/'event_receiver.py', '--aet', ae, '--port', port, '--record', record, *extra],
                           port, log_name=log or f'{self.current}-receiver-{ae}')

    def pacs_args(self, root, peers=(), **options):
        root = Path(root)
        args = [self.bin/'pacs_server', '--port', PACS_PORT, '--ae-title', PACS_AE, '--storage-dir', root/'archive', '--db-path', root/'index.db']
        for peer in peers:
            args += ['--peer', peer]
        help_text = subprocess.run([str(self.bin/'pacs_server'), '--help'], capture_output=True, text=True).stdout
        for option, value in options.items():
            flag = '--'+option.replace('_', '-')
            if flag in help_text:
                args += [flag, value]
        return args

    def group_with(self, name, make_server, body):
        """Run the cases of `body` inside a server's lifetime; a server that cannot start is itself a failed case."""
        try:
            with make_server() as process:
                body(process)
        except Exception as exc:
            message = str(exc)

            def report():
                raise ValueError(message)
            self.case(f'{name}.server', report)

    def query_db(self, root, sql, *arguments):
        with sqlite3.connect(f'file:{Path(root)/"index.db"}?mode=ro', uri=True, timeout=10) as connection:
            connection.row_factory = sqlite3.Row
            return [dict(row) for row in connection.execute(sql, arguments)]

    def seed_objects(self, port, ae, *names):
        for name in names:
            self.tool(['storescu', '-aet', SCU_AE, '-aec', ae, '127.0.0.1', port, self.images/f'{name}.dcm'])

    # -- fixtures and versions

    def fixtures(self):
        self.images.mkdir()
        for name, uid in OBJECTS.items():
            write_image(self.images/f'{name}.dcm', uid)
        return f'{len(OBJECTS)} objects'

    def versions(self):
        versions = dict(java=self.tool(['java', '-version'])[:200].split('\n')[1:2],
                        dcm4che=re.findall(r'upsscu: (\S+)', self.tool([self.dcm4che/'upsscu', '-V'])),
                        dcmtk=subprocess.run(['dcmprscp', '--version'], capture_output=True, text=True).stdout.split('\n')[0])
        code = 'import pydicom, pynetdicom; print(pynetdicom.__version__, pydicom.__version__)'
        versions['pynetdicom_pydicom'] = subprocess.run(['python3', '-c', code], capture_output=True, text=True).stdout.strip()
        (self.output/'peer-versions.json').write_text(json.dumps(versions, indent=1)+'\n')
        # 2.1.1's wheel still states 2.1.0 in __version__; the wheel's digest in install.py is what pins it.
        require(versions['pynetdicom_pydicom'].startswith('2.1.'), f'pynetdicom is not the pinned series: {versions}')
        require(versions['dcm4che'] == ['5.35.2'], f'dcm4che is not 5.35.2: {versions}')
        return json.dumps(versions)

    # -- MPPS

    def mppsscu(self, target, *arguments):
        return self.tool([self.dcm4che/'mppsscu', '-b', 'MPPSSCU', '-c', f'{target.ae}@127.0.0.1:{target.port}', *arguments])

    def peer_mpps(self):
        directory = self.work/'peer-mpps'
        target = Target('dcm4che-mppsscp', PEER_MPPS_PORT, PEER_AE)
        with self.server([self.dcm4che/'mppsscp', '-b', f'{PEER_AE}:{PEER_MPPS_PORT}', '--directory', directory], PEER_MPPS_PORT):
            uid = self.uid()
            found = rsp_statuses(self.mppsscu(target, '--ppsuid', uid, self.images/'kept.dcm'))
            require(found == [('N-CREATE-RSP', SUCCESS), ('N-SET-RSP', SUCCESS)], f'mppsscu statuses: {found}')
            stored = pydicom.dcmread(directory/uid, force=True)
            require(stored.PerformedProcedureStepStatus == 'COMPLETED', 'mppsscp did not hold the final status')
            require(self.statuses(self.scripted(target, [mpps_create(self.uid())], [MPPS])) == [SUCCESS], 'scripted create refused by mppsscp')
            self.mpps_refusals(target)
        return 'mppsscu to mppsscp, and the refusal table holds on dcm4che'

    def mpps_refusals(self, target):
        for name, setup, request, expected in MPPS_REFUSALS:
            uid = self.uid()
            steps = [*setup(uid), request(uid)]
            found = self.statuses(self.established(self.scripted(target, steps, [MPPS])))
            require(found == [SUCCESS]*(len(steps)-1)+[expected], f'{name}: expected {expected}, got {found}')

    def mpps_row(self, root, uid):
        rows = self.query_db(root, 'SELECT * FROM mpps WHERE mpps_uid = ?', uid)
        require(len(rows) == 1, f'{uid}: {len(rows)} MPPS rows')
        return rows[0]

    def mpps_group(self):
        target = Target('pacs_server', PACS_PORT, PACS_AE)
        root = self.work/'mpps-pacs'
        requires = ['dimse_n.peer.mpps']

        def completes():
            uid = self.uid()
            found = rsp_statuses(self.mppsscu(target, '--ppsuid', uid, self.images/'kept.dcm'))
            require(found == [('N-CREATE-RSP', SUCCESS), ('N-SET-RSP', SUCCESS)], f'mppsscu statuses: {found}')
            row = self.mpps_row(root, uid)
            require(row['status'] == 'COMPLETED', f'row status {row["status"]}')
            # The peer names these inside the Scheduled Step Attributes Sequence, where PS3.4 puts them.
            require(row['study_uid'] == IMAGE_STUDY and row['accession_no'] == IMAGE_ACCESSION, f'study and accession in the row: {row}')
            return f'dcm4che mppsscu to pacs_server; row {row["status"]}, with the study it named'

        def discontinues():
            uid = self.uid()
            found = rsp_statuses(self.mppsscu(target, '--ppsuid', uid, '--dc', self.images/'kept.dcm'))
            require(found == [('N-CREATE-RSP', SUCCESS), ('N-SET-RSP', SUCCESS)], f'mppsscu statuses: {found}')
            row = self.mpps_row(root, uid)
            require(row['status'] == 'DISCONTINUED', f'row status {row["status"]}')
            return row['status']

        def refusals():
            self.mpps_refusals(target)
            return f'{len(MPPS_REFUSALS)} refusals equal dcm4che mppsscp'

        def in_progress_survives():
            self.mpps_restart_uid = self.uid()
            found = self.statuses(self.established(self.scripted(target, [mpps_create(self.mpps_restart_uid)], [MPPS])))
            require(found == [SUCCESS], f'create: {found}')
            require(self.mpps_row(root, self.mpps_restart_uid)['status'] == 'IN PROGRESS', 'row not IN PROGRESS')
            return 'IN PROGRESS row written before the restart'

        def body(_):
            self.case('dimse_n.mpps.scp.pacs-server.completes', completes, requires=requires)
            self.case('dimse_n.mpps.scp.pacs-server.discontinues', discontinues, requires=requires)
            self.case('dimse_n.mpps.scp.pacs-server.refusals', refusals, requires=requires)
            self.case('dimse_n.mpps.scp.pacs-server.persist-before-restart', in_progress_survives, requires=requires)
        self.group_with('dimse_n.mpps.pacs-server', lambda: self.server(self.pacs_args(root), PACS_PORT, log_name='mpps-pacs-server'), body)

        def completes_after_restart():
            uid = self.mpps_restart_uid
            found = self.statuses(self.established(self.scripted(target, [mpps_set(uid, mpps_completion()), mpps_set(uid, mpps_completion())], [MPPS])))
            require(found == [SUCCESS, '0110'], f'after restart: {found}')
            require(self.mpps_row(root, uid)['status'] == 'COMPLETED', 'row not COMPLETED')
            return 'the same instance completed after the restart, then refused a further N-SET'
        self.group_with('dimse_n.mpps.pacs-server-restart',
                        lambda: self.server(self.pacs_args(root), PACS_PORT, log_name='mpps-pacs-server-restart'),
                        lambda _: self.case('dimse_n.mpps.scp.pacs-server.persist-after-restart', completes_after_restart,
                                            requires=['dimse_n.mpps.scp.pacs-server.persist-before-restart']))
        self.mpps_tool_group(requires)
        self.mpps_scu_group(requires)

    def mpps_tool_group(self, requires):
        directory = self.work/'mpps-tool'
        directory.mkdir()
        target = Target('mpps_scp', MPPS_TOOL_PORT, PACS_AE)

        def completes():
            uid = self.uid()
            found = rsp_statuses(self.mppsscu(target, '--ppsuid', uid, self.images/'kept.dcm'))
            require(found == [('N-CREATE-RSP', SUCCESS), ('N-SET-RSP', SUCCESS)], f'mppsscu statuses: {found}')
            kept = ' '.join(path.read_text() for path in directory.rglob('*') if path.is_file())
            require(uid in kept and 'COMPLETED' in kept, 'mpps_scp did not record the final state')
            return 'dcm4che mppsscu to mpps_scp, recorded COMPLETED'

        def refusals():
            self.mpps_refusals(target)
            return f'{len(MPPS_REFUSALS)} refusals equal dcm4che mppsscp'
        self.group_with('dimse_n.mpps.mpps-scp',
                        lambda: self.server([self.bin/'mpps_scp', MPPS_TOOL_PORT, PACS_AE, '--output-dir', directory], MPPS_TOOL_PORT, log_name='mpps-tool'),
                        lambda _: (self.case('dimse_n.mpps.scp.mpps-scp.completes', completes, requires=requires),
                                   self.case('dimse_n.mpps.scp.mpps-scp.refusals', refusals, requires=requires)))

    def mpps_scu_group(self, requires):
        directory = self.work/'mpps-scu'
        target = Target('dcm4che-mppsscp', PEER_MPPS_PORT, PEER_AE)

        def scu(operation, *arguments, allowed=(0,)):
            return self.tool([self.bin/'mpps_scu', '127.0.0.1', PEER_MPPS_PORT, PEER_AE, operation, *arguments, '--calling-ae', 'PACS_MPPS_SCU'], allowed=allowed)

        def completes():
            uid, series, study = self.uid(), self.uid(), self.uid()
            scu('create', '--mpps-uid', uid, '--patient-id', 'SCU1', '--patient-name', 'SCU^ONE', '--modality', 'OT', '--study-uid', study)
            created = pydicom.dcmread(directory/uid, force=True)
            require(created.PerformedProcedureStepStatus == 'IN PROGRESS', 'peer did not store IN PROGRESS')
            require(created.PatientID == 'SCU1' and str(created.PatientName) == 'SCU^ONE', 'identity differs at the peer')
            scu('set', '--mpps-uid', uid, '--status', 'COMPLETED', '--series-uid', series)
            final = pydicom.dcmread(directory/uid, force=True)
            require(final.PerformedProcedureStepStatus == 'COMPLETED', 'peer did not store COMPLETED')
            require(any(item.SeriesInstanceUID == series for item in final.PerformedSeriesSequence), 'series not at the peer')
            return 'mpps_scu to dcm4che mppsscp with IOD validation on; peer holds COMPLETED and the series'

        def unknown():
            scu('set', '--mpps-uid', self.uid(), '--status', 'COMPLETED', '--series-uid', self.uid(), allowed=(1,))
            return 'N-SET of an unknown instance ends in exit 1'

        def duplicate():
            uid = self.uid()
            scu('create', '--mpps-uid', uid, '--patient-id', 'SCU2', '--modality', 'OT', '--study-uid', self.uid())
            scu('create', '--mpps-uid', uid, '--patient-id', 'SCU2', '--modality', 'OT', '--study-uid', self.uid(), allowed=(1,))
            return 'a duplicate N-CREATE ends in exit 1'
        self.group_with('dimse_n.mpps.mpps-scu',
                        lambda: self.server([self.dcm4che/'mppsscp', '-b', f'{PEER_AE}:{PEER_MPPS_PORT}', '--directory', directory], PEER_MPPS_PORT, log_name='mpps-scu-peer'),
                        lambda _: (self.case('dimse_n.mpps.scu.completes', completes, requires=requires),
                                   self.case('dimse_n.mpps.scu.unknown-instance', unknown, requires=requires),
                                   self.case('dimse_n.mpps.scu.duplicate-create', duplicate, requires=requires)))
        self.mpps_scu_target = target

    # -- Worklist

    def findscu_mwl(self, target, keys, directory):
        directory.mkdir(parents=True)
        args = [self.dcm4che/'findscu', '-b', 'MWLSCU', '-c', f'{target.ae}@127.0.0.1:{target.port}', '-M', 'MWL',
                '--out-dir', directory, '--out-file', 'match']
        for key in keys:
            args += ['-m', key]
        text = self.tool([*args, '--', self.dcm4che.parent/'etc/findscu/mwl.xml'])
        finals = [status for name, status in rsp_statuses(text) if name == 'C-FIND-RSP' and status not in ('FF00', 'FF01')]
        require(finals == [SUCCESS], f'final C-FIND status: {finals}')
        return [pydicom.dcmread(path, force=True) for path in sorted(directory.iterdir())]

    def check_mwl_row(self, row, item, unstored=()):
        for keyword in ('PatientID', 'AccessionNumber', 'StudyInstanceUID', 'RequestedProcedureID', 'RequestedProcedureDescription'):
            if keyword not in unstored:
                require(str(row.get(keyword, '')).strip() == item[keyword], f'{keyword}: {row.get(keyword)!r} is not {item[keyword]!r}')
        require(str(row.get('PatientName', '')) == item['PatientName'], f'PatientName: {row.get("PatientName")}')
        steps = row.get('ScheduledProcedureStepSequence')
        require(steps is not None and len(steps) == 1, f'{item["PatientID"]}: no Scheduled Procedure Step Sequence item')
        for keyword in ('Modality', 'ScheduledStationAETitle', 'ScheduledProcedureStepStartDate', 'ScheduledProcedureStepStartTime',
                        'ScheduledProcedureStepID', 'ScheduledProcedureStepDescription'):
            require(str(steps[0].get(keyword, '')).strip() == item[keyword], f'{keyword}: {steps[0].get(keyword)!r} is not {item[keyword]!r}')

    def mwl_matrix(self, target, prefix, requires, unstored=()):
        by_id = {item['PatientID']: item for item in MWL_ITEMS}

        def query(name, keys, expected):
            def run():
                rows = self.findscu_mwl(target, keys, self.work/f'{prefix}-{self.uid()}')
                found = sorted(str(row.get('PatientID', '')) for row in rows)
                require(found == sorted(expected), f'{name}: matched {found}, expected {sorted(expected)}')
                for row in rows:
                    self.check_mwl_row(row, by_id[str(row.PatientID)], unstored)
                return f'{len(rows)} rows with their scheduled step'
            return run
        for name, keys, expected in MWL_QUERIES:
            self.case(f'{prefix}.{name}', query(name, keys, expected), requires=requires)

    def mwl_identifier(self):
        step = dict(Modality='', ScheduledStationAETitle='', ScheduledProcedureStepStartDate='', ScheduledProcedureStepStartTime='',
                    ScheduledProcedureStepID='', ScheduledProcedureStepDescription='')
        return dict(PatientID='', PatientName='', ScheduledProcedureStepSequence=[step])

    def write_mwl_peer_items(self):
        path = self.work/'mwl-items.json'
        path.write_text(json.dumps(MWL_ITEMS))
        return path

    def peer_mwl(self):
        target = Target('scripted-mwl', PEER_MWL_PORT, PEER_AE)
        record = self.work/'peer-mwl.jsonl'
        with self.server(['python3', self.scripts/'mwl_scp.py', '--aet', PEER_AE, '--port', PEER_MWL_PORT, '--items', self.write_mwl_peer_items(), '--record', record], PEER_MWL_PORT):
            rows = self.findscu_mwl(target, ('ScheduledProcedureStepSequence.Modality=CT',), self.work/'peer-mwl-out')
            require(sorted(str(row.PatientID) for row in rows) == ['MWL1', 'MWL3'], f'scripted MWL peer matched {[row.PatientID for row in rows]}')
            steps = self.records(record)[-1]['keys']['ScheduledProcedureStepSequence']
            require(steps[0]['Modality'] == 'CT', f'peer saw keys {steps}')
        return 'dcm4che findscu -M MWL against the scripted MWL SCP'

    def worklist_tool_items(self):
        path = self.work/'worklist.json'
        records = [dict(patientId=item['PatientID'], patientName=item['PatientName'], accessionNumber=item['AccessionNumber'],
                        studyInstanceUid=item['StudyInstanceUID'], modality=item['Modality'], scheduledStationAeTitle=item['ScheduledStationAETitle'],
                        scheduledProcedureStepStartDate=item['ScheduledProcedureStepStartDate'],
                        scheduledProcedureStepStartTime=item['ScheduledProcedureStepStartTime'],
                        scheduledProcedureStepId=item['ScheduledProcedureStepID'],
                        scheduledProcedureStepDescription=item['ScheduledProcedureStepDescription'],
                        requestedProcedureId=item['RequestedProcedureID'], requestedProcedureDescription=item['RequestedProcedureDescription'])
                   for item in MWL_ITEMS]
        path.write_text(json.dumps(records))
        return path

    def seed_pacs_worklist(self, root):
        """The public way to add items is the REST API; the DIMSE service only reads the table it fills."""
        with sqlite3.connect(Path(root)/'index.db', timeout=10) as connection:
            for item in MWL_ITEMS:
                connection.execute(
                    'INSERT INTO worklist (step_id, patient_id, patient_name, accession_no, requested_proc_id, study_uid, scheduled_datetime, '
                    'station_ae, modality, procedure_desc) VALUES (?,?,?,?,?,?,?,?,?,?)',
                    (item['ScheduledProcedureStepID'], item['PatientID'], item['PatientName'], item['AccessionNumber'], item['RequestedProcedureID'],
                     item['StudyInstanceUID'], item['ScheduledProcedureStepStartDate']+item['ScheduledProcedureStepStartTime'],
                     item['ScheduledStationAETitle'], item['Modality'], item['ScheduledProcedureStepDescription']))
        return f'{len(MWL_ITEMS)} items'

    def mwl_group(self):
        requires = ['dimse_n.peer.mwl']
        tool = Target('worklist_scp', MWL_TOOL_PORT, PACS_AE)

        def refuse_without_sequence(target):
            def run():
                result = self.scripted(target, [dict(op='c_find', model=MWL, dataset=dict(PatientID=''))], [MWL])
                require(self.statuses(self.established(result)) == ['A900'], f'statuses {self.statuses(result)}')
                return 'an identifier without the Scheduled Procedure Step Sequence is refused with A900'
            return run

        def tool_body(_):
            self.mwl_matrix(tool, 'dimse_n.mwl.scp.worklist-scp', requires)
            self.case('dimse_n.mwl.scp.worklist-scp.refuse.missing-sequence', refuse_without_sequence(tool), requires=requires)
        self.group_with('dimse_n.mwl.worklist-scp',
                        lambda: self.server([self.bin/'worklist_scp', MWL_TOOL_PORT, PACS_AE, '--worklist-file', self.worklist_tool_items()], MWL_TOOL_PORT, log_name='mwl-tool'),
                        tool_body)
        root = self.work/'mwl-pacs'
        target = Target('pacs_server', PACS_PORT, PACS_AE)

        def body(_):
            time.sleep(.5)
            self.case('dimse_n.mwl.seed.pacs-server', lambda: self.seed_pacs_worklist(root), phase='fixture_preparation')
            # The worklist table has no column for the Requested Procedure Description: a declared gap.
            self.mwl_matrix(target, 'dimse_n.mwl.scp.pacs-server', [*requires, 'dimse_n.mwl.seed.pacs-server'], unstored=('RequestedProcedureDescription',))

            self.case('dimse_n.mwl.scp.pacs-server.refuse.missing-sequence', refuse_without_sequence(target), requires=requires)
        self.group_with('dimse_n.mwl.pacs-server', lambda: self.server(self.pacs_args(root), PACS_PORT, log_name='mwl-pacs-server'), body)
        self.mwl_scu_group(requires)

    def mwl_scu_group(self, requires):
        record = self.work/'mwl-scu.jsonl'
        output = self.work/'mwl-scu.json'

        def scu(*arguments, allowed=(0,)):
            return self.tool([self.bin/'worklist_scu', '-aec', PEER_AE, '--calling-ae', 'PACS_MWL_SCU', *arguments, '127.0.0.1', PEER_MWL_PORT], allowed=allowed)

        def positive():
            text = scu('--modality', 'CT', '--format', 'json', '--output-file', output)
            require(text is not None, 'no output')
            rows = json.loads(output.read_text())['worklistItems']
            found = sorted(row['patient']['id'] for row in rows)
            require(found == ['MWL1', 'MWL3'], f'worklist_scu reported {found}')
            keys = self.records(record)[-1]
            require(keys['calling_ae'] == 'PACS_MWL_SCU', f'peer saw calling AE {keys["calling_ae"]}')
            require(keys['keys']['ScheduledProcedureStepSequence'][0].get('Modality') == 'CT', f'peer saw {keys["keys"]}')
            return 'worklist_scu to the scripted peer; peer saw the sequence key'

        def no_results():
            scu('--patient-id', 'NOBODY', allowed=(1,))
            return 'no match is exit 1'
        self.group_with('dimse_n.mwl.scu',
                        lambda: self.server(['python3', self.scripts/'mwl_scp.py', '--aet', PEER_AE, '--port', PEER_MWL_PORT, '--items', self.write_mwl_peer_items(), '--record', record], PEER_MWL_PORT, log_name='mwl-scu-peer'),
                        lambda _: (self.case('dimse_n.mwl.scu.positive', positive, requires=requires),
                                   self.case('dimse_n.mwl.scu.no-results', no_results, requires=requires)))

        def refused():
            self.tool([self.bin/'worklist_scu', '-aec', PEER_AE, '--modality', 'CT', '127.0.0.1', PEER_MWL_PORT], allowed=(2,))
            return 'a failure status from the SCP is exit 2, not an empty success'
        self.group_with('dimse_n.mwl.scu-failure',
                        lambda: self.server(['python3', self.scripts/'mwl_scp.py', '--aet', PEER_AE, '--port', PEER_MWL_PORT, '--items', self.write_mwl_peer_items(), '--record', record, '--fail-with', 'A700'], PEER_MWL_PORT, log_name='mwl-scu-peer-failure'),
                        lambda _: self.case('dimse_n.mwl.scu.failure-status', refused, requires=requires))

    # -- Storage Commitment

    def stgcmtscu(self, target, directory, *names, bind=None, extra=()):
        directory.mkdir(parents=True, exist_ok=True)
        before = {path.name for path in directory.iterdir()}
        bind = bind or f'{STGCMT_AE}:{STGCMT_LISTEN_PORT}'
        text = self.tool([self.dcm4che/'stgcmtscu', '-b', bind, '-c', f'{target.ae}@127.0.0.1:{target.port}', '--directory', directory, *extra,
                          *[self.images/f'{name}.dcm' for name in names]], timeout=60)
        new = [path for path in directory.iterdir() if path.name not in before]
        require(len(new) == 1, f'{len(new)} event reports stored')
        return text, pydicom.dcmread(new[0], force=True), new[0].name

    def check_commitment(self, event, committed, failed, reason=None):
        uids = lambda seq: sorted(str(item.ReferencedSOPInstanceUID) for item in (seq or []))
        require(uids(event.get('ReferencedSOPSequence')) == sorted(OBJECTS[name] for name in committed), f'committed: {uids(event.get("ReferencedSOPSequence"))}')
        require(uids(event.get('FailedSOPSequence')) == sorted(OBJECTS[name] for name in failed), f'failed: {uids(event.get("FailedSOPSequence"))}')
        for item in event.get('FailedSOPSequence') or []:
            require(item.FailureReason == reason, f'failure reason {item.FailureReason:04X}')

    def peer_stgcmt(self):
        directory, qr = self.work/'peer-stgcmt', self.work/'peer-qr'
        qr.mkdir()
        shutil.copy(self.images/'kept.dcm', qr/'kept.dcm')
        self.tool([self.dcm4che/'dcmdir', '-c', qr/'DICOMDIR', '--fs-id', 'N', qr/'kept.dcm'])
        map_file = self.work/'peer-ae.properties'
        map_file.write_text(f'{STGCMT_AE}=127.0.0.1:{STGCMT_LISTEN_PORT}\n{SCU_AE}=127.0.0.1:{RECEIVER_PORT}\n')
        target = Target('dcm4che-dcmqrscp', PEER_QR_PORT, PEER_AE)
        references = [(SECONDARY_CAPTURE, OBJECTS['kept']), (SECONDARY_CAPTURE, OBJECTS['never-stored'])]
        record = self.work/'peer-stgcmt-events.jsonl'
        with self.server([self.dcm4che/'dcmqrscp', '-b', f'{PEER_AE}:{PEER_QR_PORT}', '--dicomdir', qr/'DICOMDIR', '--ae-config', map_file], PEER_QR_PORT):
            text, event, name = self.stgcmtscu(target, directory, 'kept')
            require('eventID=1' in text and str(event.TransactionUID) == name, 'stgcmtscu: event 1 not correlated')
            self.check_commitment(event, ['kept'], [])
            _, event, _ = self.stgcmtscu(target, directory, 'second')
            self.check_commitment(event, [], ['second'], 0x0112)
            with self.receiver(SCU_AE, RECEIVER_PORT, record):
                transaction = self.uid()
                found = self.statuses(self.scripted(target, [dict(op='n_action', **{'class': STORAGE_COMMITMENT}, instance=STORAGE_COMMITMENT_INSTANCE, action=1,
                                                                  dataset=commitment_request(transaction, references))], [STORAGE_COMMITMENT]))
                require(found == [SUCCESS], f'scripted N-ACTION: {found}')
                rows = self.events(record, 1)
                require(len(rows) == 1 and rows[0]['requestor_scp_role'], f'receiver rows: {rows}')
                require(rows[0]['event_type'] == 2 and rows[0]['dataset']['TransactionUID'] == transaction, f'event {rows[0]}')
            for name, fields, dataset, expected in STGCMT_REFUSALS:
                if name in REFERENCE_STGCMT_REFUSALS:
                    step = dict(op='n_action', **{'class': STORAGE_COMMITMENT}, instance=STORAGE_COMMITMENT_INSTANCE, action=1, dataset=dataset(references))
                    step.update(fields)
                    found = self.statuses(self.scripted(target, [step], [STORAGE_COMMITMENT]))
                    require(found == [expected], f'{name}: {found}')
        directory = self.work/'peer-stgcmt-same'
        with self.server([self.dcm4che/'dcmqrscp', '-b', f'{PEER_AE}:{PEER_QR_PORT}', '--dicomdir', qr/'DICOMDIR', '--ae-config', map_file, '--stgcmt-same-assoc'], PEER_QR_PORT, log_name='peer-stgcmt-same'):
            text, event, _ = self.stgcmtscu(target, directory, 'kept', bind=STGCMT_AE, extra=('--keep-alive',))
            self.check_commitment(event, ['kept'], [])
        return 'stgcmtscu and the Python receiver against dcmqrscp: reverse and same association, committed and failed items'

    def commitment_step(self, transaction, references):
        return dict(op='n_action', **{'class': STORAGE_COMMITMENT}, instance=STORAGE_COMMITMENT_INSTANCE, action=1, dataset=commitment_request(transaction, references))

    def stgcmt_group(self):
        root = self.work/'stgcmt-pacs'
        target = Target('pacs_server', PACS_PORT, PACS_AE)
        directory = self.work/'stgcmt-events'
        requires = ['dimse_n.peer.stgcmt']
        seeded = 'dimse_n.stgcmt.seed'
        record = self.work/'stgcmt-receiver.jsonl'
        later_record = self.work/'stgcmt-later.jsonl'
        peers = (f'{STGCMT_AE}:127.0.0.1:{STGCMT_LISTEN_PORT}', f'{SCU_AE}:127.0.0.1:{RECEIVER_PORT}', f'LATER:127.0.0.1:{RECEIVER2_PORT}', f'{UNREACHABLE_AE}:127.0.0.1:{DEAD_PORT}')
        references = lambda *names: [(SECONDARY_CAPTURE, OBJECTS[name]) for name in names]

        def seed():
            self.seed_objects(PACS_PORT, PACS_AE, 'kept', 'second', 'vanishes')
            found = {row['sop_uid'] for row in self.query_db(root, 'SELECT sop_uid FROM instances')}
            require({OBJECTS['kept'], OBJECTS['second'], OBJECTS['vanishes']} <= found, f'index holds {found}')
            return 'three objects stored with DCMTK storescu'

        def reverse(names, committed, failed, event_id, reason=None):
            def run():
                text, event, name = self.stgcmtscu(target, directory, *names)
                require(f'eventID={event_id}' in text, f'event type: {re.findall("eventID=[0-9]+", text)}')
                sent = re.findall(r'\(0008,1195\) UI \[([0-9.]+)\]', text)
                require(str(event.TransactionUID) == name and name in sent, f'transaction {event.TransactionUID} is not the one requested ({sent})')
                require(re.search(rf'{STGCMT_AE}<-{PACS_AE}\(\d+\) >> \d+:N-EVENT-REPORT-RQ', text), 'event did not come from PACS on an association it opened')
                self.check_commitment(event, committed, failed, reason)
                return f'{STGCMT_AE} received event {event_id} on a new association from {PACS_AE}'
            return run

        def same_association():
            result = self.established(self.scripted(target, [self.commitment_step(self.uid(), references('kept'))], [STORAGE_COMMITMENT], calling='NOT_IN_MAP',
                                                    wait_events=1, wait_seconds=10))
            require(self.statuses(result) == [SUCCESS], f'N-ACTION: {self.statuses(result)}')
            require(len(result['events']) == 1 and result['events'][0]['event_type'] == 1, f'events on the association: {result["events"]}')
            return 'an AE with no known address still held the association and received the event on it'

        def scu_keeps_alive():
            text, event, _ = self.stgcmtscu(target, self.work/'stgcmt-keepalive', 'kept', bind='NOT_IN_MAP2', extra=('--keep-alive',))
            self.check_commitment(event, ['kept'], [])
            return 'dcm4che stgcmtscu --keep-alive received the result on its association'

        def released_reverse():
            transaction = self.uid()
            with self.receiver(SCU_AE, RECEIVER_PORT, record):
                result = self.established(self.scripted(target, [self.commitment_step(transaction, references('kept', 'never-stored'))], [STORAGE_COMMITMENT]))
                require(self.statuses(result) == [SUCCESS], f'N-ACTION: {self.statuses(result)}')
                rows = [row for row in self.events(record, 1) if row['dataset'].get('TransactionUID') == transaction]
            require(len(rows) == 1, f'{len(rows)} events for the transaction')
            row = rows[0]
            require(row['requestor_scp_role'] and row['calling_ae'] == PACS_AE, f'roles/AE: {row}')
            require(row['sop_class'] == STORAGE_COMMITMENT and row['instance'] == STORAGE_COMMITMENT_INSTANCE, f'identity: {row}')
            require(row['event_type'] == 2, f'event type {row["event_type"]}')
            self.check_commitment(pydicom_like(row['dataset']), ['kept'], ['never-stored'], 0x0112)
            return 'the Python receiver got one report with both lists, under the SCP role'

        def class_conflict():
            transaction = self.uid()
            with self.receiver(SCU_AE, RECEIVER_PORT, record):
                wrong_class = '1.2.840.10008.5.1.4.1.1.2'
                self.established(self.scripted(target, [self.commitment_step(transaction, [(wrong_class, OBJECTS['kept'])])], [STORAGE_COMMITMENT]))
                rows = [row for row in self.events(record, 1) if row['dataset'].get('TransactionUID') == transaction]
            require(len(rows) == 1 and rows[0]['event_type'] == 2, f'rows: {rows}')
            failed = rows[0]['dataset'].get('FailedSOPSequence', [])
            require([item.get('FailureReason') for item in failed] == [0x0119], f'failed: {failed}')
            return 'an instance named under the wrong SOP Class is 0119H, not committed'

        def missing_file():
            stored = self.query_db(root, 'SELECT file_path FROM instances WHERE sop_uid = ?', OBJECTS['vanishes'])
            Path(stored[0]['file_path']).unlink()
            transaction = self.uid()
            with self.receiver(SCU_AE, RECEIVER_PORT, record):
                self.established(self.scripted(target, [self.commitment_step(transaction, references('vanishes', 'kept'))], [STORAGE_COMMITMENT]))
                rows = [row for row in self.events(record, 1) if row['dataset'].get('TransactionUID') == transaction]
            require(len(rows) == 1 and rows[0]['event_type'] == 2, f'rows: {rows}')
            self.check_commitment(pydicom_like(rows[0]['dataset']), ['kept'], ['vanishes'], 0x0112)
            return 'an indexed object whose file is gone is reported as failed, so commitment means the bytes exist'

        def retry_reaches_late_listener():
            transaction = self.uid()
            result = self.established(self.scripted(target, [self.commitment_step(transaction, references('kept'))], [STORAGE_COMMITMENT], calling='LATER'))
            require(self.statuses(result) == [SUCCESS], f'N-ACTION: {self.statuses(result)}')
            time.sleep(1.2)
            with self.receiver('LATER', RECEIVER2_PORT, later_record):
                rows = [row for row in self.events(later_record, 1, timeout=15) if row['dataset'].get('TransactionUID') == transaction]
            require(len(rows) == 1 and rows[0]['event_type'] == 1, f'{len(rows)} events after the listener came up: {rows}')
            return 'the listener was down when N-ACTION finished; a later attempt delivered exactly one report'

        def unreachable_then_healthy():
            result = self.established(self.scripted(target, [self.commitment_step(self.uid(), references('kept'))], [STORAGE_COMMITMENT], calling=UNREACHABLE_AE))
            require(self.statuses(result) == [SUCCESS], f'N-ACTION: {self.statuses(result)}')
            time.sleep(4)
            transaction = self.uid()
            with self.receiver(SCU_AE, RECEIVER_PORT, record):
                self.established(self.scripted(target, [self.commitment_step(transaction, references('kept'))], [STORAGE_COMMITMENT]))
                rows = [row for row in self.events(record, 1) if row['dataset'].get('TransactionUID') == transaction]
            require(len(rows) == 1, 'a later commitment did not get its report')
            log = (self.output/'stgcmt-pacs-server-server.log').read_text(errors='replace')
            require('N-EVENT-REPORT' in log and UNREACHABLE_AE in log, 'the undelivered report left no trace in the server log')
            return 'an undeliverable report is logged and does not stop later commitments'

        def failure_reply():
            transaction = self.uid()
            with self.receiver(SCU_AE, RECEIVER_PORT, record, '--status', '0110', log=f'{self.current}-receiver'):
                self.established(self.scripted(target, [self.commitment_step(transaction, references('kept'))], [STORAGE_COMMITMENT]))
                rows = [row for row in self.events(record, 1) if row['dataset'].get('TransactionUID') == transaction]
                time.sleep(2.5)
                rows = [row for row in self.records(record) if row.get('kind') == 'event' and row['dataset'].get('TransactionUID') == transaction]
            require(len(rows) == 1 and rows[0]['replied'] == '0110', f'{len(rows)} deliveries: an answered report must not be repeated')
            return 'a report the peer answered with a failure status is not sent again'

        def refusals():
            for name, fields, dataset, expected in STGCMT_REFUSALS:
                step = dict(op='n_action', **{'class': STORAGE_COMMITMENT}, instance=STORAGE_COMMITMENT_INSTANCE, action=1, dataset=dataset(references('kept')))
                step.update(fields)
                found = self.statuses(self.established(self.scripted(target, [step], [STORAGE_COMMITMENT])))
                require(found == [expected], f'{name}: expected {expected}, got {found}')
            return f'{len(STGCMT_REFUSALS)} malformed requests refused with their PS3.7 status'

        def body(_):
            self.case(seeded, seed, requires=['dimse_n.fixtures'], phase='fixture_preparation')
            need = [*requires, seeded]
            self.case('dimse_n.stgcmt.scp.reverse.all-committed', reverse(['kept', 'second'], ['kept', 'second'], [], 1), requires=need)
            self.case('dimse_n.stgcmt.scp.reverse.partial', reverse(['kept', 'never-stored'], ['kept'], ['never-stored'], 2, 0x0112), requires=need)
            self.case('dimse_n.stgcmt.scp.reverse.none', reverse(['never-stored'], [], ['never-stored'], 2, 0x0112), requires=need)
            self.case('dimse_n.stgcmt.scp.same-association.scripted', same_association, requires=need)
            self.case('dimse_n.stgcmt.scp.same-association.dcm4che', scu_keeps_alive, requires=need)
            self.case('dimse_n.stgcmt.scp.released-before-event', released_reverse, requires=need)
            self.case('dimse_n.stgcmt.scp.negative.class-conflict', class_conflict, requires=need)
            self.case('dimse_n.stgcmt.scp.negative.file-gone', missing_file, requires=need)
            self.case('dimse_n.stgcmt.scp.negative.refusals', refusals, requires=need)
            self.case('dimse_n.stgcmt.scp.retry.late-listener', retry_reaches_late_listener, requires=need)
            self.case('dimse_n.stgcmt.scp.retry.unreachable-then-healthy', unreachable_then_healthy, requires=need)
            self.case('dimse_n.stgcmt.scp.retry.failure-reply-not-repeated', failure_reply, requires=need)
        self.group_with('dimse_n.stgcmt.pacs-server',
                        lambda: self.server(self.pacs_args(root, peers, event_attempts=5, event_retry_delay=1), PACS_PORT, log_name='stgcmt-pacs-server'), body)

        def after_restart():
            transaction = self.uid()
            with self.receiver(SCU_AE, RECEIVER_PORT, record):
                self.established(self.scripted(target, [self.commitment_step(transaction, references('kept'))], [STORAGE_COMMITMENT]))
                rows = [row for row in self.events(record, 1) if row['dataset'].get('TransactionUID') == transaction]
            require(len(rows) == 1 and rows[0]['event_type'] == 1, f'rows: {rows}')
            return 'objects stored before a restart are still committed after it'
        self.group_with('dimse_n.stgcmt.pacs-server-restart',
                        lambda: self.server(self.pacs_args(root, peers, event_attempts=5, event_retry_delay=1), PACS_PORT, log_name='stgcmt-pacs-server-restart'),
                        lambda _: self.case('dimse_n.stgcmt.scp.persistence.after-restart', after_restart, requires=requires+[seeded]))
        self.stgcmt_scu_group(requires)

    def stgcmt_scu_group(self, requires):
        qr = self.work/'scu-qr'
        qr.mkdir()
        shutil.copy(self.images/'kept.dcm', qr/'kept.dcm')
        map_file = self.work/'scu-ae.properties'
        map_file.write_text(f'{SCU_LISTEN_AE}=127.0.0.1:{SCU_LISTEN_PORT}\n')

        def driver(*extra, calling=SCU_LISTEN_AE, allowed=(0,)):
            transaction = self.uid()
            args = [self.bin/'dimse_n_scu', '127.0.0.1', PEER_QR_PORT, PEER_AE, 'stgcmt', '--calling-ae', calling, '--transaction', transaction,
                    '--ref', f'{SECONDARY_CAPTURE}:{OBJECTS["kept"]}', '--ref', f'{SECONDARY_CAPTURE}:{OBJECTS["never-stored"]}', '--wait', '20', *extra]
            text = self.command(args, timeout=60, allowed=allowed)
            return transaction, json.loads(text)

        def check(reply, transaction):
            require(reply['status'] == SUCCESS, f'N-ACTION {reply["status"]}')
            require(reply.get('event_type') == 2 and reply.get('transaction') == transaction, f'event: {reply}')
            require(reply.get('committed') == [OBJECTS['kept']], f'committed: {reply.get("committed")}')
            require([row['instance'] for row in reply.get('failed', [])] == [OBJECTS['never-stored']], f'failed: {reply.get("failed")}')
            require(reply['failed'][0]['reason'] == '0112', f'reason: {reply["failed"][0]}')

        def reverse():
            transaction, reply = driver('--listen', SCU_LISTEN_PORT)
            check(reply, transaction)
            require(reply.get('association') == 'different', f'delivered on {reply.get("association")}')
            return 'the storage_commitment_scu class received dcm4che dcmqrscp\'s report on a new association'

        def same():
            transaction, reply = driver()
            check(reply, transaction)
            require(reply.get('association') == 'same', f'delivered on {reply.get("association")}')
            return 'the storage_commitment_scu class received the report on its own association'

        def refused():
            _, reply = driver('--action', '2', allowed=(1,))
            require(reply['status'] == '0123', f'status {reply["status"]}')
            return 'a refused request is exit 1 with the status'

        def prepare():
            self.tool([self.dcm4che/'dcmdir', '-c', qr/'DICOMDIR', '--fs-id', 'N', qr/'kept.dcm'])
            return 'one object in the reference archive'
        self.case('dimse_n.stgcmt.scu.prepare', prepare, requires=['dimse_n.fixtures'], phase='fixture_preparation')
        self.group_with('dimse_n.stgcmt.scu',
                        lambda: self.server([self.dcm4che/'dcmqrscp', '-b', f'{PEER_AE}:{PEER_QR_PORT}', '--dicomdir', qr/'DICOMDIR', '--ae-config', map_file], PEER_QR_PORT, log_name='stgcmt-scu-peer'),
                        lambda _: (self.case('dimse_n.stgcmt.scu.reverse', reverse, requires=requires+['dimse_n.stgcmt.scu.prepare']),
                                   self.case('dimse_n.stgcmt.scu.refused', refused, requires=requires+['dimse_n.stgcmt.scu.prepare'])))
        self.group_with('dimse_n.stgcmt.scu-same',
                        lambda: self.server([self.dcm4che/'dcmqrscp', '-b', f'{PEER_AE}:{PEER_QR_PORT}', '--dicomdir', qr/'DICOMDIR', '--ae-config', map_file, '--stgcmt-same-assoc'], PEER_QR_PORT, log_name='stgcmt-scu-peer-same'),
                        lambda _: self.case('dimse_n.stgcmt.scu.same-association', same, requires=requires+['dimse_n.stgcmt.scu.prepare']))

    # -- UPS

    def ups_cell(self, target, event, state):
        def run():
            uid, transaction = self.uid(), self.uid()
            setup = ups_setup(uid, state, transaction)
            prepared, step = ups_event_step(event, uid, state, transaction)
            lead = setup+prepared
            result = self.established(self.scripted(target, [*lead, step, n_get_ups(uid)], UPS_CONTEXTS))
            found = self.statuses(result)
            require(found[:len(lead)] == [SUCCESS]*len(lead), f'getting to {state}: {found[:len(lead)]}')
            expected = UPS_TABLE[event][state]
            require(found[len(lead)] == expected[0], f'{event} in {state}: expected {expected[0]}, got {found[len(lead)]}')
            after = expected[1] if len(expected) > 1 else state
            if after == 'null':
                require(found[-1] == 'C307', f'N-GET of a UPS that should not exist: {found[-1]}')
            else:
                data = result['steps'][-1].get('dataset', {})
                require(found[-1] == SUCCESS and data.get('ProcedureStepState') == after, f'state afterwards: {found[-1]}, {data.get("ProcedureStepState")}, expected {after}')
                require('TransactionUID' not in data, 'N-GET returned the Transaction UID, which holds the lock')
            return f'{expected[0]}; state {after}'
        return run

    def ups_table(self, target, prefix, requires=()):
        for event in UPS_TABLE:
            for state in UPS_STATES:
                self.case(f'{prefix}.{event}.{state.lower().replace(" ", "-")}', self.ups_cell(target, event, state), requires=requires)

    def ups_set_cases(self, target, prefix, requires=()):
        def run(state, request, expected):
            def body():
                uid, transaction = self.uid(), self.uid()
                setup = ups_setup(uid, state, transaction)
                result = self.established(self.scripted(target, [*setup, request(uid, transaction), n_get_ups(uid)], UPS_CONTEXTS))
                found = self.statuses(result)
                require(found[:len(setup)] == [SUCCESS]*len(setup), f'getting to {state}: {found}')
                require(found[len(setup)] == expected, f'expected {expected}, got {found[len(setup)]}')
                if state != 'null':
                    label = result['steps'][-1]['dataset'].get('ProcedureStepLabel')
                    wanted = 'Renamed' if expected == SUCCESS else 'Validate step'
                    require(label == wanted, f'stored label {label!r}, expected {wanted!r}')
                return f'{expected}; stored value consistent'
            return body
        for name, state, request, expected in UPS_SET_CASES:
            self.case(f'{prefix}.{name}', run(state, request, expected), requires=requires)

    def ups_create_cases(self, target, prefix, requires=()):
        def not_scheduled():
            uid = self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid, ProcedureStepState='IN PROGRESS'), n_get_ups(uid)], UPS_CONTEXTS)))
            require(found == ['C309', 'C307'], f'{found}')
            return 'C309 and nothing created'

        def missing_label():
            uid = self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid, label=None), n_get_ups(uid)], UPS_CONTEXTS)))
            require(found == ['0120', 'C307'], f'{found}')
            return '0120 and nothing created'

        def with_transaction():
            uid = self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid, TransactionUID=self.uid()), n_get_ups(uid)], UPS_CONTEXTS)))
            require(found == ['0106', 'C307'], f'{found}')
            return '0106 and nothing created'

        def bad_priority():
            uid = self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid, ScheduledProcedureStepPriority='URGENT'), n_get_ups(uid)], UPS_CONTEXTS)))
            require(found == ['0106', 'C307'], f'{found}')
            return '0106 and nothing created'

        def worklist_label():
            uid = self.uid()
            result = self.established(self.scripted(target, [n_create_ups(uid, WorklistLabel=''), n_get_ups(uid)], UPS_CONTEXTS))
            require(self.statuses(result) == [SUCCESS, SUCCESS], f'{self.statuses(result)}')
            data = result['steps'][-1]['dataset']
            require(data.get('WorklistLabel'), 'the SCP did not fill in the Worklist Label')
            require(data.get('SOPClassUID') == UPS_PUSH and data.get('SOPInstanceUID') == uid, f'identity: {data.get("SOPClassUID")}, {data.get("SOPInstanceUID")}')
            require(data.get('ScheduledProcedureStepModificationDateTime'), 'no Modification DateTime')
            require(data.get('PatientName') == 'UPS^PATIENT' and data.get('InputReadinessState') == 'READY', 'attributes from the N-CREATE are not all held')
            return 'what the SCP must add is there, and what the SCU sent is kept'
        self.case(f'{prefix}.not-scheduled', not_scheduled, requires=requires)
        self.case(f'{prefix}.missing-label', missing_label, requires=requires)
        self.case(f'{prefix}.with-transaction', with_transaction, requires=requires)
        self.case(f'{prefix}.bad-priority', bad_priority, requires=requires)
        self.case(f'{prefix}.scp-fills-attributes', worklist_label, requires=requires)

        def assigned_uid():
            label = f'Assigned {self.uid()}'[:60]
            result = self.established(self.scripted(target, [n_create_ups(None, label=label),
                                                             dict(op='c_find', model=UPS_QUERY, dataset=dict(ProcedureStepLabel=label, SOPInstanceUID=''))], UPS_CONTEXTS))
            require(result['steps'][0]['status'] == SUCCESS and len(result['steps'][1]['rows']) == 1, f'{result["steps"]}')
            require(result['steps'][1]['rows'][0].get('SOPInstanceUID'), 'the step has no UID')
            return 'an N-CREATE with no UID got one from the SCP'
        self.case(f'{prefix}.scp-assigns-uid', assigned_uid, requires=requires)

    def ups_get_cases(self, target, prefix, requires=()):
        def subset():
            uid = self.uid()
            result = self.established(self.scripted(target, [n_create_ups(uid), n_get_ups(uid, ('00741000', '00741204'))], UPS_CONTEXTS))
            data = result['steps'][-1].get('dataset', {})
            require(self.statuses(result) == [SUCCESS, SUCCESS] and set(data) == {'ProcedureStepState', 'ProcedureStepLabel'}, f'{self.statuses(result)}: {sorted(data)}')
            return 'exactly the requested attributes'

        def lock_hidden():
            uid, transaction = self.uid(), self.uid()
            result = self.established(self.scripted(target, [n_create_ups(uid), change_state(uid, 'IN PROGRESS', transaction), n_get_ups(uid, ('00081195', '00741000'))], UPS_CONTEXTS))
            data = result['steps'][-1].get('dataset', {})
            require(result['steps'][-1]['status'] == SUCCESS and transaction not in json.dumps(data), f'N-GET returned {data}')
            return 'the Transaction UID cannot be read back, even when asked for by tag'
        self.case(f'{prefix}.requested-subset', subset, requires=requires)
        self.case(f'{prefix}.transaction-hidden', lock_hidden, requires=requires)

    def ups_final_state_cases(self, target, prefix, requires=()):
        def missing(omit):
            def run():
                uid, transaction = self.uid(), self.uid()
                steps = [n_create_ups(uid), change_state(uid, 'IN PROGRESS', transaction), n_set_ups(uid, transaction, ups_performed(omit)),
                         change_state(uid, 'COMPLETED', transaction), n_get_ups(uid)]
                result = self.established(self.scripted(target, steps, UPS_CONTEXTS))
                found = self.statuses(result)
                require(found[:3] == [SUCCESS]*3 and found[3] == 'C304', f'{found}')
                require(result['steps'][-1]['dataset'].get('ProcedureStepState') == 'IN PROGRESS', 'state moved despite C304')
                return 'C304 and still IN PROGRESS'
            return run

        def cancel_needs_time():
            uid, transaction = self.uid(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid), change_state(uid, 'IN PROGRESS', transaction), change_state(uid, 'CANCELED', transaction)], UPS_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS, 'C304'], f'{found}')
            return 'CANCELED without its final-state attributes is C304'
        for omit in PERFORMED_REQUIRED:
            self.case(f'{prefix}.completed-missing.{omit}', missing(omit), requires=requires)
        self.case(f'{prefix}.canceled-missing-cancellation-time', cancel_needs_time, requires=requires)

    def ups_watch_cases(self, target, prefix, requires, receiver_port, peers_known=True):
        record = self.work/f'{prefix}-events.jsonl'

        def fresh():
            uid = self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid)], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'create: {found}')
            return uid

        def events_for(uid, count, event_type=None):
            rows = [row for row in self.events(record, None, timeout=0.2) if row['instance'] == uid]
            deadline = time.monotonic()+12
            while len(rows) < count and time.monotonic() < deadline:
                time.sleep(.1)
                rows = [row for row in self.records(record) if row.get('kind') == 'event' and row['instance'] == uid]
            time.sleep(.4)
            rows = [row for row in self.records(record) if row.get('kind') == 'event' and row['instance'] == uid]
            return [row for row in rows if event_type is None or row['event_type'] == event_type]

        def check_identity(row):
            require(row['requestor_scp_role'], f'event sent without the SCP role: {row}')
            require(row['abstract_syntax'] == UPS_EVENT, f'event on context {row["abstract_syntax"]}')
            require(row['sop_class'] == UPS_PUSH, f'Affected SOP Class {row["sop_class"]}')
            require(row['called_ae'] == RECEIVER_AE, f'called AE {row["called_ae"]}')

        def specific():
            uid, transaction = fresh(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [watch_action(uid, 3, RECEIVER_AE, False), change_state(uid, 'IN PROGRESS', transaction),
                                                                          n_set_ups(uid, transaction, ups_performed()), change_state(uid, 'COMPLETED', transaction)], UPS_CONTEXTS)))
            require(found == [SUCCESS]*4, f'{found}')
            rows = events_for(uid, 3, 1)
            require([row['dataset']['ProcedureStepState'] for row in rows] == ['SCHEDULED', 'IN PROGRESS', 'COMPLETED'], f'reports: {[row["dataset"] for row in rows]}')
            check_identity(rows[0])
            require(all(row['dataset'].get('InputReadinessState') == 'READY' for row in rows), f'{rows}')
            return 'initial report, then each state in order, in the receiving process'

        def unsubscribe():
            uid, transaction = fresh(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [watch_action(uid, 3, RECEIVER_AE, False)], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'{found}')
            require(len(events_for(uid, 1, 1)) == 1, 'no initial report')
            found = self.statuses(self.established(self.scripted(target, [watch_action(uid, 4, RECEIVER_AE), change_state(uid, 'IN PROGRESS', transaction)], UPS_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS], f'{found}')
            time.sleep(1.5)
            rows = events_for(uid, 1, 1)
            require(len(rows) == 1, f'{len(rows)} reports: unsubscribed AE still told')
            return 'no report after unsubscribing'

        def global_lifecycle():
            found = self.statuses(self.established(self.scripted(target, [watch_action(UPS_GLOBAL, 3, RECEIVER_AE, False)], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'subscribe: {found}')
            first = fresh()
            require([row['dataset']['ProcedureStepState'] for row in events_for(first, 1, 1)] == ['SCHEDULED'], 'no report for a step created under a global subscription')
            found = self.statuses(self.established(self.scripted(target, [dict(watch_action(UPS_GLOBAL, 5, RECEIVER_AE))], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'suspend: {found}')
            second, transaction = fresh(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [change_state(first, 'IN PROGRESS', transaction), change_state(second, 'IN PROGRESS', transaction)], UPS_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS], f'{found}')
            require([row['dataset']['ProcedureStepState'] for row in events_for(first, 2, 1)] == ['SCHEDULED', 'IN PROGRESS'], 'suspending dropped an existing subscription')
            time.sleep(1)
            require(not events_for(second, 0), 'a step created after suspending was still reported')
            found = self.statuses(self.established(self.scripted(target, [watch_action(UPS_GLOBAL, 4, RECEIVER_AE), n_set_ups(first, transaction, ups_performed()), change_state(first, 'COMPLETED', transaction)], UPS_CONTEXTS)))
            require(found == [SUCCESS]*3, f'{found}')
            time.sleep(1)
            require(len(events_for(first, 2, 1)) == 2, 'unsubscribing globally left a subscription behind')
            return 'CC.2.3-2 global, suspend and global unsubscribe'

        def global_with_lock():
            existing = fresh()
            found = self.statuses(self.established(self.scripted(target, [watch_action(UPS_GLOBAL, 3, RECEIVER_AE, True)], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'{found}')
            require(len(events_for(existing, 1, 1)) == 1, 'global with lock did not report an existing step')
            found = self.statuses(self.established(self.scripted(target, [watch_action(UPS_GLOBAL, 4, RECEIVER_AE)], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'{found}')
            return 'an existing step gets its initial report'

        def refusals():
            uid = fresh()
            cases = (('unknown-step', watch_action(self.uid(), 3, RECEIVER_AE, False), 'C307'),
                     ('unknown-receiver', watch_action(uid, 3, 'NOBODY_KNOWN', False), 'C308'),
                     ('no-receiver', watch_action(uid, 3, None, False), '0120'),
                     ('no-lock', watch_action(uid, 3, RECEIVER_AE, None), '0120'),
                     ('filtered-global', watch_action(UPS_FILTERED_GLOBAL, 3, RECEIVER_AE, False), 'C307'),
                     ('suspend-specific', watch_action(uid, 5, RECEIVER_AE), 'C314'))
            for name, step, expected in cases:
                found = self.statuses(self.established(self.scripted(target, [step], UPS_CONTEXTS)))
                require(found == [expected], f'{name}: expected {expected}, got {found}')
            return f'{len(cases)} refused subscription requests'

        def cancel_requested():
            uid, transaction = fresh(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [change_state(uid, 'IN PROGRESS', transaction), watch_action(uid, 3, RECEIVER_AE, False),
                                                                          request_cancel(uid, 'Stop now'), n_get_ups(uid)], UPS_CONTEXTS, calling='CANCELLER')))
            require(found == [SUCCESS]*4, f'{found}')
            rows = events_for(uid, 1, 2)
            require(len(rows) == 1 and rows[0]['dataset'].get('RequestingAE') == 'CANCELLER' and rows[0]['dataset'].get('ReasonForCancellation') == 'Stop now', f'{rows}')
            return 'the performer hears who asked and why; the step stays IN PROGRESS until it decides'

        def cancel_scheduled():
            uid = fresh()
            found = self.statuses(self.established(self.scripted(target, [watch_action(uid, 3, RECEIVER_AE, False), request_cancel(uid, 'Stop now')], UPS_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS], f'{found}')
            rows = events_for(uid, 3, 1)
            require([row['dataset']['ProcedureStepState'] for row in rows] == ['SCHEDULED', 'IN PROGRESS', 'CANCELED'], f'{rows}')
            return 'a SCHEDULED step goes through IN PROGRESS to CANCELED, and the subscriber sees each'

        def progress_and_assignment():
            uid, transaction = fresh(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [
                watch_action(uid, 3, RECEIVER_AE, False), change_state(uid, 'IN PROGRESS', transaction),
                n_set_ups(uid, transaction, dict(ProcedureStepProgressInformationSequence=[dict(ProcedureStepProgress='40', ProcedureStepProgressDescription='Working')])),
                n_set_ups(uid, transaction, dict(ScheduledStationNameCodeSequence=[dict(CodeValue='ST1', CodingSchemeDesignator='99N', CodeMeaning='Station')]))], UPS_CONTEXTS)))
            require(found == [SUCCESS]*4, f'{found}')
            progress, assigned = events_for(uid, 1, 3), events_for(uid, 1, 5)
            require(len(progress) == 1 and progress[0]['dataset']['ProcedureStepProgressInformationSequence'][0]['ProcedureStepProgress'] == '40', f'{progress}')
            require(len(assigned) == 1, f'{assigned}')
            return 'progress and assignment events reach the receiver'

        def body(_):
            self.case(f'{prefix}.specific', specific, requires=requires)
            self.case(f'{prefix}.unsubscribe', unsubscribe, requires=requires)
            self.case(f'{prefix}.global', global_lifecycle, requires=requires)
            self.case(f'{prefix}.global-with-lock', global_with_lock, requires=requires)
            self.case(f'{prefix}.refusals', refusals, requires=requires)
            self.case(f'{prefix}.cancel-requested', cancel_requested, requires=requires)
            self.case(f'{prefix}.cancel-scheduled', cancel_scheduled, requires=requires)
            self.case(f'{prefix}.progress-and-assignment', progress_and_assignment, requires=requires)
        self.group_with(f'{prefix}.receiver', lambda: self.receiver(RECEIVER_AE, receiver_port, record, log=f'{prefix}-receiver'), body)

    def ups_context_cases(self, target, prefix, requires=()):
        uid = lambda: self.uid()

        def run(context, step, operation, expected_allowed):
            def body():
                step_on_context = dict(step, meta=context)
                result = self.established(self.scripted(target, [step_on_context], [context]))
                found = self.statuses(result)[0]
                allowed = operation in UPS_OPERATIONS[context]
                require(allowed == expected_allowed, 'test table disagrees with itself')
                if allowed:
                    require(found != UNRECOGNIZED_OPERATION, f'{operation} refused on its own SOP Class: {found}')
                else:
                    require(found == UNRECOGNIZED_OPERATION, f'{operation} on the wrong SOP Class answered {found}')
                return f'{found}'
            return run_case(body)
        run_case = lambda body: body
        requests = (('n_create', lambda: n_create_ups(uid())), ('n_get', lambda: n_get_ups(uid())),
                    ('n_set', lambda: n_set_ups(uid(), None, dict(ProcedureStepLabel='X'))), ('state', lambda: change_state(uid(), 'IN PROGRESS', uid())),
                    ('cancel', lambda: request_cancel(uid())), ('watch', lambda: watch_action(uid(), 4, RECEIVER_AE)))
        for context, name in ((UPS_PUSH, 'push'), (UPS_PULL, 'pull'), (UPS_WATCH, 'watch'), (UPS_QUERY, 'query')):
            for operation, make in requests:
                allowed = operation in UPS_OPERATIONS[context]
                self.case(f'{prefix}.{name}.{operation}', run(context, make(), operation, allowed), requires=requires)

    def upsscu_flow(self, target, prefix, requires, directory_name):
        """The same flow from a client nobody here wrote, with dcm4che's own receiver as the watcher."""
        def upsscu(*arguments):
            return rsp_statuses(self.tool([self.dcm4che/'upsscu', '-b', 'UPSSCU', '-c', f'{target.ae}@127.0.0.1:{target.port}', *arguments]))

        def create_xml(path, label):
            path.write_text('<?xml version="1.0" encoding="UTF-8"?><NativeDicomModel>'
                            '<DicomAttribute tag="00741000" vr="CS"><Value number="1">SCHEDULED</Value></DicomAttribute>'
                            f'<DicomAttribute tag="00741204" vr="LO"><Value number="1">{label}</Value></DicomAttribute>'
                            '<DicomAttribute tag="00741202" vr="LO"><Value number="1">Work</Value></DicomAttribute>'
                            '<DicomAttribute tag="00741200" vr="CS"><Value number="1">HIGH</Value></DicomAttribute>'
                            '<DicomAttribute tag="00404005" vr="DT"><Value number="1">20261102120000</Value></DicomAttribute>'
                            '<DicomAttribute tag="00404041" vr="CS"><Value number="1">READY</Value></DicomAttribute>'
                            '<DicomAttribute tag="00100020" vr="LO"><Value number="1">UPSCLI</Value></DicomAttribute>'
                            '</NativeDicomModel>')
            return path

        def flow():
            uid, transaction, directory = self.uid(), self.uid(), self.work/directory_name
            directory.mkdir()
            xml = create_xml(directory/'create.xml', 'From dcm4che')
            expected = (
                (('-O', 'create', '-u', uid, xml), ('N-CREATE-RSP', SUCCESS)),
                (('-O', 'create', '-u', uid, xml), ('N-CREATE-RSP', '0111')),
                (('-O', 'get', '-u', uid), ('N-GET-RSP', SUCCESS)),
                (('-O', 'get', '-u', uid, '-p'), ('N-GET-RSP', SUCCESS)),
                (('-O', 'get', '-u', self.uid()), ('N-GET-RSP', 'C307')),
                (('-O', 'changeState', '-u', uid, '-C', transaction), ('N-ACTION-RSP', 'C310')),
                (('-O', 'changeState', '-u', uid, '-P', transaction), ('N-ACTION-RSP', SUCCESS)),
                (('-O', 'changeState', '-u', uid, '-P', f'{transaction}.9'), ('N-ACTION-RSP', 'C301')),
                (('-O', 'changeState', '-u', uid, '-P', transaction), ('N-ACTION-RSP', 'C302')),
                (('-O', 'changeState', '-u', uid, '-C', transaction), ('N-ACTION-RSP', 'C304')),
                (('-O', 'requestCancel', '-u', self.uid()), ('N-ACTION-RSP', 'C307')))
            for arguments, wanted in expected:
                found = upsscu(*arguments)
                require(found == [wanted], f'upsscu {arguments[:4]}: expected {wanted}, got {found}')
            return f'{len(expected)} exchanges from dcm4che upsscu with their exact statuses'

        def receive():
            uid, transaction, directory = self.uid(), self.uid(), self.work/f'{directory_name}-events'
            directory.mkdir()
            xml = create_xml(directory/'create.xml', 'Watched')
            log_path = self.output/f'{self.current}-upsscu-receive-server.log'
            with self.server([self.dcm4che/'upsscu', '-b', f'{RECEIVER2_AE}:{RECEIVER2_PORT}', '-c', f'{target.ae}@127.0.0.1:{target.port}', '-O', 'receive'], RECEIVER2_PORT,
                             log_name=f'{self.current}-upsscu-receive'):
                require(upsscu('-O', 'create', '-u', uid, xml) == [('N-CREATE-RSP', SUCCESS)], 'create')
                require(upsscu('-O', 'subscribe', '-u', uid, '--receiving-ae', RECEIVER2_AE) == [('N-ACTION-RSP', SUCCESS)], 'subscribe')
                require(upsscu('-O', 'changeState', '-u', uid, '-P', transaction) == [('N-ACTION-RSP', SUCCESS)], 'claim')
                deadline, seen = time.monotonic()+12, []
                while time.monotonic() < deadline and len(seen) < 2:
                    time.sleep(.2)
                    seen = re.findall(r'N-EVENT-REPORT-RSP\[[^\]\n]*?status=0H', log_path.read_text(errors='replace'))
                text = log_path.read_text(errors='replace')
            require(len(seen) == 2, f'dcm4che receiver acknowledged {len(seen)} reports')
            require(len(re.findall(r'N-EVENT-REPORT-RQ\[pcid=\d+, eventID=1', text)) == 2, 'reports were not of type 1')
            return 'dcm4che upsscu -O receive accepted the initial report and the state change; the Python receiver checks their values'
        self.case(f'{prefix}.flow', flow, requires=requires)
        self.case(f'{prefix}.receive', receive, requires=requires)

    def peer_ups_group(self):
        target = Target('reference', PEER_UPS_PORT, PEER_AE)
        record = self.work/'peer-ups.jsonl'
        peers = [item for ae, port in ((RECEIVER_AE, RECEIVER_PORT), (RECEIVER2_AE, RECEIVER2_PORT)) for item in ('--peer', f'{ae}:127.0.0.1:{port}')]

        def body(_):
            prefix = 'dimse_n.peer.ups'
            self.ups_table(target, f'{prefix}.table')
            self.ups_set_cases(target, f'{prefix}.set')
            self.ups_create_cases(target, f'{prefix}.create')
            self.ups_get_cases(target, f'{prefix}.get')
            self.ups_final_state_cases(target, f'{prefix}.final')
            self.ups_context_cases(target, f'{prefix}.context')
            self.upsscu_flow(target, f'{prefix}.dcm4che', [], 'peer-upsscu')
            self.ups_watch_cases(target, f'{prefix}.watch', [], RECEIVER_PORT)
        self.group_with('dimse_n.peer.ups', lambda: self.server(['python3', self.scripts/'ups_reference_scp.py', '--aet', PEER_AE, '--port', PEER_UPS_PORT, '--record', record, *peers], PEER_UPS_PORT, log_name='peer-ups'), body)
        self.case('dimse_n.peer.ups', lambda: self.prerequisite('dimse_n.peer.ups.'))

    def prerequisite(self, prefix):
        """A peer group passes when every case under it passed."""
        rows = [row for row in self.results if row['id'].startswith(prefix)]
        failed = [row['id'] for row in rows if row['status'] != 'passed']
        require(rows and not failed, f'{len(failed)} of {len(rows)} failed: {failed[:8]}')
        return f'{len(rows)} qualification cases passed'

    def ups_group(self):
        requires = ['dimse_n.peer.ups']
        root = self.work/'ups-pacs'
        target = Target('pacs_server', PACS_PORT, PACS_AE)
        peers = (f'{RECEIVER_AE}:127.0.0.1:{RECEIVER_PORT}', f'{RECEIVER2_AE}:127.0.0.1:{RECEIVER2_PORT}', f'{UNREACHABLE_AE}:127.0.0.1:{DEAD_PORT}',
                 f'LATE:127.0.0.1:{LATE_PORT}')
        survivor = {}

        def row(uid):
            rows = self.query_db(root, 'SELECT * FROM ups_workitems WHERE workitem_uid = ?', uid)
            require(len(rows) == 1, f'{len(rows)} rows for {uid}')
            return rows[0]

        def persisted():
            uid, transaction = self.uid(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid), change_state(uid, 'IN PROGRESS', transaction)], UPS_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS], f'{found}')
            stored = row(uid)
            require(stored['state'] == 'IN PROGRESS' and stored['transaction_uid'] == transaction, f'row: {stored}')
            survivor.update(uid=uid, transaction=transaction)
            return 'state and lock are in the database'

        def subscribed_before_restart():
            found = self.statuses(self.established(self.scripted(target, [watch_action(survivor['uid'], 3, RECEIVER2_AE, True)], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'{found}')
            rows = self.query_db(root, 'SELECT * FROM ups_subscriptions WHERE workitem_uid = ? AND subscriber_ae = ?', survivor['uid'], RECEIVER2_AE)
            require(len(rows) == 1 and rows[0]['deletion_lock'] == 1, f'rows {rows}')
            return 'subscription with lock is in the database'

        def unreachable_subscriber():
            uid = self.uid()
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid), watch_action(uid, 3, UNREACHABLE_AE, True)], UPS_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS], f'{found}')
            time.sleep(1)
            found = self.statuses(self.established(self.scripted(target, [change_state(uid, 'IN PROGRESS', self.uid())], UPS_CONTEXTS)))
            require(found == [SUCCESS], f'state change must not depend on delivery: {found}')
            rows = self.query_db(root, 'SELECT * FROM ups_subscriptions WHERE workitem_uid = ? AND subscriber_ae = ?', uid, UNREACHABLE_AE)
            require(len(rows) == 1 and rows[0]['deletion_lock'] == 1, f'CC.2.4.3: failed delivery must not affect the subscription: {rows}')
            return 'a subscriber that cannot be reached neither blocks the step nor loses its subscription or lock'

        def late_subscriber():
            uid, record = self.uid(), self.work/'ups-late.jsonl'
            found = self.statuses(self.established(self.scripted(target, [n_create_ups(uid), watch_action(uid, 3, 'LATE', False), change_state(uid, 'IN PROGRESS', self.uid())], UPS_CONTEXTS)))
            require(found == [SUCCESS]*3, f'{found}')
            time.sleep(1.5)
            with self.receiver('LATE', LATE_PORT, record):
                rows = [row for row in self.events(record, 2, timeout=20) if row['instance'] == uid]
            require([row['dataset']['ProcedureStepState'] for row in rows] == ['SCHEDULED', 'IN PROGRESS'], f'{rows}')
            return 'reports queued while the subscriber was down arrive once it is up, each once and in order'

        def concurrent_claim():
            uid = self.uid()
            require(self.statuses(self.established(self.scripted(target, [n_create_ups(uid)], UPS_CONTEXTS))) == [SUCCESS], 'create')
            found = []

            def claim(number):
                found.append(self.scripted(target, [change_state(uid, 'IN PROGRESS', f'{uid}.{number}')], UPS_CONTEXTS)['steps'][0]['status'])
            threads = [threading.Thread(target=claim, args=(number,)) for number in range(1, 5)]
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join()
            require(found.count(SUCCESS) == 1 and all(status in (SUCCESS, 'C301', 'C302') for status in found), f'claims: {found}')
            require(row(uid)['state'] == 'IN PROGRESS', 'state')
            return f'four simultaneous claims: {sorted(found)}'

        def query():
            uid = self.uid()
            require(self.statuses(self.established(self.scripted(target, [n_create_ups(uid, label='Findable'), change_state(uid, 'IN PROGRESS', uid+'.1')], UPS_CONTEXTS))) == [SUCCESS, SUCCESS], 'setup')
            identifier = dict(ProcedureStepLabel='Findable', ProcedureStepState='', TransactionUID='', WorklistLabel='', SOPInstanceUID='')
            result = self.established(self.scripted(target, [dict(op='c_find', model=UPS_QUERY, dataset=identifier), dict(op='c_find', model=UPS_PULL, dataset=dict(identifier, ProcedureStepLabel='NoSuchLabel'))], UPS_CONTEXTS))
            rows = result['steps'][0]['rows']
            require(result['steps'][0]['status'] == SUCCESS and len(rows) == 1 and rows[0].get('ProcedureStepState') == 'IN PROGRESS', f'{result["steps"][0]}')
            require(rows[0].get('SOPInstanceUID') == uid and not rows[0].get('TransactionUID'), f'row {rows[0]}')
            require(result['steps'][1]['status'] == SUCCESS and result['steps'][1]['rows'] == [], f'{result["steps"][1]}')
            directory = self.work/f'ups-find-{self.uid()}'
            directory.mkdir()
            text = self.tool([self.dcm4che/'findscu', '-b', 'FINDSCU', '-c', f'{target.ae}@127.0.0.1:{target.port}', '-M', 'UPSPull', '-m', 'ProcedureStepLabel=Findable', '-r', 'ProcedureStepState', '-r', 'SOPInstanceUID', '--out-dir', directory, '--out-file', 'match'])
            require(rsp_statuses(text)[-1] == ('C-FIND-RSP', SUCCESS), f'{rsp_statuses(text)}')
            matched = [pydicom.dcmread(path, force=True) for path in directory.iterdir()]
            require(len(matched) == 1 and matched[0].SOPInstanceUID == uid, f'dcm4che findscu matched {len(matched)}')
            return 'C-FIND on UPS Query and UPS Pull, from scripted and dcm4che clients; the lock is not returned'

        def body(_):
            prefix = 'dimse_n.ups.scp'
            self.ups_table(target, f'{prefix}.table', requires)
            self.ups_set_cases(target, f'{prefix}.set', requires)
            self.ups_create_cases(target, f'{prefix}.create', requires)
            self.ups_get_cases(target, f'{prefix}.get', requires)
            self.ups_final_state_cases(target, f'{prefix}.final', requires)
            self.ups_context_cases(target, f'{prefix}.context', requires)
            self.upsscu_flow(target, f'{prefix}.dcm4che', requires, 'ups-upsscu')
            self.ups_watch_cases(target, f'{prefix}.watch', requires, RECEIVER_PORT)
            self.case(f'{prefix}.query', query, requires=requires)
            self.case(f'{prefix}.concurrent-claim', concurrent_claim, requires=requires)
            self.case(f'{prefix}.unreachable-subscriber', unreachable_subscriber, requires=requires)
            self.case(f'{prefix}.retry.late-subscriber', late_subscriber, requires=requires)
            self.case(f'{prefix}.persist-before-restart', persisted, requires=requires)
            self.case(f'{prefix}.subscribe-before-restart', subscribed_before_restart, requires=[*requires, f'{prefix}.persist-before-restart'])
        self.group_with('dimse_n.ups.pacs-server', lambda: self.server(self.pacs_args(root, peers, event_attempts=20, event_retry_delay=1), PACS_PORT, log_name='ups-pacs-server'), body)

        record = self.work/'ups-restart.jsonl'

        def restart_reports():
            rows = self.events(record, 1, timeout=12)
            status = [row for row in rows if row['event_type'] == 4]
            require(status, f'no SCP Status Change report in {rows}')
            data = status[0]['dataset']
            require(data.get('SCPStatus') == 'RESTARTED' and data.get('SubscriptionListStatus') == 'WARM START' and data.get('UnifiedProcedureStepListStatus') == 'WARM START', f'{data}')
            return 'the restarted SCP told its subscriber, naming a warm start'

        def lock_survives():
            uid, transaction = survivor['uid'], survivor['transaction']
            found = self.statuses(self.established(self.scripted(target, [change_state(uid, 'IN PROGRESS', transaction+'.9'), n_get_ups(uid)], UPS_CONTEXTS)))
            require(found == ['C301', SUCCESS], f'{found}')
            before = len(self.events(record, None, timeout=.1))
            found = self.statuses(self.established(self.scripted(target, [n_set_ups(uid, transaction, ups_performed()), change_state(uid, 'COMPLETED', transaction)], UPS_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS], f'{found}')
            rows = [row for row in self.events(record, before+1) if row['event_type'] == 1 and row['instance'] == uid]
            require(rows and rows[-1]['dataset']['ProcedureStepState'] == 'COMPLETED', f'{rows}')
            require(row(uid)['state'] == 'COMPLETED', 'row')
            return 'after the restart the lock still refuses a wrong UID and the subscriber still hears the final state'

        def restart_body(_):
            prefix = 'dimse_n.ups.scp'
            need = [*requires, f'{prefix}.subscribe-before-restart']
            self.case(f'{prefix}.restart-reports-status', restart_reports, requires=need)
            self.case(f'{prefix}.lock-and-subscription-survive-restart', lock_survives, requires=need)

        def restart_server():
            return self.server(self.pacs_args(root, peers, event_attempts=2, event_retry_delay=1), PACS_PORT, log_name='ups-pacs-server-restart')
        self.group_with('dimse_n.ups.pacs-server-restart.receiver', lambda: self.receiver(RECEIVER2_AE, RECEIVER2_PORT, record, log='ups-restart-receiver'),
                        lambda _: self.group_with('dimse_n.ups.pacs-server-restart', restart_server, restart_body))
        self.ups_scu_group(requires)

    def ups_scu_group(self, requires):
        target = Target('reference', PEER_UPS_PORT, PEER_AE)
        record = self.work/'ups-scu-peer.jsonl'

        def driver(operation, *arguments, allowed=(0,)):
            text = self.command([self.bin/'dimse_n_scu', '127.0.0.1', PEER_UPS_PORT, PEER_AE, operation, *arguments], timeout=60, allowed=allowed)
            return json.loads(text)

        def peer_view(uid):
            return self.established(self.scripted(target, [n_get_ups(uid)], UPS_CONTEXTS))['steps'][0]

        def lifecycle():
            uid, transaction = self.uid(), self.uid()
            for operation, arguments, wanted in (('ups-create', ('--uid', uid, '--label', 'From PACS', '--worklist', 'Work', '--priority', 'HIGH'), SUCCESS),
                                                 ('ups-get', ('--uid', uid), SUCCESS),
                                                 ('ups-state', ('--uid', uid, '--state', 'IN PROGRESS', '--transaction', transaction), SUCCESS)):
                reply = driver(operation, *arguments)
                require(reply['status'] == wanted, f'{operation}: {reply}')
            seen = peer_view(uid)['dataset']
            require(seen['ProcedureStepState'] == 'IN PROGRESS' and seen['ProcedureStepLabel'] == 'From PACS' and seen['ScheduledProcedureStepPriority'] == 'HIGH', f'peer holds {seen}')
            reply = driver('ups-set', '--uid', uid, '--transaction', transaction, '--label', 'Renamed')
            require(reply['status'] == SUCCESS and peer_view(uid)['dataset']['ProcedureStepLabel'] == 'Renamed', f'{reply}')
            return 'ups_push_scu against the reference; the reference holds what it sent'

        def statuses_surface():
            uid = self.uid()
            for operation, arguments, wanted in (('ups-get', ('--uid', uid), 'C307'), ('ups-state', ('--uid', uid, '--state', 'IN PROGRESS', '--transaction', self.uid()), 'C307'),
                                                 ('ups-cancel', ('--uid', uid, '--reason', 'No'), 'C307')):
                reply = driver(operation, *arguments, allowed=(1,))
                require(reply['status'] == wanted, f'{operation}: {reply}')
            return 'failure statuses reach the caller and exit 1'

        def cancel():
            uid = self.uid()
            driver('ups-create', '--uid', uid, '--label', 'Cancel me')
            reply = driver('ups-cancel', '--uid', uid, '--reason', 'Not needed')
            require(reply['status'] == SUCCESS and peer_view(uid)['dataset']['ProcedureStepState'] == 'CANCELED', f'{reply}')
            return 'request_cancel uses Action Type 2, so the reference canceled the step'

        def subscribe():
            uid = self.uid()
            driver('ups-create', '--uid', uid, '--label', 'Watch me')
            reply = driver('ups-subscribe', '--uid', uid, '--receiving-ae', RECEIVER_AE, '--lock')
            require(reply['status'] == SUCCESS, f'{reply}')
            sent = [row for row in self.records(record) if row.get('kind') == 'sent' and row['instance'] == uid]
            deadline = time.monotonic()+8
            while not sent and time.monotonic() < deadline:
                time.sleep(.1)
                sent = [row for row in self.records(record) if row.get('kind') == 'sent' and row['instance'] == uid]
            require(sent and sent[0]['receiver'] == RECEIVER_AE, f'{sent}')
            reply = driver('ups-unsubscribe', '--uid', uid, '--receiving-ae', RECEIVER_AE)
            require(reply['status'] == SUCCESS, f'{reply}')
            reply = driver('ups-subscribe', '--uid', uid, '--receiving-ae', 'NOBODY_KNOWN', allowed=(1,))
            require(reply['status'] == 'C308', f'{reply}')
            for operation, arguments in (('ups-subscribe', ('--receiving-ae', RECEIVER_AE)), ('ups-suspend', ('--receiving-ae', RECEIVER_AE)),
                                         ('ups-unsubscribe', ('--receiving-ae', RECEIVER_AE))):
                reply = driver(operation, *arguments)
                require(reply['status'] == SUCCESS, f'{operation}: {reply}')
            return 'ups_watch_scu names the Receiving AE and Deletion Lock, so the reference accepted it'

        def body(_):
            self.case('dimse_n.ups.scu.lifecycle', lifecycle, requires=requires)
            self.case('dimse_n.ups.scu.failure-statuses', statuses_surface, requires=requires)
            self.case('dimse_n.ups.scu.request-cancel', cancel, requires=requires)
            self.case('dimse_n.ups.scu.subscribe', subscribe, requires=requires)
        self.group_with('dimse_n.ups.scu', lambda: self.server(['python3', self.scripts/'ups_reference_scp.py', '--aet', PEER_AE, '--port', PEER_UPS_PORT, '--record', record, '--peer', f'{RECEIVER_AE}:127.0.0.1:{RECEIVER_PORT}'], PEER_UPS_PORT, log_name='ups-scu-peer'), body)

    # -- Print

    def print_config(self, directory, printer, port):
        directory.mkdir(parents=True)
        for name in ('db', 'spool'):
            (directory/name).mkdir()
        config = directory/'print.cfg'
        config.write_text(f'''[[GENERAL]]
[DATABASE]
Directory = {directory/"db"}
[PRINT]
Spooler = dcmprscu
Server = dcmprscp
Directory = {directory/"spool"}
LogFile = print.log
[NETWORK]
Aetitle = PRINT_SCU
[[COMMUNICATION]]
[{printer}]
Aetitle = {printer}
Description = Print SCP under test
Hostname = 127.0.0.1
Port = {port}
Type = LOCALPRINTER
BorderDensity = 150\\20\\BLACK\\WHITE
DisableNewVRs = false
DisplayFormat=1,1\\1,2\\2,2\\2,3\\3,3
EmptyImageDensity = 20\\BLACK\\WHITE
FilmDestination = MAGAZINE\\PROCESSOR
FilmSizeID = 8INX10IN\\14INX17IN
ImplicitOnly = false
MagnificationType = REPLICATE\\BILINEAR\\CUBIC\\NONE
MaxDensity = 320
MaxPDU = 32768
MediumType = PAPER\\CLEAR FILM\\BLUE FILM
MinDensity = 20
OmitSOPClassUIDFromCreateResponse = false
PresentationLUTMatchRequired = false
PresentationLUTinFilmSession = false
ResolutionID = STANDARD\\HIGH
SmoothingType = NONE
Supports12Bit = true
SupportsDecimateCrop = false
SupportsImageSize = false
SupportsPresentationLUT = false
SupportsTrim = false
''')
        return config

    def print_flow(self, session, box, meta=GRAY_META, image_box=GRAY_IMAGE_BOX, image=None, display='STANDARD\\2,2'):
        return [print_step('n_create', FILM_SESSION, session, meta=meta, dataset=SESSION),
                print_step('n_create', FILM_BOX, box, meta=meta, dataset=film_box_values(session, ImageDisplayFormat=display)),
                print_step('n_set', image_box, '@image_box:0', meta=meta, dataset=image or image_box_values()),
                print_step('n_action', FILM_BOX, box, meta=meta, action=1),
                print_step('n_delete', FILM_BOX, box, meta=meta),
                print_step('n_delete', FILM_SESSION, session, meta=meta)]

    def check_print_flow(self, result, boxes=4, image_box=GRAY_IMAGE_BOX):
        found = self.statuses(self.established(result))
        require(found == [SUCCESS]*6, f'flow statuses: {found}')
        references = result['steps'][1].get('dataset', {}).get('ReferencedImageBoxSequence', [])
        require(len(references) == boxes, f'{len(references)} image boxes for the display format')
        require(all(row['ReferencedSOPClassUID'] == image_box for row in references), f'image box class {references[0]}')
        require(len({row['ReferencedSOPInstanceUID'] for row in references}) == boxes, 'image box UIDs repeat')

    # (name, session?, the request, status) of PS3.4 H.4; `session` and `box` exist unless stated.
    def print_request(self, name, session, box):
        base = [print_step('n_create', FILM_SESSION, session, dataset=SESSION)]
        with_box = [*base, print_step('n_create', FILM_BOX, box, dataset=film_box_values(session))]
        without_session = film_box_values(session)
        del without_session['ReferencedFilmSessionSequence']
        return {
            'film-box-without-session': (base, print_step('n_create', FILM_BOX, box, dataset=without_session)),
            'film-box-unknown-session': (base, print_step('n_create', FILM_BOX, box, dataset=film_box_values(self.uid()))),
            'film-box-bad-format': (base, print_step('n_create', FILM_BOX, box, dataset=film_box_values(session, ImageDisplayFormat='STANDARD\\A,B'))),
            'session-bad-copies': ([], print_step('n_create', FILM_SESSION, session, dataset={**{key: value for key, value in SESSION.items() if key != 'NumberOfCopies'}, '@20000010:IS': 'many'})),
            'image-box-unknown': (with_box, print_step('n_set', GRAY_IMAGE_BOX, self.uid(), dataset=image_box_values())),
            'image-box-no-sequence': (with_box, print_step('n_set', GRAY_IMAGE_BOX, '@image_box:0', dataset=dict(ImageBoxPosition=1))),
            'image-box-wrong-position': (with_box, print_step('n_set', GRAY_IMAGE_BOX, '@image_box:0', dataset=image_box_values(position=3))),
            'action-unknown-box': (with_box, print_step('n_action', FILM_BOX, self.uid(), action=1)),
            'action-wrong-type': (with_box, print_step('n_action', FILM_BOX, box, action=7)),
            'action-empty-page': (with_box, print_step('n_action', FILM_BOX, box, action=1)),
            'session-action-without-box': (base, print_step('n_action', FILM_SESSION, session, action=1)),
            'delete-unknown-box': (with_box, print_step('n_delete', FILM_BOX, self.uid())),
            'delete-unknown-session': (base, print_step('n_delete', FILM_SESSION, self.uid())),
            'second-session': (base, print_step('n_create', FILM_SESSION, self.uid(), dataset=SESSION)),
            'printer-wrong-instance': ([], print_step('n_get', PRINTER, self.uid())),
        }[name]

    def print_refusals(self, target, expected):
        def run(name, wanted):
            def body():
                session, box = self.uid(), self.uid()
                lead, request = self.print_request(name, session, box)
                result = self.established(self.scripted(target, [*lead, request], PRINT_CONTEXTS))
                found = self.statuses(result)
                require(found[:-1] == [SUCCESS]*len(lead), f'setup: {found}')
                require(found[-1] == wanted, f'expected {wanted}, got {found[-1]}')
                return found[-1]
            return body
        return [(name, run(name, wanted)) for name, wanted in expected.items()]

    def peer_print(self):
        directory = self.work/'peer-print'
        config = self.print_config(directory, PEER_AE, PEER_PRINT_PORT)
        target = Target('dcmprscp', PEER_PRINT_PORT, PEER_AE)
        with self.server(['dcmprscp', '-c', config, '-p', PEER_AE, '--dump'], PEER_PRINT_PORT, log_name='peer-print-scp'):
            self.spooled_print(config, PEER_AE, directory)
            self.check_print_flow(self.scripted(target, self.print_flow(self.uid(), self.uid()), PRINT_CONTEXTS))
            printer = self.established(self.scripted(target, [print_step('n_get', PRINTER, PRINTER_INSTANCE)], PRINT_CONTEXTS))
            require(self.statuses(printer) == [SUCCESS] and printer['steps'][0]['dataset'].get('PrinterStatus'), f'printer: {printer}')
            answers = {}
            for name in PACS_PRINT_REFUSALS:
                session, box = self.uid(), self.uid()
                lead, request = self.print_request(name, session, box)
                found = self.statuses(self.established(self.scripted(target, [*lead, request], PRINT_CONTEXTS)))
                require(found[:-1] == [SUCCESS]*len(lead), f'{name} setup: {found}')
                answers[name] = found[-1]
            (self.output/'print-reference-answers.json').write_text(json.dumps(answers, indent=1)+'\n')
            differences = {name: answer for name, answer in answers.items() if answer != PACS_PRINT_REFUSALS[name]}
            require(differences == REFERENCE_PRINT_DIFFERENCES, f'dcmprscp differs from the expected table in other ways: {differences}')
        return 'dcmprscu to dcmprscp, and the scripted Print SCU flow and refusals against dcmprscp'

    def spooled_print(self, config, printer, directory):
        """Create a hardcopy with DCMTK's own tool and send it with dcmprscu, returning the log."""
        self.tool(['dcmpsprt', '-c', config, '-p', printer, self.images/'kept.dcm'])
        stored = sorted((directory/'db').glob('SP_*'))
        require(stored, 'dcmpsprt wrote no stored print object')
        text = self.tool(['dcmprscu', '-c', config, '-p', printer, '--dump', '--print', stored[-1]])
        statuses = re.findall(r'DIMSE Status\s+: 0x([0-9A-Fa-f]{4})', text)
        operations = re.findall(r'Message Type\s+: (N-[A-Z-]+) RQ', text)
        require(operations == ['N-GET', 'N-CREATE', 'N-CREATE', 'N-SET', 'N-ACTION', 'N-DELETE', 'N-DELETE'], f'dcmprscu sent {operations}')
        require(statuses and all(status == SUCCESS for status in statuses), f'dcmprscu statuses {statuses}')
        return text

    def print_group(self):
        requires = ['dimse_n.peer.print']
        root = self.work/'print-pacs'
        target = Target('pacs_server', PACS_PORT, PACS_AE)
        spool = Path(f'{root/"archive"}.print-spool')
        config_directory = self.work/'print-pacs-config'
        config = self.print_config(config_directory, PACS_AE, PACS_PORT)

        def job_for(box):
            manifest = spool/box/'job.json'
            deadline = time.monotonic()+5
            while not manifest.exists() and time.monotonic() < deadline:
                time.sleep(.1)
            require(manifest.exists(), f'no print job for film box {box}')
            return json.loads(manifest.read_text())

        def dcmprscu():
            text = self.spooled_print(config, PACS_AE, config_directory)
            jobs = sorted(spool.iterdir()) if spool.exists() else []
            require(len(jobs) == 1, f'{len(jobs)} jobs spooled')
            job = json.loads((jobs[0]/'job.json').read_text())
            require(len(job['images']) >= 1 and job['images'][0]['rows'] == 4 and job['images'][0]['columns'] == 4, f'job {job}')
            return f'DCMTK dcmprscu printed through pacs_server; job {jobs[0].name}'

        def flow():
            session, box = self.uid(), self.uid()
            result = self.scripted(target, self.print_flow(session, box), PRINT_CONTEXTS)
            self.check_print_flow(result)
            job = job_for(box)
            require(job['film_session'] == session and job['display_format'] == 'STANDARD\\2,2', f'job {job}')
            require(job['copies'] == 1 and job['medium_type'] == 'PAPER' and job['film_destination'] == 'MAGAZINE' and job['priority'] == 'MED', f'job {job}')
            image = job['images'][0]
            require(image['position'] == 1 and image['rows'] == 4 and image['columns'] == 4, f'image {image}')
            require((spool/box/image['pixels']).read_bytes() == GRAY_PIXELS, 'printed pixels differ from those sent')
            return 'session, box, image, print and delete; the spooled job holds the sent pixels'

        def color():
            session, box = self.uid(), self.uid()
            image = dict(ImageBoxPosition=1, BasicColorImageSequence=[dict(
                SamplesPerPixel=3, PhotometricInterpretation='RGB', PlanarConfiguration=0, Rows=2, Columns=2, BitsAllocated=8, BitsStored=8, HighBit=7,
                PixelRepresentation=0, PixelAspectRatio=['1', '1'], PixelData=base64.b64encode(bytes(range(12))).decode())])
            result = self.scripted(target, self.print_flow(session, box, meta=COLOR_META, image_box=COLOR_IMAGE_BOX, image=image, display='STANDARD\\1,1'), PRINT_CONTEXTS)
            self.check_print_flow(result, boxes=1, image_box=COLOR_IMAGE_BOX)
            job = job_for(box)
            require((spool/box/job['images'][0]['pixels']).read_bytes() == bytes(range(12)) and job['images'][0]['photometric'] == 'RGB', f'job {job}')
            return 'Basic Color Print Meta uses the Color Image Box class throughout'

        def printer():
            result = self.established(self.scripted(target, [print_step('n_get', PRINTER, PRINTER_INSTANCE, meta=None), print_step('n_get', PRINTER, PRINTER_INSTANCE, meta=GRAY_META)], PRINT_CONTEXTS))
            require(self.statuses(result) == [SUCCESS, SUCCESS], f'{self.statuses(result)}')
            for step in result['steps']:
                require(step['dataset'].get('PrinterStatus') == 'NORMAL' and step['dataset'].get('PrinterStatusInfo') == 'NORMAL', f'{step}')
            return 'Printer N-GET on its own context and under the Meta SOP Class'

        def lifetime():
            session, box = self.uid(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [print_step('n_create', FILM_SESSION, session, dataset=SESSION),
                                                                          print_step('n_create', FILM_BOX, box, dataset=film_box_values(session))], PRINT_CONTEXTS)))
            require(found == [SUCCESS, SUCCESS], f'{found}')
            found = self.statuses(self.established(self.scripted(target, [print_step('n_action', FILM_BOX, box, action=1), print_step('n_delete', FILM_SESSION, session)], PRINT_CONTEXTS)))
            require(found == ['0112', '0112'], f'{found}')
            return 'a session and its boxes end with their association and are not reachable from another'

        def isolation():
            first, second = self.uid(), self.uid()
            found = self.statuses(self.established(self.scripted(target, [print_step('n_create', FILM_SESSION, first, dataset=SESSION),
                                                                          print_step('n_create', FILM_BOX, self.uid(), dataset=film_box_values(second))], PRINT_CONTEXTS)))
            require(found == [SUCCESS, '0106'], f'{found}')
            return 'a Film Box only joins the session it names'

        def body(_):
            self.case('dimse_n.print.scp.dcmprscu', dcmprscu, requires=requires)
            self.case('dimse_n.print.scp.flow', flow, requires=requires)
            self.case('dimse_n.print.scp.color', color, requires=requires)
            self.case('dimse_n.print.scp.printer', printer, requires=requires)
            self.case('dimse_n.print.scp.lifetime', lifetime, requires=requires)
            self.case('dimse_n.print.scp.names-its-session', isolation, requires=requires)

            def no_common_context():
                result = self.scripted(target, [], ['1.2.826.0.1.3680043.8.498.1320.999'])
                require(not result['established'] and result['rejected'], f'not an A-ASSOCIATE-RJ: {result}')
                return 'no SOP Class in common is answered with A-ASSOCIATE-RJ'
            self.case('dimse_n.association.no-common-context', no_common_context, requires=requires)
            for name, run in self.print_refusals(target, PACS_PRINT_REFUSALS):
                self.case(f'dimse_n.print.scp.refuse.{name}', run, requires=requires)
        self.group_with('dimse_n.print.pacs-server', lambda: self.server(self.pacs_args(root), PACS_PORT, log_name='print-pacs-server'), body)
        self.print_scu_group(requires)

    def peer_print_scripted(self):
        directory = self.work/'peer-print-scripted'
        config = self.print_config(directory, PEER_AE, PRINT_SCRIPTED_PORT)
        record = self.work/'peer-print-scripted.jsonl'
        with self.server(['python3', self.scripts/'print_scp.py', '--aet', PEER_AE, '--port', PRINT_SCRIPTED_PORT, '--record', record], PRINT_SCRIPTED_PORT, log_name='peer-print-scripted'):
            self.spooled_print(config, PEER_AE, directory)
        printed = [row for row in self.records(record) if row['kind'] == 'printed']
        require(len(printed) == 1 and printed[0]['images'][0]['image_class'] == GRAY_IMAGE_BOX and printed[0]['images'][0]['rows'] >= 1, f'{printed}')
        return 'DCMTK dcmprscu printed through the scripted Print SCP, which recorded the image'

    def print_color_group(self, requires):
        record = self.work/'print-scu-color.jsonl'

        def color():
            rgb = self.images/'color.dcm'
            write_color_image(rgb, '1.2.826.0.1.3680043.8.498.1320.100.9')
            self.tool([self.bin/'print_scu', '127.0.0.1', PRINT_SCRIPTED_PORT, PEER_AE, 'print', '--calling-ae', 'PACS_PRINT_SCU', '--medium', 'PAPER', '--format', 'STANDARD\\1,1', '--image', rgb, '--color'])
            rows = [row for row in self.records(record) if row['kind'] == 'printed']
            require(len(rows) == 1 and rows[0]['context'] == COLOR_META, f'{rows}')
            image = rows[0]['images'][0]
            require(image['image_class'] == COLOR_IMAGE_BOX and image['photometric'] == 'RGB' and image['rows'] == 2 and image['columns'] == 2, f'{image}')
            require(image['sha256'] == hashlib.sha256(COLOR_PIXELS).hexdigest(), 'the printer holds other pixels than those sent')
            return 'print_scu --color went through the Color Print Meta SOP Class with its image intact'
        self.group_with('dimse_n.print.scu-color', lambda: self.server(['python3', self.scripts/'print_scp.py', '--aet', PEER_AE, '--port', PRINT_SCRIPTED_PORT, '--record', record], PRINT_SCRIPTED_PORT, log_name='print-scu-color-peer'),
                        lambda _: self.case('dimse_n.print.scu.color', color, requires=[*requires, 'dimse_n.peer.print-scripted']))

    def print_scu_group(self, requires):
        self.print_color_group(requires)
        directory = self.work/'print-scu-peer'
        config = self.print_config(directory, PEER_AE, PEER_PRINT_PORT)
        log = self.output/'print-scu-peer-server.log'

        def scu(*arguments, allowed=(0,)):
            return self.tool([self.bin/'print_scu', '127.0.0.1', PEER_PRINT_PORT, PEER_AE, *arguments, '--calling-ae', 'PACS_PRINT_SCU'], allowed=allowed)

        def printed():
            scu('print', '--copies', '2', '--medium', 'PAPER', '--format', 'STANDARD\\1,1', '--film-size', '8INX10IN', '--image', self.images/'kept.dcm')
            text = log.read_text(errors='replace')
            operations = re.findall(r'Message Type\s+: (N-[A-Z-]+) RQ', text)
            require(operations == ['N-CREATE', 'N-CREATE', 'N-SET', 'N-ACTION', 'N-DELETE'], f'dcmprscp saw {operations}')
            statuses = re.findall(r'DIMSE Status\s+: 0x([0-9A-Fa-f]{4})', text)
            require(statuses and all(status == SUCCESS for status in statuses), f'dcmprscp answered {statuses}')
            printed_images = sorted((directory/'db').glob('HG_*'))
            require(printed_images, f'dcmprscp holds no hardcopy image in {sorted(path.name for path in (directory/"db").iterdir())}')
            image = pydicom.dcmread(printed_images[-1], force=True)
            require(image.Rows == 4 and image.Columns == 4, f'printed image is {image.Rows}x{image.Columns}')
            require(bytes(image.PixelData)[:len(GRAY_PIXELS)] == GRAY_PIXELS, 'the printer holds other pixels than those sent')
            return 'print_scu through dcmprscp: every request answered Success, and the printer holds the sent image'

        def status():
            text = scu('status')
            require('NORMAL' in text, f'output: {text[:200]}')
            return 'print_scu status reads the reference printer through N-GET'

        def refused():
            scu('print', '--medium', 'PAPER', '--format', 'NOT_A_FORMAT', allowed=(1,))
            return 'a Film Box the printer refuses is exit 1'
        self.group_with('dimse_n.print.scu', lambda: self.server(['dcmprscp', '-c', config, '-p', PEER_AE, '--dump'], PEER_PRINT_PORT, log_name='print-scu-peer'),
                        lambda _: (self.case('dimse_n.print.scu.print', printed, requires=requires),
                                   self.case('dimse_n.print.scu.status', status, requires=requires),
                                   self.case('dimse_n.print.scu.refused', refused, requires=requires)))

    # -- N-GET

    def nget_group(self):
        requires = ['dimse_n.peer.print']
        directory = self.work/'nget-peer'
        config = self.print_config(directory, PEER_AE, PEER_PRINT_PORT)

        def positive():
            reply = json.loads(self.command([self.bin/'dimse_n_scu', '127.0.0.1', PEER_PRINT_PORT, PEER_AE, 'n-get', '--context', GRAY_META, '--class', PRINTER, '--instance', PRINTER_INSTANCE, '--tag', '21100010'], timeout=30))
            require(reply['status'] == SUCCESS and reply['dataset'].get('21100010') == 'NORMAL', f'{reply}')
            return 'the n_get_scu class read Printer Status from dcmprscp'

        def unknown():
            reply = json.loads(self.command([self.bin/'dimse_n_scu', '127.0.0.1', PEER_PRINT_PORT, PEER_AE, 'n-get', '--context', GRAY_META, '--class', PRINTER, '--instance', self.uid()], timeout=30, allowed=(1,)))
            require(reply['status'] not in (SUCCESS, None), f'{reply}')
            return f'n_get_scu reports the refusal: {reply["status"]}'
        self.group_with('dimse_n.nget.scu', lambda: self.server(['dcmprscp', '-c', config, '-p', PEER_AE], PEER_PRINT_PORT, log_name='nget-peer'),
                        lambda _: (self.case('dimse_n.nget.scu.printer', positive, requires=requires),
                                   self.case('dimse_n.nget.scu.unknown-instance', unknown, requires=requires)))

    # -- running

    def requirement_results(self):
        status = {row['id']: row['status'] for row in self.results}
        results = {}
        declared = declared_support(Path(__file__).resolve().parents[3])
        for requirement, patterns in requirement_cases(declared).items():
            matched = [case for case in status if any(fnmatch.fnmatchcase(case, pattern) for pattern in patterns)]
            results[requirement] = dict(patterns=patterns, cases=len(matched), passed=sum(status[case] == 'passed' for case in matched),
                                        status='unexecuted' if not matched else 'passed' if all(status[case] == 'passed' for case in matched) else 'failed')
        return results

    def peers_group(self):
        self.case('dimse_n.peer.versions', self.versions, phase='prerequisite')
        self.case('dimse_n.peer.mpps', self.peer_mpps, requires=['dimse_n.fixtures'], phase='prerequisite')
        self.case('dimse_n.peer.mwl', self.peer_mwl, phase='prerequisite')
        self.case('dimse_n.peer.stgcmt', self.peer_stgcmt, requires=['dimse_n.fixtures'], phase='prerequisite')
        self.case('dimse_n.peer.print', self.peer_print, requires=['dimse_n.fixtures'], phase='prerequisite')
        self.case('dimse_n.peer.print-scripted', self.peer_print_scripted, requires=['dimse_n.fixtures'], phase='prerequisite')
        self.peer_ups_group()

    def run_all(self, groups):
        self.case('dimse_n.fixtures', self.fixtures, phase='fixture_preparation')
        self.peers_group()
        for name, group in (('mpps', self.mpps_group), ('mwl', self.mwl_group), ('stgcmt', self.stgcmt_group), ('ups', self.ups_group),
                            ('print', self.print_group), ('nget', self.nget_group)):
            if name in groups:
                group()
        counts = {status: sum(1 for row in self.results if row['status'] == status) for status in ('passed', 'failed', 'blocked')}
        (self.output/'dimse-n-summary.json').write_text(json.dumps(dict(
            counts=counts, requirement_results=self.requirement_results(),
            failures=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'failed'],
            blocked=[row['id'] for row in self.results if row['status'] == 'blocked']), indent=1)+'\n')
        print(json.dumps(counts))
        return int(counts['failed'] > 0 or counts['blocked'] > 0)


def pydicom_like(plain):
    """check_commitment reads pydicom Datasets; give it the same shape from a receiver's JSON record."""
    ds = Dataset()
    for keyword in ('ReferencedSOPSequence', 'FailedSOPSequence'):
        if keyword in plain:
            items = []
            for row in plain[keyword]:
                item = Dataset()
                item.ReferencedSOPClassUID, item.ReferencedSOPInstanceUID = row['ReferencedSOPClassUID'], row['ReferencedSOPInstanceUID']
                if 'FailureReason' in row:
                    item.FailureReason = row['FailureReason']
                items.append(item)
            setattr(ds, keyword, items)
    return ds


# Where dcmprscp answers otherwise than PACS must, with why PS3.4 or PS3.7 sides with the PACS status.
REFERENCE_PRINT_DIFFERENCES = {
    'action-wrong-type': 'B603',  # dcmprscp never reads the Action Type ID; PS3.7 10.1.4.1.6 makes it 0123H.
    'second-session': '0111',  # H.4.1.2.1 allows one session per association and names no status; the UID here is not a duplicate.
    'image-box-wrong-position': '0000',  # dcmprscp takes the position as sent; PACS refuses rather than print in the wrong place.
}
PACS_PRINT_REFUSALS = {
    'film-box-without-session': '0120', 'film-box-unknown-session': '0106', 'film-box-bad-format': '0106', 'session-bad-copies': '0106',
    'image-box-unknown': '0112', 'image-box-no-sequence': '0120', 'image-box-wrong-position': '0106', 'action-unknown-box': '0112', 'action-wrong-type': '0123', 'action-empty-page': 'B603',
    'session-action-without-box': 'C600', 'delete-unknown-box': '0112', 'delete-unknown-session': '0112', 'second-session': '0213',
    'printer-wrong-instance': '0112'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--peers-dir', default='/opt/pacs-peers')
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--source-digest', required=True)
    parser.add_argument('--groups', default='mpps,mwl,stgcmt,ups,print,nget')
    args = parser.parse_args()
    args.profile = 'uncompressed'
    return DimseN(args).run_all(set(args.groups.split(',')))


if __name__ == '__main__':
    raise SystemExit(main())
