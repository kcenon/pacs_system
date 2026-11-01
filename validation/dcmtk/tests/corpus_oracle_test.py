"""Run inside the validation image (requires pydicom and numpy): corpus oracles must reject bad output."""
import argparse
import copy
from pathlib import Path
import sys
import tempfile
import unittest
import warnings

import numpy as np
import pydicom

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
warnings.filterwarnings('ignore', category=UserWarning)
from corpus import LOSSY_BOUNDS, PIXEL_FORMATS, STORAGE_SOPS
from corpus_codecs import (Codecs, encapsulated, native_values, require_exact, require_lossy_bound, save_any,
                           source_values, stored_values, to_rgb)
from corpus_fixtures import BUILDERS, build_fixture, native_bytes, pixel_array
from corpus_probe import Corpus, dataset_signature, dictionary_tag


class CorpusOracles(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.corpus = Corpus(argparse.Namespace(output=str(self.root/'evidence'), build_dir=str(self.root),
                                                profile='uncompressed', source_revision='test', source_digest='test'))
        self.sop = next(sop for sop in STORAGE_SOPS if sop.family == 'ct')

    def written(self, name, ds):
        path = self.root/name
        ds.save_as(path, write_like_original=False)
        return path

    def test_every_family_builds_a_fixture_with_its_own_identifiers(self):
        self.assertEqual({sop.family for sop in STORAGE_SOPS} - set(BUILDERS), set())
        instances = set()
        for index, sop in enumerate(STORAGE_SOPS, 1):
            ds = build_fixture(sop, '2.25.1318', index)
            self.assertEqual(str(ds.SOPClassUID), sop.uid)
            instances.add(str(ds.SOPInstanceUID))
            if sop.pixel_format is None:
                self.assertNotIn('PixelData', ds, sop.name)
            elif sop.pixel_format == 'float32':
                self.assertIn('FloatPixelData', ds, sop.name)
            else:
                self.assertEqual(bytes(ds.PixelData), native_bytes(PIXEL_FORMATS[sop.pixel_format], sop.frames), sop.name)
        self.assertEqual(len(instances), len(STORAGE_SOPS))

    def test_frames_differ_and_reach_both_range_extremes(self):
        format_ = PIXEL_FORMATS['mono16s']
        array = pixel_array(format_, 3)
        self.assertFalse(np.array_equal(array[0], array[1]))
        self.assertEqual((int(array.min()), int(array.max())), (-32768, 32767))

    def test_stored_values_read_only_the_bits_stored(self):
        format_ = PIXEL_FORMATS['mono12s']
        self.assertEqual(stored_values(np.array([0xF800, 0x0800, 0x07FF, 0xFFFF]), format_).tolist(),
                         [-2048, -2048, 2047, -1])

    def test_exact_comparison_names_the_first_difference(self):
        expected = np.zeros((2, 3, 4), dtype='i8')
        changed = expected.copy()
        changed[1, 2, 3] = 7
        with self.assertRaisesRegex(ValueError, 'frame 1 sample .2, 3. is 7'):
            require_exact(changed, expected, 'check')
        with self.assertRaisesRegex(ValueError, 'shape'):
            require_exact(changed[:1], expected, 'check')
        self.assertIn('exact 24 samples', require_exact(expected.copy(), expected, 'check'))

    def test_lossy_bounds_reject_values_outside_them(self):
        format_ = PIXEL_FORMATS['mono8']
        expected = np.full((1, 4, 4), 100, dtype='i8')
        near = LOSSY_BOUNDS and '1.2.840.10008.1.2.4.81'
        self.assertIn('max error 5', require_lossy_bound(expected+5, expected, format_, near))
        with self.assertRaisesRegex(ValueError, 'exceeds'):
            require_lossy_bound(expected+6, expected, format_, near)
        with self.assertRaisesRegex(ValueError, 'below'):
            require_lossy_bound(expected+100, expected, format_, '1.2.840.10008.1.2.4.50')

    def test_color_normalization_only_accepts_known_photometrics(self):
        values = np.array([[[[128, 128, 128]]]], dtype='i8')
        self.assertEqual(to_rgb(values, 'YBR_FULL').tolist(), [[[[128, 128, 128]]]])
        with self.assertRaises(ValueError):
            to_rgb(values, 'PALETTE COLOR')

    def test_native_reader_handles_planar_configuration(self):
        format_ = PIXEL_FORMATS['rgb8p1']
        sop = next(sop for sop in STORAGE_SOPS if sop.family == 'sc')
        ds = build_fixture(sop._replace(pixel_format='rgb8p1'), '2.25.1318', 1)
        self.assertTrue(np.array_equal(native_values(ds, format_, 1), source_values(format_, 1)))
        ds.PixelData = bytes(ds.PixelData)[:-2]
        with self.assertRaisesRegex(ValueError, 'samples'):
            native_values(ds, format_, 1)

    def test_signature_finds_changes_at_any_depth_but_ignores_meta(self):
        ds = build_fixture(next(sop for sop in STORAGE_SOPS if sop.family == 'sr-basic'), '2.25.1318', 1)
        same = copy.deepcopy(ds)
        same.file_meta.ImplementationVersionName = 'OTHER'
        self.assertEqual(dataset_signature(ds), dataset_signature(same))
        changed = copy.deepcopy(ds)
        changed.ContentSequence[0].TextValue = 'different'
        self.assertNotEqual(dataset_signature(ds), dataset_signature(changed))

    def test_delivery_check_rejects_every_kind_of_damage(self):
        original = self.written('original.dcm', build_fixture(self.sop, '2.25.1318', 1))
        uid = str(pydicom.dcmread(original).SOPInstanceUID)
        syntax = '1.2.840.10008.1.2.1'
        self.assertIn('match', self.corpus.check_received(original, {uid: original}, syntax))
        with self.assertRaisesRegex(ValueError, 'not delivered'):
            self.corpus.check_received(original, {}, syntax)
        with self.assertRaisesRegex(ValueError, 'Stored transfer syntax'):
            self.corpus.check_received(original, {uid: original}, '1.2.840.10008.1.2')
        for name, change in (('pixels', lambda ds: setattr(ds, 'PixelData', bytes(ds.PixelData)[:-2]+b'\x01\x02')),
                             ('text', lambda ds: setattr(ds, 'StudyID', 'CHANGED')),
                             ('dropped', lambda ds: delattr(ds, 'KVP'))):
            damaged = pydicom.dcmread(original)
            change(damaged)
            with self.assertRaisesRegex(ValueError, 'Dataset changed', msg=name):
                self.corpus.check_received(original, {uid: self.written(name+'.dcm', damaged)}, syntax)

    def test_syntaxes_unknown_to_pydicom_still_round_trip_as_files(self):
        carrier = build_fixture(self.sop, '2.25.1318', 1)
        path = self.root/'htj2k.dcm'
        save_any(encapsulated(carrier, [b'\xff\x4f\xff\xd9'], {}, '1.2.840.10008.1.2.4.201'), path)
        got = pydicom.dcmread(path)
        self.assertEqual(str(got.file_meta.TransferSyntaxUID), '1.2.840.10008.1.2.4.201')
        self.assertIn(b'\xff\x4f\xff\xd9', bytes(got.PixelData))

    def test_every_vr_sample_uses_a_tag_the_dictionary_agrees_with(self):
        for vr in ('FL', 'FD', 'UL', 'SL', 'US', 'SS', 'AT', 'UC', 'UR', 'OF', 'OL', 'OD', 'SV', 'UV'):
            tag = dictionary_tag(vr)
            self.assertIsNotNone(tag, vr)
            self.assertEqual(pydicom.datadict.dictionary_VR(tag), vr)

    def test_codec_scratch_files_are_unique(self):
        codecs = Codecs(lambda *args, **kwargs: None, self.root/'work')
        self.assertNotEqual(codecs.scratch('.dcm'), codecs.scratch('.dcm'))


if __name__ == '__main__':
    unittest.main()
