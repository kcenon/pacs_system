#!/usr/bin/env python3
"""Independent DICOMDIR graph/offset/CLI checks (pydicom and DCMTK required).
Usage: test_dicomdir_offsets.py BUILD_DIR NEW_EVIDENCE_DIR
"""
import copy
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import unittest

import pydicom
from pydicom.dataset import Dataset, FileDataset, FileMetaDataset
from pydicom.fileset import FileSet
from pydicom.filewriter import dcmwrite
from pydicom.uid import CTImageStorage, ExplicitVRLittleEndian

FIRST, LAST, NEXT, LOWER = 0x00041200, 0x00041202, 0x00041400, 0x00041420


def source(path, patient, study, series, image):
    uid = f'1.2.826.0.1.3680043.10.1301.{patient}.{study}.{series}.{image}'
    meta = FileMetaDataset()
    meta.MediaStorageSOPClassUID = CTImageStorage
    meta.MediaStorageSOPInstanceUID = uid
    meta.TransferSyntaxUID = ExplicitVRLittleEndian
    ds = FileDataset(str(path), {}, file_meta=meta, preamble=bytes(128))
    ds.is_little_endian, ds.is_implicit_VR = True, False
    ds.SOPClassUID, ds.SOPInstanceUID = CTImageStorage, uid
    ds.PatientID, ds.PatientName = f'P{patient}', f'PATIENT^{patient}'
    ds.PatientBirthDate, ds.PatientSex = '', ''
    ds.StudyInstanceUID, ds.SeriesInstanceUID = uid.rsplit('.', 2)[0], uid.rsplit('.', 1)[0]
    ds.StudyDate, ds.StudyTime, ds.StudyID = '20261101', '120000', f'S{study}'
    ds.StudyDescription, ds.AccessionNumber = f'STUDY {study}', ''
    ds.Modality, ds.SeriesNumber, ds.InstanceNumber = 'CT', series, image
    ds.Rows, ds.Columns, ds.SamplesPerPixel = 1, 2, 1
    ds.PhotometricInterpretation = 'MONOCHROME2'
    ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 16, 16, 15, 0
    ds.PixelData = b'\x01\x00\x02\x00'
    ds.save_as(path, write_like_original=False)
    return ds


def linked_write(ds, path, physical_order=None, undefined=False):
    """Independent pydicom encoder; rebase existing graph links after rearranging Items."""
    ds = copy.deepcopy(ds)
    records = list(ds.DirectoryRecordSequence)
    original_offsets = [r.seq_item_tell for r in records]
    if physical_order is not None:
        records = [records[i] for i in physical_order]
        original_offsets = [original_offsets[i] for i in physical_order]
        ds.DirectoryRecordSequence = records
    for record in records:
        record.is_undefined_length_sequence_item = undefined
    ds[0x00041220].is_undefined_length = undefined
    root_links = [int(ds[tag].value) for tag in (FIRST, LAST)]
    record_links = [[int(record[tag].value) for tag in (NEXT, LOWER)] for record in records]
    # pydicom 2.x eagerly follows DICOMDIR links when reading; zero placeholders
    # permit inspecting the first pass before rebasing the original graph.
    for tag in (FIRST, LAST):
        ds[tag].value = 0
    for record in records:
        for tag in (NEXT, LOWER):
            record[tag].value = 0
    stream = io.BytesIO()
    dcmwrite(stream, ds, write_like_original=False)
    parsed = pydicom.dcmread(io.BytesIO(stream.getvalue()))
    positions = {old: new.seq_item_tell for old, new in zip(original_offsets, parsed.DirectoryRecordSequence)}
    positions[0] = 0
    for tag, offset in zip((FIRST, LAST), root_links):
        ds[tag].value = positions[offset]
    for record, links in zip(records, record_links):
        for tag, offset in zip((NEXT, LOWER), links):
            record[tag].value = positions[offset]
    dcmwrite(path, ds, write_like_original=False)


