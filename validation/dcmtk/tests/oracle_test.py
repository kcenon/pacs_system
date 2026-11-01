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
        self.assertEqual(self.v.command_results[-1]['outcome'],'timeout')
        self.assertEqual(self.v.command_results[-1]['returncode'],-9)

    def test_missing_executable_and_signal_preserve_valid_parent_report(self):
        import json
        import xml.etree.ElementTree as ET
        self.v.case('missing',lambda:self.v.command(['/does-not-exist/pacs-1311']))
        self.v.case('signal',lambda:self.v.command([sys.executable,'-c',
                    'import os,signal; os.kill(os.getpid(),signal.SIGTERM)']))
        self.assertEqual([r['status'] for r in self.v.results],['failed','failed'])
        self.assertEqual([r['outcome'] for r in self.v.command_results],['spawn_error','signal'])
        self.assertIsNone(self.v.command_results[0]['returncode'])
        self.assertEqual(self.v.results[0]['failure_category'],'environment_unavailable')
        self.assertEqual(self.v.command_results[1]['returncode'],-15)
        ET.parse(self.v.output/'results.xml')
        payload=json.loads((self.v.output/'results.json').read_text())
        self.assertEqual(len(payload['commands']),2)

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

    def test_move_accepts_empty_destination_query_exit_one(self):
        self.v.bin.mkdir()
        query=self.v.bin/'query_scu'
        query.write_text('#!/usr/bin/env python3\n'
                         'print(\'{"resultCount":0,"results":[]}\')\nraise SystemExit(1)\n')
        query.chmod(0o755)
        retrieve=self.v.bin/'retrieve_scu'
        retrieve.write_text('#!/usr/bin/env python3\nraise SystemExit(0)\n')
        retrieve.chmod(0o755)
        self.v.data['ct']=[self.ds]
        def receive(host,ae,study,directory):
            directory.mkdir()
            self.ds.save_as(directory/'object.dcm')
        self.v.get_oracle=receive
        self.assertIn('1 exact',self.v.move_scu())

    def prepare_fixture_sources(self):
        root = Path(self.temp.name)/'source-fixtures'
        for mod,count in [('ct',5),('mr',6),('cr',2)]:
            (root/mod).mkdir(parents=True)
            for i in range(count):
                self.ds.save_as(root/mod/f'{i}.dcm')
        self.v.args.fixtures = str(root)

    def test_diagnostic_and_end_to_end_namespaces_cannot_overlap(self):
        self.prepare_fixture_sources()
        seen = set()
        for chain in ('end-to-end','diagnostic-reference','diagnostic-scp'):
            data,paths = self.v.make_fixtures(chain)
            uids = {str(ds.SOPInstanceUID) for values in data.values() for ds in values}
            self.assertEqual(len(uids),13)
            self.assertTrue(all(len(uid)<=64 for uid in uids))
            self.assertFalse(seen & uids)
            seen |= uids
            self.assertEqual(sum(len(group) for group in paths.values()),13)

    def prepare_reference_diagnostic_probe(self):
        self.prepare_fixture_sources()
        self.v.case('peer.preflight',lambda:'peer ready',phase='peer_preparation')
        self.v.case('store.primary',lambda:self.v.command([sys.executable,'-c','raise SystemExit(9)']))
        self.v.case('get.ct',lambda:self.fail('original chain must remain blocked'),requires=['store.primary'])
        calls = []
        self.v.query = lambda mod,data:calls.append('query.'+mod)
        self.v.get_scu = lambda mod,data,prefix:calls.append('get.'+mod)
        self.v.move_scu = lambda data,prefix:calls.append('move.scu')
        return calls

    def test_diagnostics_run_after_store_failure_without_repairing_original_chain(self):
        calls = self.prepare_reference_diagnostic_probe()
        self.v.seed_reference = lambda:'reference seeded'
        def receive(host,ae,study,directory):
            directory.mkdir()
            for group in self.v.fixture_sets['diagnostic-reference']['data'].values():
                for ds in group:
                    if ds.StudyInstanceUID == study:
                        ds.save_as(directory/f'{ds.SOPInstanceUID}.dcm')
        self.v.get_oracle = receive
        self.v.reference_diagnostics()
        self.assertEqual(set(calls), {'query.ct','query.mr','query.cr','get.ct','get.mr','get.cr','move.scu'})
        rows = {r['id']:r for r in self.v.results}
        self.assertEqual(rows['store.primary']['status'],'failed')
        self.assertEqual(rows['get.ct']['status'],'blocked')
        self.assertFalse(rows['get.ct']['executed'])
        self.assertTrue(rows['diagnostic.move.scu']['executed'])
        self.assertEqual(rows['diagnostic.reference.ready']['status'],'passed')

    def test_failed_seed_blocks_all_diagnostics(self):
        calls = self.prepare_reference_diagnostic_probe()
        self.v.seed_reference = lambda:self.v.command([sys.executable,'-c','raise SystemExit(8)'])
        self.v.reference_ready = lambda:self.fail('failed seeding must block readiness')
        self.v.reference_diagnostics()
        self.assertEqual(calls,[])
        rows = {r['id']:r for r in self.v.results}
        self.assertEqual(rows['diagnostic.reference.seed']['failure_category'],'peer_preparation_failure')
        for name in ['diagnostic.reference.ready','diagnostic.query.ct','diagnostic.get.ct','diagnostic.move.scu']:
            self.assertEqual(rows[name]['status'],'blocked')
            self.assertFalse(rows[name]['executed'])
            self.assertEqual(rows[name]['tests'],0)

    def test_corrupt_reference_pixels_fail_preparation_and_block_product_calls(self):
        calls = self.prepare_reference_diagnostic_probe()
        self.v.seed_reference = lambda:'reference seeded'
        def corrupt(host,ae,study,directory):
            directory.mkdir()
            for group in self.v.fixture_sets['diagnostic-reference']['data'].values():
                for ds in group:
                    if ds.StudyInstanceUID == study:
                        import copy
                        bad = copy.deepcopy(ds)
                        bad.PixelData=b'\x01\x00\x03\x00'
                        bad.save_as(directory/f'{ds.SOPInstanceUID}.dcm')
        self.v.get_oracle = corrupt
        self.v.reference_diagnostics()
        self.assertEqual(calls,[])
        row=next(r for r in self.v.results if r['id']=='diagnostic.reference.ready')
        self.assertEqual(row['status'],'failed')
        self.assertIn('PixelData mismatch',row['detail'])

    def test_public_import_must_index_the_exact_fixture_count(self):
        self.prepare_fixture_sources()
        self.v.make_fixtures('diagnostic-scp')
        self.v.command=lambda *a,**k:'Scan complete: 0 files indexed'
        with self.assertRaisesRegex(ValueError,'expected 13'):
            self.v.import_scp()
        self.v.command=lambda *a,**k:'Scan complete: 13 files indexed'
        self.assertIn('13 instances',self.v.import_scp())
        self.v.command=lambda *a,**k:'Scan complete: 13 files indexed (1 errors)'
        with self.assertRaisesRegex(ValueError,'indexing errors'):
            self.v.import_scp()

    def test_existing_reference_output_is_not_reused_as_readiness_evidence(self):
        calls = self.prepare_reference_diagnostic_probe()
        self.v.seed_reference = lambda:'reference seeded'
        stale = self.v.output/'diagnostic-reference-oracle-ct'
        stale.mkdir()
        self.ds.save_as(stale/'stale.dcm')
        self.v.reference_diagnostics()
        self.assertEqual(calls,[])
        ready=next(r for r in self.v.results if r['id']=='diagnostic.reference.ready')
        self.assertEqual(ready['status'],'failed')
        self.assertIn('File exists',ready['detail'])


if __name__=='__main__':
    unittest.main()
