#!/usr/bin/env python3
"""Independent raw/reader/CLI Big Endian oracles; synthetic, not full IOD conformance.

Usage: test_big_endian_values.py BUILD_DIR NEW_EVIDENCE_DIR
Requires pydicom, numpy and DCMTK dump2dcm/dcmconv/dcmdump/dcm2xml.
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

LE, BE = '1.2.840.10008.1.2.1', '1.2.840.10008.1.2.2'
CLASS, INSTANCE = '1.2.840.10008.5.1.4.1.1.2', '1.2.3.1293.10'
WIDE = {'OB', 'OD', 'OF', 'OL', 'OV', 'OW', 'SQ', 'SV', 'UV', 'UN', 'UC', 'UR', 'UT'}
# Independent typed values; struct packs each representation, never PACS output.
VALUES = [
    ('US', 'H', [0x0102, 0x0304]), ('SS', 'h', [-2, -32768]),
    ('UL', 'I', [0x01020304, 0x55667788]), ('SL', 'i', [-2, -2147483648]),
    ('FL', 'f', [1., -2.]), ('FD', 'd', [1., -2.]),
    ('SV', 'q', [-2, 0x0102030405060708]), ('UV', 'Q', [0x0102030405060708, 0x1112131415161718]),
    ('AT', 'H', [0x0018, 0x1063, 0x0028, 0x0010]), ('OW', 'H', [0x0102, 0x0304]),
    ('OL', 'I', [0x01020304, 0x55667788]), ('OF', 'f', [1., -2.]),
    ('OD', 'd', [1., -2.]), ('OV', 'Q', [0x0102030405060708, 0x1112131415161718]),
    ('OB', 'B', [1, 2, 3, 4]), ('PN', 'B', [65, 66, 67, 68]),
]


def element(tag, vr, value, endian):
    prefix = struct.pack(endian+'HH', *tag)+vr.encode()
    return prefix+(b'\0\0'+struct.pack(endian+'I', len(value)) if vr in WIDE else
                   struct.pack(endian+'H', len(value)))+value


def text(tag, vr, value, endian):
    value = value.encode()
    if len(value) % 2:
        value += b'\0' if vr == 'UI' else b' '
    return element(tag, vr, value, endian)


def sequence(content, endian, undefined):
    item = struct.pack(endian+'HHI', 0xfffe, 0xe000, 0xffffffff if undefined else len(content))+content
    if undefined:
        item += struct.pack(endian+'HHI', 0xfffe, 0xe00d, 0)
    header = struct.pack(endian+'HH', 0x0008, 0x1115)+b'SQ\0\0'
    return header+struct.pack(endian+'I', 0xffffffff if undefined else len(item))+item+(
        struct.pack(endian+'HHI', 0xfffe, 0xe0dd, 0) if undefined else b'')


def file_bytes(payload, endian):
    meta = FileMetaDataset()
    meta.MediaStorageSOPClassUID = CLASS
    meta.MediaStorageSOPInstanceUID = INSTANCE
    meta.TransferSyntaxUID = LE if endian == '<' else BE
    meta.ImplementationClassUID = '1.2.3.1293'
    stream = io.BytesIO()
    write_file_meta_info(stream, meta)
    return bytes(128)+b'DICM'+stream.getvalue()+payload


def fixture(endian, undefined=False):
    values = b''.join(element((0x7776, 0x1000+i), vr, struct.pack(endian+fmt*len(data), *data), endian)
                      for i, (vr, fmt, data) in enumerate(VALUES))
    identifiers = text((8, 0x0016), 'UI', CLASS, endian)+text((8, 0x0018), 'UI', INSTANCE, endian)
    return file_bytes(identifiers+sequence(sequence(values, endian, not undefined), endian, undefined)+values, endian)


class BigEndianValues(unittest.TestCase):
    def setUp(self):
        self.output = OUTPUT/self._testMethodName
        self.output.mkdir()
        self.commands = []

    def command(self, args, expected=0):
        args = list(map(str, args))
        result = subprocess.run(args, capture_output=True, text=True, timeout=20)
        self.commands.append(dict(argv=args, exit=result.returncode, expected=expected))
        index = len(self.commands)
        (self.output/f'command-{index}.stdout').write_text(result.stdout)
        (self.output/f'command-{index}.stderr').write_text(result.stderr)
        (self.output/'commands.json').write_text(json.dumps(self.commands, indent=2)+'\n')
        self.assertEqual(result.returncode, expected, result.stdout+result.stderr)
        return result

    def inspect(self, path, endian):
        raw = path.read_bytes()
        parsed = pydicom.dcmread(path)
        self.assertEqual(str(parsed.file_meta.TransferSyntaxUID), BE if endian == '>' else LE)
        # Both the root and twice-nested values must be independently readable.
        inner = parsed.ReferencedSeriesSequence[0].ReferencedSeriesSequence[0]
        for index, (vr, fmt, values) in enumerate(VALUES):
            encoded = struct.pack(endian+fmt*len(values), *values)
            self.assertEqual(raw.count(element((0x7776, 0x1000+index), vr, encoded, endian)), 2, vr)
            for dataset in (parsed, inner):
                item = dataset[(0x7776, 0x1000+index)]
                self.assertEqual(item.VR, vr)
                if vr.startswith('O'):
                    self.assertEqual(struct.unpack(endian+fmt*len(values), item.value), tuple(values), vr)
                elif vr == 'AT':
                    self.assertEqual(list(map(int, item.value)), [0x00181063, 0x00280010])
                elif vr == 'PN':
                    self.assertEqual(str(item.value), 'ABCD')
                else:
                    self.assertEqual(list(item.value), values, vr)
        self.command(['dcmdump', path])
        self.command(['dcm2xml', '+M', path])

    def test_independent_le_to_be_to_le_all_vrs_and_sequences(self):
        for undefined in (False, True):
            with self.subTest(undefined=undefined):
                source = self.output/f'input-{undefined}.dcm'
                source.write_bytes(fixture('<', undefined))
                be, le = self.output/f'big-{undefined}.dcm', self.output/f'little-{undefined}.dcm'
                self.command([BINARY, source, be, '--explicit-be'])
                self.inspect(be, '>')
                self.command([BINARY, be, le, '--explicit'])
                self.inspect(le, '<')

    def test_independent_big_endian_input_to_little_endian(self):
        for undefined in (False, True):
            with self.subTest(undefined=undefined):
                source = self.output/f'independent-big-{undefined}.dcm'
                source.write_bytes(fixture('>', undefined))
                self.inspect(source, '>')
                target = self.output/f'little-{undefined}.dcm'
                self.command([BINARY, source, target, '--explicit'])
                self.inspect(target, '<')

    def pixels(self, bits, vr):
        source = self.output/'source.dump'
        pixels = '0102\\0304\\0506\\0708' if bits == 16 else ('0102\\0304' if vr == 'OW' else '02\\01\\04\\03')
        source.write_text(f'(0008,0016) UI [{CLASS}]\n(0008,0018) UI [{INSTANCE}]\n'
            '(0028,0002) US 1\n(0028,0004) CS [MONOCHROME2]\n(0028,0010) US 1\n(0028,0011) US 4\n'+
            f'(0028,0100) US {bits}\n(0028,0101) US {bits}\n(0028,0102) US {bits-1}\n(0028,0103) US 0\n'+
            f'(7fe0,0010) {vr} {pixels}\n')
        original, independent = self.output/'source-le.dcm', self.output/'dcmtk-be.dcm'
        self.command(['dump2dcm', '+te', source, original])
        self.command(['dump2dcm', '+tb', source, independent])
        target, restored, oracle = self.output/'pacs-be.dcm', self.output/'pacs-le.dcm', self.output/'dcmtk-le.dcm'
        self.command([BINARY, original, target, '--explicit-be'])
        source_ds, big_ds, independent_ds = (pydicom.dcmread(path) for path in (original, target, independent))
        self.assertEqual(big_ds.PixelData, independent_ds.PixelData)
        if vr == 'OW':
            self.assertEqual(big_ds.PixelData[:2], b'\x01\x02')
        else:
            self.assertEqual(big_ds.PixelData, source_ds.PixelData)
        self.assertEqual(big_ds.pixel_array.tolist(), source_ds.pixel_array.tolist())
        self.command([BINARY, target, restored, '--explicit'])
        self.command(['dcmconv', '+te', target, oracle])
        self.assertEqual(pydicom.dcmread(restored).PixelData, source_ds.PixelData)
        self.assertEqual(pydicom.dcmread(oracle).PixelData, source_ds.PixelData)
        self.command([BINARY, independent, self.output/'independent-to-le.dcm', '--explicit'])
        self.assertEqual(pydicom.dcmread(self.output/'independent-to-le.dcm').PixelData, source_ds.PixelData)

    def test_dcmtk_16_bit_ow_pixels(self):
        self.pixels(16, 'OW')

    def test_dcmtk_8_bit_ow_still_swaps_words(self):
        self.pixels(8, 'OW')

    def test_dcmtk_8_bit_ob_is_unchanged(self):
        self.pixels(8, 'OB')

    def test_malformed_binary_lengths_fail_without_output(self):
        for index, (vr, fmt, values) in enumerate(VALUES[:14]):
            payload = struct.pack('>'+fmt*len(values), *values)
            payload = payload[:-2 if vr == 'AT' else -1]
            raw = element((0x7776, 0x1000+index), vr, payload, '>')
            for label, value in [('root', raw), ('defined', sequence(raw, '>', False)),
                                 ('undefined', sequence(raw, '>', True))]:
                with self.subTest(vr=vr, depth=label):
                    source, output = self.output/f'{vr}-{label}.dcm', self.output/f'{vr}-{label}-out.dcm'
                    source.write_bytes(file_bytes(value, '>'))
                    result = self.command([BINARY, source, output, '--explicit'], expected=2)
                    self.assertIn('Incomplete binary value', result.stderr)
                    self.assertFalse(output.exists())


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    BINARY = Path(sys.argv[1]).resolve()/'bin/dcm_conv'
    OUTPUT = Path(sys.argv[2]).resolve()
    for binary in (str(BINARY), 'dump2dcm', 'dcmconv', 'dcmdump', 'dcm2xml'):
        if not shutil.which(binary):
            sys.exit('Missing required binary: '+binary)
    OUTPUT.mkdir(parents=True, exist_ok=False)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(BigEndianValues)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    (OUTPUT/'results.json').write_text(json.dumps(dict(tests=result.testsRun, failed=len(result.failures)+len(result.errors),
        skipped=len(result.skipped)), indent=2)+'\n')
    sys.exit(0 if result.wasSuccessful() and result.testsRun == 6 and not result.skipped else 1)
