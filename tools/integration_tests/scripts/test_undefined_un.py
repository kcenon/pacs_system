#!/usr/bin/env python3
"""Independent PS3.5 6.2.2 undefined-length UN fixtures and CLI validation.
Usage: test_undefined_un.py BUILD_DIR NEW_EVIDENCE_DIR
Requires pydicom and DCMTK dcmdump/dcmconv. All data are synthetic.
"""
import io
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import unittest

import pydicom
from pydicom.dataset import FileMetaDataset
from pydicom.filewriter import write_file_meta_info

UN = (0x0011, 0x1010)
NESTED = (0x0008, 0x1115)
RAW = b'\xfe\xff\xdd\xe0\x00\x00\x00\x00'
CLASS = '1.2.840.10008.5.1.4.1.1.2'
INSTANCE = '1.2.826.0.1.3680043.10.1302.1'
WIDE = {'OB', 'OD', 'OF', 'OL', 'OV', 'OW', 'SQ', 'SV', 'UV', 'UN', 'UC', 'UR', 'UT'}


def explicit(tag, vr, value, endian, length=None):
    n = len(value) if length is None else length
    return struct.pack(endian+'HH', *tag)+vr.encode()+(
        b'\0\0'+struct.pack(endian+'I', n) if vr in WIDE else struct.pack(endian+'H', n))+value


def implicit(tag, value, length=None):
    return struct.pack('<HHI', *tag, len(value) if length is None else length)+value


def delimiter(element, length=0):
    return struct.pack('<HHI', 0xfffe, element, length)


def item(value, undefined):
    return implicit((0xfffe, 0xe000), value, 0xffffffff if undefined else None)+(
        delimiter(0xe00d) if undefined else b'')


def value():
    nested = item(implicit((0x0010, 0x0010), b'NESTED^ONE')+
                  implicit((0x0028, 0x0010), b'\x02\x01'), True)+delimiter(0xe0dd)
    # The unknown defined-length bytes contain a fake sequence delimiter.
    first = implicit(NESTED, nested, 0xffffffff)+implicit((0x0010, 0x0010), b'ITEM^ONE')
    first += implicit((0x0028, 0x0010), b'\x04\x03')+implicit((0x7777, 0x1010), RAW)
    second = implicit((0x0010, 0x0010), b'ITEM^TWO')+implicit((0x0028, 0x0010), b'\x06\x05')
    return item(first, False)+item(second, True)+item(b'', False)+delimiter(0xe0dd)


def file_bytes(endian, un_value=None, defined=False):
    meta = FileMetaDataset()
    meta.MediaStorageSOPClassUID, meta.MediaStorageSOPInstanceUID = CLASS, INSTANCE
    meta.TransferSyntaxUID = '1.2.840.10008.1.2.1' if endian == '<' else '1.2.840.10008.1.2.2'
    meta.ImplementationClassUID = '1.2.826.0.1.3680043.10.1302'
    stream = io.BytesIO()
    write_file_meta_info(stream, meta)
    def text(tag, vr, text):
        raw = text.encode()
        if len(raw) % 2:
            raw += b'\0' if vr == 'UI' else b' '
        return explicit(tag, vr, raw, endian)
    body = text((8, 0x16), 'UI', CLASS)+text((8, 0x18), 'UI', INSTANCE)
    body += text((0x0011, 0x0010), 'LO', 'UN_SEQUENCE')
    body += explicit(UN, 'UN', RAW if defined else value() if un_value is None else un_value,
                     endian, None if defined else 0xffffffff)
    body += text((0x0020, 0x0013), 'IS', '17')
    body += explicit((0x0028, 0x0010), 'US', struct.pack(endian+'H', 0x0708), endian)
    body += text((0x7776, 0x0010), 'LO', 'AFTER_UN')
    body += explicit((0x7fe0, 0x0010), 'OW', struct.pack(endian+'HH', 0x0102, 0x0304), endian)
    return bytes(128)+b'DICM'+stream.getvalue()+body


def malformed_values():
    good = value()
    first_length = struct.unpack_from('<I', good, 4)[0]
    second = 8+first_length
    return {
        'missing-sequence-delimiter': good[:-8],
        'truncated-sequence-delimiter': good[:-4],
        'nonzero-sequence-delimiter': good[:-4]+struct.pack('<I', 2),
        'wrong-item-tag': b'\xfe\xff\x01\xe0'+good[4:],
        'truncated-item-header': good[:4],
        'oversized-item': good[:4]+struct.pack('<I', 0xfffffffe)+good[8:],
        'defined-item-truncated-value': item(implicit((0x0010, 0x0010), b'AB', 20), False)+delimiter(0xe0dd),
        'undefined-item-truncated-value': item(implicit((0x0010, 0x0010), b'AB', 1000), True)+delimiter(0xe0dd),
        'missing-item-delimiter': item(b'', True)[:-8]+delimiter(0xe0dd),
        'nonzero-item-delimiter': item(b'', True)[:-4]+struct.pack('<I', 2)+delimiter(0xe0dd),
        'delimiter-in-defined-item': item(delimiter(0xe00d), False)+delimiter(0xe0dd),
        'partial-element-header': item(b'\x10\x00\x10\x00', False)+delimiter(0xe0dd),
        'bad-defined-nested-sequence': item(implicit(NESTED, b'NOTITEM!'), False)+delimiter(0xe0dd),
        'bad-undefined-nested-sequence': item(implicit(NESTED, item(b'', True), 0xffffffff), False)+delimiter(0xe0dd),
        'odd-item-value': item(implicit((0x0010, 0x0010), b'A'), False)+delimiter(0xe0dd),
        'second-item-missing-delimiter': good[:second]+item(implicit((0x0010, 0x0010), b'ITEM^TWO'), True)[:-8]+delimiter(0xe0dd),
    }


