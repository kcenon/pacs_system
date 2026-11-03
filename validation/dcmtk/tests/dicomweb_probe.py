#!/usr/bin/env python3
"""DICOMweb and DIMSE against one archive, through clients that share no PACS code (#1321).

The same tables run first against Orthanc with its DICOMweb plugin, with no PACS process at all: a case PACS is held to must
hold between independent implementations, or be listed with the reason why not. Objects stored over one protocol are read over
the other and compared with what was sent: UIDs, attributes, frames, fragments, files and index rows.
"""
import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import fnmatch
import hashlib
from io import BytesIO
import json
import math
import os
from pathlib import Path
import re
import shutil
import socket
import sqlite3
import struct
import subprocess
import sys
import time
from urllib.parse import quote, urlencode, urljoin, urlsplit
import warnings

import pydicom
from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.encaps import generate_pixel_data_frame
from pydicom.sequence import Sequence

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
sys.path.insert(0, str(ROOT/'validation/peers/http'))
warnings.filterwarnings('ignore', category=UserWarning)
from dicomweb_cross import (ACCEPT_CASES, BASIC_TEXT_SR, BULK_EQUIVALENT_CASES, CT, DICOM_MULTIPART, INJECTION_VALUES, EXPLICIT, FAILED_SOP_SEQUENCE, FAILURE_REASON, FRAME_REFUSALS,
                            IMPLICIT, JPEG_BASELINE, JPEG_LS, JSON_MEDIA, MULTIFRAME_BYTE, OCTET_MULTIPART, PAGE_SERIES_INSTANCES, PAGE_SIZES,
                            PAGE_STUDIES, QIDO_BAD_PARAMETERS, QIDO_FILTERS, QIDO_STUDIES, READ, REFERENCE_DIFFERENCES, REFERENCE_LENIENCY, REFERENCED_CLASS,
                            REFERENCED_INSTANCE, REFERENCED_SOP_SEQUENCE, REFUSED_TOKENS, RETRIEVE_URL, RLE, ROUTES, SEARCH,
                            SECONDARY_CAPTURE, STOW_ALL_FAILED, STOW_PARTIAL, STOW_REFUSALS, WRITE, XML_CASES, XML_MULTIPART, requirement_cases)
from http_peer import PayloadError, TokenKit, build_multipart, parse_media_type, parse_response_headers, read_jpeg, read_png, split_multipart
from run import Validation, require

PACS_DIMSE, PACS_HTTP, ORTHANC_DIMSE, ORTHANC_HTTP, MOVE_PORT, AUTH_DIMSE, AUTH_HTTP, LIMIT_DIMSE, LIMIT_HTTP = 23400, 23401, 23402, 23403, 23404, 23405, 23406, 23407, 23408
PACS_AE, ORTHANC_AE, SCU_AE, MOVE_AE = 'PACS_W', 'ORTHANC_W', 'WEB_SCU', 'WEB_MOVE'
ISSUER, AUDIENCE = 'https://issuer.example.test', 'pacs-validation'
SR_TEXT = 'No object should ever be lost.'
PIXEL_TAG = 0x7FE00010
STORE_FLAGS = {EXPLICIT: ['-xe'], IMPLICIT: ['-xi'], RLE: ['-xr'], JPEG_LS: ['-xt'], JPEG_BASELINE: ['-xy']}
GET_FLAGS = {EXPLICIT: ['+xe'], IMPLICIT: ['+xi'], RLE: ['+xr'], JPEG_LS: ['+xt'], JPEG_BASELINE: ['+xy']}


class Target:
    def __init__(self, name, http_port, dimse_port, ae, root, root_directory=None, is_pacs=False):
        self.name, self.http_port, self.dimse_port, self.ae, self.root = name, http_port, dimse_port, ae, root
        self.directory, self.is_pacs = root_directory, is_pacs

    def base(self):
        return f'http://127.0.0.1:{self.http_port}'


class Reply:
    def __init__(self, status, headers, body, url):
        self.status, self.headers, self.body, self.url = status, headers, body, url

    def header(self, name):
        values = self.headers.get(name, [])
        return values[-1] if values else ''

    def media(self):
        return parse_media_type(self.header('content-type'))[0] if self.header('content-type') else ''

    def json(self):
        return json.loads(self.body.decode('utf-8'))

    def parts(self):
        return split_multipart(self.header('content-type'), self.body)

    def brief(self):
        return f'HTTP {self.status} {self.header("content-type")!r} {self.body[:160]!r}'


class Fixture:
    def __init__(self, key, path, study, series, sop, sop_class, syntax, frames=()):
        self.key, self.path, self.study, self.series, self.sop, self.sop_class = key, Path(path), study, series, sop, sop_class
        self.syntax, self.frames = syntax, list(frames)

    def dataset(self):
        return pydicom.dcmread(self.path)

    def default_syntax(self):
        """PS3.18 8.7.3: Explicit VR Little Endian unless the only pixel data held is lossy."""
        return self.syntax if self.syntax == JPEG_BASELINE else EXPLICIT

    def as_stored_syntax(self):
        """transfer-syntax=*: what is held, except the two syntaxes Web Services shall not use."""
        return EXPLICIT if self.syntax == IMPLICIT else self.syntax


def encoded_frames(ds):
    """The frames of a dataset read from a file, as encoded."""
    frame_count = int(getattr(ds, 'NumberOfFrames', 1) or 1)
    if ds.file_meta.TransferSyntaxUID in (EXPLICIT, IMPLICIT):
        size = len(ds.PixelData)//frame_count
        return [ds.PixelData[index*size:(index+1)*size] for index in range(frame_count)]
    return list(generate_pixel_data_frame(ds.PixelData, frame_count))


def fragment_count(ds):
    """Items of encapsulated pixel data after the Basic Offset Table (PS3.5 A.4)."""
    data, position, count = ds.PixelData, 0, -1
    while position+8 <= len(data):
        group, element, length = struct.unpack('<HHL', data[position:position+8])
        if (group, element) == (0xFFFE, 0xE0DD):
            break
        require((group, element) == (0xFFFE, 0xE000), f'no item at {position} of the encapsulated pixel data')
        count += 1
        position += 8+length
    return count


def same_value(vr, found, sent):
    """Equal as DICOM values: DS 120 and the JSON number 120.0 are one value, and so are nested items read either way."""
    if vr == 'SQ':
        if len(found) != len(sent):
            return False
        for found_item, sent_item in zip(found, sent):
            found_view, sent_view = attribute_view(found_item), attribute_view(sent_item)
            if set(found_view) != set(sent_view):
                return False
            if not all(same_value(sent_view[tag][0], found_view[tag][1], sent_view[tag][1]) for tag in sent_view):
                return False
        return True
    listing = lambda value: list(value) if isinstance(value, (list, pydicom.multival.MultiValue)) else [value]
    found_values, sent_values = listing(found), listing(sent)
    if len(found_values) != len(sent_values):
        return False
    if vr in ('DS', 'FL', 'FD'):
        return all(float(a) == float(b) for a, b in zip(found_values, sent_values))
    if vr in ('IS', 'US', 'SS', 'UL', 'SL', 'UV', 'SV'):
        return all(int(a) == int(b) for a, b in zip(found_values, sent_values))
    return all(str(a) == str(b) for a, b in zip(found_values, sent_values))


def attribute_view(ds):
    """Everything but file meta and pixel data, comparable across two reads of the same object."""
    view = {}
    for element in ds:
        if element.tag.group == 2 or element.tag == PIXEL_TAG:
            continue
        view[int(element.tag)] = (element.VR, element.value)
    return view


def compare_attributes(found, source, label):
    found_view, source_view = attribute_view(found), attribute_view(source)
    missing = sorted(set(source_view)-set(found_view))
    extra = sorted(set(found_view)-set(source_view))
    require(not missing, f'{label}: attributes lost: {[hex(tag) for tag in missing]}')
    require(not extra, f'{label}: attributes added: {[hex(tag) for tag in extra]}')
    for tag, (vr, value) in source_view.items():
        require(same_value(vr, found_view[tag][1], value), f'{label}: {tag:08x} is {found_view[tag][1]!r}, sent {value!r}')


