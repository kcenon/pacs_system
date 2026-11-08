#!/usr/bin/env python3
"""Exercise every valid SOP, syntax, role and codec combination in validation/dcmtk/corpus.py.

Case groups stay separate on purpose: wire cases prove that bytes pass through
unchanged, and codec cases prove that PACS itself encodes or decodes. A failed or
missing result stays failed; the probe never retries or relaxes a bound.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import warnings

import numpy as np
import pydicom
from pydicom._dicom_dict import DicomDictionary
from pydicom.dataset import Dataset
from pydicom.encaps import encapsulate, encapsulate_extended, generate_pixel_data_frame, itemize_frame
from pydicom.uid import ImplicitVRLittleEndian

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
warnings.filterwarnings('ignore', category=UserWarning)
from corpus import (BIG_ENDIAN, CODEC_FORMATS, CODEC_FRAMES, COMPRESSED_SYNTAXES, DECODER_AGREEMENT, DEFLATED,
                    EXPLICIT, IMPLICIT, IOD_VALIDATOR_CONFLICTS, IOD_VALIDATOR_MAX_BITS, LOSSY_BOUNDS, LOSSY_QUALITY,
                    NATIVE_SYNTAXES, PIXEL_FORMATS, STORAGE_SOPS, SYNTAX_BY_UID, UNSUPPORTED_SYNTAXES, codec_valid,
                    iod_validator_lines, requirement_cases, requirement_matches, wire_valid)
from corpus_codecs import (ALLOWED_COLOR, OPENJPEG_SYNTAXES, Codecs, encapsulated, native_values, require_exact,
                           require_lossy_bound, save_any, source_values, stored_values, to_rgb)
from corpus_fixtures import build_fixture, code, item, sequence
from run import Validation, require
from support import declared_support

SCP_PORT, REFERENCE_PORT, REJECT_PORT = 22610, 22611, 22612
SCP_AE, REFERENCE_AE = 'PACS_CORPUS', 'REFERENCE'
SC_UID = '1.2.840.10008.5.1.4.1.1.7'
DCM_CONV_OPTIONS = {'1.2.840.10008.1.2.4.50': ['--jpeg-baseline'], '1.2.840.10008.1.2.4.70': ['--jpeg-lossless'],
                    '1.2.840.10008.1.2.4.90': ['--jpeg2000'], '1.2.840.10008.1.2.4.91': ['--jpeg2000-lossy'],
                    '1.2.840.10008.1.2.4.201': ['--htj2k'], '1.2.840.10008.1.2.4.202': ['--htj2k-rpcl'],
                    '1.2.840.10008.1.2.4.203': ['--htj2k-lossy'], '1.2.840.10008.1.2.5': ['--rle'],
                    '1.2.840.10008.1.2.4.80': ['-t', '1.2.840.10008.1.2.4.80'],
                    '1.2.840.10008.1.2.4.81': ['-t', '1.2.840.10008.1.2.4.81']}
# Debian replaces superseded security revisions, so these are version prefixes. The full
# versions are recorded in tool-versions.json. A newer series must be requalified first.
TOOL_PINS = {'dcmtk': '3.6.9', 'dicom3tools': '1.00~2024', 'libopenjp2-tools': '2.5.', 'ffmpeg': '7:7.1.',
             'python3-pydicom': '2.4.'}
TOOLS = ['dcmdump', 'storescu', 'storescp', 'dcmcjpeg', 'dcmdjpeg', 'dcmcjpls', 'dcmdjpls', 'dcmcrle', 'dcmdrle',
         'dcmconv', 'dcm2json', 'dcm2xml', 'opj_compress', 'opj_decompress', 'ojph_compress', 'ffmpeg', 'dciodvfy']


VR_SAMPLES = {
    'AE': 'CORPUS_AE', 'AS': '042Y', 'AT': (0x0018, 0x1063), 'CS': 'TERM', 'DA': '20261101', 'DS': '-1.5',
    'DT': '20261101120000', 'FD': -2.5, 'FL': 2.5, 'IS': '-3', 'LO': 'Long string', 'LT': 'Long text',
    'OB': b'\x01\x02', 'OD': b'\0' * 8, 'OF': b'\0' * 4, 'OL': b'\0' * 4, 'OV': b'\0' * 8, 'OW': b'\x01\x02',
    'PN': 'Family^Given', 'SH': 'Short', 'SL': -7, 'SS': -8, 'ST': 'Short text', 'SV': -9, 'TM': '120000',
    'UC': 'Unlimited characters', 'UI': '2.25.1318', 'UL': 7, 'UR': 'https://example.invalid/resource', 'US': 8,
    'UT': 'Unlimited text', 'UV': 9,
}


def dictionary_tag(vr):
    """The first plain standard tag with this exact VR, skipping meta and pixel groups.

    OV exists only in the Extended Offset Table, which the offset cases exercise.
    """
    for tag, entry in sorted(DicomDictionary.items()):
        if (entry[0] == vr and entry[1] in ('1', '1-n') and not entry[3] and 0x0008 <= (tag >> 16) < 0x7FE0
                and tag & 0xFFFF != 0 and (tag >> 16) != 0x0028):
            return tag
    return None


def dataset_signature(ds, skip=()):
    """Everything an object carries, read independently and compared by value."""
    signature = []
    for element in ds:
        if element.tag.group == 0x0002 or element.tag.element == 0 or element.tag in skip:
            continue
        if element.VR == 'SQ':
            value = [dataset_signature(entry) for entry in element.value]
        elif isinstance(element.value, (bytes, bytearray)):
            value = hashlib.sha256(bytes(element.value)).hexdigest()
        elif isinstance(element.value, pydicom.multival.MultiValue):
            value = [str(v) for v in element.value]
        else:
            value = str(element.value) if element.value is not None else ''
        signature.append((str(element.tag), element.VR if element.VR not in ('OB', 'OW', 'UN', 'OB or OW') else 'binary', value))
    return signature


def first_difference(left, right):
    for a, b in zip(left, right):
        if a != b:
            return f'{a[0]} differs: {str(a[2])[:60]} vs {str(b[2])[:60]}'
    return f'element count {len(left)} vs {len(right)}'


def pixel_items(ds):
    frames = int(ds.get('NumberOfFrames', 1) or 1)
    return list(generate_pixel_data_frame(ds.PixelData, frames))


class Corpus(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.index_uid = f'{self.run_uid}.1318'
        self.save_every, self.since_save = 200, 0
        self.fixture_paths, self.fixture_data = {}, {}
        self.fixture_meta = []
        self.tool_versions = {}
        self.codecs = Codecs(self.command, self.output/'codec-work', self.bin/'dcm_conv')

    def save(self):
        # Results stay complete on disk at the end; rewriting them per case is quadratic.
        self.since_save += 1
        if self.since_save >= self.save_every or self.final:
            self.since_save = 0
            super().save()

    final = False

    # Preflight ---------------------------------------------------------------------------

    def preflight_tools(self):
        for tool in TOOLS:
            path = shutil.which(tool)
            require(path is not None, f'Required independent tool is missing: {tool}')
            self.tool_versions[tool] = dict(path=path)
        packages = subprocess.run(['dpkg-query', '-W', '-f=${Package} ${Version}\n', 'dcmtk', 'dicom3tools',
                                   'libopenjp2-tools', 'ffmpeg', 'python3-pydicom', 'python3-numpy'],
                                  capture_output=True, text=True)
        self.tool_versions['packages'] = packages.stdout.splitlines()
        installed = dict(line.split(' ', 1) for line in packages.stdout.splitlines())
        for package, prefix in TOOL_PINS.items():
            require(installed.get(package, '').startswith(prefix),
                    f'{package} {installed.get(package)} is not the qualified {prefix} series')
        self.tool_versions['pydicom'], self.tool_versions['numpy'] = pydicom.__version__, np.__version__
        (self.output/'tool-versions.json').write_text(json.dumps(self.tool_versions, indent=2)+'\n')
        return f'{len(TOOLS)} independent tools present'

    # Fixtures ----------------------------------------------------------------------------

    def fixtures_all(self):
        directory = self.output/'fixtures'
        directory.mkdir()
        for index, sop in enumerate(STORAGE_SOPS, 1):
            self.case(f'corpus.fixture.{sop.uid}', lambda sop=sop, index=index: self.one_fixture(sop, index, directory),
                      requires=['corpus.preflight'], phase='fixture_preparation')
        (self.output/'fixture-manifest.json').write_text(json.dumps(self.fixture_meta, indent=1)+'\n')

    def one_fixture(self, sop, index, directory):
        ds = build_fixture(sop, self.index_uid, index)
        path = directory/f'{index:03d}-{sop.family}.dcm'
        ds.save_as(path, write_like_original=False)
        narrowed = 'PixelData' in ds and ds.BitsAllocated > IOD_VALIDATOR_MAX_BITS
        checked = self.narrow_copy(ds, path) if narrowed else path
        done = subprocess.run(['dciodvfy', '-new', str(checked)], capture_output=True, text=True, errors='replace')
        (directory/f'{path.stem}.dciodvfy.txt').write_text(done.stdout+done.stderr)
        lines = iod_validator_lines(done.returncode, done.stdout+done.stderr)
        errors = [line for line in lines if line.startswith('Error')]
        unknown = any('Information Object Not found' in line for line in errors)
        conflicts = [line for line in errors if any(text in line for text, _ in IOD_VALIDATOR_CONFLICTS.get(sop.family, []))]
        remaining = [line for line in errors if line not in conflicts and 'Information Object Not found' not in line]
        self.fixture_paths[sop.uid], self.fixture_data[sop.uid] = path, pydicom.dcmread(path)
        self.fixture_meta.append(dict(sop=sop.uid, name=sop.name, family=sop.family, file=path.name,
                                      sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                                      iod_validator='no table in dciodvfy' if unknown else 'dciodvfy',
                                      iod_validator_input=checked.name if narrowed else 'the fixture itself',
                                      validator_conflicts=conflicts, errors=remaining))
        require(not remaining, f'dciodvfy: {len(remaining)} error(s), first: {remaining[0][:150]}' if remaining else '')
        return ('no independent IOD table for this class' if unknown else
                f'dciodvfy clean{" on a 16-bit copy" if narrowed else ""} with {len(conflicts)} recorded validator conflict(s)')

    def narrow_copy(self, ds, path):
        """The same attributes with pixels dciodvfy can read. The corpus pixel oracles still check the real ones."""
        folder = self.output/'iod-copies'
        folder.mkdir(exist_ok=True)
        narrow = copy.deepcopy(ds)
        narrow.BitsAllocated, narrow.BitsStored, narrow.HighBit = IOD_VALIDATOR_MAX_BITS, IOD_VALIDATOR_MAX_BITS, IOD_VALIDATOR_MAX_BITS-1
        narrow.PixelData = bytes(len(ds.PixelData)*IOD_VALIDATOR_MAX_BITS//ds.BitsAllocated)
        target = folder/path.name
        narrow.save_as(target, write_like_original=False)
        return target

    # Wire ---------------------------------------------------------------------------------

    def network_config(self, name, syntaxes, sops=None):
        """Write a DCMTK profile naming exact SOP classes and syntaxes."""
        uids = sops if sops is not None else [sop.uid for sop in STORAGE_SOPS]
        lines = ['[[TransferSyntaxes]]', '[Syntaxes]']
        lines += [f'TransferSyntax{i} = {uid}' for i, uid in enumerate(syntaxes, 1)]
        lines += ['[[PresentationContexts]]', '[Contexts]']
        lines += [f'PresentationContext{i} = {uid}\\Syntaxes' for i, uid in enumerate(uids, 1)]
        lines += ['[[Profiles]]', '[Default]', 'PresentationContexts = Contexts']
        path = self.output/f'dcmtk-{name}.cfg'
        path.write_text('\n'.join(lines)+'\n')
        return path

    def wire_fixture(self, sop, syntax, directory):
        """Write one object in one syntax using only independent encoders."""
        native = self.fixture_data[sop.uid]
        path = directory/f'{sop.family}-{sop.uid}.dcm'
        if syntax.uid == EXPLICIT:
            shutil.copy2(self.fixture_paths[sop.uid], path)
            return path
        if syntax.uid == IMPLICIT:
            ds = copy.deepcopy(native)
            ds.file_meta.TransferSyntaxUID = ImplicitVRLittleEndian
            ds.is_little_endian, ds.is_implicit_VR = True, True
            ds.save_as(path, write_like_original=False)
            return path
        items, attributes, _ = self.codecs.encode(sop.pixel_format, sop.frames, syntax.uid, self.carrier(sop.pixel_format, sop.frames))
        save_any(encapsulated(copy.deepcopy(native), items, attributes, syntax.uid), path)
        return path

    def carrier(self, format_key, frames):
        """A Secondary Capture object holding a format, for tools that read DICOM."""
        sop = next(sop for sop in STORAGE_SOPS if sop.uid == SC_UID)
        ds = build_fixture(sop._replace(pixel_format=format_key, frames=frames), self.index_uid+'.9', 1)
        return ds

    def check_received(self, expected_path, received, syntax_uid):
        """A delivered object must match by value and, when compressed, byte for byte."""
        expected = pydicom.dcmread(expected_path)
        uid = str(expected.SOPInstanceUID)
        require(uid in received, 'Object was not delivered')
        actual = pydicom.dcmread(received[uid])
        got = str(actual.file_meta.TransferSyntaxUID)
        require(got == syntax_uid, f'Stored transfer syntax {got}, expected {syntax_uid}')
        require(str(actual.SOPClassUID) == str(expected.SOPClassUID), 'SOP Class changed')
        left, right = dataset_signature(actual), dataset_signature(expected)
        require(left == right, 'Dataset changed: '+first_difference(left, right))
        return 'dataset and pixel bytes match' if 'PixelData' in expected else 'dataset matches; IOD has no Pixel Data'

    def received_index(self, directory):
        found = {}
        for path in Path(directory).rglob('*'):
            if path.is_file() and not path.name.endswith('.log'):
                try:
                    found[str(pydicom.dcmread(path, stop_before_pixels=True, force=True).SOPInstanceUID)] = path
                except Exception:
                    continue
        return found

    def wire_all(self):
        everything = NATIVE_SYNTAXES+COMPRESSED_SYNTAXES
        reference_config = self.network_config('reference', [s.uid for s in everything])
        for syntax in everything:
            self.wire_syntax(syntax, reference_config)

    def wire_syntax(self, syntax, reference_config):
        """Incoming to PACS, outgoing from PACS, and archived objects forwarded onward."""
        root = self.output/'wire'/syntax.key
        fixtures, archive, sent, forwarded = root/'fixtures', root/'archive', root/'scu-received', root/'forward-received'
        for directory in (fixtures, archive, sent, forwarded):
            directory.mkdir(parents=True)
        members, paths = [], {}
        for sop in STORAGE_SOPS:
            if not wire_valid(sop, syntax)[0]:
                continue
            members.append(sop)

        def prepare():
            for sop in members:
                if sop.uid not in self.fixture_paths:
                    continue
                paths[sop.uid] = self.wire_fixture(sop, syntax, fixtures)
            require(paths, 'No fixtures prepared')
            return f'{len(paths)} objects in {syntax.key}'

        prepared = f'corpus.prepare.{syntax.key}'
        self.case(prepared, prepare, phase='fixture_preparation')
        proposal = self.network_config(f'propose-{syntax.key}', [syntax.uid], [s.uid for s in members])

        def incoming():
            with self.server([self.bin/'store_scp', SCP_PORT, SCP_AE, '--storage-dir', archive], SCP_PORT) as process:
                self.command(['storescu', '-nh', '-aet', 'CORPUS_SCU', '-aec', SCP_AE, '-xf', proposal, 'Default',
                              '127.0.0.1', SCP_PORT, *paths.values()], timeout=180, allowed=(0, 1, 2, 3, 4, 5, 21, 22, 23, 24))
            require(process.returncode == 0, f'store_scp shutdown exit {process.returncode}')
            return 'storescu completed and store_scp shut down cleanly'

        send_in = f'corpus.send.scp.{syntax.key}'
        self.case(send_in, incoming, requires=[prepared])
        archived = self.received_index(archive) if archive.exists() else {}
        for sop in members:
            self.case(f'corpus.wire.scp.{sop.uid}.{syntax.key}',
                      lambda sop=sop: self.check_received(paths[sop.uid], archived, syntax.uid), requires=[send_in])

        def outgoing(sources, directory):
            with self.server(['storescp', '-aet', REFERENCE_AE, '-xf', reference_config, 'Default', '-od', directory,
                              '-uf', '--exec-sync', REFERENCE_PORT], REFERENCE_PORT, log_name=self.current+'-reference'):
                flags = {IMPLICIT: ['-xv'], EXPLICIT: ['-xe']}.get(syntax.uid, [])
                self.command([self.bin/'store_scu', *flags, '-aec', REFERENCE_AE, '127.0.0.1', REFERENCE_PORT, *sources],
                             timeout=180, allowed=(0, 1, 2))
            return f'store_scu sent {len(sources)} object(s)'

        send_out = f'corpus.send.scu.{syntax.key}'
        self.case(send_out, lambda: outgoing(list(paths.values()), sent), requires=[prepared])
        delivered = self.received_index(sent) if sent.exists() else {}
        for sop in members:
            self.case(f'corpus.wire.scu.{sop.uid}.{syntax.key}',
                      lambda sop=sop: self.check_received(paths[sop.uid], delivered, syntax.uid), requires=[send_out])

        send_forward = f'corpus.send.forward.{syntax.key}'
        self.case(send_forward, lambda: outgoing(list(archived.values()) or require(False, 'Archive is empty'), forwarded),
                  requires=[send_in])
        passed_on = self.received_index(forwarded) if forwarded.exists() else {}
        for sop in members:
            self.case(f'corpus.wire.forward.{sop.uid}.{syntax.key}',
                      lambda sop=sop: self.check_received(paths[sop.uid], passed_on, syntax.uid), requires=[send_forward])

    def wire_server(self):
        """pacs_server shares the storage service, so it must accept every declared class too."""
        root = self.output/'wire'/'pacs-server'
        archive = root/'archive'
        archive.mkdir(parents=True)
        members = [sop for sop in STORAGE_SOPS if sop.uid in self.fixture_paths]

        def send():
            proposal = self.network_config('propose-server', [EXPLICIT], [sop.uid for sop in members])
            with self.server([self.bin/'pacs_server', '--port', SCP_PORT, '--ae-title', SCP_AE,
                              '--storage-dir', archive, '--db-path', root/'index.db'], SCP_PORT):
                self.command(['storescu', '-nh', '-aet', 'CORPUS_SCU', '-aec', SCP_AE, '-xf', proposal, 'Default',
                              '127.0.0.1', SCP_PORT, *[self.fixture_paths[sop.uid] for sop in members]],
                             timeout=180, allowed=(0, 1, 2, 3, 4, 5, 21, 22, 23, 24))
            return f'storescu sent {len(members)} objects to pacs_server'

        self.case('corpus.send.pacs-server', send, requires=['corpus.preflight'])
        stored = self.received_index(archive)
        for sop in members:
            self.case(f'corpus.wire.pacs-server.{sop.uid}',
                      lambda sop=sop: self.check_received(self.fixture_paths[sop.uid], stored, EXPLICIT),
                      requires=['corpus.send.pacs-server'])

    # Declared negative paths per SOP class ------------------------------------------------

    def negatives(self):
        """An SCP refusing a class must store nothing; an SCU refused by a peer must report it."""
        directory = self.output/'negative'
        directory.mkdir()
        archive = directory/'archive'
        archive.mkdir()
        with self.server([self.bin/'store_scp', SCP_PORT, SCP_AE, '--storage-dir', archive, '--accept', 'SC'], SCP_PORT,
                         log_name='corpus-negative-scp'):
            for sop in STORAGE_SOPS:
                if sop.uid != SC_UID:
                    self.case(f'corpus.negative.scp.{sop.uid}', lambda sop=sop: self.negative_scp(sop, archive),
                              requires=[f'corpus.fixture.{sop.uid}'])
        # The SCP above accepts SC, so refusing SC takes one configured for another class.
        with self.server([self.bin/'store_scp', SCP_PORT, SCP_AE, '--storage-dir', archive, '--accept', 'CR'], SCP_PORT,
                         log_name='corpus-negative-scp-sc'):
            sc = next(sop for sop in STORAGE_SOPS if sop.uid == SC_UID)
            self.case(f'corpus.negative.scp.{SC_UID}', lambda: self.negative_scp(sc, archive),
                      requires=[f'corpus.fixture.{SC_UID}'])
        restricted = self.network_config('verification-only', [EXPLICIT, IMPLICIT], ['1.2.840.10008.1.1'])
        received = directory/'received'
        received.mkdir()
        with self.server(['storescp', '-aet', REFERENCE_AE, '-xf', restricted, 'Default', '-od', received,
                          REFERENCE_PORT], REFERENCE_PORT, log_name='corpus-negative-reference'):
            for sop in STORAGE_SOPS:
                self.case(f'corpus.negative.scu.{sop.uid}', lambda sop=sop: self.negative_scu(sop, received),
                          requires=[f'corpus.fixture.{sop.uid}'])

    def negative_scp(self, sop, archive):
        path = self.fixture_paths[sop.uid]
        before = sorted(p.name for p in archive.rglob('*') if p.is_file())
        output = self.command(['storescu', '-aet', 'CORPUS_SCU', '-aec', SCP_AE, '-R', '127.0.0.1', SCP_PORT, path],
                              timeout=30, allowed=tuple(range(1, 256)))
        after = sorted(p.name for p in archive.rglob('*') if p.is_file())
        require(before == after, 'A refused class changed the archive')
        return 'refused with a non-zero peer exit and no stored object'

    def negative_scu(self, sop, received):
        path = self.fixture_paths[sop.uid]
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        self.command([self.bin/'store_scu', '-aec', REFERENCE_AE, '127.0.0.1', REFERENCE_PORT, path],
                     timeout=30, allowed=tuple(range(1, 256)))
        require(hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'Source file changed')
        require(not any(p.is_file() for p in received.rglob('*')), 'A refused class was delivered')
        return 'store_scu reported the refusal; nothing delivered; source unchanged'

    # Codecs -------------------------------------------------------------------------------

    def codec_source(self, format_key, frames):
        path = self.output/'codec-sources'/f'{format_key}.f{frames}.dcm'
        path.parent.mkdir(exist_ok=True)
        if not path.exists():
            self.carrier(format_key, frames).save_as(path, write_like_original=False)
        return path

    def encode_case(self, syntax, format_key, frames):
        """PACS encodes; independent tools decide what the output really holds."""
        format_ = PIXEL_FORMATS[format_key]
        source, target = self.codec_source(format_key, frames), self.codecs.scratch('.dcm')
        quality = ['-q', LOSSY_QUALITY[syntax.uid]] if syntax.uid in LOSSY_QUALITY else []
        self.command([self.bin/'dcm_conv', source, target, *DCM_CONV_OPTIONS[syntax.uid], *quality, '--no-progress'], timeout=60)
        original, ds = pydicom.dcmread(source), pydicom.dcmread(target)
        require(str(ds.file_meta.TransferSyntaxUID) == syntax.uid, f'Output syntax {ds.file_meta.TransferSyntaxUID}')
        items = list(generate_pixel_data_frame(ds.PixelData, frames))
        require(len(items) == frames, f'{len(items)} frames in output, expected {frames}')
        if format_.samples > 1:
            allowed = ALLOWED_COLOR[syntax.uid]
            require(str(ds.PhotometricInterpretation) in allowed,
                    f'Photometric {ds.PhotometricInterpretation} not in {sorted(allowed)}')
        expected = source_values(format_, frames)
        actual = self.codecs.decode(target, format_, frames)
        if syntax.lossy:
            self.require_lossy_metadata(ds, original, syntax)
            detail = require_lossy_bound(actual, expected, format_, syntax.uid)
        else:
            self.require_lossless_metadata(ds, original)
            detail = require_exact(actual, expected, 'independent decode')
        if syntax.uid in OPENJPEG_SYNTAXES and format_.samples == 1 and not format_.signed and format_.bits_stored in (8, 16):
            second = self.codecs.ffmpeg_decode(ds, format_, frames)
            if syntax.lossy:
                detail += '; FFmpeg: '+require_lossy_bound(second, expected, format_, syntax.uid)
            else:
                detail += '; FFmpeg: '+require_exact(second, expected, 'FFmpeg decode')
        return detail

    @staticmethod
    def require_lossless_metadata(ds, original):
        require(str(ds.SOPInstanceUID) == str(original.SOPInstanceUID), 'Lossless target changed SOP Instance UID')
        require(str(ds.get('LossyImageCompression', '00')) == '00', 'Lossless output claims lossy compression')

    @staticmethod
    def require_lossy_metadata(ds, original, syntax):
        bound = LOSSY_BOUNDS[syntax.uid]
        require(str(ds.get('LossyImageCompression')) == '01', 'Lossy Image Compression is not 01')
        ratios = ds.get('LossyImageCompressionRatio')
        require(ratios and float(list(ratios)[-1] if isinstance(ratios, pydicom.multival.MultiValue) else ratios) > 0,
                'Lossy Image Compression Ratio is missing')
        method = ds.get('LossyImageCompressionMethod')
        methods = list(method) if isinstance(method, pydicom.multival.MultiValue) else [method]
        require(str(methods[-1]) == bound['method'], f'Lossy method {methods[-1]}, expected {bound["method"]}')
        require(str(ds.SOPInstanceUID) != str(original.SOPInstanceUID), 'Lossy output kept the SOP Instance UID')
        require(str(ds.file_meta.MediaStorageSOPInstanceUID) == str(ds.SOPInstanceUID), 'Meta SOP Instance UID differs')
        require(str(ds.ImageType[0]) == 'DERIVED', 'Lossy output is not DERIVED')
        require('SourceImageSequence' in ds and
                str(ds.SourceImageSequence[0].ReferencedSOPInstanceUID) == str(original.SOPInstanceUID),
                'Source Image Sequence does not reference the original')
        require(bool(ds.get('DerivationDescription')), 'Derivation Description is missing')

    def decode_case(self, syntax, format_key, frames):
        """Independent encoders produce the input; PACS decodes it to native bytes."""
        format_ = PIXEL_FORMATS[format_key]
        carrier = self.carrier(format_key, frames)
        items, attributes, encoder = self.codecs.encode(format_key, frames, syntax.uid, copy.deepcopy(carrier))
        compressed, output = self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')
        save_any(encapsulated(copy.deepcopy(carrier), items, attributes, syntax.uid), compressed)
        self.command([self.bin/'dcm_conv', compressed, output, '--explicit', '--no-progress'], timeout=60)
        ds = pydicom.dcmread(output)
        require(str(ds.file_meta.TransferSyntaxUID) == EXPLICIT, 'Output is not Explicit VR Little Endian')
        actual = to_rgb(native_values(ds, format_, frames), str(ds.PhotometricInterpretation))
        expected = source_values(format_, frames)
        if syntax.lossy:
            require(str(ds.get('LossyImageCompression')) == '01', 'Decoding must keep Lossy Image Compression 01')
            detail = require_lossy_bound(actual, expected, format_, syntax.uid)
            reference = self.codecs.decode(compressed, format_, frames)
            agreement = DECODER_AGREEMENT.get(syntax.uid, 1)
            difference = int(np.abs(actual.astype('i8')-reference.astype('i8')).max())
            require(difference <= agreement, f'PACS and independent decoders differ by {difference}, limit {agreement}')
            return f'{encoder}; {detail}; decoder difference {difference}'
        return f'{encoder}; '+require_exact(actual, expected, 'PACS decode')

    def codecs_all(self):
        for format_key in CODEC_FORMATS:
            for syntax in COMPRESSED_SYNTAXES:
                if not codec_valid(format_key, syntax)[0]:
                    continue
                for frames in CODEC_FRAMES:
                    variant = f'{format_key}.f{frames}'
                    self.case(f'corpus.codec.encode.{syntax.key}.{variant}',
                              lambda s=syntax, f=format_key, n=frames: self.encode_case(s, f, n), requires=['corpus.preflight'])
                    self.case(f'corpus.codec.decode.{syntax.key}.{variant}',
                              lambda s=syntax, f=format_key, n=frames: self.decode_case(s, f, n), requires=['corpus.preflight'])
        self.dht_cases()
        self.ffmpeg_j2k_cases()
        self.offset_cases()
        self.color_jpeg_lossless_case()

    def dht_cases(self):
        """Lossless JPEG streams from other encoders use other Huffman tables."""
        def check(option, format_key, frames=1):
            format_ = PIXEL_FORMATS[format_key]
            source, encoded, output = self.codec_source(format_key, frames), self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')
            self.command(['dcmcjpeg', '+e1', *option, source, encoded])
            self.command([self.bin/'dcm_conv', encoded, output, '--explicit', '--no-progress'], timeout=60)
            actual = native_values(pydicom.dcmread(output), format_, frames)
            return require_exact(actual, source_values(format_, frames), 'PACS decode of DCMTK tables')
        for name, option, format_key in (('optimized-16bit', ['+ho'], 'mono16u'), ('optimized-12bit', ['+ho'], 'mono12'),
                                         ('optimized-8bit', ['+ho'], 'mono8'), ('standard-8bit', ['-ho'], 'mono8')):
            self.case(f'corpus.jpeg-dht.{name}', lambda o=option, f=format_key: check(o, f), requires=['corpus.preflight'])
        self.case('corpus.jpeg-dht.extreme-differences', self.extreme_jpeg_lossless, requires=['corpus.preflight'])

    def extreme_jpeg_lossless(self):
        """Differences of exactly 32768 use SSSS 16, which has no additional bits (ITU T.81 H.1.2.2)."""
        format_ = PIXEL_FORMATS['mono16u']
        carrier = self.carrier('mono16u', 1)
        data = np.zeros((format_.rows, format_.columns), dtype='<u2')
        data[::2, ::2], data[1::2, 1::2] = 32768, 65535
        carrier.PixelData = data.tobytes()
        source, encoded, decoded, reference = [self.codecs.scratch('.dcm') for _ in range(4)]
        carrier.save_as(source, write_like_original=False)
        # PACS output must satisfy DCMTK; DCMTK output must satisfy PACS.
        self.command([self.bin/'dcm_conv', source, encoded, '--jpeg-lossless', '--no-progress'], timeout=60)
        self.command(['dcmdjpeg', encoded, reference])
        require_exact(native_values(pydicom.dcmread(reference), format_, 1), stored_values(data[None], format_), 'DCMTK decode of PACS stream')
        independent = self.codecs.scratch('.dcm')
        self.command(['dcmcjpeg', '+e1', source, independent])
        self.command([self.bin/'dcm_conv', independent, decoded, '--explicit', '--no-progress'], timeout=60)
        return require_exact(native_values(pydicom.dcmread(decoded), format_, 1), stored_values(data[None], format_), 'PACS decode of DCMTK stream')

    def ffmpeg_j2k_cases(self):
        """JPEG 2000 streams from an encoder that shares no code with PACS's OpenJPEG."""
        def check(format_key):
            format_ = PIXEL_FORMATS[format_key]
            carrier = self.carrier(format_key, 3)
            items = self.codecs.ffmpeg_encode(format_, 3)
            compressed, output = self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')
            save_any(encapsulated(copy.deepcopy(carrier), items, {}, '1.2.840.10008.1.2.4.90'), compressed)
            self.command([self.bin/'dcm_conv', compressed, output, '--explicit', '--no-progress'], timeout=60)
            return require_exact(native_values(pydicom.dcmread(output), format_, 3), source_values(format_, 3), 'PACS decode of FFmpeg stream')
        for format_key in ('mono8', 'mono12', 'mono16u'):
            self.case(f'corpus.codec.decode.jpeg2000-lossless.ffmpeg.{format_key}', lambda f=format_key: check(f), requires=['corpus.preflight'])

    def offset_cases(self):
        """Empty, populated and extended offset tables, and frames split across fragments."""
        def layout_bytes(ds, items, layout):
            if layout == 'empty-bot':
                ds.PixelData = encapsulate(items, has_bot=False)
            elif layout == 'populated-bot':
                ds.PixelData = encapsulate(items, has_bot=True)
            elif layout == 'extended-table':
                ds.PixelData, ds.ExtendedOffsetTable, ds.ExtendedOffsetTableLengths = encapsulate_extended(items)
            else:
                split = []
                for frame in items:
                    cut = (len(frame)//2) & ~1
                    split += [frame[:cut], frame[cut:]]
                # Two even fragments per frame, located through the Basic Offset Table.
                offsets, position, body = [], 0, b''
                for index in range(0, len(split), 2):
                    offsets.append(position)
                    for fragment in split[index:index+2]:
                        encoded = b''.join(itemize_frame(fragment, 1))
                        body += encoded
                        position += len(encoded)
                table = b''.join(int(value).to_bytes(4, 'little') for value in offsets)
                ds.PixelData = b'\xfe\xff\x00\xe0'+len(table).to_bytes(4, 'little')+table+body
            ds['PixelData'].VR = 'OB'
            ds['PixelData'].is_undefined_length = True

        def check(syntax, layout):
            format_ = PIXEL_FORMATS['mono16u']
            carrier = self.carrier('mono16u', 3)
            items, attributes, encoder = self.codecs.encode('mono16u', 3, syntax.uid, copy.deepcopy(carrier))
            ds = encapsulated(copy.deepcopy(carrier), items, attributes, syntax.uid)
            layout_bytes(ds, items, layout)
            compressed, output = self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')
            save_any(ds, compressed)
            self.command([self.bin/'dcm_conv', compressed, output, '--explicit', '--no-progress'], timeout=60)
            detail = require_exact(native_values(pydicom.dcmread(output), format_, 3), source_values(format_, 3), f'{layout} decode')
            return f'{encoder}; {detail}'

        for syntax in COMPRESSED_SYNTAXES:
            if syntax.lossy:
                continue
            layouts = ['empty-bot', 'populated-bot', 'extended-table']
            if syntax.tool_family != 'rle':
                layouts.append('split-fragments')
            for layout in layouts:
                self.case(f'corpus.offsets.{syntax.key}.{layout}', lambda s=syntax, l=layout: check(s, l), requires=['corpus.preflight'])

    def color_jpeg_lossless_case(self):
        """The built-in codec claims grayscale only: color must decode correctly or fail plainly."""
        format_ = PIXEL_FORMATS['rgb8']
        source, encoded, output = self.codec_source('rgb8', 1), self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')

        def check():
            self.command(['dcmcjpeg', '+e1', source, encoded])
            before = hashlib.sha256(encoded.read_bytes()).hexdigest()
            done = self.command([self.bin/'dcm_conv', encoded, output, '--explicit', '--no-progress'], timeout=60, allowed=(0, 2))
            require(hashlib.sha256(encoded.read_bytes()).hexdigest() == before, 'Source file changed')
            if output.exists():
                return require_exact(to_rgb(native_values(pydicom.dcmread(output), format_, 1), 'RGB'), source_values(format_, 1), 'color decode')
            return 'refused explicitly with no output'
        self.case('corpus.reject.codec.jpeg-lossless-color', check, requires=['corpus.preflight'])
        self.case('corpus.reject.encode.jpeg-lossless-color', self.reject_color_encode, requires=['corpus.preflight'])
        self.case('corpus.reject.encode.jpeg-ls-near-signed', self.reject_signed_near_lossless, requires=['corpus.preflight'])
        self.case('corpus.reject.encode.jpeg-baseline-signed', self.reject_signed_baseline, requires=['corpus.preflight'])

    def reject_color_encode(self):
        source, output = self.codec_source('rgb8', 1), self.codecs.scratch('.dcm')
        self.command([self.bin/'dcm_conv', source, output, '--jpeg-lossless', '--no-progress'], timeout=60, allowed=(2,))
        require(not output.exists(), 'A refused encode left an output file')
        return 'dcm_conv exit 2 with no output'

    def reject_signed_near_lossless(self):
        source, output = self.codec_source('mono16s', 1), self.codecs.scratch('.dcm')
        self.command([self.bin/'dcm_conv', source, output, '-t', '1.2.840.10008.1.2.4.81', '-q', '50', '--no-progress'],
                     timeout=60, allowed=(2,))
        require(not output.exists(), 'A refused encode left an output file')
        return 'dcm_conv exit 2 with no output'

    def reject_signed_baseline(self):
        source, output = self.codec_source('mono8s', 1), self.codecs.scratch('.dcm')
        self.command([self.bin/'dcm_conv', source, output, '--jpeg-baseline', '--no-progress'], timeout=60, allowed=(2,))
        require(not output.exists(), 'A refused encode left an output file')
        return 'dcm_conv exit 2 with no output'

    # Unsupported syntaxes -----------------------------------------------------------------

    def unsupported_source(self, uid, directory):
        """A file carrying the syntax, without pretending its payload is decodable."""
        path = directory/f'{UNSUPPORTED_SYNTAXES[uid]}.dcm'
        if uid == DEFLATED:
            self.command(['dcmconv', '+td', self.codec_source('mono8', 1), path])
        elif uid == BIG_ENDIAN:
            self.command(['dcmconv', '+tb', self.codec_source('mono16u', 1), path])
        else:
            save_any(encapsulated(self.carrier('mono8', 1), [b'\xff\x4f\xff\xd9'+b'\0'*12], {}, uid), path)
        return path

    def unsupported_all(self):
        directory = self.output/'unsupported'
        directory.mkdir()

        def prepare():
            for uid in UNSUPPORTED_SYNTAXES:
                self.unsupported_source(uid, directory)
            return f'{len(UNSUPPORTED_SYNTAXES)} files'
        self.case('corpus.reject.prepare', prepare, phase='fixture_preparation', requires=['corpus.preflight'])
        for uid, key in UNSUPPORTED_SYNTAXES.items():
            path = directory/f'{key}.dcm'
            self.case(f'corpus.reject.network.{key}', lambda u=uid, p=path: self.reject_network(u, p, directory),
                      requires=['corpus.reject.prepare'])
            if uid != BIG_ENDIAN:
                self.case(f'corpus.reject.codec.{key}', lambda u=uid, p=path: self.reject_codec(u, p),
                          requires=['corpus.reject.prepare'])
                self.case(f'corpus.reject.scu.{key}', lambda u=uid, p=path: self.reject_scu(u, p, directory),
                          requires=['corpus.reject.prepare'])

    def reject_network(self, uid, path, directory):
        archive = directory/f'{UNSUPPORTED_SYNTAXES[uid]}-archive'
        archive.mkdir()
        proposal = self.network_config(f'unsupported-{UNSUPPORTED_SYNTAXES[uid]}', [uid], [SC_UID])
        with self.server([self.bin/'store_scp', SCP_PORT, SCP_AE, '--storage-dir', archive], SCP_PORT) as process:
            self.command(['storescu', '-aet', 'CORPUS_SCU', '-aec', SCP_AE, '-xf', proposal, 'Default',
                          '127.0.0.1', SCP_PORT, path], timeout=30, allowed=tuple(range(1, 256)))
        require(not any(p.is_file() for p in archive.rglob('*')), 'An unsupported syntax reached the archive')
        require(process.returncode == 0, f'store_scp shutdown exit {process.returncode}')
        return 'PACS_SCP refused the only proposed context; archive empty'

    def reject_codec(self, uid, path):
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        native, target = self.codec_source('mono8', 1), self.codecs.scratch('.dcm')
        self.command([self.bin/'dcm_conv', native, target, '-t', uid, '--no-progress'], timeout=30, allowed=(1, 2))
        require(not target.exists(), 'Encoding to an unsupported syntax left an output')
        target = self.codecs.scratch('.dcm')
        self.command([self.bin/'dcm_conv', path, target, '--explicit', '--no-progress'], timeout=30, allowed=(1, 2))
        require(not target.exists(), 'Decoding an unsupported syntax left an output')
        require(hashlib.sha256(path.read_bytes()).hexdigest() == before, 'Source file changed')
        return 'dcm_conv refused both directions; no output; source unchanged'

    def reject_scu(self, uid, path, directory):
        """Sending may be refused, or may pass the bytes on untouched. It must not alter them."""
        received = directory/f'{UNSUPPORTED_SYNTAXES[uid]}-received'
        received.mkdir()
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        config = self.network_config(f'peer-{UNSUPPORTED_SYNTAXES[uid]}', [uid], [SC_UID])
        with self.server(['storescp', '-aet', REFERENCE_AE, '-xf', config, 'Default', '-od', received,
                          REFERENCE_PORT], REFERENCE_PORT, log_name=self.current+'-reference'):
            self.command([self.bin/'store_scu', '-aec', REFERENCE_AE, '127.0.0.1', REFERENCE_PORT, path],
                         timeout=30, allowed=(0, 1, 2))
        require(hashlib.sha256(path.read_bytes()).hexdigest() == before, 'Source file changed')
        delivered = [p for p in received.rglob('*') if p.is_file()]
        if not delivered:
            return 'store_scu did not deliver; source unchanged'
        require(len(delivered) == 1, 'More than one object delivered')
        if uid == DEFLATED:
            return 'delivered; peer owns the deflated stream; source unchanged'
        require(pydicom.dcmread(delivered[0]).PixelData == pydicom.dcmread(path).PixelData, 'Forwarding changed the bytes')
        return 'forwarded with identical bytes; PACS claims no codec here'

    # File formats --------------------------------------------------------------------------

    def rich_dataset(self):
        """One attribute of every VR, plus nested defined and undefined length sequences."""
        ds = self.carrier('mono8', 1)
        ds.SpecificCharacterSet = 'ISO_IR 192'
        ds.StudyDescription = 'Rich \ud55c\uae00 corpus'
        for vr, value in VR_SAMPLES.items():
            tag = dictionary_tag(vr)
            if tag is not None and tag not in ds:
                ds.add_new(tag, vr, value)
        ds.add_new((0x0009, 0x0010), 'LO', 'PACS_CORPUS')
        ds.add_new((0x0009, 0x1001), 'UN', b'\x01\x02\x03\x04')
        inner = item(TextValue='deep', ReferencedSOPSequence=sequence(item(
            ReferencedSOPClassUID=SC_UID, ReferencedSOPInstanceUID=self.index_uid+'.77')))
        ds.ContentSequence = sequence(item(ValueType='TEXT', ContentSequence=sequence(
            item(ValueType='CONTAINER', ContentSequence=sequence(inner)))))
        ds['ContentSequence'].is_undefined_length = True
        ds.ContentSequence[0].is_undefined_length_sequence_item = True
        ds.ContentSequence[0]['ContentSequence'].is_undefined_length = True
        return ds

    def native_roundtrip(self, syntax_uid, flag):
        ds = self.rich_dataset()
        source, output = self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')
        ds.save_as(source, write_like_original=False)
        self.command([self.bin/'dcm_conv', source, output, flag, '--no-progress'], timeout=60)
        got = pydicom.dcmread(output)
        require(str(got.file_meta.TransferSyntaxUID) == syntax_uid, f'Output syntax {got.file_meta.TransferSyntaxUID}')
        original = pydicom.dcmread(source)
        left, right = dataset_signature(got), dataset_signature(original)
        require(left == right, 'Conversion changed values: '+first_difference(left, right))
        # DCMTK must also read the output completely.
        self.command(['dcmconv', '+te', output, self.codecs.scratch('.dcm')])
        return f'{len(right)} top-level elements preserved'

    def json_roundtrip(self):
        ds = self.rich_dataset()
        source, json_path, back = self.codecs.scratch('.dcm'), self.codecs.scratch('.json'), self.codecs.scratch('.dcm')
        ds.save_as(source, write_like_original=False)
        self.command([self.bin/'dcm_to_json', source, json_path, '--compact', '--no-meta', '--bulk-data', 'inline'], timeout=60)
        data = json.loads(json_path.read_text())
        require(isinstance(data, (dict, list)), 'JSON document is not an object')
        document = data[0] if isinstance(data, list) else data
        # pydicom reads PS3.18 JSON independently of PACS.
        parsed = Dataset.from_json({k: v for k, v in document.items() if k not in ('7FE00010',)})
        original = pydicom.dcmread(source)
        require(str(parsed.SOPInstanceUID) == str(original.SOPInstanceUID), 'SOP Instance UID differs in JSON')
        require(str(parsed.StudyDescription) == str(original.StudyDescription), 'UTF-8 text differs in JSON')
        require(str(parsed.ContentSequence[0].ContentSequence[0].ContentSequence[0].TextValue) == 'deep', 'Nested text differs')
        self.command([self.bin/'json_to_dcm', json_path, back, '--template', source], timeout=60)
        got = pydicom.dcmread(back)
        skip = {pydicom.tag.Tag(0x7FE0, 0x0010)}
        left, right = dataset_signature(got, skip), dataset_signature(original, skip)
        require(left == right, 'JSON round trip changed values: '+first_difference(left, right))
        return f'pydicom read the JSON and {len(right)} elements survived the round trip'

    def xml_roundtrip(self):
        ds = self.rich_dataset()
        source, xml_path, back = self.codecs.scratch('.dcm'), self.codecs.scratch('.xml'), self.codecs.scratch('.dcm')
        ds.save_as(source, write_like_original=False)
        self.command([self.bin/'dcm_to_xml', source, xml_path, '--no-meta', '--bulk-data', 'inline'], timeout=60)
        import xml.etree.ElementTree as ET
        namespace = '{http://dicom.nema.org/PS3.19/models/NativeDICOM}'
        root = ET.parse(xml_path).getroot()
        require(root.tag == namespace+'NativeDicomModel', f'Root element {root.tag}')
        original = pydicom.dcmread(source)
        attributes = {node.get('tag'): node for node in root.findall(namespace+'DicomAttribute')}
        require(attributes['00080018'].find(namespace+'Value').text == str(original.SOPInstanceUID), 'SOP Instance UID differs in XML')
        self.command([self.bin/'xml_to_dcm', xml_path, back, '--template', source], timeout=60)
        got = pydicom.dcmread(back)
        skip = {pydicom.tag.Tag(0x7FE0, 0x0010)}
        left, right = dataset_signature(got, skip), dataset_signature(original, skip)
        require(left == right, 'XML round trip changed values: '+first_difference(left, right))
        return f'Native model parsed and {len(right)} elements survived the round trip'

    def image_to_dicom(self):
        """A JPEG from DCMTK becomes Secondary Capture holding its decoded samples."""
        format_ = PIXEL_FORMATS['mono8']
        carrier = self.carrier('mono8', 1)
        items, _, encoder = self.codecs.encode('mono8', 1, '1.2.840.10008.1.2.4.50', copy.deepcopy(carrier))
        image, output = self.codecs.scratch('.jpg'), self.codecs.scratch('.dcm')
        image.write_bytes(items[0])
        self.command([self.bin/'img_to_dcm', image, output], timeout=60)
        ds = pydicom.dcmread(output)
        require(str(ds.file_meta.TransferSyntaxUID) == EXPLICIT, f'Syntax {ds.file_meta.TransferSyntaxUID}')
        require(str(ds.SOPClassUID) == SC_UID, f'SOP Class {ds.SOPClassUID}')
        require((int(ds.Rows), int(ds.Columns)) == (format_.rows, format_.columns), 'Image dimensions changed')
        actual = native_values(ds, format_, 1)
        return f'{encoder}; '+require_lossy_bound(actual, source_values(format_, 1), format_, '1.2.840.10008.1.2.4.50')

    def charset_case(self, sample):
        """Text must reach UTF-8 intact, and conversion must leave the stored bytes alone."""
        declared, expected, raw = sample
        ds = self.carrier('mono8', 1)
        ds.SpecificCharacterSet = declared
        ds.PatientName = 'PLACEHOLDER'
        source, output, json_path, back = [self.codecs.scratch(s) for s in ('.dcm', '.dcm', '.json', '.dcm')]
        ds.save_as(source, write_like_original=False)
        # The bytes come from Python codecs and PS3.5 Annex H, not from any DICOM writer.
        value = raw+b' ' if len(raw) % 2 else raw
        original = b'\x10\x00\x10\x00PN\x0c\x00PLACEHOLDER '
        data = source.read_bytes()
        require(original in data, 'Patient Name element not found')
        source.write_bytes(data.replace(original, b'\x10\x00\x10\x00PN'+len(value).to_bytes(2, 'little')+value))
        self.command([self.bin/'dcm_conv', source, output, '--explicit', '--no-progress'], timeout=60)
        require(value in output.read_bytes(), 'Conversion changed the stored bytes')
        self.command([self.bin/'dcm_to_json', source, json_path, '--compact', '--no-meta'], timeout=60)
        document = json.loads(json_path.read_bytes().decode('utf-8'))
        document = document[0] if isinstance(document, list) else document
        name = document['00100010']['Value'][0]
        shown = name.get('Alphabetic', '') if isinstance(name, dict) else str(name)
        require(shown == expected, f'Name decoded as {shown!r}, expected {expected!r}')
        self.command([self.bin/'json_to_dcm', json_path, back], timeout=60)
        round_trip = pydicom.dcmread(back)
        require(str(round_trip.PatientName) == expected, f'JSON round trip read {round_trip.PatientName!r}')
        return f'{declared} reached UTF-8 intact and survived the JSON round trip'

    def files_all(self):
        for name, uid, flag in (('implicit-le', IMPLICIT, '--implicit'), ('explicit-le', EXPLICIT, '--explicit')):
            self.case(f'corpus.file.native.{name}', lambda u=uid, f=flag: self.native_roundtrip(u, f), requires=['corpus.preflight'])
        self.case('corpus.file.json.roundtrip', self.json_roundtrip, requires=['corpus.preflight'])
        self.case('corpus.file.xml.roundtrip', self.xml_roundtrip, requires=['corpus.preflight'])
        self.case('corpus.file.img-to-dcm', self.image_to_dicom, requires=['corpus.preflight'])
        def escaped(prefix, codec, *components):
            return b'^'.join(prefix+component.encode(codec) for component in components)
        samples = {
            'ascii': ('ISO_IR 6', 'CORPUS^ASCII', b'CORPUS^ASCII'),
            'latin1': ('ISO_IR 100', 'M\u00fcller^Fran\u00e7ois', 'M\u00fcller^Fran\u00e7ois'.encode('latin1')),
            'utf8': ('ISO_IR 192', '\ud55c\uae00^\u6f22\u5b57', '\ud55c\uae00^\u6f22\u5b57'.encode('utf-8')),
            'korean': (['', 'ISO 2022 IR 149'], '\ud64d^\uae38\ub3d9',
                       escaped(b'\x1b$)C', 'euc_kr', '\ud64d', '\uae38\ub3d9')),
            'japanese': (['', 'ISO 2022 IR 87'], '\u5c71\u7530^\u592a\u90ce',
                         b'^'.join(part.encode('iso2022_jp') for part in ('\u5c71\u7530', '\u592a\u90ce'))),
            'katakana': ('ISO_IR 13', '\uff94\uff8f\uff80\uff9e', '\uff94\uff8f\uff80\uff9e'.encode('cp932')),
            'katakana-extended': (['', 'ISO 2022 IR 13'], '\uff94\uff8f\uff80\uff9e',
                                  b'\x1b)I'+'\uff94\uff8f\uff80\uff9e'.encode('cp932')),
            'chinese': (['', 'ISO 2022 IR 58'], '\u738b^\u5c0f\u4e1c',
                        escaped(b'\x1b$)A', 'gb2312', '\u738b', '\u5c0f\u4e1c')),
        }
        for name, sample in samples.items():
            self.case(f'corpus.file.charset.{name}', lambda s=sample: self.charset_case(s), requires=['corpus.preflight'])
        self.malformed_all()

    def malformed_all(self):
        """Invalid input must fail cleanly: no signal, no output and no hang."""
        carrier = self.carrier('mono16u', 3)
        item_bytes = {}

        def stream(uid):
            if uid not in item_bytes:
                item_bytes[uid] = self.codecs.encode('mono16u', 3, uid, copy.deepcopy(carrier))
            return item_bytes[uid]

        def rewrite(uid, transform):
            items, attributes, _ = stream(uid)
            path = self.codecs.scratch('.dcm')
            save_any(encapsulated(copy.deepcopy(carrier), items, attributes, uid), path)
            data = bytearray(path.read_bytes())
            start = data.rindex(b'\xe0\x7f\x10\x00')
            transform(data, start)
            path.write_bytes(bytes(data))
            return path

        def trim_end(data, start):
            del data[-40:]

        def drop_delimiter(data, start):
            del data[-8:]

        def huge_item(data, start):
            first = data.index(b'\xfe\xff\x00\xe0', start+12)
            data[first+4:first+8] = (0x7FFFFFF0).to_bytes(4, 'little')

        def wrong_frame_offset(data, start):
            first = data.index(b'\xfe\xff\x00\xe0', start+12)
            data[first+8+4:first+8+8] = (0x00FFFFF0).to_bytes(4, 'little')

        def corrupt_stream(data, start):
            first = data.index(b'\xfe\xff\x00\xe0', start+12)
            second = first+8+int.from_bytes(data[first+4:first+8], 'little')
            for index in range(second+8, min(second+72, len(data)-8)):
                data[index] ^= 0xA5

        cases = {'truncated-file': trim_end, 'missing-sequence-delimiter': drop_delimiter, 'item-length-past-end': huge_item,
                 'frame-offset-past-end': wrong_frame_offset, 'corrupt-codestream': corrupt_stream}
        for syntax in COMPRESSED_SYNTAXES:
            if syntax.lossy:
                continue
            for name, transform in cases.items():
                self.case(f'corpus.malformed.{syntax.key}.{name}',
                          lambda s=syntax, t=transform, n=name: self.malformed_case(rewrite(s.uid, t), s, n),
                          requires=['corpus.preflight'])
        self.case('corpus.malformed.not-dicom', self.malformed_not_dicom, requires=['corpus.preflight'])
        for name in ('truncated-value', 'truncated-header', 'value-length-past-end', 'sequence-item-not-an-item',
                     'sequence-missing-delimiter', 'item-missing-delimiter'):
            self.case(f'corpus.malformed.native.{name}', lambda n=name: self.malformed_native(n), requires=['corpus.preflight'])

    def malformed_case(self, path, syntax, name):
        target = self.codecs.scratch('.dcm')
        self.command([self.bin/'dcm_conv', path, target, '--explicit', '--no-progress'], timeout=30, allowed=(1, 2))
        require(not target.exists(), 'Malformed input left an output file')
        return 'dcm_conv failed cleanly with no output'

    def malformed_native(self, name):
        """Broken native files, built byte by byte. Conversion must not invent content."""
        ds = self.carrier('mono8', 1)
        del ds.PixelData
        ds.ContentSequence = sequence(item(TextValue='deep'))
        ds['ContentSequence'].is_undefined_length = True
        ds.ContentSequence[0].is_undefined_length_sequence_item = True
        path, target = self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')
        ds.save_as(path, write_like_original=False)
        data = bytearray(path.read_bytes())
        sequence_at = data.index(b'\x40\x00\x30\xa7SQ\x00\x00\xff\xff\xff\xff')
        item_at = sequence_at+12
        require(data[item_at:item_at+4] == b'\xfe\xff\x00\xe0', 'Fixture layout changed')
        item_end = data.index(b'\xfe\xff\x0d\xe0', item_at)
        sequence_end = data.index(b'\xfe\xff\xdd\xe0', item_at)
        patient_at = data.index(b'\x10\x00\x10\x00PN')
        if name == 'truncated-value':
            del data[patient_at+12:]
        elif name == 'truncated-header':
            del data[patient_at+5:]
        elif name == 'value-length-past-end':
            data[patient_at+6:patient_at+8] = (0xFFF0).to_bytes(2, 'little')
        elif name == 'sequence-item-not-an-item':
            data[item_at:item_at+4] = b'\x08\x00\x18\x00'
        elif name == 'sequence-missing-delimiter':
            del data[sequence_end:sequence_end+8]
            del data[sequence_end:]
        else:
            del data[item_end:item_end+8]
            del data[item_end:]
        path.write_bytes(bytes(data))
        self.command([self.bin/'dcm_conv', path, target, '--explicit', '--no-progress'], timeout=30, allowed=(1, 2))
        require(not target.exists(), 'Malformed input left an output file')
        return 'dcm_conv failed cleanly with no output'

    def malformed_not_dicom(self):
        path, target = self.codecs.scratch('.dcm'), self.codecs.scratch('.dcm')
        path.write_bytes(b'not a DICOM file')
        self.command([self.bin/'dcm_conv', path, target, '--explicit', '--no-progress'], timeout=30, allowed=(1, 2))
        require(not target.exists(), 'Invalid input left an output file')
        return 'dcm_conv failed cleanly with no output'

    # Report --------------------------------------------------------------------------------

    def run_all(self, groups):
        self.case('corpus.preflight', self.preflight_tools, phase='peer_preparation')
        if 'fixtures' in groups or 'wire' in groups or 'negative' in groups:
            self.fixtures_all()
        if 'wire' in groups:
            self.wire_all()
            self.wire_server()
        if 'negative' in groups:
            self.negatives()
        if 'codecs' in groups:
            self.codecs_all()
        if 'unsupported' in groups:
            self.unsupported_all()
        if 'files' in groups:
            self.files_all()
        self.final = True
        self.save()
        counts = {status: sum(1 for row in self.results if row['status'] == status) for status in ('passed', 'failed', 'blocked')}
        by_prefix = {}
        for row in self.results:
            prefix = '.'.join(row['id'].split('.')[:3])
            bucket = by_prefix.setdefault(prefix, dict(passed=0, failed=0, blocked=0))
            bucket[row['status']] += 1
        declared = declared_support(Path(__file__).resolve().parents[3])
        requirement_map = requirement_cases(declared)
        summary = dict(counts=counts, groups=by_prefix, requirement_map=requirement_map,
                       requirement_results=self.requirement_results(requirement_map),
                       failures=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'failed'],
                       blocked=[row['id'] for row in self.results if row['status'] == 'blocked'],
                       unvalidated_iods=[row['sop'] for row in self.fixture_meta if row['iod_validator'] != 'dciodvfy'])
        (self.output/'corpus-summary.json').write_text(json.dumps(summary, indent=1)+'\n')
        print(json.dumps(counts))
        return int(counts['failed'] > 0 or counts['blocked'] > 0)

    def requirement_results(self, requirement_map):
        results = {}
        ids = [row['id'] for row in self.results]
        status = {row['id']: row['status'] for row in self.results}
        for requirement, pattern in requirement_map.items():
            matched = requirement_matches(pattern, ids)
            results[requirement] = dict(pattern=pattern, cases=len(matched),
                                        passed=sum(status[case] == 'passed' for case in matched),
                                        status='unexecuted' if not matched else
                                        'passed' if all(status[case] == 'passed' for case in matched) else 'failed')
        return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--source-digest', required=True)
    parser.add_argument('--groups', default='fixtures,wire,negative,codecs,unsupported,files')
    args = parser.parse_args()
    args.profile = 'uncompressed'
    return Corpus(args).run_all(set(args.groups.split(',')))


if __name__ == '__main__':
    raise SystemExit(main())