class UndefinedUN(unittest.TestCase):
    def setUp(self):
        self.root = OUTPUT/self._testMethodName
        self.root.mkdir()
        self.commands = []

    def command(self, argv, expected=0):
        argv = list(map(str, argv))
        r = subprocess.run(argv, capture_output=True, text=True, timeout=20)
        n = len(self.commands)
        self.commands.append(dict(argv=argv, exit=r.returncode, expected=expected))
        (self.root/f'command-{n}.stdout').write_text(r.stdout)
        (self.root/f'command-{n}.stderr').write_text(r.stderr)
        (self.root/'commands.json').write_text(json.dumps(self.commands, indent=2)+'\n')
        self.assertEqual(r.returncode, expected, r.stdout+r.stderr)
        return r

    def inspect(self, path, defined=False):
        ds = pydicom.dcmread(path)
        self.assertEqual(int(ds.InstanceNumber), 17)
        self.assertEqual(ds.Rows, 0x0708)
        self.assertEqual(str(ds[(0x7776, 0x0010)].value), 'AFTER_UN')
        self.assertEqual(struct.unpack(('<' if ds.is_little_endian else '>')+'HH', ds.PixelData), (0x0102, 0x0304))
        if defined:
            self.assertEqual(ds[UN].VR, 'UN')
            self.assertEqual(ds[UN].value, RAW)
            return
        self.assertEqual(ds[UN].VR, 'SQ')
        items = ds[UN].value
        self.assertEqual(len(items), 3)
        self.assertEqual(str(items[0].PatientName), 'ITEM^ONE')
        self.assertEqual(items[0].Rows, 0x0304)
        self.assertEqual(items[0][(0x7777, 0x1010)].value, RAW)
        self.assertEqual(str(items[1].PatientName), 'ITEM^TWO')
        self.assertEqual(items[1].Rows, 0x0506)
        self.assertEqual(len(items[2]), 0)
        nested = items[0][NESTED].value
        self.assertEqual(len(nested), 1)
        self.assertEqual(str(nested[0].PatientName), 'NESTED^ONE')
        self.assertEqual(nested[0].Rows, 0x0102)

    def roundtrip(self, endian):
        path = self.root/'source.dcm'
        path.write_bytes(file_bytes(endian))
        # pydicom 2.4 cannot directly frame a BE outer UN value. DCMTK's
        # independent CP-246 decoder validates that source, then pydicom checks
        # the normalized explicit SQ output. Preserve the original wire bytes.
        if endian == '<':
            self.inspect(path)
        dump = self.command(['dcmdump', path])
        for expected in ('ITEM^ONE', 'ITEM^TWO', 'NESTED^ONE', 'AFTER_UN', '1800'):
            self.assertIn(expected, dump.stdout)
        oracle = self.root/'dcmtk.dcm'
        self.command(['dcmconv', '+te', path, oracle])
        self.inspect(oracle)
        for flag in (('--explicit-be',) if endian == '<' else ('--explicit',)):
            with self.subTest(output=flag):
                output = self.root/(flag+'.dcm')
                self.command([BINARY, path, output, flag])
                self.inspect(output)
                self.command(['dcmdump', output])
                restored = self.root/(flag+'-restored.dcm')
                self.command([BINARY, output, restored, '--explicit' if endian == '<' else '--explicit-be'])
                self.inspect(restored)

    def test_explicit_little_endian_undefined_un(self):
        self.roundtrip('<')

    def test_explicit_big_endian_undefined_un(self):
        self.roundtrip('>')

    def test_defined_un_remains_opaque_in_both_byte_orders(self):
        for endian in ('<', '>'):
            for flag in ('--explicit', '--explicit-be'):
                with self.subTest(input=endian, output=flag):
                    source = self.root/(endian+flag+'-source.dcm')
                    source.write_bytes(file_bytes(endian, defined=True))
                    output = self.root/(endian+flag+'.dcm')
                    self.command([BINARY, source, output, flag])
                    self.inspect(output, defined=True)

    def malformed(self, endian):
        for name, payload in malformed_values().items():
            with self.subTest(name=name):
                source, output = self.root/(name+'.dcm'), self.root/(name+'-out.dcm')
                source.write_bytes(file_bytes(endian, payload))
                result = self.command([BINARY, source, output, '--explicit'], 2)
                self.assertRegex(result.stderr, '(sequence|Sequence|item|Item|Truncated|Incomplete|Odd)')
                self.assertFalse(output.exists())

    def test_malformed_little_endian_un_fails(self):
        self.malformed('<')

    def test_malformed_big_endian_un_fails(self):
        self.malformed('>')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    BINARY = Path(sys.argv[1]).resolve()/'bin/dcm_conv'
    OUTPUT = Path(sys.argv[2]).resolve()
    for tool in (str(BINARY), 'dcmdump', 'dcmconv'):
        if not shutil.which(tool):
            sys.exit('Missing required binary: '+tool)
    OUTPUT.mkdir(parents=True, exist_ok=False)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(UndefinedUN))
    (OUTPUT/'results.json').write_text(json.dumps(dict(tests=result.testsRun,
        failed=len(result.failures)+len(result.errors), skipped=len(result.skipped)), indent=2)+'\n')
    sys.exit(0 if result.wasSuccessful() and result.testsRun == 5 and not result.skipped else 1)
