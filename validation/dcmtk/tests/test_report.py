import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'validation/dcmtk'))
from report import evaluate, junit_counts, validate_manifest, write_junit


class GateTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads((ROOT/'validation/dcmtk/features.json').read_text())
        self.provenance = dict(source_revision='revision',source_digest='contents',
                               peer_revision='peer',manifest_digest='manifest')
        self.reports = []
        for profile in self.manifest['profiles']:
            ids = list(self.manifest['interop_cases'])
            if profile=='uncompressed': ids += [c['id'] for c in self.manifest['components']]
            self.reports.append(self.provenance | dict(profile=profile,cases=[
                dict(id=i,status='passed',tests=1,failed=0,skipped=0) for i in ids]))

    def evaluate(self):
        return evaluate(self.manifest,self.reports,self.provenance)

    def test_repository_manifest_maps_every_case(self):
        validate_manifest(self.manifest)

    def test_all_green_components_do_not_hide_coverage_gaps(self):
        result = self.evaluate()
        self.assertEqual(result['status'],'failed')
        self.assertTrue(result['coverage_gaps'])
        self.assertFalse(result['validation_errors'])

    def test_complete_evidence_can_pass_when_requirements_have_no_gaps(self):
        for feature in self.manifest['features']:
            feature['gaps'] = []
            if not feature['components'] and not feature['interop']:
                feature['components'] = ['component.core_tests']
        self.assertEqual(self.evaluate()['status'],'passed')

    def test_missing_profile_fails(self):
        self.reports.pop()
        self.assertTrue(any('missing profile' in e for e in self.evaluate()['validation_errors']))

    def test_missing_case_fails(self):
        self.reports[0]['cases'].pop()
        self.assertTrue(any('missing component.' in e for e in self.evaluate()['validation_errors']))

    def test_zero_tests_skips_failures_and_unknown_states_fail(self):
        original = copy.deepcopy(self.reports[0]['cases'][0])
        for mutation in [dict(tests=0),dict(tests=True),dict(tests='1'),dict(skipped=1),dict(failed=1),
                         dict(status='skipped'),dict(status='not-run'),dict(status='failed')]:
            with self.subTest(mutation=mutation):
                self.reports[0]['cases'][0] = original | mutation
                self.assertTrue(self.evaluate()['validation_errors'])

    def test_source_or_manifest_mismatch_fails(self):
        for key in self.provenance:
            with self.subTest(key=key):
                saved=self.reports[0][key]
                self.reports[0][key]='different'
                self.assertTrue(any('mismatched '+key in e for e in self.evaluate()['validation_errors']))
                self.reports[0][key]=saved

    def test_duplicate_profiles_and_cases_fail(self):
        self.reports.append(copy.deepcopy(self.reports[0]))
        self.reports[0]['cases'].append(copy.deepcopy(self.reports[0]['cases'][0]))
        self.assertGreaterEqual(len(self.evaluate()['validation_errors']),2)

    def test_unknown_manifest_reference_rejected(self):
        self.manifest['features'][0]['components'].append('unknown')
        with self.assertRaises(ValueError): validate_manifest(self.manifest)

    def test_malformed_evidence_still_produces_failure_summary(self):
        self.reports += [None, {'profile': []}]
        self.reports[0]['cases'] = 'not a list'
        self.reports[1]['cases'].append(None)
        result = self.evaluate()
        self.assertEqual(result['status'], 'failed')
        self.assertGreaterEqual(len(result['validation_errors']), 4)

    def test_unmapped_case_rejected(self):
        self.manifest['interop_cases'].append('unmapped')
        with self.assertRaises(ValueError): validate_manifest(self.manifest)

    def test_junit_requires_real_cases_and_counts_nested_failures_skips(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'x.xml'
            p.write_text('<testsuite tests="100"/>')
            with self.assertRaises(ValueError): junit_counts(p)
            p.write_text('<testsuites><testsuite><testcase/><testcase><failure/></testcase>'
                         '<testcase><skipped/></testcase></testsuite></testsuites>')
            self.assertEqual(junit_counts(p),dict(tests=3,failed=1,skipped=1))

    def test_incomplete_is_a_junit_failure_not_a_skip(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'x.xml'
            write_junit(p,[dict(id='missing',status='incomplete',detail='No external peer')])
            self.assertEqual(junit_counts(p),dict(tests=1,failed=1,skipped=0))


class TopologyTests(unittest.TestCase):
    def test_no_host_ports_or_host_bind_mounts(self):
        spec=importlib.util.spec_from_file_location('driver',ROOT/'scripts/run_dcmtk_validation.py')
        module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        config=module.compose_config('peer','runner','uncompressed')
        self.assertEqual(len(config['services']),6)
        for service in config['services'].values():
            self.assertNotIn('ports',service)
            for mount in service.get('volumes',[]):
                self.assertIn(mount.split(':')[0],config['volumes'])
        self.assertEqual(config['services']['pacs-server']['environment']['GENERATE_TEST_DATA'],'false')
        self.assertIn('healthcheck',config['services']['test-client'])


if __name__=='__main__':
    unittest.main()
