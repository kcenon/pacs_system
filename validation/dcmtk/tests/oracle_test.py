"""Run inside the validation image (requires its pinned distribution pydicom)."""
import argparse
from pathlib import Path
import sys
import tempfile
import unittest

import pydicom
from pydicom.dataset import FileDataset, FileMetaDataset
from pydicom.uid import ExplicitVRLittleEndian

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from run import Validation


class OracleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.v = Validation(argparse.Namespace(output=str(root/'evidence'),build_dir=str(root),
            profile='uncompressed',source_revision='test',source_digest='test'))
        self.dest = root/'received'
        self.dest.mkdir()
        meta = FileMetaDataset()
        meta.TransferSyntaxUID = ExplicitVRLittleEndian
        meta.MediaStorageSOPClassUID = '1.2.840.10008.5.1.4.1.1.2'
        meta.MediaStorageSOPInstanceUID = '2.25.1'
        self.ds = FileDataset(None,{},file_meta=meta,preamble=b'\0'*128)
        self.ds.is_implicit_VR=False
        self.ds.is_little_endian=True
        self.ds.SOPClassUID=meta.MediaStorageSOPClassUID
        self.ds.SOPInstanceUID=meta.MediaStorageSOPInstanceUID
        self.ds.StudyInstanceUID='2.25.2'
        self.ds.SeriesInstanceUID='2.25.3'
        self.ds.PatientID='SYNTHETIC'
        self.ds.PatientName='TEST^PATIENT'
        self.ds.Rows=1
        self.ds.Columns=2
        self.ds.BitsAllocated=16
        self.ds.BitsStored=16
        self.ds.PixelRepresentation=0
        self.ds.SamplesPerPixel=1
        self.ds.PhotometricInterpretation='MONOCHROME2'
        self.ds.PixelData=b'\x01\x00\x02\x00'
        self.ds.save_as(self.dest/'object.dcm')

    def test_exact_object_passes(self):
        self.assertIn('1 exact',self.v.verify(self.dest,[self.ds]))

    def test_single_pixel_byte_corruption_fails(self):
        modified=pydicom.dcmread(self.dest/'object.dcm')
        modified.PixelData=b'\x01\x00\x03\x00'
        modified.save_as(self.dest/'object.dcm')
        with self.assertRaisesRegex(ValueError,'PixelData mismatch'): self.v.verify(self.dest,[self.ds])

    def test_metadata_corruption_fails(self):
        modified=pydicom.dcmread(self.dest/'object.dcm')
        modified.PatientName='WRONG^PATIENT'
        modified.save_as(self.dest/'object.dcm')
        with self.assertRaises(ValueError): self.v.verify(self.dest,[self.ds])

    def test_missing_object_fails(self):
        (self.dest/'object.dcm').unlink()
        with self.assertRaises(ValueError): self.v.verify(self.dest,[self.ds])

    def test_duplicate_uid_fails(self):
        self.ds.save_as(self.dest/'duplicate.dcm')
        with self.assertRaisesRegex(ValueError,'Duplicate'): self.v.verify(self.dest,[self.ds])

    def test_compressed_fallback_fails(self):
        self.v.codec=(['dcmcrle'],'dcmdrle','1.2.840.10008.1.2.5','+xr','-xr')
        with self.assertRaisesRegex(ValueError,'fallback'): self.v.verify(self.dest,[self.ds])

    def test_missing_pixels_fail(self):
        del self.ds.PixelData
        with self.assertRaisesRegex(ValueError,'PixelData'): self.v.signature(self.ds)

    def test_nonzero_and_timeout_are_recorded_as_failures(self):
        self.v.case('nonzero',lambda:self.v.command([sys.executable,'-c','raise SystemExit(7)']))
        self.v.case('timeout',lambda:self.v.command([sys.executable,'-c','import time; time.sleep(10)'],timeout=.1))
        self.assertEqual([r['status'] for r in self.v.results],['failed','failed'])
        self.assertIn('exit 124',self.v.results[-1]['detail'])

    def test_zero_and_skipped_component_cannot_pass(self):
        self.v.case('empty',lambda:dict(tests=0,failed=0,skipped=0))
        self.v.case('skip',lambda:dict(tests=2,failed=0,skipped=1))
        self.assertTrue(all(r['status']=='failed' for r in self.v.results))

    def test_compressed_preflight_requires_compressed_move_and_integrity_cases(self):
        self.v.args.profile='jpeg-lossless'
        self.v.args.peer_tests='/peer-tests'
        self.v.command=lambda *args,**kwargs:'Transfer Syntax Results: 9/9 passed'
        with self.assertRaisesRegex(ValueError,'Incomplete'): self.v.preflight()
        self.v.command=lambda *args,**kwargs:'Transfer Syntax Results: 16/16 passed'
        self.assertEqual(self.v.preflight()['tests'],16)

    def test_corrupt_component_junit_preserves_process_failure(self):
        self.v.bin.mkdir()
        fake=self.v.bin/'fake-component'
        fake.write_text('#!/usr/bin/env python3\nimport sys\nfrom pathlib import Path\n'
                        'p=next(a.split("=",1)[1] for a in sys.argv if a.startswith("junit::out="))\n'
                        'Path(p).write_text("<testsuites/>invalid")\nraise SystemExit(6)\n')
        fake.chmod(0o755)
        self.v.case('corrupt',lambda:self.v.component(dict(id='corrupt',binary=fake.name,args=[],timeout_seconds=2)))
        result=self.v.results[-1]
        self.assertEqual(result['status'],'failed')
        self.assertEqual(result['tests'],0)
        self.assertIn('exit 6',result['detail'])
        self.assertIn('invalid/missing JUnit',result['detail'])

    def test_failed_prerequisite_blocks_without_running_dependent_action(self):
        self.v.case('store',lambda:(_ for _ in ()).throw(ValueError('store failed')))
        self.v.case('retrieve',lambda:self.fail('dependent action executed'),requires=['store'])
        self.assertEqual(self.v.results[-1]['status'],'blocked')
        self.assertEqual(self.v.results[-1]['tests'],0)


if __name__=='__main__':
    unittest.main()