class DirectoryOffsets(unittest.TestCase):
    def setUp(self):
        self.root = OUTPUT/self._testMethodName
        self.root.mkdir()
        self.commands = []

    def command(self, argv, expected=0):
        argv = list(map(str, argv))
        result = subprocess.run(argv, capture_output=True, text=True, timeout=20)
        n = len(self.commands)
        self.commands.append(dict(argv=argv, exit=result.returncode, expected=expected))
        (self.root/f'command-{n}.stdout').write_text(result.stdout)
        (self.root/f'command-{n}.stderr').write_text(result.stderr)
        (self.root/'commands.json').write_text(json.dumps(self.commands, indent=2)+'\n')
        self.assertEqual(result.returncode, expected, result.stdout+result.stderr)
        return result

    def corpus(self):
        folder = self.root/'FILES'
        folder.mkdir()
        # Deliberately reverse creation order so filesystem enumeration is not an oracle.
        for patient in (2, 1):
            for study in (2, 1):
                for series in (2, 1):
                    for image in (2, 1):
                        source(folder/f'P{patient}S{study}R{series}I{image}', patient, study, series, image)
        self.command([BINARY, 'create', folder, '-o', self.root/'DICOMDIR'])
        return self.root/'DICOMDIR'

    def inspect(self, path, count=16):
        raw = path.read_bytes()
        ds = pydicom.dcmread(path)
        self.assertEqual(str(ds.file_meta.MediaStorageSOPClassUID), '1.2.840.10008.1.3.10')
        self.assertTrue(ds.file_meta.MediaStorageSOPInstanceUID)
        self.assertNotIn('SOPClassUID', ds)
        self.assertNotIn('SOPInstanceUID', ds)
        self.assertEqual(raw[144+int(ds.file_meta.FileMetaInformationGroupLength):][:2], b'\x04\x00')
        records = {r.seq_item_tell: r for r in ds.DirectoryRecordSequence}
        for offset, record in records.items():
            self.assertEqual(raw[offset:offset+4], b'\xfe\xff\x00\xe0')
            for tag in (NEXT, LOWER):
                self.assertEqual(record[tag].VR, 'UL')
                if record[tag].value:
                    self.assertIn(record[tag].value, records)
        seen, roots, images = set(), [], []
        def walk(offset, level, ancestors=()):
            while offset:
                self.assertNotIn(offset, seen)
                seen.add(offset)
                record = records[offset]
                self.assertEqual(record.DirectoryRecordType, ['PATIENT', 'STUDY', 'SERIES', 'IMAGE'][level])
                if level == 0:
                    roots.append(offset)
                if level == 3:
                    images.append((ancestors[0].PatientID, ancestors[1].StudyInstanceUID,
                                   ancestors[2].SeriesInstanceUID, record.ReferencedSOPInstanceUIDInFile))
                walk(record[LOWER].value, level+1, ancestors+(record,))
                offset = record[NEXT].value
        walk(ds[FIRST].value, 0)
        self.assertEqual(len(seen), len(records))
        self.assertEqual(ds[LAST].value, roots[-1] if roots else 0)
        self.assertEqual(len(images), count)
        fs = FileSet(path)
        self.assertEqual(len(fs), count)
        for item in fs:
            self.assertEqual(item.load().SOPInstanceUID, item.SOPInstanceUID)
        self.command(['dcmdump', path])
        self.command([BINARY, 'verify', '--check-files', '--check-consistency', path])
        return ds, images

    def test_generated_offsets_order_metadata_and_fileset(self):
        path = self.corpus()
        ds, images = self.inspect(path)
        self.assertEqual(images, sorted(images))
        self.assertEqual(len(ds.DirectoryRecordSequence), 30)
        listing = self.command([BINARY, 'list', '--tree', '--long', path]).stdout
        self.assertLess(listing.index('PATIENT^1'), listing.index('PATIENT^2'))
        for p in (1, 2):
            for s in (1, 2):
                for r in (1, 2):
                    self.assertLess(listing.index(f'P{p}S{s}R{r}I1'), listing.index(f'P{p}S{s}R{r}I2'))
        # Same sources produce stable record order despite a fresh File-set UID.
        self.command([BINARY, 'create', self.root/'FILES', '-o', self.root/'SECOND'])
        _, second = self.inspect(self.root/'SECOND')
        self.assertEqual(images, second)

    def test_list_follows_links_in_shuffled_physical_records(self):
        path = self.corpus()
        before = self.command([BINARY, 'list', '--tree', path]).stdout
        ds = pydicom.dcmread(path)
        shuffled = self.root/'SHUFFLED'
        linked_write(ds, shuffled, list(reversed(range(len(ds.DirectoryRecordSequence)))))
        self.inspect(shuffled)
        after = self.command([BINARY, 'list', '--tree', shuffled]).stdout
        self.assertEqual(before[before.index('[PATIENT]'):], after[after.index('[PATIENT]'):])

    def test_independent_pydicom_filesets_defined_and_undefined(self):
        self.corpus()
        fs = FileSet()
        for path in sorted((self.root/'FILES').iterdir()):
            fs.add(path)
        independent = self.root/'ORACLE'
        fs.write(independent)
        path = independent/'DICOMDIR'
        original, _ = self.inspect(path)
        self.command([BINARY, 'list', '--tree', path])
        for undefined in (False, True):
            with self.subTest(undefined=undefined):
                target = independent/f'RECODE{int(undefined)}'
                linked_write(original, target, undefined=undefined)
                self.inspect(target)
                self.command([BINARY, 'list', '--tree', target])

    def test_independent_dcmtk_directory_lengths(self):
        self.corpus()
        names = [str(path.relative_to(self.root)) for path in sorted((self.root/'FILES').iterdir())]
        for encoding in ('+e', '-e'):
            with self.subTest(encoding=encoding):
                target = self.root/('DCMTKDEF' if encoding == '+e' else 'DCMTKUND')
                self.command(['dcmmkdir', '+id', self.root, '+D', target, encoding, *names])
                self.inspect(target)
                self.command([BINARY, 'list', '--tree', target])

    def test_empty_directories(self):
        empty = self.root/'EMPTY'
        empty.mkdir()
        self.command([BINARY, 'create', empty])
        self.inspect(empty/'DICOMDIR', 0)
        self.command([BINARY, 'list', empty/'DICOMDIR'])
        (empty/'BROKEN').write_bytes(b'not a DICOM file')
        self.command([BINARY, 'create', empty, '-o', 'INVALID'], 2)
        self.assertFalse((empty/'INVALID').exists())
        oracle = self.root/'ORACLE'
        oracle.mkdir()
        FileSet().write(oracle)
        self.inspect(oracle/'DICOMDIR', 0)

    def test_corrupt_offsets_are_rejected_by_list_and_verify(self):
        path = self.corpus()
        ds = pydicom.dcmread(path)
        records = ds.DirectoryRecordSequence
        raw = path.read_bytes()
        # Raw fixed-width mutations preserve every unrelated byte and offset.
        first = int(ds[FIRST].value)
        last = int(ds[LAST].value)
        def location(tag, start=132):
            signature = struct.pack('<HH', tag >> 16, tag & 65535)+b'UL\x04\x00'
            return raw.index(signature, start)+8
        changes = {
            'root-out-of-range': (location(FIRST), len(raw)+100),
            'root-not-item': (location(FIRST), first+8),
            'root-meta': (location(FIRST), 132),
            'next-self-cycle': (location(NEXT, first), first),
            'lower-cycle': (location(LOWER, first), first),
            'multiply-owned': (location(LOWER, last), int(records[0][LOWER].value)),
            'next-out-of-range': (location(NEXT, first), 0xfffffffe),
            'lower-not-item': (location(LOWER, first), first+4),
            'wrong-last-root': (location(LAST), first),
            'missing-last-root': (location(LAST), 0),
            'orphan-records': (location(LOWER, first), 0),
            'wrong-child-type': (location(LOWER, first), last),
            'zero-first-with-records': (location(FIRST), 0),
        }
        for name, (position, value) in changes.items():
            with self.subTest(name=name):
                bad = bytearray(raw)
                struct.pack_into('<I', bad, position, value)
                target = self.root/name
                target.write_bytes(bad)
                for command in ('list', 'verify'):
                    result = self.command([BINARY, command, target], 2)
                    self.assertRegex(result.stderr, 'DICOMDIR.*(link|root|record)|Unreachable')

    def test_corrupt_structure_and_metadata_are_rejected(self):
        path = self.corpus()
        raw = path.read_bytes()
        ds = pydicom.dcmread(path)
        first = ds.DirectoryRecordSequence[0].seq_item_tell
        for name, mutate in {
            'truncated-item': lambda b: b[:first+7],
            'truncated-value': lambda b: b[:-1],
            'item-size-overflow': lambda b: b[:first+4]+b'\xfe\xff\xff\xff'+b[first+8:],
            'item-tag': lambda b: b[:first]+b'\x00\x00'+b[first+2:],
            'odd-item-length': lambda b: b[:first+4]+b'\x01\x00\x00\x00'+b[first+8:],
        }.items():
            target = self.root/name
            target.write_bytes(mutate(raw))
            self.command([BINARY, 'verify', target], 2)
        for tag in (FIRST, LAST):
            bad = copy.deepcopy(ds)
            del bad[tag]
            target = self.root/f'missing-{tag}'
            # Removing the field invalidates offsets too, but absence must never pass.
            bad.save_as(target, write_like_original=False)
            self.command([BINARY, 'verify', target], 2)
        bad = copy.deepcopy(ds)
        bad.SOPClassUID, bad.SOPInstanceUID = '1.2.840.10008.1.3.10', '1.2.3'
        target = self.root/'DATASETSOP'
        linked_write(bad, target)
        self.command([BINARY, 'verify', target], 2)

    def test_filename_spelling_and_output_parent_are_preserved(self):
        # Filename profile validation is separate; the CLI must never silently
        # uppercase a real path or resolve it relative to the wrong directory.
        folder = self.root/'lowerdir'
        folder.mkdir()
        source(folder/'image.dcm', 1, 1, 1, 1)
        target = self.root/'DICOMDIR'
        self.command([BINARY, 'create', folder, '-o', target])
        ds = pydicom.dcmread(target)
        self.assertEqual(list(ds.DirectoryRecordSequence[-1].ReferencedFileID), ['lowerdir', 'image.dcm'])
        self.command([BINARY, 'verify', '--check-files', target])
        self.assertEqual(FileSet(target).__len__(), 1)

    def test_update_rebuilds_offsets(self):
        path = self.corpus()
        original, _ = self.inspect(path)
        added = self.root/'ADDED'
        added.mkdir()
        source(added/'P3S1R1I1', 3, 1, 1, 1)
        self.command([BINARY, 'update', '-a', added, path])
        ds, _ = self.inspect(path, 17)
        self.assertEqual(len(ds.DirectoryRecordSequence), 34)
        self.assertEqual(ds.file_meta.MediaStorageSOPInstanceUID, original.file_meta.MediaStorageSOPInstanceUID)
        self.assertEqual(ds.FileSetID, original.FileSetID)
        self.command([BINARY, 'list', path])
        # An empty File-set ID is Type 2 and must stay empty during update.
        empty = self.root/'EMPTYSET'
        empty.mkdir()
        FileSet().write(empty)
        original = pydicom.dcmread(empty/'DICOMDIR')
        added = empty/'ADDED'
        added.mkdir()
        source(added/'P4S1R1I1', 4, 1, 1, 1)
        self.command([BINARY, 'update', '-a', added, empty/'DICOMDIR'])
        updated, _ = self.inspect(empty/'DICOMDIR', 1)
        self.assertFalse(updated.FileSetID)
        self.assertEqual(updated.file_meta.MediaStorageSOPInstanceUID, original.file_meta.MediaStorageSOPInstanceUID)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    BINARY = Path(sys.argv[1]).resolve()/'bin/dcm_dir'
    OUTPUT = Path(sys.argv[2]).resolve()
    for tool in (str(BINARY), 'dcmdump', 'dcmmkdir'):
        if not shutil.which(tool):
            sys.exit('Missing required binary: '+tool)
    OUTPUT.mkdir(parents=True, exist_ok=False)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(DirectoryOffsets)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    (OUTPUT/'results.json').write_text(json.dumps(dict(tests=result.testsRun,
        failed=len(result.failures)+len(result.errors), skipped=len(result.skipped)), indent=2)+'\n')
    sys.exit(0 if result.wasSuccessful() and result.testsRun == 9 and not result.skipped else 1)