class DicomWebCross(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.peers = Path(args.peers_dir)
        self.dcm4che = self.peers/'dcm4che-5.35.2/bin'
        self.work = self.output/'work'
        self.work.mkdir()
        (self.work/'http').mkdir()
        self.images = self.work/'objects'
        self.images.mkdir()
        self.sequence = 0
        self.uid_base = f'2.25.1321{int(time.time())}'
        self.sets = {}
        self.tokens = None
        self.lenient_for = ()

    def lenient(self, key):
        """Whether the target being run is the reference, for a difference REFERENCE_LENIENCY states. PACS never is."""
        require(key in REFERENCE_LENIENCY, f'{key} is not a stated difference')
        return key in self.lenient_for

    # -- plumbing

    def uid(self, *_labels):
        """A new UID: digits and dots, as PS3.5 9.1 has them. Labels only say what it is for at the call."""
        self.sequence += 1
        return f'{self.uid_base}.{self.sequence}'

    def http(self, target, method, path, accept=JSON_MEDIA, headers=(), data=None, content_type=None, token=None, timeout=90):
        """One request by curl; `path` starts at the DICOMweb root, or at the host when it begins with @."""
        self.sequence += 1
        head, body = self.work/'http'/f'{self.sequence}.head', self.work/'http'/f'{self.sequence}.body'
        url = target.base()+(path[1:] if path.startswith('@') else target.root+path)
        args = ['curl', '-sS', '--http1.1', '--path-as-is', '--max-time', timeout, '-D', head, '-o', body, '-X', method, '-H', 'Expect:']
        args += ['-H', 'Accept:' if accept is None else f'Accept: {accept}']
        if token is not None:
            args += ['-H', f'Authorization: {token}']
        if content_type is not None:
            args += ['-H', f'Content-Type: {content_type}']
        for header in headers:
            args += ['-H', header]
        if data is not None:
            upload = self.work/'http'/f'{self.sequence}.upload'
            upload.write_bytes(data)
            args += ['--data-binary', f'@{upload}']
        self.command([*args, url], timeout=timeout+5)
        status, reply_headers = parse_response_headers(head.read_bytes())
        reply = Reply(status, reply_headers, body.read_bytes() if body.exists() else b'', url)
        for path_used in (head, body, self.work/'http'/f'{self.sequence}.upload'):
            if path_used.exists() and path_used.stat().st_size > 1 << 20:
                path_used.unlink()
        return reply

    def bearer(self, kind='valid', scope=f'{READ} {WRITE}'):
        return 'Bearer '+self.token_of(kind, scope)

    def tool(self, *args, timeout=120, allowed=(0,)):
        return self.command([*args], timeout=timeout, allowed=allowed)

    @staticmethod
    def wait_http(port):
        for _ in range(100):
            try:
                with socket.create_connection(('127.0.0.1', port), .1):
                    return
            except OSError:
                time.sleep(.1)
        raise ValueError(f'nothing listens on HTTP port {port}')

    # -- servers

    @contextmanager
    def pacs(self, name='pacs', http=PACS_HTTP, dimse=PACS_DIMSE, extra=(), fresh=True):
        directory = self.work/name
        if fresh and directory.exists():
            shutil.rmtree(directory)
        directory.mkdir(exist_ok=True)
        args = [self.bin/'pacs_server', '--port', dimse, '--ae-title', PACS_AE, '--storage-dir', directory/'archive', '--db-path', directory/'index.db',
                '--http-port', http, '--peer', f'{MOVE_AE}:127.0.0.1:{MOVE_PORT}', *extra]
        with self.server(args, dimse, log_name=f'server-{name}'):
            self.wait_http(http)
            yield Target('pacs', http, dimse, PACS_AE, '/dicomweb', directory, True)

    @contextmanager
    def orthanc(self):
        directory = self.work/'orthanc'
        directory.mkdir()
        config = directory/'orthanc.json'
        config.write_text(json.dumps(dict(
            Name='REFERENCE', StorageDirectory=str(directory/'storage'), IndexDirectory=str(directory/'index'), HttpPort=ORTHANC_HTTP,
            DicomPort=ORTHANC_DIMSE, DicomAet=ORTHANC_AE, DicomCheckCalledAet=False, RemoteAccessAllowed=True, AuthenticationEnabled=False,
            HttpTimeout=60, DicomAlwaysAllowStore=True, DicomAlwaysAllowFind=True, DicomAlwaysAllowGet=True, DicomAlwaysAllowMove=True,
            DicomModalities={MOVE_AE: [MOVE_AE, '127.0.0.1', MOVE_PORT]}, Plugins=['/usr/share/orthanc/plugins/libOrthancDicomWeb.so'],
            DicomWeb=dict(Enable=True, Root='/dicom-web/', EnableWado=True, WadoRoot='/wado', Ssl=False, StudiesMetadata='Full', SeriesMetadata='Full'),
            UnknownSopClassAccepted=True, DefaultEncoding='Utf8', StableAge=1, LimitFindResults=0, LimitFindInstances=0, HttpsVerifyPeers=False,
            OverwriteInstances=False, Name_='REF')))
        with self.server(['Orthanc', config, '--logdir='+str(directory)], ORTHANC_HTTP, log_name='server-orthanc'):
            self.wait_http(ORTHANC_DIMSE)
            yield Target('orthanc', ORTHANC_HTTP, ORTHANC_DIMSE, ORTHANC_AE, '/dicom-web')

    @contextmanager
    def move_receiver(self, directory):
        directory.mkdir(parents=True, exist_ok=True)
        with self.server(['storescp', '-aet', MOVE_AE, '+xa', '-od', directory, '-uf', MOVE_PORT], MOVE_PORT, log_name=f'server-move-{self.sequence}'):
            yield directory

    # -- fixtures

    def write_object(self, name, sop_class, study, series, sop, patient, syntax=EXPLICIT, rows=6, columns=8, frames=None, samples=1,
                     bits=8, number=1, series_number=1, modality='OT', charset=None, extra=None):
        """One Part 10 object with these exact values; pixels are what `frames` says."""
        ds = Dataset()
        ds.SOPClassUID, ds.SOPInstanceUID, ds.StudyInstanceUID, ds.SeriesInstanceUID = sop_class, sop, study, series
        if charset:
            ds.SpecificCharacterSet = charset
        ds.PatientName, ds.PatientID, ds.PatientBirthDate, ds.PatientSex = patient['PatientName'], patient['PatientID'], patient.get('PatientBirthDate', ''), patient.get('PatientSex', '')
        ds.StudyDate, ds.StudyTime, ds.StudyID = patient.get('StudyDate', '20260102'), patient.get('StudyTime', '101112'), patient.get('StudyID', 'S1')
        ds.AccessionNumber, ds.StudyDescription = patient.get('AccessionNumber', 'ACC'), patient.get('StudyDescription', 'Study')
        ds.ReferringPhysicianName = patient.get('ReferringPhysicianName', 'REF^DOC')
        ds.Modality, ds.SeriesNumber, ds.InstanceNumber, ds.SeriesDescription = modality, str(series_number), str(number), f'Series {series_number}'
        ds.InstitutionName = 'Web Validation'
        ds.ImageComments = 'first\\second'.split('\\')[0]
        ds.add_new(0x00091001, 'LO', 'private value')
        ds.add_new(0x00090010, 'LO', 'PROBE PRIVATE')
        item = Dataset()
        item.CodeValue, item.CodingSchemeDesignator, item.CodeMeaning = '113100', 'DCM', 'Basic Application'
        ds.ProcedureCodeSequence = Sequence([item])
        if frames is not None:
            ds.Rows, ds.Columns, ds.BitsAllocated = rows, columns, bits
            ds.BitsStored, ds.HighBit, ds.PixelRepresentation = (bits, bits-1, 0) if bits == 8 else (12, 11, 0)
            ds.SamplesPerPixel, ds.PhotometricInterpretation = samples, 'MONOCHROME2' if samples == 1 else 'RGB'
            if samples == 3:
                ds.PlanarConfiguration = 0
            if len(frames) > 1 or sop_class == MULTIFRAME_BYTE:
                ds.NumberOfFrames = str(len(frames))
            ds.PixelData = b''.join(frames)
            ds['PixelData'].VR = 'OB' if bits == 8 else 'OW'
        for keyword, value in (extra or {}).items():
            setattr(ds, keyword, value)
        meta = FileMetaDataset()
        meta.MediaStorageSOPClassUID, meta.MediaStorageSOPInstanceUID = sop_class, sop
        meta.TransferSyntaxUID = syntax
        ds.file_meta = meta
        ds.is_little_endian, ds.is_implicit_VR = True, syntax == IMPLICIT
        path = self.images/f'{name}.dcm'
        ds.save_as(path, write_like_original=False)
        return path

    @staticmethod
    def gray8(rows, columns, seed):
        values = bytearray(((row*columns+column)*11+seed*37) % 256 for row in range(rows) for column in range(columns))
        values[0], values[1] = 0, 255
        return bytes(values)

    @staticmethod
    def gray12(rows, columns, seed):
        values = [((row*columns+column)*89+seed*131) % 4096 for row in range(rows) for column in range(columns)]
        return b''.join(value.to_bytes(2, 'little') for value in values)

    @staticmethod
    def rgb(rows, columns, seed):
        return bytes(((row*7+seed) % 256, (column*13+seed) % 256, (row+column)*5 % 256)[channel] for row in range(rows) for column in range(columns) for channel in range(3))

    @staticmethod
    def noise(rows, columns, seed):
        """Hard to compress, so each JPEG-LS frame spans several fragments."""
        data = b''
        counter = 0
        while len(data) < rows*columns:
            data += hashlib.sha256(f'{seed}/{counter}'.encode()).digest()
            counter += 1
        return data[:rows*columns]

    @staticmethod
    def smooth(rows, columns, seed):
        return bytes((row*2+column*3+seed) % 256 for row in range(rows) for column in range(columns))

    def object_set(self, prefix):
        """Every kind of object, under UIDs and patients of this set only. Compressed ones are made by DCMTK from native frames."""
        patient = lambda name, suffix: dict(PatientName=name, PatientID=f'{prefix}-{suffix}', PatientBirthDate='19800102', PatientSex='F', StudyDate='20260203', StudyTime='090807',
                                            StudyID=f'{prefix}{suffix}', AccessionNumber=f'{prefix}{suffix}A', StudyDescription=f'{suffix} study', ReferringPhysicianName='REF^DOC')
        fixtures = {}

        def native(key, sop_class, study, series, patient_values, **values):
            sop = self.uid(prefix, 'sop')
            path = self.write_object(f'{prefix}-{key}', sop_class, study, series, sop, patient_values, **values)
            fixtures[key] = Fixture(key, path, study, series, sop, sop_class, values.get('syntax', EXPLICIT), values.get('frames', ()))
            return fixtures[key]

        study, first, second = self.uid(prefix, 'ct'), self.uid(prefix, 'ct', 1), self.uid(prefix, 'ct', 2)
        ct_patient = patient('DOE^JOHN', 'CT')
        ct_frames = [self.gray12(6, 8, seed) for seed in range(3)]
        for index, (key, series, number, series_number) in enumerate((('ct1', first, 1, 1), ('ct2', first, 2, 1), ('ct3', second, 1, 2))):
            native(key, CT, study, series, ct_patient, frames=[ct_frames[index]], bits=16, number=number, series_number=series_number, modality='CT',
                   extra=dict(ImagePositionPatient=['1.5', '-2.25', f'{index}'], ImageOrientationPatient=['1', '0', '0', '0', '1', '0'], RescaleIntercept='-1024', RescaleSlope='1',
                              KVP='120', SliceThickness='2.5', WindowCenter='2048', WindowWidth='4096', ImageType=['ORIGINAL', 'PRIMARY', 'AXIAL']))

        study, series = self.uid(prefix, 'mf'), self.uid(prefix, 'mf', 1)
        mf_frames = [self.gray8(6, 8, seed) for seed in range(1, 4)]
        native('mf', MULTIFRAME_BYTE, study, series, patient('ROE^JANE', 'MF'), frames=mf_frames, extra=dict(ConversionType='WSD', ImageType=['DERIVED', 'SECONDARY']))

        study, series = self.uid(prefix, 'rgb'), self.uid(prefix, 'rgb', 1)
        native('rgb', SECONDARY_CAPTURE, study, series, patient('ROE^RICHARD', 'RGB'), frames=[self.rgb(6, 8, 9)], samples=3, extra=dict(ConversionType='WSD'))

        study = self.uid(prefix, 'enc')
        enc_patient = patient('COMPRESSED^CASE', 'ENC')
        rle_frames = [self.gray8(16, 24, seed) for seed in range(5, 8)]
        native('rle-source', MULTIFRAME_BYTE, study, self.uid(prefix, 'enc', 1), enc_patient, frames=rle_frames, rows=16, columns=24, extra=dict(ConversionType='WSD'))
        jls_frames = [self.noise(64, 64, seed) for seed in range(2)]
        native('jls-source', MULTIFRAME_BYTE, study, self.uid(prefix, 'enc', 2), enc_patient, frames=jls_frames, rows=64, columns=64, series_number=2, extra=dict(ConversionType='WSD'))
        jpeg_frames = [self.smooth(32, 48, 3)]
        native('jpeg-source', SECONDARY_CAPTURE, study, self.uid(prefix, 'enc', 3), enc_patient, frames=jpeg_frames, rows=32, columns=48, series_number=3, extra=dict(ConversionType='WSD'))
        for key, syntax, tool in (('rle', RLE, ['dcmcrle']), ('jls', JPEG_LS, ['dcmcjpls', '+el', '+fs', '1']), ('jpeg', JPEG_BASELINE, ['dcmcjpeg', '+eb', '+un'])):
            source = fixtures.pop(f'{key}-source')
            target_path = self.images/f'{prefix}-{key}.dcm'
            self.tool(*tool, source.path, target_path)
            fixtures[key] = Fixture(key, target_path, source.study, source.series, source.sop, source.sop_class, syntax, source.frames)
            source.path.unlink()

        for key, name, charset, suffix in (('hangul', '홍^길동', 'ISO_IR 192', 'KO'), ('latin1', 'Müller^Jörg', 'ISO_IR 100', 'L1')):
            study, series = self.uid(prefix, key), self.uid(prefix, key, 1)
            native(key, SECONDARY_CAPTURE, study, series, patient(name, suffix), frames=[self.gray8(6, 8, 4)], charset=charset, extra=dict(ConversionType='WSD'))

        study, series = self.uid(prefix, 'sr'), self.uid(prefix, 'sr', 1)
        native('sr', BASIC_TEXT_SR, study, series, patient('TEXT^REPORT', 'SR'), modality='SR', extra=dict(ContentDate='20260203', ContentTime='090807', ValueType='TEXT', TextValue=SR_TEXT))

        if prefix == 'd':
            study, series = self.uid(prefix, 'imp'), self.uid(prefix, 'imp', 1)
            native('implicit', SECONDARY_CAPTURE, study, series, patient('IMPLICIT^VR', 'IMP'), syntax=IMPLICIT, frames=[self.gray8(6, 8, 6)],
                   extra=dict(ConversionType='WSD', KVP='120', SliceThickness='2.5', ExposureTime='10', ImageOrientationPatient=['1', '0', '0', '0', '1', '0'], ProtocolName='Implicit Protocol'))
        return fixtures

    def fixtures(self):
        for prefix in ('s', 'd'):
            self.sets[prefix] = self.object_set(prefix)
        for prefix, fixtures in self.sets.items():
            for key, tool in (('rle', 'dcmdrle'), ('jls', 'dcmdjpls')):
                decoded = self.images/f'{prefix}-{key}-decoded.dcm'
                self.tool(tool, fixtures[key].path, decoded)
                ds = pydicom.dcmread(decoded)
                require(ds.PixelData == b''.join(fixtures[key].frames), f'{tool} does not return what {key} was made from')
                decoded.unlink()
            jls = fixtures['jls'].dataset()
            require(fragment_count(jls) > len(fixtures['jls'].frames), 'the JPEG-LS fixture was meant to span several fragments per frame')
            require(jls.file_meta.TransferSyntaxUID == JPEG_LS and fixtures['rle'].dataset().file_meta.TransferSyntaxUID == RLE, 'fixture transfer syntaxes')
            for fixture in fixtures.values():
                self.tool('dcmftest', fixture.path)
        return f'{sum(map(len, self.sets.values()))} objects in {len(self.sets)} sets, decoded back by DCMTK'

    def versions(self):
        rows = {}
        for name, args in (('curl', ['curl', '--version']), ('openssl', ['openssl', 'version']), ('orthanc', ['dpkg-query', '-W', 'orthanc', 'orthanc-dicomweb']),
                           ('dcmtk', ['dpkg-query', '-W', 'dcmtk']), ('dcm4che', [self.dcm4che/'stowrs', '--version']), ('python', ['python3', '--version'])):
            rows[name] = self.tool(*args).strip().splitlines()[:2]
        rows['pydicom'] = pydicom.__version__
        (self.output/'peer-versions.json').write_text(json.dumps(rows, indent=1, default=str)+'\n')
        return json.dumps(rows, default=str)[:300]

    def qualify_tokens(self):
        self.tokens = TokenKit(self.private/'keys')
        now = int(time.time())
        claims = dict(iss=ISSUER, aud=AUDIENCE, sub='probe', exp=now+3600, iat=now, nbf=now-5, scope=READ)
        require(self.tokens.verify_with_jwks(self.tokens.token(claims)), 'RS256 token does not verify against the JWKS')
        require(self.tokens.verify_with_jwks(self.tokens.token(claims, alg='ES256')), 'ES256 token does not verify against the JWKS')
        require(not self.tokens.verify_with_jwks(self.tokens.token(claims, key=self.tokens.other)), 'a token from the other key verified')
        return 'tokens and JWKS agree under openssl with the key rebuilt from the JWKS; a token from another key does not'

    def qualify_multipart(self):
        kind, body = build_multipart([('application/dicom', b'one\r\n--not-it\r\n'), ('application/dicom', b'')])
        parts = split_multipart(kind, body)
        require([part.data for part in parts] == [b'one\r\n--not-it\r\n', b''], 'multipart round trip')
        for label, content_type, payload in (('truncated', kind, body[:-20]), ('no type', 'multipart/related; boundary=probe-boundary-7f3a9c', body), ('no boundary', 'multipart/related; type="application/dicom"', body)):
            try:
                split_multipart(content_type, payload)
            except PayloadError:
                continue
            raise ValueError(f'the reader accepted a {label} payload')
        return 'the reader keeps binary payloads and refuses truncated or undeclared ones'

    # -- what one read must show

    def dicom_from_bytes(self, data, label):
        try:
            return pydicom.dcmread(BytesIO(data))
        except Exception as exc:
            raise ValueError(f'{label}: not a Part 10 object: {exc}') from exc

    def compare_object(self, data, fixture, syntax, label):
        """A retrieved Part 10 object against what was sent, with `syntax` expected."""
        found, source = self.dicom_from_bytes(data, label), fixture.dataset()
        require(str(found.SOPInstanceUID) == fixture.sop, f'{label}: SOP Instance {found.SOPInstanceUID}, expected {fixture.sop}')
        if fixture.syntax == JPEG_BASELINE and found.file_meta.TransferSyntaxUID == EXPLICIT and self.lenient('lossy-default'):
            compare_attributes(found, source, label)
            return
        if fixture.syntax == IMPLICIT and found.file_meta.TransferSyntaxUID == IMPLICIT and self.lenient('implicit-as-held'):
            syntax = IMPLICIT
        require(found.file_meta.TransferSyntaxUID == syntax, f'{label}: transfer syntax {found.file_meta.TransferSyntaxUID}, expected {syntax}')
        require(found.file_meta.MediaStorageSOPInstanceUID == fixture.sop, f'{label}: file meta names {found.file_meta.MediaStorageSOPInstanceUID}')
        compare_attributes(found, source, label)
        if fixture.frames:
            if syntax in (EXPLICIT, IMPLICIT):
                require(found.PixelData == b''.join(fixture.frames), f'{label}: decoded pixels differ from the frames made ({len(found.PixelData)} bytes)')
            else:
                require(encoded_frames(found) == encoded_frames(source), f'{label}: encoded frames differ')
                require(fragment_count(found) == fragment_count(source), f'{label}: {fragment_count(found)} fragments, sent {fragment_count(source)}')
        else:
            require('PixelData' not in found, f'{label}: pixel data appeared')

    def expect_instances(self, reply, fixtures, label, syntax=lambda fixture: fixture.default_syntax()):
        """A multipart/related application/dicom response holding exactly these objects, once each."""
        require(reply.status == 200, f'{label}: {reply.brief()}')
        parts = reply.parts()
        media, parameters = parse_media_type(reply.header('content-type'))
        require(parameters.get('type', '').startswith('application/dicom'), f'{label}: multipart type {parameters.get("type")}')
        require(len(parts) == len(fixtures), f'{label}: {len(parts)} parts for {len(fixtures)} objects')
        by_uid = {fixture.sop: fixture for fixture in fixtures}
        seen = []
        for part in parts:
            require(part.content_type() == 'application/dicom', f'{label}: part is {part.content_type()!r}')
            ds = self.dicom_from_bytes(part.data, label)
            seen.append(str(ds.SOPInstanceUID))
            require(seen[-1] in by_uid, f'{label}: {seen[-1]} was not asked for')
            self.compare_object(part.data, by_uid[seen[-1]], syntax(by_uid[seen[-1]]), f'{label} {by_uid[seen[-1]].key}')
        require(len(set(seen)) == len(seen), f'{label}: an object came twice')

    def study_of(self, prefix, key):
        return [fixture for fixture in self.sets[prefix].values() if fixture.study == self.sets[prefix][key].study]

    def series_of(self, prefix, key):
        return [fixture for fixture in self.sets[prefix].values() if fixture.series == self.sets[prefix][key].series]

    @staticmethod
    def study_path(fixture):
        return f'/studies/{fixture.study}'

    @staticmethod
    def series_path(fixture):
        return f'/studies/{fixture.study}/series/{fixture.series}'

    @staticmethod
    def instance_path(fixture):
        return f'/studies/{fixture.study}/series/{fixture.series}/instances/{fixture.sop}'

    # -- storing

    def stow(self, target, fixtures, path='/studies', token=None):
        kind, body = build_multipart([('application/dicom', fixture.path.read_bytes()) for fixture in fixtures])
        return self.http(target, 'POST', path, data=body, content_type=kind, token=token)

    @staticmethod
    def json_value(item, tag, default=None):
        values = item.get(tag, {}).get('Value', [])
        return values[0] if values else default

    def check_stow_response(self, reply, stored, label='STOW'):
        require(reply.status == 200, f'{label}: {reply.brief()}')
        answer = reply.json()
        require(FAILED_SOP_SEQUENCE not in answer or not answer[FAILED_SOP_SEQUENCE].get('Value'), f'{label}: Failed SOP Sequence is {answer.get(FAILED_SOP_SEQUENCE)}')
        items = answer.get(REFERENCED_SOP_SEQUENCE, {}).get('Value', [])
        require(sorted((self.json_value(row, REFERENCED_CLASS), self.json_value(row, REFERENCED_INSTANCE)) for row in items)
                == sorted((fixture.sop_class, fixture.sop) for fixture in stored), f'{label}: Referenced SOP Sequence {items}')
        require(self.json_value(answer, RETRIEVE_URL) or all(self.json_value(row, RETRIEVE_URL) for row in items), f'{label}: no Retrieve URL anywhere')
        return items

    def store_by_stow(self, target, prefix):
        """Every object of the set by STOW, grouped by study as a client would."""
        fixtures = list(self.sets[prefix].values())
        items = []
        for study in sorted({fixture.study for fixture in fixtures}):
            group = [fixture for fixture in fixtures if fixture.study == study]
            items += self.check_stow_response(self.stow(target, group), group)
        by_sop = {self.json_value(row, REFERENCED_INSTANCE): row for row in items}
        for fixture in fixtures:
            url = self.json_value(by_sop[fixture.sop], RETRIEVE_URL)
            if url:
                reply = self.http(target, 'GET', '@'+urlsplit(urljoin(target.base()+'/', url)).path, accept=DICOM_MULTIPART)
                self.expect_instances(reply, [fixture], f'Retrieve URL of {fixture.key}')
        return f'{len(fixtures)} objects stored, each Retrieve URL answered with its object'

    def store_by_cstore(self, target, prefix):
        fixtures = list(self.sets[prefix].values())
        for syntax in sorted({fixture.syntax for fixture in fixtures}):
            group = [fixture for fixture in fixtures if fixture.syntax == syntax]
            self.tool('storescu', *STORE_FLAGS[syntax], '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, *[fixture.path for fixture in group])
        return f'{len(fixtures)} objects stored by storescu in their own transfer syntaxes'

    # -- reading over DIMSE

    def find(self, target, level, keys, answer):
        directory = self.work/f'find-{self.uid()}'
        directory.mkdir()
        arguments = ['findscu', '-S', '-X', '-od', directory, '-aet', SCU_AE, '-aec', target.ae, '-k', f'QueryRetrieveLevel={level}']
        for key in (*keys, answer):
            arguments += ['-k', key]
        self.tool(*arguments, '127.0.0.1', target.dimse_port)
        return [pydicom.dcmread(path) for path in sorted(directory.iterdir())]

    def find_all(self, target, prefix):
        """C-FIND at the three levels names exactly what the set holds: no more, no less."""
        fixtures = self.sets[prefix].values()
        for study in sorted({fixture.study for fixture in fixtures}):
            members = [fixture for fixture in fixtures if fixture.study == study]
            rows = self.find(target, 'STUDY', [f'StudyInstanceUID={study}'], 'PatientID')
            require([str(row.StudyInstanceUID) for row in rows] == [study], f'C-FIND study {study}: {len(rows)} rows')
            source = members[0].dataset()
            require(str(rows[0].PatientID) == str(source.PatientID), f'C-FIND patient {rows[0].PatientID}')
            rows = self.find(target, 'SERIES', [f'StudyInstanceUID={study}'], 'SeriesInstanceUID')
            require(sorted(str(row.SeriesInstanceUID) for row in rows) == sorted({fixture.series for fixture in members}), f'C-FIND series of {study}')
            for series in {fixture.series for fixture in members}:
                rows = self.find(target, 'IMAGE', [f'StudyInstanceUID={study}', f'SeriesInstanceUID={series}'], 'SOPInstanceUID')
                require(sorted(str(row.SOPInstanceUID) for row in rows) == sorted(fixture.sop for fixture in members if fixture.series == series), f'C-FIND images of {series}')
        return f'{len(fixtures)} objects named exactly at study, series and image level'

    def get_object(self, target, fixture, directory):
        directory.mkdir(parents=True)
        self.tool('getscu', '-S', *GET_FLAGS.get(fixture.syntax, []), '-to', 10, '-ta', 10, '-td', 30, '-aet', SCU_AE, '-aec', target.ae, '-od', directory,
                  '-k', 'QueryRetrieveLevel=IMAGE', '-k', f'StudyInstanceUID={fixture.study}', '-k', f'SeriesInstanceUID={fixture.series}', '-k', f'SOPInstanceUID={fixture.sop}',
                  '127.0.0.1', target.dimse_port)
        files = sorted(path for path in directory.rglob('*') if path.is_file())
        require(len(files) == 1, f'C-GET of {fixture.key}: {len(files)} files')
        return files[0]

    def get_each(self, target, prefix):
        """C-GET of each object compares equal to what was sent, in the syntax it was stored in."""
        for fixture in self.sets[prefix].values():
            path = self.get_object(target, fixture, self.work/f'get-{prefix}-{target.name}-{fixture.key}-{self.uid()}')
            self.compare_object(path.read_bytes(), fixture, fixture.syntax, f'C-GET {fixture.key}')
        return f'{len(self.sets[prefix])} objects equal after C-GET'

    def get_study(self, target, prefix):
        members = self.study_of(prefix, 'ct1')
        directory = self.work/f'get-study-{prefix}-{target.name}-{self.uid()}'
        directory.mkdir()
        self.tool('getscu', '-S', '-aet', SCU_AE, '-aec', target.ae, '-od', directory, '-k', 'QueryRetrieveLevel=STUDY', '-k', f'StudyInstanceUID={members[0].study}', '127.0.0.1', target.dimse_port)
        self.check_files(directory, members, 'C-GET study')
        return f'{len(members)} objects, none missing or twice'

    def check_files(self, directory, members, label):
        files = sorted(path for path in directory.rglob('*') if path.is_file())
        by_sop = {fixture.sop: fixture for fixture in members}
        found = [str(pydicom.dcmread(path).SOPInstanceUID) for path in files]
        require(sorted(found) == sorted(by_sop), f'{label}: {found}')
        for path in files:
            fixture = by_sop[str(pydicom.dcmread(path).SOPInstanceUID)]
            self.compare_object(path.read_bytes(), fixture, fixture.syntax, f'{label} {fixture.key}')

    def move_study(self, target, prefix):
        members = self.study_of(prefix, 'ct1')
        with self.move_receiver(self.work/f'move-{prefix}-{target.name}-{self.uid()}') as directory:
            self.tool('movescu', '-S', '-aet', SCU_AE, '-aec', target.ae, '-aem', MOVE_AE, '-k', 'QueryRetrieveLevel=STUDY', '-k', f'StudyInstanceUID={members[0].study}', '127.0.0.1', target.dimse_port)
            self.check_files(directory, members, 'C-MOVE study')
        return f'{len(members)} objects reached a third process, none missing or twice'

    # -- reading over HTTP

    def qido(self, target, path, token=None, **parameters):
        query = urlencode(parameters, safe='*?^,-', quote_via=quote)
        reply = self.http(target, 'GET', f'{path}?{query}' if query else path, token=token)
        require(reply.status == 200, f'QIDO {path}?{query}: {reply.brief()}')
        return reply.json(), reply

    def qido_studies(self, target, prefix):
        fixtures = self.sets[prefix].values()
        for study in sorted({fixture.study for fixture in fixtures}):
            members = [fixture for fixture in fixtures if fixture.study == study]
            rows, _ = self.qido(target, '/studies', StudyInstanceUID=study)
            require(len(rows) == 1, f'QIDO study {study}: {len(rows)} rows')
            row, source = rows[0], members[0].dataset()
            for tag, expected in (('00100020', str(source.PatientID)), ('00080020', str(source.StudyDate)), ('00080030', str(source.StudyTime)), ('00080050', str(source.AccessionNumber)),
                                  ('00200010', str(source.StudyID)), ('0020000d', study), ('00100030', str(source.PatientBirthDate)), ('00100040', str(source.PatientSex))):
                key = tag.upper()
                require(str(self.json_value(row, key)) == expected, f'QIDO study {members[0].key}: {key} is {row.get(key)}, expected {expected!r}')
            name = self.json_value(row, '00100010', {})
            require(name.get('Alphabetic') == str(source.PatientName), f'QIDO study {members[0].key}: name {name}, expected {source.PatientName}')
            require(self.json_value(row, '00201206') == len({fixture.series for fixture in members}), f'QIDO study {members[0].key}: series count {row.get("00201206")}')
            require(self.json_value(row, '00201208') == len(members), f'QIDO study {members[0].key}: instance count {row.get("00201208")}')
            modalities = sorted(row.get('00080061', {}).get('Value', []))
            require(modalities == sorted({str(fixture.dataset().Modality) for fixture in members}), f'QIDO study {members[0].key}: modalities {modalities}')
        return f'{len({fixture.study for fixture in fixtures})} studies with their attributes and counts'

    def qido_series(self, target, prefix):
        fixtures = self.sets[prefix].values()
        for study in sorted({fixture.study for fixture in fixtures}):
            members = [fixture for fixture in fixtures if fixture.study == study]
            for label, rows in (('/series', self.qido(target, '/series', StudyInstanceUID=study)[0]), ('/studies/x/series', self.qido(target, f'/studies/{study}/series')[0])):
                expected = sorted({fixture.series for fixture in members})
                found = sorted(str(self.json_value(row, '0020000E')) for row in rows)
                require(found == expected, f'QIDO {label} of {members[0].key}: {found}, expected {expected}')
                for row in rows:
                    series = self.json_value(row, '0020000E')
                    inside = [fixture for fixture in members if fixture.series == series]
                    source = inside[0].dataset()
                    require(self.json_value(row, '00080060') == str(source.Modality), f'QIDO {label}: modality {row.get("00080060")}')
                    require(str(self.json_value(row, '00200011')) == str(source.SeriesNumber), f'QIDO {label}: number {row.get("00200011")}')
                    require(self.json_value(row, '00201209') == len(inside), f'QIDO {label}: {row.get("00201209")} instances, expected {len(inside)}')
                    require(self.json_value(row, RETRIEVE_URL), f'QIDO {label}: no Retrieve URL (Table 10.6.3-4)')
        return 'series exact through both routes, with counts'

    def qido_instances(self, target, prefix):
        fixtures = self.sets[prefix].values()
        for study in sorted({fixture.study for fixture in fixtures}):
            members = [fixture for fixture in fixtures if fixture.study == study]
            searches = [('/instances', self.qido(target, '/instances', StudyInstanceUID=study)[0]), ('/studies/x/instances', self.qido(target, f'/studies/{study}/instances')[0])]
            for label, rows in searches:
                found = sorted(str(self.json_value(row, '00080018')) for row in rows)
                require(found == sorted(fixture.sop for fixture in members), f'QIDO {label} of {members[0].key}: {found}')
                self.check_instance_rows(target, rows, members, label)
            for series in {fixture.series for fixture in members}:
                inside = [fixture for fixture in members if fixture.series == series]
                rows, _ = self.qido(target, f'/studies/{study}/series/{series}/instances')
                require(sorted(str(self.json_value(row, '00080018')) for row in rows) == sorted(fixture.sop for fixture in inside), f'QIDO series instances of {inside[0].key}')
                self.check_instance_rows(target, rows, inside, 'series instances')
        return 'instances exact through three routes, with their image attributes and working Retrieve URLs'

    def check_instance_rows(self, target, rows, members, label):
        by_sop = {fixture.sop: fixture for fixture in members}
        for row in rows:
            fixture = by_sop[self.json_value(row, '00080018')]
            source = fixture.dataset()
            require(self.json_value(row, '00080016') == fixture.sop_class, f'QIDO {label} {fixture.key}: class {row.get("00080016")}')
            require(str(self.json_value(row, '00200013')) == str(source.InstanceNumber), f'QIDO {label} {fixture.key}: number {row.get("00200013")}')
            url = self.json_value(row, RETRIEVE_URL)
            require(url, f'QIDO {label} {fixture.key}: no Retrieve URL (Table 10.6.3-5)')
            reply = self.http(target, 'GET', '@'+urlsplit(urljoin(target.base()+'/', url)).path, accept=DICOM_MULTIPART)
            self.expect_instances(reply, [fixture], f'Retrieve URL of {fixture.key}')
            if fixture.frames:
                require(self.json_value(row, '00280010') == int(source.Rows), f'QIDO {label} {fixture.key}: Rows {row.get("00280010")}')
                require(self.json_value(row, '00280011') == int(source.Columns), f'QIDO {label} {fixture.key}: Columns {row.get("00280011")}')
                require(self.json_value(row, '00280100') == int(source.BitsAllocated), f'QIDO {label} {fixture.key}: Bits Allocated {row.get("00280100")}')
                if 'NumberOfFrames' in source:
                    require(int(self.json_value(row, '00280008', 0)) == int(source.NumberOfFrames), f'QIDO {label} {fixture.key}: Number of Frames {row.get("00280008")}')

    def wado_study(self, target, prefix):
        studies = {fixture.study: [other for other in self.sets[prefix].values() if other.study == fixture.study] for fixture in self.sets[prefix].values()}
        for study, members in studies.items():
            self.expect_instances(self.http(target, 'GET', f'/studies/{study}', accept=DICOM_MULTIPART), members, f'WADO study {members[0].key}')
        return f'{len(studies)} studies whole, as multipart even when one object is all there is'

    def wado_series(self, target, prefix):
        groups = {fixture.series: [other for other in self.sets[prefix].values() if other.series == fixture.series] for fixture in self.sets[prefix].values()}
        for members in groups.values():
            self.expect_instances(self.http(target, 'GET', self.series_path(members[0]), accept=DICOM_MULTIPART), members, f'WADO series {members[0].key}')
        return f'{len(groups)} series whole'

    def wado_instance(self, target, prefix):
        for fixture in self.sets[prefix].values():
            self.expect_instances(self.http(target, 'GET', self.instance_path(fixture), accept=DICOM_MULTIPART), [fixture], f'WADO instance {fixture.key}')
        return f'{len(self.sets[prefix])} instances'

    def wado_instance_single(self, target, prefix):
        """application/dicom is Optional in Table 10.4.4-1 and only for an individual instance."""
        for fixture in self.sets[prefix].values():
            reply = self.http(target, 'GET', self.instance_path(fixture), accept='application/dicom')
            require(reply.status == 200 and reply.media() == 'application/dicom', f'WADO instance single part {fixture.key}: {reply.brief()}')
            self.compare_object(reply.body, fixture, fixture.default_syntax(), f'WADO single part {fixture.key}')
        return f'{len(self.sets[prefix])} instances as a single part'

    def wado_as_stored(self, target, prefix):
        """transfer-syntax=* hands over what is held; an explicit Explicit VR request decodes, except what only exists as lossy."""
        for fixture in self.sets[prefix].values():
            wildcard = self.http(target, 'GET', self.instance_path(fixture), accept=DICOM_MULTIPART+'; transfer-syntax=*')
            self.expect_instances(wildcard, [fixture], f'as stored {fixture.key}', syntax=lambda row: row.as_stored_syntax())
            if fixture.syntax != JPEG_BASELINE:
                explicit = self.http(target, 'GET', self.instance_path(fixture), accept=f'{DICOM_MULTIPART}; transfer-syntax={EXPLICIT}')
                self.expect_instances(explicit, [fixture], f'as explicit {fixture.key}', syntax=lambda row: EXPLICIT)
        return f'{len(self.sets[prefix])} instances under both transfer-syntax parameters'

    def wado_uri(self, target, prefix):
        for fixture in self.sets[prefix].values():
            reply = self.http(target, 'GET', f'@/wado?requestType=WADO&studyUID={fixture.study}&seriesUID={fixture.series}&objectUID={fixture.sop}&contentType=application/dicom', accept='application/dicom')
            require(reply.status == 200 and reply.media() == 'application/dicom', f'WADO-URI {fixture.key}: {reply.brief()}')
            found = self.dicom_from_bytes(reply.body, f'WADO-URI {fixture.key}')
            require(str(found.SOPInstanceUID) == fixture.sop, f'WADO-URI {fixture.key}: {found.SOPInstanceUID}')
            compare_attributes(found, fixture.dataset(), f'WADO-URI {fixture.key}')
        return f'{len(self.sets[prefix])} objects through WADO-URI'

    # -- metadata

    def bulk_of(self, target, uri, fixture):
        parsed = urlsplit(urljoin(target.base()+'/', uri))
        reply = self.http(target, 'GET', '@'+parsed.path, accept=OCTET_MULTIPART)
        require(reply.status == 200, f'Bulk Data URI {uri} of {fixture.key}: {reply.brief()}')
        parts = reply.parts()
        require(parts, f'Bulk Data URI {uri}: no parts')
        for part in parts:
            require(part.headers.get('content-location') or self.lenient('bulk-content-location'), f'Bulk Data URI {uri}: part without Content-Location (10.4.3.3.5)')
        return parts

    def check_metadata(self, target, items, members, label):
        by_sop = {fixture.sop: fixture for fixture in members}
        found = []
        for item in items:
            sop = self.json_value(item, '00080018')
            require(sop in by_sop, f'{label}: {sop} was not asked for')
            found.append(sop)
            fixture = by_sop[sop]
            uris = {}

            def remember(tag, vr, uri):
                uris[int(str(tag), 16) if isinstance(tag, str) else int(tag)] = uri
                return b''

            read = Dataset.from_json(item, bulk_data_uri_handler=remember)
            source = fixture.dataset()
            read_view, source_view = attribute_view(read), attribute_view(source)
            # JSON is UTF-8 whatever the object declared (F.2), so a declaration may name either.
            read_view.pop(0x00080005, None)
            source_view.pop(0x00080005, None)
            missing = sorted(set(source_view)-set(read_view))
            extra = sorted(set(read_view)-set(source_view))
            require(not missing, f'{label} {fixture.key}: attributes lost: {[f"{tag:08x}" for tag in missing]}')
            require(not extra, f'{label} {fixture.key}: attributes added: {[f"{tag:08x}" for tag in extra]}')
            for tag, (vr, value) in source_view.items():
                require(read_view[tag][0] == vr, f'{label} {fixture.key}: {tag:08x} has VR {read_view[tag][0]}, sent {vr}')
                if tag in uris:
                    # 10.4.3.3.2 lets a server replace values of some VRs, UN among them, with a Bulk Data URI.
                    joined = b''.join(part.data for part in self.bulk_of(target, uris[tag], fixture))
                    require(joined == bytes(value), f'{label} {fixture.key}: the Bulk Data URI of {tag:08x} answers {joined!r}, sent {value!r}')
                    continue
                require(same_value(vr, read_view[tag][1], value), f'{label} {fixture.key}: {tag:08x} is {read_view[tag][1]!r}, sent {value!r}')
            if fixture.frames:
                require('7FE00010' in item, f'{label} {fixture.key}: Pixel Data is not listed ({sorted(item)[-3:]})')
                pixel = item['7FE00010']
                if 'BulkDataURI' in pixel:
                    parts = self.bulk_of(target, pixel['BulkDataURI'], fixture)
                    joined = b''.join(part.data for part in parts)
                    acceptable = [b''.join(fixture.frames), b''.join(encoded_frames(source)), source.PixelData]
                    if fixture.syntax == JPEG_BASELINE and len(joined) == len(b''.join(fixture.frames)):
                        # Decoded from a lossy codestream, so only close to what it was made from.
                        mean = sum(abs(a-b) for a, b in zip(joined, b''.join(fixture.frames)))/len(joined)
                        require(mean < 8, f'{label} {fixture.key}: decoded pixels differ by {mean:.1f} on average')
                    else:
                        require(joined in acceptable or (fixture.syntax not in (EXPLICIT, IMPLICIT) and self.lenient('bulk-encoded-form')),
                                f'{label} {fixture.key}: Bulk Data URI answers {len(joined)} other bytes')
        require(sorted(found) == sorted(by_sop), f'{label}: {sorted(found)} for {sorted(by_sop)}')

    def metadata_routes(self, target, prefix):
        fixtures = self.sets[prefix].values()
        for study in sorted({fixture.study for fixture in fixtures}):
            members = [fixture for fixture in fixtures if fixture.study == study]
            reply = self.http(target, 'GET', f'/studies/{study}/metadata')
            require(reply.status == 200 and reply.media() == JSON_MEDIA, f'study metadata: {reply.brief()}')
            self.check_metadata(target, reply.json(), members, f'study metadata {members[0].key}')
            for series in {fixture.series for fixture in members}:
                inside = [fixture for fixture in members if fixture.series == series]
                reply = self.http(target, 'GET', f'{self.series_path(inside[0])}/metadata')
                require(reply.status == 200, f'series metadata: {reply.brief()}')
                self.check_metadata(target, reply.json(), inside, f'series metadata {inside[0].key}')
            for fixture in members:
                reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/metadata')
                require(reply.status == 200, f'instance metadata: {reply.brief()}')
                self.check_metadata(target, reply.json(), [fixture], f'instance metadata {fixture.key}')
        return f'{len(self.sets[prefix])} objects: every attribute, through pydicom, and every Bulk Data URI answered'

    def metadata_dcm4che(self, target, prefix):
        """dcm4che json2dcm, a second independent reader of the same JSON."""
        fixture = self.sets[prefix]['ct1']
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/metadata')
        require(reply.status == 200, f'metadata: {reply.brief()}')
        source_json, output = self.work/f'json-{self.uid()}.json', self.work/f'json-{self.uid()}.dcm'
        item = {tag: value for tag, value in reply.json()[0].items() if 'BulkDataURI' not in value}
        source_json.write_bytes(json.dumps(item).encode())
        self.tool(self.dcm4che/'json2dcm', '-j', source_json, '-o', output, '--no-fmi')
        read = pydicom.dcmread(output, force=True)
        require(str(read.SOPInstanceUID) == fixture.sop and str(read.PatientID) == str(fixture.dataset().PatientID), 'json2dcm read other values')
        require(list(read.ImagePositionPatient) == list(fixture.dataset().ImagePositionPatient), f'json2dcm: position {read.ImagePositionPatient}')
        return 'dcm4che json2dcm reads the metadata with the same values'

    def metadata_xml(self, target, prefix):
        """dcm4che xml2dcm reads the Native DICOM Model of PS3.19: the same attributes as the JSON says, through another format and reader."""
        for key in ('ct1', 'mf', 'hangul', 'sr'):
            fixture = self.sets[prefix][key]
            reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/metadata', accept=XML_MULTIPART)
            require(reply.status == 200, f'{key} XML metadata: {reply.brief()}')
            parts = reply.parts()
            require(len(parts) == 1, f'{key} XML metadata: {len(parts)} parts')
            # xml2dcm reads the white space between elements as a value, so pretty printing is taken out first.
            text = re.sub(rb'>\s+<', b'><', parts[0].data)
            text = re.sub(rb'<DicomAttribute [^>]*><BulkData [^>]*/></DicomAttribute>', b'', text)
            # Private attributes are compared through the JSON, by pydicom; here they are left out.
            text = re.sub(rb'<DicomAttribute tag="0009[0-9A-Fa-f]{4}".*?</DicomAttribute>', b'', text)
            source_xml, output = self.work/f'xml-{self.uid()}.xml', self.work/f'xml-{self.uid()}.dcm'
            source_xml.write_bytes(text)
            self.tool(self.dcm4che/'xml2dcm', '-x', source_xml, '-o', output, '--no-fmi')
            read, source = pydicom.dcmread(output, force=True), fixture.dataset()
            read_view, source_view = attribute_view(read), attribute_view(source)
            read_view.pop(0x00080005, None)
            source_view.pop(0x00080005, None)
            source_view = {tag: value for tag, value in source_view.items() if tag >> 16 != 0x0009}
            require(set(read_view) == set(source_view), f'{key}: XML attributes differ: {sorted(set(read_view) ^ set(source_view))}')
            for tag, (vr, value) in source_view.items():
                if vr not in ('UN', 'OB', 'OW'):
                    found = read_view[tag][1]
                    # xml2dcm 5.35.2 puts an empty value before the real one on some attributes, on Orthanc's XML and
                    # on PACS's alike, though not on a reduced document; the JSON cases hold those values exactly.
                    if isinstance(found, pydicom.multival.MultiValue) and not isinstance(value, pydicom.multival.MultiValue):
                        found = [item for item in found if str(item)]
                        found = found[0] if len(found) == 1 else found
                    require(same_value(vr, found, value), f'{key}: {tag:08x} is {read_view[tag][1]!r}, sent {value!r}')
        return 'dcm4che xml2dcm reads every attribute back'

    # -- frames

    def frames_reply(self, target, fixture, frame_list, accept=OCTET_MULTIPART):
        return self.http(target, 'GET', f'{self.instance_path(fixture)}/frames/{frame_list}', accept=accept)

    def expect_frames(self, reply, fixture, numbers, label, native=True):
        require(reply.status == 200, f'{label}: {reply.brief()}')
        parts = reply.parts()
        require(len(parts) == len(numbers), f'{label}: {len(parts)} parts for frames {numbers}')
        wanted = fixture.frames if native else encoded_frames(fixture.dataset())
        for part, number in zip(parts, numbers):
            require(part.data == wanted[number-1], f'{label}: frame {number} is {len(part.data)} other bytes')
            location = part.headers.get('content-location', '')
            require(location, f'{label}: frame {number} has no Content-Location (10.4.3.3.6)')

    def frames_native(self, target, prefix):
        fixture = self.sets[prefix]['mf']
        for frame_list, numbers in (('1', [1]), ('2', [2]), ('3', [3]), ('1,2', [1, 2]), ('2,3', [2, 3]), ('1,3', [1, 3]), ('1,2,3', [1, 2, 3])):
            self.expect_frames(self.frames_reply(target, fixture, frame_list), fixture, numbers, f'frames {frame_list}')
        return 'every selection of three frames, exact bytes, in order'

    def frames_single(self, target, prefix):
        """application/octet-stream is Optional in Table 10.4.4-1 and only for a single response item."""
        fixture = self.sets[prefix]['mf']
        reply = self.frames_reply(target, fixture, '2', accept='application/octet-stream')
        require(reply.status == 200 and reply.body == fixture.frames[1], f'single part frame: {reply.brief()}')
        return 'frame 2 alone, exact bytes'

    def frames_kinds(self, target, prefix):
        """One frame of a 12 bit in 16 CT and of an RGB image: the frame is everything, little endian, nothing cut."""
        for key in ('ct1', 'rgb', 'latin1'):
            fixture = self.sets[prefix][key]
            self.expect_frames(self.frames_reply(target, fixture, '1'), fixture, [1], f'{key} frame')
        return 'single frame images of other layouts'

    def frames_compressed(self, target, prefix):
        """application/octet-stream only carries uncompressed data (8.7.3.3.1), so compressed objects are decoded for it."""
        for key in ('rle', 'jls'):
            fixture = self.sets[prefix][key]
            all_numbers = list(range(1, len(fixture.frames)+1))
            self.expect_frames(self.frames_reply(target, fixture, ','.join(map(str, all_numbers))), fixture, all_numbers, f'{key} frames decoded')
            self.expect_frames(self.frames_reply(target, fixture, str(all_numbers[-1])), fixture, [all_numbers[-1]], f'{key} last frame decoded')
        return 'RLE and JPEG-LS frames decoded to the bytes they were made from'

    def frames_compressed_as_is(self, target, prefix):
        """A compressed media type hands the frame over as held, without fragment delimiters (8.7.3.3.2)."""
        for key, media, syntax in (('rle', 'image/dicom-rle', RLE), ('jls', 'image/jls', JPEG_LS), ('jpeg', 'image/jpeg', JPEG_BASELINE)):
            fixture = self.sets[prefix][key]
            source = encoded_frames(fixture.dataset())
            numbers = list(range(1, len(source)+1))
            reply = self.frames_reply(target, fixture, ','.join(map(str, numbers)), accept=f'multipart/related; type="{media}"; transfer-syntax={syntax}')
            require(reply.status == 200, f'{key} frames as {media}: {reply.brief()}')
            parts = reply.parts()
            require(len(parts) == len(source), f'{key}: {len(parts)} parts for {len(source)} frames')
            for part, frame in zip(parts, source):
                require(part.content_type() == media, f'{key}: part type {part.content_type()!r}')
                require(part.data.rstrip(b'\x00') == frame.rstrip(b'\x00'), f'{key}: {len(part.data)} bytes for a frame of {len(frame)}')
        return 'compressed frames as held, one part each, without delimiters'

    def frames_refused(self, target, name, frame_list, status):
        fixture = self.sets['s']['mf']
        reply = self.frames_reply(target, fixture, frame_list)
        require(reply.status in status, f'frames/{frame_list}: {reply.brief()}, expected {status}')
        require(not reply.media().startswith('multipart/'), f'frames/{frame_list}: a refusal carried frames')

    # -- rendered

    def rendered_png(self, target, fixture, frame=None, query=''):
        path = f'{self.instance_path(fixture)}{f"/frames/{frame}" if frame else ""}/rendered{query}'
        reply = self.http(target, 'GET', path, accept='image/png')
        require(reply.status == 200 and reply.media() == 'image/png', f'rendered {path}: {reply.brief()}')
        return read_png(reply.body)

    def rendered_gray(self, target, prefix):
        """Frames span 0 to 255, so any linear window leaves them alone: each PNG is the frame it was asked for."""
        fixture = self.sets[prefix]['mf']
        for number, frame in enumerate(fixture.frames, 1):
            image = self.rendered_png(target, fixture, frame=number)
            require((image['width'], image['height']) == (8, 6), f'frame {number}: {image["width"]}x{image["height"]}')
            step = image['channels']
            gray = image['samples'][::step] if image['depth'] == 8 else image['samples'][::2*step]
            worst = max(abs(a-b) for a, b in zip(gray, frame))
            require(worst <= 1, f'frame {number}: differs by {worst} from its pixels')
        first = self.rendered_png(target, fixture)
        require(first['samples'][::first['channels']][2] == fixture.frames[0][2], 'instance rendering is not its first frame')
        return 'each rendered frame is that frame, pixel for pixel, within one level'

    def rendered_rgb(self, target, prefix):
        fixture = self.sets[prefix]['rgb']
        image = self.rendered_png(target, fixture)
        require((image['width'], image['height'], image['channels'] >= 3) == (8, 6, True), f'RGB: {image["width"]}x{image["height"]}, {image["channels"]} channels')
        step = image['channels']
        decoded = bytes(value for offset in range(0, len(image['samples']), step) for value in image['samples'][offset:offset+3])
        require(decoded == fixture.frames[0], 'RGB rendering differs from its pixels')
        return 'RGB exact'

    def rendered_window(self, target, prefix):
        """PS3.3 C.11.2.1.2: a window narrower than the data clips and rescales. Statement 8.1 claims it."""
        fixture = self.sets[prefix]['mf']
        center, width = 100, 101
        image = self.rendered_png(target, fixture, frame=1, query=f'?window={center},{width},linear')
        step = image['channels']
        gray = image['samples'][::step]
        for found, value in zip(gray, fixture.frames[0]):
            scaled = ((value-(center-0.5))/(width-1)+0.5)*255
            expected = 0 if value <= center-0.5-(width-1)/2 else 255 if value > center-0.5+(width-1)/2 else scaled
            require(abs(found-expected) <= 1.5, f'window: {value} became {found}, expected {expected:.1f}')
        return 'the window is applied by the linear function of PS3.3'

    def rendered_viewport(self, target, prefix):
        """8.3.5.1.3: the image is scaled to fit, keeping its proportions."""
        fixture = self.sets[prefix]['mf']
        image = self.rendered_png(target, fixture, frame=1, query='?viewport=16,12')
        require((image['width'], image['height']) == (16, 12), f'viewport 16,12: {image["width"]}x{image["height"]}')
        if not self.lenient('viewport-resampling'):
            gray = image['samples'][::image['channels']]
            doubled = bytes(fixture.frames[0][row//2*8+column//2] for row in range(12) for column in range(16))
            worst = max(abs(a-b) for a, b in zip(gray, doubled))
            require(worst <= 1, f'viewport: differs by {worst} from the frame at twice its size')
        return 'scaled to the size asked for'

    def rendered_viewport_ratio(self, target, prefix):
        """8.3.5.1.3: "maintaining the aspect ratio"."""
        fixture = self.sets[prefix]['mf']
        fitted = self.rendered_png(target, fixture, frame=1, query='?viewport=16,16')
        require((fitted['width'], fitted['height']) == (16, 12), f'viewport 16,16 keeps 4:3: {fitted["width"]}x{fitted["height"]}')
        return 'fits inside, with its proportions'

    def rendered_window_functions(self, target, prefix):
        """8.3.5.1.4 names three functions; each is PS3.3 C.11.2.1.2 or C.11.2.1.3."""
        fixture = self.sets[prefix]['mf']
        center, width = 100.0, 101.0
        for function, formula in (('linear-exact', lambda x: 0 if x <= center-width/2 else 255 if x > center+width/2 else ((x-center)/width+0.5)*255),
                                  ('sigmoid', lambda x: 255/(1+math.exp(-4*(x-center)/width)))):
            image = self.rendered_png(target, fixture, frame=1, query=f'?window={center:g},{width:g},{function}')
            for found, value in zip(image['samples'][::image['channels']], fixture.frames[0]):
                require(abs(found-formula(value)) <= 1.5, f'{function}: {value} became {found}, expected {formula(value):.1f}')
        return 'linear-exact and sigmoid by their formulas'

    def rendered_jpeg(self, target, prefix):
        fixture = self.sets[prefix]['mf']
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/frames/2/rendered', accept='image/jpeg')
        require(reply.status == 200 and reply.media() == 'image/jpeg', f'rendered jpeg: {reply.brief()}')
        info = read_jpeg(reply.body)
        require((info['width'], info['height']) == (8, 6), f'rendered jpeg: {info}')
        path, dicom, decoded = self.work/f'r-{self.uid()}.jpg', self.work/f'r-{self.uid()}.dcm', self.work/f'r-{self.uid()}-d.dcm'
        path.write_bytes(reply.body)
        self.tool('img2dcm', path, dicom)
        self.tool('dcmdjpeg', dicom, decoded)
        ds = pydicom.dcmread(decoded)
        samples = int(ds.SamplesPerPixel)
        values = ds.PixelData[::samples] if samples == 1 or int(getattr(ds, 'PlanarConfiguration', 0)) == 0 else ds.PixelData[:48]
        mean = sum(abs(a-b) for a, b in zip(values, fixture.frames[1]))/len(fixture.frames[1])
        require(mean < 40, f'rendered jpeg: mean difference {mean:.1f} from frame 2')
        return f'the JPEG decodes to frame 2 (mean difference {mean:.1f}, a lossy codec)'

    def rendered_compressed(self, target, prefix):
        """Rendering decodes first: the frame shown is the frame stored, not encoded bytes taken for pixels."""
        for key in ('rle', 'jls'):
            fixture = self.sets[prefix][key]
            image = self.rendered_png(target, fixture, frame=len(fixture.frames))
            frame = fixture.frames[-1]
            rows, columns = int(fixture.dataset().Rows), int(fixture.dataset().Columns)
            require((image['width'], image['height']) == (columns, rows), f'{key}: {image["width"]}x{image["height"]}')
            gray = image['samples'][::image['channels']]
            low, high = min(frame), max(frame)
            worst = max(abs(found-(value-low)*255/(high-low)) for found, value in zip(gray, frame))
            require(worst <= 1.5, f'{key}: differs by {worst:.1f}')
        return 'compressed frames rendered from their decoded pixels'

    def rendered_refused(self, target):
        fixture = self.sets['s']['mf']
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/frames/4/rendered', accept='image/png')
        require(reply.status in (400, 404), f'rendered frame 4 of 3: {reply.brief()}')
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/frames/abc/rendered', accept='image/png')
        require(reply.status == 400, f'rendered frame abc: {reply.brief()}')
        reply = self.http(target, 'GET', f'{self.instance_path(self.sets["s"]["sr"])}/rendered', accept='image/png')
        require(reply.status in (400, 404, 406), f'rendered text report: {reply.brief()}')
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/rendered?window=abc,def,linear', accept='image/png')
        require(reply.status == 400, f'rendered with a window of letters: {reply.brief()}')

    def rendered_width_zero(self, target):
        fixture = self.sets['s']['mf']
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/rendered?window=100,0,linear', accept='image/png')
        require(reply.status == 400, f'rendered with a width of 0: {reply.brief()}')

    # -- index and files

    def index_state(self, target, fixtures):
        """What the database and the storage folder hold for these objects, read without PACS."""
        connection = sqlite3.connect(f'file:{target.directory/"index.db"}?mode=ro', uri=True)
        connection.row_factory = sqlite3.Row
        try:
            rows = {row['sop_uid']: dict(row) for row in connection.execute(
                'SELECT i.sop_uid, i.sop_class_uid, i.file_path, i.file_size, i.transfer_syntax, i.instance_number, se.series_uid, st.study_uid, p.patient_id, p.patient_name '
                'FROM instances i JOIN series se ON i.series_pk=se.series_pk JOIN studies st ON se.study_pk=st.study_pk JOIN patients p ON st.patient_pk=p.patient_pk')}
        finally:
            connection.close()
        for fixture in fixtures:
            row = rows.get(fixture.sop)
            require(row, f'index has no row for {fixture.key}')
            source = fixture.dataset()
            require(row['study_uid'] == fixture.study and row['series_uid'] == fixture.series, f'index puts {fixture.key} under {row["study_uid"]}/{row["series_uid"]}')
            require(row['sop_class_uid'] == fixture.sop_class, f'index class of {fixture.key}: {row["sop_class_uid"]}')
            require(row['patient_id'] == str(source.PatientID), f'index patient of {fixture.key}: {row["patient_id"]}')
            require(row['patient_name'] == str(source.PatientName), f'index name of {fixture.key}: {row["patient_name"]!r}, sent {str(source.PatientName)!r}')
            require(str(row['instance_number']) == str(source.InstanceNumber), f'index number of {fixture.key}: {row["instance_number"]}')
            path = Path(row['file_path'])
            require(path.is_file(), f'index points {fixture.key} at {path}, which is not there')
            require(row['file_size'] == path.stat().st_size, f'index size of {fixture.key}: {row["file_size"]}, file has {path.stat().st_size}')
            found = pydicom.dcmread(path)
            require(row['transfer_syntax'] == str(found.file_meta.TransferSyntaxUID), f'index syntax of {fixture.key}: {row["transfer_syntax"]!r}, file has {found.file_meta.TransferSyntaxUID}')
            require(found.file_meta.TransferSyntaxUID == fixture.syntax, f'{fixture.key} was sent in {fixture.syntax} and is kept in {found.file_meta.TransferSyntaxUID}')
            compare_attributes(found, source, f'file of {fixture.key}')
            if fixture.frames and fixture.syntax in (EXPLICIT, IMPLICIT):
                require(found.PixelData == b''.join(fixture.frames), f'file of {fixture.key}: pixels differ')
            elif fixture.frames:
                require(encoded_frames(found) == encoded_frames(source), f'file of {fixture.key}: frames differ')
        return rows

    def archive_exact(self, target):
        """Every file under the archive is named by exactly one row, and every row by one file."""
        connection = sqlite3.connect(f'file:{target.directory/"index.db"}?mode=ro', uri=True)
        try:
            rows = [Path(row[0]).resolve() for row in connection.execute('SELECT file_path FROM instances')]
        finally:
            connection.close()
        files = sorted(path.resolve() for path in (target.directory/'archive').rglob('*') if path.is_file() and not path.name.startswith('.'))
        require(len(rows) == len(set(rows)), 'two rows name one file')
        extra, missing = sorted(set(files)-set(rows)), sorted(set(rows)-set(files))
        require(not extra, f'{len(extra)} files nothing names, such as {extra[:2]}')
        require(not missing, f'{len(missing)} rows with no file, such as {missing[:2]}')
        return len(files)

    # -- overlap between the two protocols

    def duplicates(self, target):
        """Stored both ways: one object, unchanged, wherever it is looked for, whichever protocol came second."""
        stored_by_web, stored_by_dimse = self.sets['s']['ct1'], self.sets['d']['ct1']
        for label, fixture, second in (('stow-then-cstore', stored_by_web, 'cstore'), ('cstore-then-stow', stored_by_dimse, 'stow')):
            if second == 'cstore':
                self.tool('storescu', '-xe', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, fixture.path, allowed=(0, 1))
            else:
                reply = self.stow(target, [fixture])
                answered = reply.json() if reply.status in (200, 202, 409) else {}
                listed = [self.json_value(row, REFERENCED_INSTANCE) for row in answered.get(REFERENCED_SOP_SEQUENCE, {}).get('Value', [])] + \
                         [self.json_value(row, REFERENCED_INSTANCE) for row in answered.get(FAILED_SOP_SEQUENCE, {}).get('Value', [])]
                require(reply.status in (200, 202, 409) and listed == [fixture.sop], f'{label}: {reply.brief()}')
            self.expect_instances(self.http(target, 'GET', self.series_path(fixture), accept=DICOM_MULTIPART), self.series_of(fixture.key and ('s' if fixture is stored_by_web else 'd'), 'ct1'), label)
            rows, _ = self.qido(target, '/instances', SOPInstanceUID=fixture.sop)
            require(len(rows) == 1, f'{label}: QIDO names it {len(rows)} times')
            found = self.find(target, 'IMAGE', [f'StudyInstanceUID={fixture.study}', f'SeriesInstanceUID={fixture.series}', f'SOPInstanceUID={fixture.sop}'], 'InstanceNumber')
            require(len(found) == 1, f'{label}: C-FIND names it {len(found)} times')
        return 'one object each, over QIDO, WADO and C-FIND'

    def changed_duplicate(self, target):
        """A second object under a known SOP Instance UID but in another study must not leave it half in both."""
        first = self.sets['s']['ct2']
        ds = first.dataset()
        other_study, other_series = self.uid('moved'), self.uid('moved', 1)
        ds.StudyInstanceUID, ds.SeriesInstanceUID, ds.PatientID = other_study, other_series, 'MOVED'
        ds.PatientName = 'MOVED^AWAY'
        path = self.images/'changed-duplicate.dcm'
        ds.save_as(path, write_like_original=False)
        changed = Fixture('changed', path, other_study, other_series, first.sop, first.sop_class, EXPLICIT, first.frames)
        reply = self.stow(target, [changed])
        require(reply.status in (200, 202, 409), f'changed duplicate: {reply.brief()}')
        at_first, _ = self.qido(target, '/instances', StudyInstanceUID=first.study, SOPInstanceUID=first.sop)
        at_other, _ = self.qido(target, '/instances', StudyInstanceUID=other_study, SOPInstanceUID=first.sop)
        require(len(at_first)+len(at_other) == 1, f'the object is listed {len(at_first)} times in its study and {len(at_other)} in the other')
        holder = first if at_first else changed
        wado = self.http(target, 'GET', self.instance_path(holder), accept=DICOM_MULTIPART)
        self.expect_instances(wado, [holder], 'changed duplicate')
        found = self.find(target, 'IMAGE', [f'StudyInstanceUID={holder.study}', f'SeriesInstanceUID={holder.series}', f'SOPInstanceUID={first.sop}'], 'InstanceNumber')
        require(len(found) == 1, 'C-FIND lost track of it')
        return f'it stays whole in {"its first" if at_first else "the second"} study, by HTTP {reply.status}'

    # -- negotiation

    def resource_path(self, resource, fixture):
        return {'study': self.study_path(fixture), 'series': self.series_path(fixture), 'instance': self.instance_path(fixture),
                'study-metadata': self.study_path(fixture)+'/metadata', 'series-metadata': self.series_path(fixture)+'/metadata',
                'instance-metadata': self.instance_path(fixture)+'/metadata', 'frames': self.instance_path(fixture)+'/frames/1',
                'two-frames': self.instance_path(fixture)+'/frames/1,2', 'rendered': self.instance_path(fixture)+'/rendered',
                'search': f'/studies?StudyInstanceUID={fixture.study}'}[resource]

    def negotiate(self, target, resource, accept, status, shape, label):
        fixture = self.sets['s']['mf']
        reply = self.http(target, 'GET', self.resource_path(resource, fixture), accept=accept)
        require(reply.status == status, f'{label}: {reply.brief()}, expected {status}')
        if shape == 'multipart-dicom':
            self.expect_instances(reply, [fixture], label, syntax=lambda row: row.syntax)
        elif shape == 'single-dicom':
            require(reply.media() == 'application/dicom', f'{label}: {reply.brief()}')
            self.compare_object(reply.body, fixture, fixture.syntax, label)
        elif shape == 'json':
            require(reply.media() == JSON_MEDIA, f'{label}: {reply.brief()}')
            require(isinstance(reply.json(), list) and reply.json(), f'{label}: not a list with a row')
        elif shape == 'multipart-octet':
            self.expect_frames(reply, fixture, [1], label)
        elif shape == 'single-octet':
            require(reply.media() == 'application/octet-stream' and reply.body == fixture.frames[0], f'{label}: {reply.brief()}')
        elif shape == 'jpeg':
            require(reply.media() == 'image/jpeg', f'{label}: {reply.brief()}')
            read_jpeg(reply.body)
        elif shape == 'png':
            require(reply.media() == 'image/png', f'{label}: {reply.brief()}')
            read_png(reply.body)
        elif shape == 'multipart-xml':
            parts = reply.parts()
            require(parts and all(part.content_type() == 'application/dicom+xml' for part in parts), f'{label}: {reply.brief()}')
            require(b'NativeDicomModel' in parts[0].data, f'{label}: not the Native DICOM Model')
        elif shape == 'bulk-of-instances':
            parts = reply.parts()
            require(b''.join(part.data for part in parts) == b''.join(fixture.frames), f'{label}: {sum(len(part.data) for part in parts)} bytes for the pixels of one object')
            require(all(part.headers.get('content-location') for part in parts), f'{label}: part without Content-Location (10.4.3.3.5)')

    def accept_query(self, target):
        """8.3.3.1: the Accept Query Parameter narrows what the header allows."""
        fixture = self.sets['s']['mf']
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/rendered?accept=image/png', accept='*/*')
        require(reply.status == 200 and reply.media() == 'image/png', f'rendered accept=image/png: {reply.brief()}')
        read_png(reply.body)
        return 'the parameter selects PNG where the header left JPEG the default'

    # -- search

    def qido_seed(self, target):
        """The search fixtures: named studies, and enough rows that any page size has to be walked."""
        paths = []
        self.qido_keys = {}
        for row in QIDO_STUDIES:
            study, series = self.uid('q', row['key']), self.uid('q', row['key'], 1)
            self.qido_keys[row['key']] = study
            paths.append(self.write_object(f'q-{row["key"]}', SECONDARY_CAPTURE, study, series, self.uid('q', row['key'], 2), row, modality=row['Modality'], frames=[self.gray8(2, 2, 1)], rows=2, columns=2,
                                           charset='ISO_IR 192' if not row['PatientName'].isascii() else None, extra=dict(ConversionType='WSD')))
        self.page_studies = []
        for index in range(PAGE_STUDIES):
            study, series = self.uid('p', index), self.uid('p', index, 1)
            self.page_studies.append(study)
            paths.append(self.write_object(f'p-{index}', SECONDARY_CAPTURE, study, series, self.uid('p', index, 2), dict(PatientID=f'PAGE{index:04}', PatientName=f'PAGE^{index:04}', StudyDate='20250101'),
                                           frames=[self.gray8(2, 2, 1)], rows=2, columns=2, extra=dict(ConversionType='WSD')))
        self.many_study = self.uid('many')
        self.many_series = [self.uid('many', 1), self.uid('many', 2)]
        self.many_instances = {series: [] for series in self.many_series}
        for series_number, series in enumerate(self.many_series, 1):
            for index in range(PAGE_SERIES_INSTANCES):
                sop = self.uid('many', series_number, index)
                self.many_instances[series].append(sop)
                paths.append(self.write_object(f'many-{series_number}-{index}', SECONDARY_CAPTURE, self.many_study, series, sop, dict(PatientID='MANY', PatientName='MANY^SERIES'), number=index+1, series_number=series_number,
                                               frames=[self.gray8(2, 2, 1)], rows=2, columns=2, extra=dict(ConversionType='WSD')))
        for start in range(0, len(paths), 100):
            self.tool('storescu', '-xe', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, *paths[start:start+100], timeout=180)
        return f'{len(paths)} objects stored'

    def qido_uid_list(self, target):
        """8.3.4.1: a UID key may list several, comma-separated."""
        study_list = f'{self.qido_keys["alpha"]},{self.qido_keys["gamma"]}'
        rows, _ = self.qido(target, '/studies', StudyInstanceUID=study_list)
        require(sorted(str(self.json_value(row, '0020000D')) for row in rows) == sorted((self.qido_keys['alpha'], self.qido_keys['gamma'])), f'UID list: {rows}')
        return 'exactly the two listed'

    def qido_percent_encoded(self, target):
        """The same search, with its reserved characters percent-encoded as RFC 3986 allows."""
        reply = self.http(target, 'GET', '/studies?PatientName=ALPHA%2A&PatientID=QID%2A')
        rows = reply.json()
        require(sorted(str(self.json_value(row, '0020000D')) for row in rows) == sorted((self.qido_keys['alpha'], self.qido_keys['beta'])), f'encoded: {reply.brief()}')
        return 'decoded before matching'

    def qido_warning_fuzzy(self, target):
        reply = self.http(target, 'GET', '/studies?PatientID=QID1&fuzzymatching=true')
        require(reply.status == 200 and 'fuzzymatching parameter is not supported' in reply.header('warning'), f'fuzzy: {reply.header("warning")!r}')
        return 'literal matching performed, and said so (8.3.4.2)'

    def qido_includefield(self, target):
        """An attribute outside the result table comes back when asked for, by keyword and by tag (8.3.4.3)."""
        fixture = self.sets['d']['ct1']
        rows, _ = self.qido(target, '/instances', SOPInstanceUID=fixture.sop, includefield='KVP')
        require(len(rows) == 1 and float(self.json_value(rows[0], '00180060')) == 120.0, f'includefield=KVP: {rows}')
        rows, _ = self.qido(target, '/studies', StudyInstanceUID=fixture.study, includefield='00080080')
        require(len(rows) == 1 and self.json_value(rows[0], '00080080') == 'Web Validation', f'includefield=00080080: {rows}')
        return 'by keyword at the instance level, by tag at the study level'

    def qido_includefield_all(self, target):
        fixture = self.sets['d']['ct1']
        rows, _ = self.qido(target, '/instances', SOPInstanceUID=fixture.sop, includefield='all')
        require(len(rows) == 1 and float(self.json_value(rows[0], '00180050')) == 2.5 and self.json_value(rows[0], '00080080') == 'Web Validation', f'includefield=all: {rows}')
        return 'every attribute of the instance but Bulk Data'

    def qido_filter(self, target, parameters, keys):
        scoped = dict(parameters)
        if 'PatientID' not in scoped and '00100020' not in scoped:
            scoped['PatientID'] = 'QID*'
        rows, _ = self.qido(target, '/studies', **scoped)
        found = sorted(str(self.json_value(row, '0020000D')) for row in rows)
        require(found == sorted(self.qido_keys[key] for key in keys), f'{scoped}: {found}, expected {keys} {[self.qido_keys[key] for key in keys]}')

    def qido_page_walk(self, target, path, parameters, expected, size):
        """limit and offset (8.3.4.4): the union is exact, nothing twice, the order stable, and the Warning states what remains."""
        walked, offset, pages = [], 0, 0
        while True:
            rows, reply = self.qido(target, path, **parameters, limit=size, offset=offset)
            again, _ = self.qido(target, path, **parameters, limit=size, offset=offset)
            uids = [self.page_key(row) for row in rows]
            require(uids == [self.page_key(row) for row in again], f'{path} offset {offset}: order changed between identical requests')
            require(len(rows) <= size, f'{path}: {len(rows)} rows for limit {size}')
            walked += uids
            offset += size
            pages += 1
            remaining = len(expected)-len(walked)
            warning = reply.header('warning')
            if remaining > 0:
                require(len(rows) == size, f'{path} offset {offset-size}: {len(rows)} rows, with {remaining} left')
                require(re.search(rf'299 .*{remaining} additional results', warning) or self.lenient('page-warning'), f'{path} limit {size} offset {offset-size}: Warning {warning!r}, with {remaining} left')
            else:
                require('additional results' not in warning, f'{path}: Warning {warning!r} on the last page')
                break
            require(pages < 400, f'{path}: the walk does not end')
        require(len(set(walked)) == len(walked), f'{path}: a row came twice')
        require(sorted(walked) == sorted(expected), f'{path}: walked {len(walked)} rows, expected {len(expected)}')

    @staticmethod
    def page_key(row):
        for tag in ('00080018', '0020000E', '0020000D'):
            if tag in row:
                return row[tag]['Value'][0]
        raise ValueError(f'a row without a UID: {sorted(row)}')

    def page_routes(self):
        instances = [sop for series in self.many_series for sop in self.many_instances[series]]
        return (('studies', '/studies', dict(PatientID='PAGE*'), self.page_studies),
                ('series', '/series', dict(StudyInstanceUID=self.many_study), self.many_series),
                ('instances', '/instances', dict(StudyInstanceUID=self.many_study), instances),
                ('study-series', f'/studies/{self.many_study}/series', {}, self.many_series),
                ('study-instances', f'/studies/{self.many_study}/instances', {}, instances),
                ('series-instances', f'/studies/{self.many_study}/series/{self.many_series[1]}/instances', {}, self.many_instances[self.many_series[1]]))

    def qido_no_limit(self, target, path, parameters, expected):
        """No limit given: whatever the server chooses to return, the Warning accounts for the rest (8.3.4.4), so nothing goes missing unseen."""
        rows, reply = self.qido(target, path, **parameters)
        uids = [self.page_key(row) for row in rows]
        require(len(set(uids)) == len(uids), f'{path}: a row came twice')
        require(set(uids) <= set(expected), f'{path}: rows nobody asked for')
        remaining = len(expected)-len(uids)
        warning = reply.header('warning')
        if remaining:
            require(re.search(rf'299 .*{remaining} additional results', warning) or self.lenient('page-warning'), f'{path}: {len(uids)} of {len(expected)} rows and Warning {warning!r}')
        else:
            require('additional results' not in warning, f'{path}: Warning {warning!r} with nothing left')
        return f'{len(uids)} rows, Warning accounts for {remaining} more'

    def qido_offset_past_end(self, target):
        rows, reply = self.qido(target, '/studies', PatientID='PAGE*', offset=PAGE_STUDIES+5)
        require(rows == [] and reply.status == 200, f'past the end: {reply.brief()}')

    def qido_empty(self, target):
        for path, parameters in (('/studies', dict(PatientID='NOSUCHPATIENT')), ('/series', dict(StudyInstanceUID='1.2.3.4.5.6.7')), ('/instances', dict(SOPInstanceUID='1.2.3.4.5.6.7'))):
            reply = self.http(target, 'GET', f'{path}?{urlencode(parameters)}')
            require(reply.status == 200 and reply.media() == JSON_MEDIA and reply.json() == [], f'nothing matches {path}: {reply.brief()}')
        return 'nothing matching is 200 with an empty list, on all three routes'

    def qido_series_filters(self, target):
        fixture = self.sets['d']['ct1']
        rows, _ = self.qido(target, '/series', StudyInstanceUID=fixture.study, SeriesNumber='2')
        require([self.json_value(row, '0020000E') for row in rows] == [self.sets['d']['ct3'].series], f'SeriesNumber filter: {rows}')
        rows, _ = self.qido(target, '/series', StudyInstanceUID=fixture.study, Modality='MR')
        require(rows == [], f'Modality filter: {rows}')
        rows, _ = self.qido(target, '/instances', StudyInstanceUID=fixture.study, SeriesInstanceUID=fixture.series, InstanceNumber='2')
        require([self.json_value(row, '00080018') for row in rows] == [self.sets['d']['ct2'].sop], f'InstanceNumber filter: {rows}')
        rows, _ = self.qido(target, f'/studies/{fixture.study}/series/{fixture.series}/instances', SOPInstanceUID=fixture.sop)
        require([self.json_value(row, '00080018') for row in rows] == [fixture.sop], f'SOPInstanceUID filter: {rows}')
        rows, _ = self.qido(target, f'/studies/{fixture.study}/instances', SOPClassUID=SECONDARY_CAPTURE)
        require(rows == [], f'SOPClassUID filter: {rows}')
        return 'series and instance keys narrow exactly'

    def qido_limit_with_key(self, target):
        fixture = self.sets['d']['ct1']
        rows, _ = self.qido(target, f'/studies/{fixture.study}/instances', SOPClassUID=CT, limit=1, offset=2)
        require(len(rows) == 1, f'limit and offset with a filter: {len(rows)} rows')
        return 'the third of three, alone'

    def qido_hangul_row(self, target):
        rows, reply = self.qido(target, '/studies', PatientID='QID6')
        require(len(rows) == 1, f'Hangul row: {len(rows)}')
        name = self.json_value(rows[0], '00100010', {}).get('Alphabetic')
        require(name == '김^철수', f'Hangul name came back as {name!r}')
        return 'UTF-8 through search, intact'

    # -- refusals

    def stow_refused(self, target, name, status):
        fixture = self.sets['s']['sr']
        good = fixture.path.read_bytes()
        kind, body = build_multipart([('application/dicom', good)])
        boundary = 'probe-boundary-7f3a9c'
        request = {'text-plain': ('text/plain', b'hello'), 'single-part-dicom': ('application/dicom', good), 'multipart-mixed': (f'multipart/mixed; boundary={boundary}', body),
                   'no-content-type': (None, body), 'no-boundary': ('multipart/related; type="application/dicom"', body),
                   'wrong-boundary': ('multipart/related; type="application/dicom"; boundary=something-else', body),
                   'truncated': (kind, body[:len(body)//2]), 'empty-body': (kind, b'')}[name]
        reply = self.http(target, 'POST', '/studies', data=request[1], content_type=request[0])
        require(reply.status == status, f'{name}: {reply.brief()}, expected {status}')

    def broken_part(self, name, study):
        """A part that cannot be stored, and the UID the failure should name if it has one."""
        kept = self.images/f'broken-{name}-{self.uid()}.dcm'
        sop, series = self.uid('broken'), self.uid('broken', 1)
        source = dict(PatientName='BROKEN^PART', PatientID='BROKEN')
        if name == 'not-dicom':
            return 'application/dicom', b'this is not a DICOM object at all', None
        if name == 'wrong-part-type':
            return 'text/plain', b'free text in a part', None
        path = self.write_object(kept.stem, SECONDARY_CAPTURE, study, series, sop, source, frames=[self.gray8(2, 2, 1)], rows=2, columns=2, extra=dict(ConversionType='WSD'))
        ds = pydicom.dcmread(path)
        named = sop
        if name == 'no-sop-instance':
            del ds.SOPInstanceUID
            named = None
        elif name == 'no-sop-class':
            del ds.SOPClassUID
        elif name == 'no-study':
            del ds.StudyInstanceUID
        elif name == 'no-series':
            del ds.SeriesInstanceUID
        elif name == 'traversal-sop-instance':
            ds.SOPInstanceUID = named = '../../../../escaped-sop'
        elif name == 'absolute-path-sop-instance':
            ds.SOPInstanceUID = named = '/tmp/escaped-absolute'
        elif name == 'traversal-study':
            ds.StudyInstanceUID = '../../../../escaped-study'
        elif name == 'traversal-series':
            ds.SeriesInstanceUID = '../../../../escaped-series'
        elif name == 'wrong-target-study':
            ds.StudyInstanceUID = self.uid('elsewhere')
        ds.file_meta.MediaStorageSOPInstanceUID = named or sop
        ds.save_as(path, write_like_original=False)
        return 'application/dicom', path.read_bytes(), named

    def stow_all_failed(self, target, name, marker):
        """Each part fails: 409, the failure named in Failed SOP Sequence, and neither protocol ever shows it."""
        study = self.uid('refused', marker)
        kind, data, named = self.broken_part(name, study)
        content_type, body = build_multipart([(kind, data)])
        reply = self.http(target, 'POST', f'/studies/{study}' if name == 'wrong-target-study' else '/studies', data=body, content_type=content_type)
        require(reply.status in (400, 409), f'{name}: {reply.brief()}, expected 400 or 409')
        answer = reply.json()
        failed = answer.get(FAILED_SOP_SEQUENCE, {}).get('Value', [])
        require(len(failed) == 1, f'{name}: Failed SOP Sequence holds {len(failed)} items: {answer}')
        require(self.json_value(failed[0], FAILURE_REASON) is not None, f'{name}: no Failure Reason in {failed[0]}')
        if named:
            require(self.json_value(failed[0], REFERENCED_INSTANCE) == named, f'{name}: names {failed[0].get(REFERENCED_INSTANCE)}')
        self.nothing_escaped(target, name, named, study)

    def nothing_escaped(self, target, name, named, study):
        rows, _ = self.qido(target, '/studies', StudyInstanceUID=study)
        require(rows == [], f'{name}: QIDO shows the refused study')
        if target.is_pacs:
            connection = sqlite3.connect(f'file:{target.directory/"index.db"}?mode=ro', uri=True)
            try:
                present = connection.execute("SELECT count(*) FROM instances WHERE sop_uid LIKE '%escaped%'").fetchone()[0]
            finally:
                connection.close()
            require(present == 0, f'{name}: the index holds {present} escaped rows')
            strays = [path for path in target.directory.parent.rglob('*escaped*')]+[path for path in Path('/tmp').glob('*escaped*')]
            require(not strays, f'{name}: files written outside the archive: {strays[:3]}')

    def stow_partial(self, target, name, marker):
        """A bad part next to a good one: 202, the good one stored and retrievable, the bad one named."""
        study = self.uid('partial', marker)
        series, sop = self.uid('partial', marker, 1), self.uid('partial', marker, 2)
        path = self.write_object(f'partial-{marker}', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='PARTIAL', PatientName='PARTIAL^GOOD'), frames=[self.gray8(2, 2, 1)], rows=2, columns=2, extra=dict(ConversionType='WSD'))
        good = Fixture(f'partial-{marker}', path, study, series, sop, SECONDARY_CAPTURE, EXPLICIT, [self.gray8(2, 2, 1)])
        kind, data, named = self.broken_part(name, study)
        content_type, body = build_multipart([('application/dicom', path.read_bytes()), (kind, data)])
        reply = self.http(target, 'POST', f'/studies/{study}' if name == 'wrong-target-study' else '/studies', data=body, content_type=content_type)
        require(reply.status == 202, f'{name}: {reply.brief()}, expected 202')
        answer = reply.json()
        stored = [self.json_value(row, REFERENCED_INSTANCE) for row in answer.get(REFERENCED_SOP_SEQUENCE, {}).get('Value', [])]
        failed = answer.get(FAILED_SOP_SEQUENCE, {}).get('Value', [])
        require(stored == [sop], f'{name}: Referenced SOP Sequence names {stored}')
        require(len(failed) == 1, f'{name}: Failed SOP Sequence holds {len(failed)} items: {answer}')
        require(self.json_value(failed[0], FAILURE_REASON) is not None, f'{name}: no Failure Reason')
        if named:
            require(self.json_value(failed[0], REFERENCED_INSTANCE) == named, f'{name}: names {failed[0].get(REFERENCED_INSTANCE)}')
        self.expect_instances(self.http(target, 'GET', self.study_path(good), accept=DICOM_MULTIPART), [good], f'{name}: what was stored')

    def retrieve_refused(self, target):
        """404 for what does not exist, including a real object asked for under the wrong parents."""
        ct1, ct3, mf = self.sets['s']['ct1'], self.sets['s']['ct3'], self.sets['s']['mf']
        absent = '1.2.3.4.5.6.7.8.9'
        rows = [
            ('unknown-study', f'/studies/{absent}', DICOM_MULTIPART),
            ('unknown-study-metadata', f'/studies/{absent}/metadata', JSON_MEDIA),
            ('unknown-series', f'/studies/{ct1.study}/series/{absent}', DICOM_MULTIPART),
            ('unknown-series-metadata', f'/studies/{ct1.study}/series/{absent}/metadata', JSON_MEDIA),
            ('unknown-instance', f'/studies/{ct1.study}/series/{ct1.series}/instances/{absent}', DICOM_MULTIPART),
            ('unknown-instance-metadata', f'/studies/{ct1.study}/series/{ct1.series}/instances/{absent}/metadata', JSON_MEDIA),
            ('unknown-instance-frames', f'/studies/{ct1.study}/series/{ct1.series}/instances/{absent}/frames/1', OCTET_MULTIPART),
            ('unknown-instance-rendered', f'/studies/{ct1.study}/series/{ct1.series}/instances/{absent}/rendered', 'image/jpeg'),
            ('series-under-other-study', f'/studies/{mf.study}/series/{ct1.series}', DICOM_MULTIPART),
            ('series-metadata-under-other-study', f'/studies/{mf.study}/series/{ct1.series}/metadata', JSON_MEDIA),
            ('instance-under-other-series', f'/studies/{ct1.study}/series/{ct3.series}/instances/{ct1.sop}', DICOM_MULTIPART),
            ('instance-under-other-study', f'/studies/{mf.study}/series/{ct1.series}/instances/{ct1.sop}', DICOM_MULTIPART),
            ('instance-metadata-under-other-series', f'/studies/{ct1.study}/series/{ct3.series}/instances/{ct1.sop}/metadata', JSON_MEDIA),
            ('frames-under-other-series', f'/studies/{ct1.study}/series/{ct3.series}/instances/{ct1.sop}/frames/1', OCTET_MULTIPART),
            ('rendered-under-other-series', f'/studies/{ct1.study}/series/{ct3.series}/instances/{ct1.sop}/rendered', 'image/jpeg'),
        ]
        return rows

    def search_under_absent(self, target):
        """10.6.3-1 has 200 with an empty list for no matches and 8.5 has 404 for no such resource: either way, no rows of something else."""
        ct1, mf = self.sets['s']['ct1'], self.sets['s']['mf']
        for path in (f'/studies/1.2.3.4.5.6.7.8.9/series', f'/studies/1.2.3.4.5.6.7.8.9/instances', f'/studies/{mf.study}/series/{ct1.series}/instances'):
            reply = self.http(target, 'GET', path)
            require(reply.status == 404 or (reply.status == 200 and reply.json() == []), f'{path}: {reply.brief()}')
        return 'nothing from anywhere else'

    def frames_non_image(self, target):
        reply = self.frames_reply(target, self.sets['s']['sr'], '1')
        require(reply.status in (400, 404), f'frames of a text report: {reply.brief()}')

    def injection(self, target):
        """Text that would change a query if it reached one as SQL: nothing matches, in a path or in a key, and the archive still answers."""
        fixture = self.sets['d']['ct1']
        for value in INJECTION_VALUES:
            encoded = quote(value, safe='')
            for path in (f'/studies/{encoded}', f'/studies/{fixture.study}/series/{encoded}', f'/studies/{fixture.study}/series/{fixture.series}/instances/{encoded}'):
                reply = self.http(target, 'GET', path, accept=DICOM_MULTIPART)
                require(reply.status in (400, 404), f'{path}: {reply.brief()}')
            for key in ('PatientID', 'StudyInstanceUID', 'AccessionNumber', 'PatientName'):
                reply = self.http(target, 'GET', f'/studies?{key}={encoded}')
                # % is an ordinary character (C.2.2): no stored value is only that.
                require(reply.status == 400 or (reply.status == 200 and reply.json() == []), f'{key}={value}: {reply.brief()}')
        rows, _ = self.qido(target, '/instances', SOPInstanceUID=fixture.sop)
        require(len(rows) == 1, 'the archive no longer answers')
        return f'{len(INJECTION_VALUES)} values in three path positions and four keys matched nothing'

    def nested_binary(self, target):
        """A value inside a sequence is an attribute too (10.4.3.3.2): it comes inline or by a URI that works, and is not dropped."""
        study, series, sop = self.uid(), self.uid(), self.uid()
        icon = Dataset()
        icon.SamplesPerPixel, icon.PhotometricInterpretation, icon.Rows, icon.Columns = 1, 'MONOCHROME2', 2, 2
        icon.BitsAllocated, icon.BitsStored, icon.HighBit, icon.PixelRepresentation = 8, 8, 7, 0
        pixels = bytes((1, 2, 3, 4))
        icon.PixelData = pixels
        icon['PixelData'].VR = 'OB'
        path = self.write_object('nested-binary', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='NESTED', PatientName='NESTED^BINARY'), frames=[self.gray8(2, 2, 1)], rows=2, columns=2,
                                 extra=dict(ConversionType='WSD', IconImageSequence=Sequence([icon])))
        fixture = Fixture('nested-binary', path, study, series, sop, SECONDARY_CAPTURE, EXPLICIT, [self.gray8(2, 2, 1)])
        self.check_stow_response(self.stow(target, [fixture]), [fixture])
        reply = self.http(target, 'GET', f'{self.instance_path(fixture)}/metadata')
        require(reply.status == 200, f'metadata: {reply.brief()}')
        element = reply.json()[0]['00880200']['Value'][0]['7FE00010']
        if 'InlineBinary' in element:
            require(base64.b64decode(element['InlineBinary']) == pixels, f'inline: {element}')
            return 'inline, and equal'
        require('BulkDataURI' in element, f'neither inline nor by URI: {element}')
        parts = self.bulk_of(target, element['BulkDataURI'], fixture)
        require(b''.join(part.data for part in parts) == pixels, 'the nested URI answers other bytes')
        return 'by a URI that answers it'

    def undeclared_bytes(self, target):
        """A name in bytes no character set explains: every response stays valid UTF-8, and the object stays whole."""
        study, series, sop = self.uid(), self.uid(), self.uid()
        frames = [self.gray8(2, 2, 1)]
        path = self.write_object('undeclared-bytes', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='BYTES', PatientName='BYTES'), frames=frames, rows=2, columns=2, extra=dict(ConversionType='WSD'))
        raw = path.read_bytes()
        require(b'BYTES\x20' in raw or b'BYTES' in raw, 'no name to change')
        changed = raw.replace(b'PN\x06\x00BYTES\x20', b'PN\x06\x00BYT\xFCS\x20', 1)
        require(changed != raw, 'the name was not found in the file')
        path.write_bytes(changed)
        self.tool('storescu', '-xe', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, path, allowed=(0,))
        rows, _ = self.qido(target, '/studies', StudyInstanceUID=study)
        require(len(rows) == 1, f'QIDO: {len(rows)} rows')
        name = self.json_value(rows[0], '00100010', {}).get('Alphabetic', '')
        require('\ufffd' in name, f'name came back as {name!r}, not marked as unreadable')
        reply = self.http(target, 'GET', f'/studies/{study}/metadata')
        require(reply.status == 200 and len(reply.json()) == 1, f'metadata: {reply.brief()}')
        require(reply.json()[0]['00100020']['Value'] == ['BYTES'], f'metadata: {reply.json()[0].get("00100020")}')
        wado = self.http(target, 'GET', f'/studies/{study}', accept=DICOM_MULTIPART)
        found = self.dicom_from_bytes(wado.parts()[0].data, 'undeclared bytes')
        require(found.PixelData == frames[0], 'pixels differ')
        require(found['PatientName'].value is not None, 'the name is gone')
        return 'valid JSON on every route, the bad byte marked, and the file as sent'

    def rest_api_off(self, target):
        """Most of the REST API checks no credentials, so this host does not serve it; a token could not protect it."""
        for path in ('/api/v1/patients', '/api/v1/studies', '/api/v1/system/status', '/api/v1/remote-nodes', '/api/v1/jobs'):
            reply = self.http(target, 'GET', '@'+path)
            require(reply.status == 404, f'{path}: {reply.brief()}')
        return 'no /api route answers'

    def no_cors_by_default(self, target):
        """Without an allowed origin no web page may read this, or a page in the viewer's browser could read an archive on loopback."""
        fixture = self.sets['d']['ct1']
        origin = 'Origin: https://elsewhere.example.test'
        reply = self.http(target, 'GET', f'/studies?StudyInstanceUID={fixture.study}', headers=[origin])
        require(reply.status == 200 and not reply.header('access-control-allow-origin'), f'search: {reply.headers.get("access-control-allow-origin")}')
        preflight = self.http(target, 'OPTIONS', '/studies', headers=[origin, 'Access-Control-Request-Method: POST'])
        require(not preflight.header('access-control-allow-origin'), f'preflight: {preflight.headers.get("access-control-allow-origin")}')
        return 'no Access-Control-Allow-Origin on a response or a preflight'

    def stow_no_accept(self, target):
        fixture = self.sets['s']['sr']
        kind, body = build_multipart([('application/dicom', fixture.path.read_bytes())])
        reply = self.http(target, 'POST', '/studies', accept=None, data=body, content_type=kind)
        require(reply.status == 406, f'no Accept: {reply.brief()}')

    def stow_xml_response(self, target):
        """Table 10.5.4-1 has application/dicom+xml as required for the response too."""
        study, series, sop = self.uid(), self.uid(), self.uid()
        path = self.write_object('xml-reply', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='XMLREPLY', PatientName='XML^REPLY'), frames=[self.gray8(2, 2, 1)], rows=2, columns=2, extra=dict(ConversionType='WSD'))
        kind, body = build_multipart([('application/dicom', path.read_bytes())])
        reply = self.http(target, 'POST', '/studies', accept='application/dicom+xml', data=body, content_type=kind)
        require(reply.status == 200 and reply.media() == 'application/dicom+xml', f'XML response: {reply.brief()}')
        require(sop.encode() in reply.body and b'NativeDicomModel' in reply.body, f'XML response does not name the object: {reply.brief()}')
        return 'the response module in the Native DICOM Model'

    def stow_implicit_part(self, target):
        """Web Services shall not use Implicit VR (8.7.3): whatever comes in, it does not go out that way."""
        study, series, sop = self.uid(), self.uid(), self.uid()
        frames = [self.gray8(6, 8, 3)]
        path = self.write_object('stow-implicit', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='IMPLICITSTOW', PatientName='IMPLICIT^STOW'), syntax=IMPLICIT, frames=frames, extra=dict(ConversionType='WSD'))
        fixture = Fixture('stow-implicit', path, study, series, sop, SECONDARY_CAPTURE, IMPLICIT, frames)
        self.check_stow_response(self.stow(target, [fixture]), [fixture])
        self.expect_instances(self.http(target, 'GET', self.study_path(fixture), accept=DICOM_MULTIPART), [fixture], 'Implicit VR part', syntax=lambda row: EXPLICIT)
        return 'stored, and returned as Explicit VR Little Endian'

    def empty_patient_id(self, target):
        """A Type 2 Patient ID may be empty, and such an object must not be accepted and then be unreachable: either both protocols find it, or both refuse it."""
        for name, send in (('stow', lambda path, study: self.http(target, 'POST', f'/studies/{study}', data=build_multipart([('application/dicom', path.read_bytes())])[1], content_type=build_multipart([('application/dicom', b'')])[0])),
                           ('cstore', None)):
            study, series, sop = self.uid(), self.uid(), self.uid()
            path = self.write_object(f'no-patient-{name}', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='', PatientName='NO^IDENTIFIER'), frames=[self.gray8(2, 2, 1)], rows=2, columns=2, extra=dict(ConversionType='WSD'))
            if send is None:
                # storescu exits with the high byte of a refusal status: A900H is 169.
                self.tool('storescu', '-xe', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, path, allowed=(0, 169))
            else:
                send(path, study)
            rows, _ = self.qido(target, '/studies', StudyInstanceUID=study)
            found = self.find(target, 'STUDY', [f'StudyInstanceUID={study}'], 'PatientID')
            require(rows == [] and found == [], f'{name}: kept where it should be refused: {len(rows)} over QIDO, {len(found)} over C-FIND')
            if target.is_pacs:
                self.archive_exact(target)
        return 'refused over both protocols, and no file or row is left'

    def cstore_not_a_uid(self, target):
        """Neither protocol keeps what STOW refuses for its UIDs."""
        study, series, sop = self.uid(), '1.2.3.letters.4', self.uid()
        path = self.write_object('cstore-not-a-uid', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='NOTAUID', PatientName='NOT^AUID'), frames=[self.gray8(2, 2, 1)], rows=2, columns=2, extra=dict(ConversionType='WSD'))
        self.tool('storescu', '-xe', '--no-uid-checks', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, path, allowed=(0, 1, 169))
        rows, _ = self.qido(target, '/studies', StudyInstanceUID=study)
        require(rows == [], 'C-STORE kept a Series Instance UID with letters in it')
        if target.is_pacs:
            self.archive_exact(target)
        return 'refused by status, nothing kept'

    def file_gone(self, target):
        """An indexed object whose file is gone: 200 would claim all of it (10.4.3.1), so the whole retrieve fails and nothing is dropped silently."""
        fixture = self.sets['s']['ct3']
        members = self.study_of('s', 'ct1')
        connection = sqlite3.connect(f'file:{target.directory/"index.db"}?mode=ro', uri=True)
        try:
            path = Path(connection.execute('SELECT file_path FROM instances WHERE sop_uid=?', (fixture.sop,)).fetchone()[0])
        finally:
            connection.close()
        kept = path.read_bytes()
        path.unlink()
        try:
            for label, resource, accept in (('study', self.study_path(fixture), DICOM_MULTIPART), ('series', self.series_path(fixture), DICOM_MULTIPART), ('instance', self.instance_path(fixture), DICOM_MULTIPART),
                                            ('metadata', f'{self.study_path(fixture)}/metadata', JSON_MEDIA), ('frames', f'{self.instance_path(fixture)}/frames/1', OCTET_MULTIPART), ('rendered', f'{self.instance_path(fixture)}/rendered', 'image/png')):
                reply = self.http(target, 'GET', resource, accept=accept)
                require(reply.status >= 400, f'{label} with {len(members)} objects and one file gone: {reply.brief()}')
                require(not reply.media().startswith('multipart/'), f'{label}: a partial response with a failure status')
        finally:
            path.write_bytes(kept)
        self.expect_instances(self.http(target, 'GET', self.study_path(fixture), accept=DICOM_MULTIPART), members, 'with the file back')
        return 'no partial response on any route, and the study is whole again once the file is back'

    def large_object(self, target):
        """40 MiB through STOW, read back over both protocols, byte for byte."""
        rows, columns, count = 1024, 1024, 40
        frames = [self.noise(rows, columns, 1000+index) for index in range(count)]
        study, series, sop = self.uid(), self.uid(), self.uid()
        path = self.write_object('large', MULTIFRAME_BYTE, study, series, sop, dict(PatientID='LARGE', PatientName='LARGE^OBJECT'), frames=frames, rows=rows, columns=columns, extra=dict(ConversionType='WSD'))
        fixture = Fixture('large', path, study, series, sop, MULTIFRAME_BYTE, EXPLICIT, frames)
        kind, body = build_multipart([('application/dicom', path.read_bytes())])
        reply = self.http(target, 'POST', '/studies', data=body, content_type=kind, timeout=180)
        self.check_stow_response(reply, [fixture], 'large STOW')
        self.expect_instances(self.http(target, 'GET', self.study_path(fixture), accept=DICOM_MULTIPART, timeout=180), [fixture], 'large WADO')
        self.expect_frames(self.frames_reply(target, fixture, f'1,{count//2},{count}'), fixture, [1, count//2, count], 'large frames')
        received = self.get_object(target, fixture, self.work/f'get-large-{target.name}')
        self.compare_object(received.read_bytes(), fixture, EXPLICIT, 'large C-GET')
        for item in (path, received):
            item.unlink()
        return f'{sum(map(len, frames))} bytes of pixels, equal over STOW, WADO, frames and C-GET'

    def wado_many(self, target):
        """A study of 120 instances in two series, stored by C-STORE, retrieved whole over HTTP: exact, unlike a search there is no page."""
        reply = self.http(target, 'GET', f'/studies/{self.many_study}', accept=DICOM_MULTIPART, timeout=120)
        require(reply.status == 200, f'whole study: {reply.brief()}')
        parts = reply.parts()
        found = [str(pydicom.dcmread(BytesIO(part.data), stop_before_pixels=True).SOPInstanceUID) for part in parts]
        expected = [sop for series in self.many_series for sop in self.many_instances[series]]
        require(sorted(found) == sorted(expected), f'{len(found)} parts, {len(set(found))} of them different, for {len(expected)} objects')
        return f'{len(parts)} objects, each once'

    # -- many at once

    def concurrent(self, target):
        """Eight clients at once, both protocols, into one study; ten objects go in over both. Afterwards the counts are exact."""
        study = self.uid('together')
        fixtures = []

        series_of_group = [self.uid('together', group) for group in range(9)]

        def make(group, index):
            series, sop = series_of_group[group], self.uid('together', group, index)
            frames = [self.gray8(16, 16, (group*17+index) % 251)]
            path = self.write_object(f'together-{group}-{index}', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='TOGETHER', PatientName='TOGETHER^ALL'), frames=frames, rows=16, columns=16,
                                     number=index+1, series_number=group+1, extra=dict(ConversionType='WSD'))
            return Fixture(f'together-{group}-{index}', path, study, series, sop, SECONDARY_CAPTURE, EXPLICIT, frames)

        groups = [[make(group, index) for index in range(10)] for group in range(9)]
        fixtures = [fixture for group in groups for fixture in group]

        def by_web(group, overlapping=False):
            reply = self.stow(target, group)
            # Sent both ways at once, each object is refused as a duplicate on whichever side came second.
            require(reply.status in ((200, 202, 409) if overlapping else (200,)), f'STOW: {reply.brief()}')

        def by_dimse(group, overlapping=False):
            self.tool('storescu', '-xe', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, *[fixture.path for fixture in group],
                      allowed=(0, 1) if overlapping else (0,))

        jobs = [(by_web, groups[index]) for index in range(4)]+[(by_dimse, groups[index]) for index in range(4, 8)]+[(by_web, groups[8], True), (by_dimse, groups[8], True)]
        with ThreadPoolExecutor(len(jobs)) as pool:
            for future in [pool.submit(*job) for job in jobs]:
                future.result()
        reply = self.http(target, 'GET', f'/studies/{study}', accept=DICOM_MULTIPART, timeout=120)
        self.expect_instances(reply, fixtures, 'together')
        rows, _ = self.qido(target, '/studies', StudyInstanceUID=study)
        require(self.json_value(rows[0], '00201208') == len(fixtures), f'instance count {rows[0].get("00201208")}, expected {len(fixtures)}')
        require(self.json_value(rows[0], '00201206') == len(groups), f'series count {rows[0].get("00201206")}, expected {len(groups)}')
        found = self.find(target, 'IMAGE', [f'StudyInstanceUID={study}', f'SeriesInstanceUID={groups[8][0].series}'], 'SOPInstanceUID')
        require(sorted(str(row.SOPInstanceUID) for row in found) == sorted(fixture.sop for fixture in groups[8]), f'C-FIND of the series sent both ways: {len(found)} rows')
        return f'{len(fixtures)} objects, each once, over HTTP and DIMSE'

    # -- clients of another make

    def clients_dcm4che(self, target):
        fixture = self.sets['s']['rgb']
        directory = self.work/f'stowrs-{self.uid()}'
        directory.mkdir()
        study, series, sop = self.uid('dcm4che'), self.uid('dcm4che', 1), self.uid('dcm4che', 2)
        path = self.write_object(f'dcm4che-{self.sequence}', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='DCM4CHE', PatientName='DCM4CHE^CLIENT'), frames=fixture.frames, samples=3, extra=dict(ConversionType='WSD'))
        stored = Fixture('dcm4che', path, study, series, sop, SECONDARY_CAPTURE, EXPLICIT, fixture.frames)
        self.tool(self.dcm4che/'stowrs', '--url', f'{target.base()}{target.root}/studies', '-a', 'json', path)
        self.expect_instances(self.http(target, 'GET', self.study_path(stored), accept=DICOM_MULTIPART), [stored], 'stowrs')
        self.tool(self.dcm4che/'wadors', '--header', '-a', DICOM_MULTIPART.replace(' ', ''), '--out-dir', directory, f'{target.base()}{target.root}{self.study_path(stored)}')
        files = sorted(path for path in directory.rglob('*') if path.is_file())
        require(len(files) == 1, f'wadors unpacked {len(files)} files')
        self.compare_object(files[0].read_bytes(), stored, EXPLICIT, 'wadors')
        return 'dcm4che stowrs stored it and dcm4che wadors unpacked it, equal to what was sent'

    # -- authorization

    def token_of(self, kind, scope):
        now = int(time.time())
        claims = dict(iss=ISSUER, aud=AUDIENCE, sub='probe', exp=now+600, iat=now, nbf=now-5, scope=scope)
        kit = self.tokens
        if kind == 'valid':
            return kit.token(claims)
        if kind == 'es256':
            return kit.token(claims, alg='ES256')
        if kind == 'wrong-signature':
            token = kit.token(claims)
            head, payload, signature = token.split('.')
            return f'{head}.{payload}.{"A" if signature[0] != "A" else "B"}{signature[1:]}'
        if kind == 'unpublished-key':
            return kit.token(claims, key=kit.other, kid=kit.OTHER_KID)
        if kind == 'expired':
            return kit.token(dict(claims, exp=now-3600, iat=now-7200, nbf=now-7200))
        if kind == 'not-yet-valid':
            return kit.token(dict(claims, nbf=now+3600))
        if kind == 'wrong-issuer':
            return kit.token(dict(claims, iss='https://someone.else.test'))
        if kind == 'wrong-audience':
            return kit.token(dict(claims, aud='another-service'))
        if kind == 'alg-none':
            return kit.unsigned(claims)
        if kind == 'hs256-with-public-key':
            return kit.hmac_with_public_key(claims)
        raise ValueError(kind)

    def authorization_for(self, kind, scope):
        if kind == 'no-token':
            return None
        if kind == 'empty-bearer':
            return 'Bearer '
        if kind == 'garbage':
            return 'Bearer not.a.token'
        if kind == 'basic-scheme':
            return 'Basic dXNlcjpwYXNz'
        return 'Bearer '+self.token_of(kind, scope)

    def route_url(self, template, fixture, transaction):
        return template.format(study=fixture.study, series=fixture.series, instance=fixture.sop, frames='1', transaction=transaction, tag='7FE00010')

    def route_request(self, name, method, template, fixture, transaction, token, stored_sop=None):
        """One request of the route; POST bodies are valid, so only authorization can refuse them."""
        path = self.route_url(template, fixture, transaction)
        accept = {'study': DICOM_MULTIPART, 'series': DICOM_MULTIPART, 'instance': DICOM_MULTIPART, 'frames': OCTET_MULTIPART, 'bulkdata': OCTET_MULTIPART, 'rendered': 'image/png', 'frame-rendered': 'image/png', 'wado-uri': 'application/dicom'}.get(name, JSON_MEDIA)
        if method == 'GET':
            return self.http(self.auth_target, 'GET', path, accept=accept, token=token)
        if name in ('store', 'store-to-study'):
            new_sop = stored_sop or self.uid('auth', 'post')
            source = self.write_object(f'auth-post-{new_sop}', SECONDARY_CAPTURE, fixture.study, fixture.series, new_sop, dict(PatientID=str(fixture.dataset().PatientID), PatientName=str(fixture.dataset().PatientName)),
                                       number=9, frames=[self.gray8(2, 2, 2)], rows=2, columns=2, extra=dict(ConversionType='WSD'))
            content_type, body = build_multipart([('application/dicom', source.read_bytes())])
            return self.http(self.auth_target, 'POST', path, data=body, content_type=content_type, token=token)
        body = json.dumps(dict(referencedSOPSequence=[dict(sopClassUID=fixture.sop_class, sopInstanceUID=fixture.sop)])).encode()
        return self.http(self.auth_target, 'POST', path, data=body, content_type='application/json', token=token)

    def auth_refused(self, name, method, template, fixture, transaction, kind):
        reply = self.route_request(name, method, template, fixture, transaction, self.authorization_for(kind, f'{READ} {WRITE}'))
        require(reply.status == 401, f'{method} {name} with {kind}: {reply.brief()}, expected 401')
        require(reply.header('www-authenticate').lower().startswith('bearer'), f'{name} with {kind}: 401 without WWW-Authenticate Bearer (8.5): {reply.header("www-authenticate")!r}')
        require(b'DICM' not in reply.body and fixture.sop.encode() not in reply.body, f'{name} with {kind}: the refusal carries the object')

    def auth_scope(self, name, method, template, scopes, fixture, transaction):
        """Each scope that does not open the route gets 403; a token that has only the others gets nothing."""
        for scope in (READ, SEARCH, WRITE, 'unrelated.scope', ''):
            if scope in scopes:
                continue
            reply = self.route_request(name, method, template, fixture, transaction, 'Bearer '+self.token_of('valid', scope))
            require(reply.status == 403, f'{method} {name} with scope {scope!r}: {reply.brief()}, expected 403')
            require(b'DICM' not in reply.body and fixture.sop.encode() not in reply.body, f'{name} with scope {scope!r}: the refusal carries the object')
        return f'scopes other than {scopes} refused'

    def auth_valid(self, name, method, template, scopes, fixture, transaction):
        for scope in scopes:
            for kind in ('valid', 'es256'):
                reply = self.route_request(name, method, template, fixture, transaction, 'Bearer '+self.token_of(kind, scope))
                require(reply.status in (200, 202), f'{method} {name} with {kind} {scope}: {reply.brief()}')
                if name in ('study', 'series', 'instance'):
                    members = {'study': self.auth_members, 'series': self.auth_members, 'instance': [fixture]}[name]
                    found = [str(self.dicom_from_bytes(part.data, name).SOPInstanceUID) for part in reply.parts()]
                    require(fixture.sop in found and set(found) >= {row.sop for row in members}, f'{name}: {found}')
        return f'opened by {scopes}, with RS256 and ES256'

    def auth_state(self):
        """Refused writes left nothing behind: the archive holds exactly what valid tokens stored."""
        target = self.auth_target
        before = self.archive_exact(target)
        study = self.uid('auth', 'state')
        series, sop = self.uid('auth', 'state', 1), self.uid('auth', 'state', 2)
        path = self.write_object('auth-state', SECONDARY_CAPTURE, study, series, sop, dict(PatientID='AUTHSTATE', PatientName='AUTH^STATE'), frames=[self.gray8(2, 2, 3)], rows=2, columns=2, extra=dict(ConversionType='WSD'))
        content_type, body = build_multipart([('application/dicom', path.read_bytes())])
        for kind in REFUSED_TOKENS:
            reply = self.http(target, 'POST', '/studies', data=body, content_type=content_type, token=self.authorization_for(kind, WRITE))
            require(reply.status == 401, f'{kind}: {reply.brief()}')
        for scope in (READ, SEARCH, ''):
            reply = self.http(target, 'POST', '/studies', data=body, content_type=content_type, token='Bearer '+self.token_of('valid', scope))
            require(reply.status == 403, f'scope {scope!r}: {reply.brief()}')
        require(self.archive_exact(target) == before, 'refused writes changed the archive')
        rows, _ = self.qido(target, '/studies', token='Bearer '+self.token_of('valid', READ), StudyInstanceUID=study)
        require(rows == [], 'a refused write is searchable')
        found = self.find(target, 'STUDY', [f'StudyInstanceUID={study}'], 'PatientID')
        require(found == [], 'a refused write is visible over C-FIND')
        reply = self.http(target, 'POST', '/studies', data=body, content_type=content_type, token='Bearer '+self.token_of('valid', WRITE))
        require(reply.status == 200, f'the valid write: {reply.brief()}')
        require(self.archive_exact(target) == before+1, 'the valid write did not add one object')
        return f'{len(REFUSED_TOKENS)} bad tokens and 3 wrong scopes changed nothing; one valid token added one object'

    def auth_group(self):
        chain = dict(phase='product')
        requires = ['web.peer.tokens', 'web.fixtures']
        keys_file = self.private/'jwks.json'
        with_keys = dict(requires=requires)

        def prepare():
            keys_file.write_text(json.dumps(self.tokens.jwks()))

        self.case('web.pacs.auth.prepare', prepare, **with_keys)
        extra = ['--oauth2-issuer', ISSUER, '--oauth2-audience', AUDIENCE, '--oauth2-jwks', keys_file]
        try:
            with self.pacs('auth', http=AUTH_HTTP, dimse=AUTH_DIMSE, extra=extra) as target:
                self.auth_target = target
                self.auth_members = [self.sets['d']['mf']]
                fixture = self.sets['d']['mf']
                self.case('web.pacs.auth.seed', lambda: self.tool('storescu', '-xe', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, fixture.path), requires=['web.pacs.auth.prepare'])

                def commitment():
                    reply = self.route_request('commitment-request', 'POST', '/storage-commitments', fixture, '', 'Bearer '+self.token_of('valid', WRITE))
                    require(reply.status in (200, 201, 202), f'commitment: {reply.brief()}')
                    answer = reply.json()
                    self.transaction = answer.get('transactionUID') or answer.get('transaction_uid')
                    require(self.transaction, f'commitment names no transaction: {answer}')
                    return f'transaction {self.transaction}'

                self.case('web.pacs.auth.commitment', commitment, requires=['web.pacs.auth.seed'])
                self.transaction = getattr(self, 'transaction', '1.2.3')
                for name, method, template, scopes in ROUTES:
                    for kind in REFUSED_TOKENS:
                        self.case(f'web.pacs.auth.refused.{name}.{kind}', lambda name=name, method=method, template=template, kind=kind: self.auth_refused(name, method, template, fixture, self.transaction, kind), requires=['web.pacs.auth.seed'])
                    self.case(f'web.pacs.auth.scope.{name}', lambda name=name, method=method, template=template, scopes=scopes: self.auth_scope(name, method, template, scopes, fixture, self.transaction), requires=['web.pacs.auth.seed'])
                    self.case(f'web.pacs.auth.valid.{name}', lambda name=name, method=method, template=template, scopes=scopes: self.auth_valid(name, method, template, scopes, fixture, self.transaction), requires=['web.pacs.auth.seed'])
                self.case('web.pacs.auth.state.refused-writes-leave-nothing', self.auth_state, requires=['web.pacs.auth.seed'])
        except Exception as exc:
            self.case('web.pacs.auth.server', lambda exc=exc: require(False, f'the server with OAuth 2.0 did not run: {exc}'))

    # -- limits and restart

    def limit_group(self):
        def body_limit():
            with self.pacs('limits', http=LIMIT_HTTP, dimse=LIMIT_DIMSE, extra=['--http-max-body-mb', 1]) as target:
                study, series = self.uid('limit'), self.uid('limit', 1)
                small = self.write_object('limit-small', SECONDARY_CAPTURE, study, series, self.uid('limit', 2), dict(PatientID='LIMIT', PatientName='LIMIT^SMALL'), frames=[self.gray8(16, 16, 1)], rows=16, columns=16, extra=dict(ConversionType='WSD'))
                big = self.write_object('limit-big', SECONDARY_CAPTURE, study, series, self.uid('limit', 3), dict(PatientID='LIMIT', PatientName='LIMIT^BIG'), number=2, frames=[self.noise(1200, 1200, 1)], rows=1200, columns=1200, extra=dict(ConversionType='WSD'))
                kind, body = build_multipart([('application/dicom', big.read_bytes())])
                reply = self.http(target, 'POST', '/studies', data=body, content_type=kind)
                require(reply.status == 413, f'{len(body)} bytes over a limit of 1 MiB: {reply.brief()}, expected 413')
                rows, _ = self.qido(target, '/instances', StudyInstanceUID=study)
                require(rows == [], 'the refused object was stored')
                kind, body = build_multipart([('application/dicom', small.read_bytes())])
                reply = self.http(target, 'POST', '/studies', data=body, content_type=kind)
                require(reply.status == 200, f'under the limit: {reply.brief()}')
                return '413 and nothing stored over the limit; stored under it'

        self.case('web.pacs.limits.body', body_limit, requires=['web.fixtures'])

        def starts_refused(arguments, words):
            directory = self.work/f'refused-{self.uid()}'
            directory.mkdir()
            done = subprocess.run([str(self.bin/'pacs_server'), '--port', str(LIMIT_DIMSE), '--ae-title', PACS_AE, '--storage-dir', str(directory/'archive'), '--db-path', str(directory/'index.db'), *map(str, arguments)],
                                  capture_output=True, text=True, timeout=30)
            require(done.returncode != 0, f'{arguments}: started; {done.stdout[-200:]}')
            require(words in done.stdout+done.stderr, f'{arguments}: exit {done.returncode}, without saying {words!r}: {(done.stdout+done.stderr)[-300:]}')

        def partial_oauth():
            keys = self.private/'jwks.json'
            keys.write_text(json.dumps(self.tokens.jwks()))
            for arguments in (['--http-port', LIMIT_HTTP, '--oauth2-issuer', ISSUER], ['--http-port', LIMIT_HTTP, '--oauth2-jwks', keys], ['--http-port', LIMIT_HTTP, '--oauth2-audience', AUDIENCE],
                              ['--http-port', LIMIT_HTTP, '--oauth2-issuer', ISSUER, '--oauth2-audience', AUDIENCE], ['--oauth2-issuer', ISSUER, '--oauth2-audience', AUDIENCE, '--oauth2-jwks', keys]):
                starts_refused(arguments, 'OAuth 2.0' if '--http-port' not in arguments else 'are needed together')
            missing = self.private/'no-such-jwks.json'
            starts_refused(['--http-port', LIMIT_HTTP, '--oauth2-issuer', ISSUER, '--oauth2-audience', AUDIENCE, '--oauth2-jwks', missing], 'Cannot load a key')
            return 'a server half configured for tokens does not start at all, so it never looks protected while open'

        def port_taken():
            with socket.socket() as listener:
                listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                listener.bind(('127.0.0.1', LIMIT_HTTP))
                listener.listen()
                directory = self.work/f'taken-{self.uid()}'
                directory.mkdir()
                process = subprocess.Popen([str(self.bin/'pacs_server'), '--port', str(LIMIT_DIMSE), '--ae-title', PACS_AE, '--storage-dir', str(directory/'archive'), '--db-path', str(directory/'index.db'), '--http-port', str(LIMIT_HTTP)],
                                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
                try:
                    output, _ = process.communicate(timeout=30)
                except subprocess.TimeoutExpired:
                    process.kill()
                    output, _ = process.communicate()
                    raise ValueError(f'the server kept running without its HTTP port: {output[-300:]}')
            require(process.returncode != 0 and 'cannot listen' in output, f'exit {process.returncode}: {output[-300:]}')
            return 'a server that cannot open its HTTP port says so and exits'

        def cors_by_option():
            origin = 'https://viewer.example.test'
            with self.pacs('cors', http=LIMIT_HTTP, dimse=LIMIT_DIMSE, extra=['--http-cors-origin', origin]) as target:
                reply = self.http(target, 'GET', '/studies', headers=[f'Origin: {origin}'])
                require(reply.status == 200 and reply.header('access-control-allow-origin') == origin, f'search: {reply.headers.get("access-control-allow-origin")}')
            # Crow answers OPTIONS itself, so a preflight gets no CORS headers: a gap in features.json, owned by #1322.
            return 'responses name the one origin it was told about'

        self.case('web.pacs.exposure.cors-by-option', cors_by_option, requires=['web.fixtures'])
        self.case('web.pacs.limits.partial-oauth-settings', partial_oauth, requires=['web.peer.tokens'])
        self.case('web.pacs.limits.http-port-taken', port_taken, requires=['web.fixtures'])

    def restart_group(self):
        web, dimse = self.sets['s']['mf'], self.sets['d']['rgb']

        def round_trip():
            with self.pacs('restart', http=LIMIT_HTTP, dimse=LIMIT_DIMSE) as target:
                self.check_stow_response(self.stow(target, [web]), [web])
                self.tool('storescu', '-xe', '-aet', SCU_AE, '-aec', target.ae, '127.0.0.1', target.dimse_port, dimse.path)
            with self.pacs('restart', http=LIMIT_HTTP, dimse=LIMIT_DIMSE, fresh=False) as target:
                for fixture in (web, dimse):
                    self.expect_instances(self.http(target, 'GET', self.study_path(fixture), accept=DICOM_MULTIPART), [fixture], f'after restart {fixture.key}')
                    rows, _ = self.qido(target, '/studies', StudyInstanceUID=fixture.study)
                    require(len(rows) == 1 and self.json_value(rows[0], '00201208') == 1, f'after restart: {rows}')
                    found = self.find(target, 'IMAGE', [f'StudyInstanceUID={fixture.study}', f'SeriesInstanceUID={fixture.series}'], 'SOPInstanceUID')
                    require([str(row.SOPInstanceUID) for row in found] == [fixture.sop], f'C-FIND after restart: {found}')
                self.expect_frames(self.frames_reply(target, web, '2'), web, [2], 'frames after restart')
                again = self.stow(target, [web])
                require(again.status == 409, f'a stored object offered again after the restart: {again.brief()}')
                self.index_state(target, [web, dimse])
                require(self.archive_exact(target) == 2, 'the archive changed across the restart')
            return 'both objects, by both protocols, after the process ended and started again'

        self.case('web.pacs.restart.objects-survive', round_trip, requires=['web.fixtures'])

    # -- running the tables

    def flow(self, target, name, action, requires=(), pacs_only=False):
        """A case on `target`. PACS is held to it only after the reference passed it, or the reference differs for a stated reason."""
        case_id = f'web.{target.name}.{name}'
        reason = next((why for pattern, why in REFERENCE_DIFFERENCES.items() if fnmatch.fnmatchcase(name, pattern)), None) if not target.is_pacs else None
        qualified = [f'web.orthanc.{name}'] if target.is_pacs and self.has_reference and not pacs_only else []
        if reason:
            def expected_to_differ():
                try:
                    action()
                except Exception as exc:
                    return f'reference differs ({reason}): {str(exc)[:200]}'
                raise ValueError(f'the reference no longer differs ({reason}); take it off the list')
            self.case(case_id, expected_to_differ, requires=[f'web.{target.name}.{item}' if not item.startswith('web.') else item for item in requires], phase='prerequisite')
            return
        self.case(case_id, action, requires=[*(f'web.{target.name}.{item}' if not item.startswith('web.') else item for item in requires), *qualified],
                  phase='product' if target.is_pacs else 'prerequisite')

    def run_tables(self, target):
        """Everything that holds for any DICOMweb and DIMSE archive, in one server's life."""
        self.lenient_for = () if target.is_pacs else tuple(REFERENCE_LENIENCY)
        self.flow(target, 'stow.store', lambda: self.store_by_stow(target, 's'), requires=['web.fixtures'])
        self.flow(target, 'cstore.store', lambda: self.store_by_cstore(target, 'd'), requires=['web.fixtures'])
        for prefix, stored, first in (('s', 'stow', 'stow.store'), ('d', 'cstore', 'cstore.store')):
            for name, action in (('find', self.find_all), ('get.each', self.get_each), ('get.study', self.get_study), ('move.study', self.move_study), ('qido.studies', self.qido_studies),
                                 ('qido.series', self.qido_series), ('qido.instances', self.qido_instances), ('wado.study', self.wado_study), ('wado.series', self.wado_series),
                                 ('wado.instance', self.wado_instance), ('wado.instance-single-part', self.wado_instance_single), ('wado.as-stored', self.wado_as_stored), ('wado.uri', self.wado_uri), ('metadata.routes', self.metadata_routes),
                                 ('metadata.dcm4che', self.metadata_dcm4che), ('metadata.xml', self.metadata_xml), ('frames.native', self.frames_native), ('frames.single-part', self.frames_single), ('frames.kinds', self.frames_kinds), ('frames.compressed', self.frames_compressed),
                                 ('frames.compressed-as-held', self.frames_compressed_as_is), ('rendered.gray', self.rendered_gray), ('rendered.rgb', self.rendered_rgb), ('rendered.window', self.rendered_window),
                                 ('rendered.viewport', self.rendered_viewport), ('rendered.viewport-keeps-proportions', self.rendered_viewport_ratio), ('rendered.window-functions', self.rendered_window_functions), ('rendered.jpeg', self.rendered_jpeg), ('rendered.compressed', self.rendered_compressed)):
                self.flow(target, f'{stored}.{name}', lambda action=action, prefix=prefix: action(target, prefix), requires=[first])
            if target.is_pacs:
                self.flow(target, f'{stored}.index', lambda prefix=prefix: f'{len(self.index_state(target, self.sets[prefix].values()))} rows read', requires=[first], pacs_only=True)
        for name, resource, accept, status, shape in (*ACCEPT_CASES, *XML_CASES, *BULK_EQUIVALENT_CASES):
            self.flow(target, f'negotiation.{name}', lambda resource=resource, accept=accept, status=status, shape=shape, name=name: self.negotiate(target, resource, accept, status, shape, name), requires=['stow.store'])
        for name, frame_list, status in FRAME_REFUSALS:
            self.flow(target, f'negative.frames.{name}', lambda name=name, frame_list=frame_list, status=status: self.frames_refused(target, name, frame_list, status), requires=['stow.store'])
        self.flow(target, 'negative.frames.non-image', lambda: self.frames_non_image(target), requires=['stow.store'])
        self.flow(target, 'negative.rendered', lambda: self.rendered_refused(target), requires=['stow.store'])
        self.flow(target, 'negative.rendered.window-width-zero', lambda: self.rendered_width_zero(target), requires=['stow.store'])
        rows = self.retrieve_refused(target)
        for name, path, accept in rows:
            def refused(path=path, accept=accept, name=name):
                reply = self.http(target, 'GET', path, accept=accept)
                require(reply.status == 404, f'{name}: {reply.brief()}, expected 404')
            self.flow(target, f'negative.retrieve.{name}', refused, requires=['stow.store'])
        self.flow(target, 'negative.search-under-absent', lambda: self.search_under_absent(target), requires=['stow.store'])
        self.flow(target, 'negative.stow.no-accept', lambda: self.stow_no_accept(target), requires=['stow.store'])
        self.flow(target, 'negative.injection', lambda: self.injection(target), requires=['cstore.store'])
        self.flow(target, 'cross.nested-binary', lambda: self.nested_binary(target), requires=['web.fixtures'])
        self.flow(target, 'cross.undeclared-bytes', lambda: self.undeclared_bytes(target), requires=['web.fixtures'])
        self.flow(target, 'negative.cstore.not-a-uid', lambda: self.cstore_not_a_uid(target), requires=['web.fixtures'])
        self.flow(target, 'stow.xml-response', lambda: self.stow_xml_response(target), requires=['web.fixtures'])
        self.flow(target, 'stow.implicit-part', lambda: self.stow_implicit_part(target), requires=['web.fixtures'])
        self.flow(target, 'cross.empty-patient-id', lambda: self.empty_patient_id(target), requires=['web.fixtures'])
        self.flow(target, 'large.round-trip', lambda: self.large_object(target), requires=['web.fixtures'])
        self.flow(target, 'negotiation.accept-query', lambda: self.accept_query(target), requires=['stow.store'])
        if target.is_pacs:
            self.flow(target, 'negative.retrieve.file-gone', lambda: self.file_gone(target), requires=['stow.store'], pacs_only=True)
            self.flow(target, 'exposure.rest-api-off', lambda: self.rest_api_off(target), requires=['web.fixtures'], pacs_only=True)
            self.flow(target, 'exposure.no-cors-by-default', lambda: self.no_cors_by_default(target), requires=['cstore.store'], pacs_only=True)
        for name, status in STOW_REFUSALS:
            self.flow(target, f'negative.stow.{name}', lambda name=name, status=status: self.stow_refused(target, name, status), requires=['stow.store'])
        for index, name in enumerate(STOW_ALL_FAILED):
            self.flow(target, f'negative.stow.all-failed.{name}', lambda name=name, index=index: self.stow_all_failed(target, name, index), requires=['stow.store'])
        for index, name in enumerate(STOW_PARTIAL):
            self.flow(target, f'negative.stow.partial.{name}', lambda name=name, index=index: self.stow_partial(target, name, index), requires=['stow.store'])
        self.flow(target, 'cross.duplicates', lambda: self.duplicates(target), requires=['stow.store', 'cstore.store'])
        self.flow(target, 'cross.changed-duplicate', lambda: self.changed_duplicate(target), requires=['stow.store'])
        self.flow(target, 'qido.seed', lambda: self.qido_seed(target), requires=['web.fixtures'])
        for name, parameters, keys in QIDO_FILTERS:
            self.flow(target, f'qido.filter.{name}', lambda parameters=parameters, keys=keys: self.qido_filter(target, parameters, keys), requires=['qido.seed'])
        self.flow(target, 'qido.filter.uid-list', lambda: self.qido_uid_list(target), requires=['qido.seed'])
        self.flow(target, 'qido.filter.percent-encoded', lambda: self.qido_percent_encoded(target), requires=['qido.seed'])
        self.flow(target, 'qido.warning.fuzzy', lambda: self.qido_warning_fuzzy(target), requires=['qido.seed'])
        self.flow(target, 'qido.includefield.named', lambda: self.qido_includefield(target), requires=['cstore.store'])
        self.flow(target, 'qido.includefield.all', lambda: self.qido_includefield_all(target), requires=['cstore.store'])
        self.flow(target, 'cstore.wado.whole-study-of-120', lambda: self.wado_many(target), requires=['qido.seed'])
        self.flow(target, 'qido.filter.series-and-instance-keys', lambda: self.qido_series_filters(target), requires=['cstore.store'])
        self.flow(target, 'qido.filter.limit-with-key', lambda: self.qido_limit_with_key(target), requires=['cstore.store'])
        self.flow(target, 'qido.filter.hangul-row', lambda: self.qido_hangul_row(target), requires=['qido.seed'])
        self.flow(target, 'qido.empty', lambda: self.qido_empty(target), requires=['qido.seed'])
        self.flow(target, 'qido.page.offset-past-end', lambda: self.qido_offset_past_end(target), requires=['qido.seed'])
        for name, query in QIDO_BAD_PARAMETERS:
            def bad(query=query):
                reply = self.http(target, 'GET', f'/studies?{query}')
                require(reply.status == 400, f'{query}: {reply.brief()}, expected 400')
            self.flow(target, f'negative.qido.{name}', bad, requires=['qido.seed'])
        if self.results and any(row['id'] == f'web.{target.name}.qido.seed' and row['status'] == 'passed' for row in self.results):
            for name, path, parameters, expected in self.page_routes():
                for size in PAGE_SIZES:
                    self.flow(target, f'qido.page.{name}.{size}', lambda path=path, parameters=parameters, expected=expected, size=size: self.qido_page_walk(target, path, parameters, expected, size), requires=['qido.seed'])
                self.flow(target, f'qido.page.{name}.no-limit', lambda path=path, parameters=parameters, expected=expected: self.qido_no_limit(target, path, parameters, expected), requires=['qido.seed'])
        self.flow(target, 'concurrent.both-protocols', lambda: self.concurrent(target), requires=['web.fixtures'])
        self.flow(target, 'clients.dcm4che', lambda: self.clients_dcm4che(target), requires=['web.fixtures'])

    def archive_case(self, target):
        return f'{self.archive_exact(target)} files, each named by exactly one row'

    def requirement_results(self):
        status = {row['id']: row['status'] for row in self.results}
        results = {}
        for requirement, patterns in requirement_cases().items():
            matched = [case for case in status if any(fnmatch.fnmatchcase(case, pattern) for pattern in patterns)]
            results[requirement] = dict(patterns=patterns, cases=len(matched), passed=sum(status[case] == 'passed' for case in matched),
                                        status='unexecuted' if not matched else 'passed' if all(status[case] == 'passed' for case in matched) else 'failed')
        return results

    def run_all(self):
        self.case('web.fixtures', self.fixtures, phase='fixture_preparation')
        self.case('web.peer.versions', self.versions, phase='prerequisite')
        self.case('web.peer.tokens', self.qualify_tokens, phase='prerequisite')
        self.case('web.peer.multipart', self.qualify_multipart, phase='prerequisite')
        self.has_reference = self.args.reference
        if self.args.reference:
            with self.orthanc() as target:
                self.run_tables(target)
        with self.pacs() as target:
            self.run_tables(target)
            self.case('web.pacs.archive-exact', lambda: self.archive_case(target), requires=['web.pacs.stow.store', 'web.pacs.cstore.store'])
        self.limit_group()
        self.restart_group()
        self.auth_group()
        counts = {status: sum(1 for row in self.results if row['status'] == status) for status in ('passed', 'failed', 'blocked')}
        (self.output/'dicomweb-summary.json').write_text(json.dumps(dict(
            counts=counts, requirement_results=self.requirement_results(), reference_differences=REFERENCE_DIFFERENCES, reference_leniency=REFERENCE_LENIENCY,
            failures=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'failed'],
            blocked=[row['id'] for row in self.results if row['status'] == 'blocked']), indent=1)+'\n')
        print(json.dumps(counts))
        return int(counts['failed'] > 0 or counts['blocked'] > 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--peers-dir', default='/opt/pacs-peers')
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--source-digest', required=True)
    parser.add_argument('--no-reference', dest='reference', action='store_false', help='skip the Orthanc qualification (diagnostic only)')
    args = parser.parse_args()
    args.profile = 'uncompressed'
    return DicomWebCross(args).run_all()


if __name__ == '__main__':
    raise SystemExit(main())
