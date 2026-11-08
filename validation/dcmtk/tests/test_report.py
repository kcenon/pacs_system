import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0,str(ROOT/'validation/dcmtk'))
from report import evaluate, junit_counts, save_summary, validate_manifest, write_junit
from inventory import attach_results, build_inventory, tracks_cases


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
        # Synthetic, fully executed inventory for the report-contract tests.
        all_tests, selections, registered = {}, {}, []
        for spec in self.manifest['components']:
            if not tracks_cases(spec):
                continue
            record = dict(name=spec['id'],tags=[],source='synthetic',line='1')
            selections[spec['id']] = [record]
            if 'binary' in spec:
                all_tests.setdefault(spec['binary'],[]).append(record)
                registered.append(dict(name=spec['id'],command=[spec['binary'],spec['id']]))
            else:
                registered.append(dict(name=spec['id'],command=['synthetic-tool']))
        inventory = build_inventory(self.manifest,dict(tests=registered),all_tests,selections,{})
        self.reports[0]['test_inventory'] = inventory
        for result in self.reports[0]['cases']:
            if result['id'] in selections:
                records = [dict(r,status='passed') for r in selections[result['id']]]
                result['case_results'] = attach_results(inventory,result['id'],records)

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

    def test_binary_success_without_individual_results_fails(self):
        for result in self.reports[0]['cases']:
            result.pop('case_results',None)
        self.assertTrue(any('individual execution' in error for error in self.evaluate()['validation_errors']))

    def test_missing_inventory_fails_even_when_all_component_counts_pass(self):
        del self.reports[0]['test_inventory']
        self.assertTrue(any('individual test inventory' in error for error in self.evaluate()['validation_errors']))

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


class LaneTests(unittest.TestCase):
    """#1328 step 3: a pr-lane run certifies its selected cases only; the full lane keeps every requirement."""

    # The same complete evidence as GateTests, without running GateTests' cases again.
    setUp = GateTests.setUp
    evaluate = GateTests.evaluate

    def pr_lane(self):
        """Turn the complete evidence of setUp into what a pr-lane run records."""
        outside = [c['id'] for c in self.manifest['components'] if 'pr' not in c.get('lanes', self.manifest['lanes'])]
        self.provenance['lane'] = 'pr'
        for report in self.reports:
            report['lane'] = 'pr'
            report['cases'] = [case for case in report['cases'] if case['id'] not in outside]
        return outside

    def test_repository_manifest_leaves_only_the_provider_out_of_the_pr_lane(self):
        self.assertEqual(set(self.manifest['lanes']), {'full', 'pr'})
        outside = [c for c in self.manifest['components'] if 'pr' not in c.get('lanes', self.manifest['lanes'])]
        self.assertEqual([c['id'] for c in outside], ['component.ai_provider'])
        self.assertEqual(outside[0]['lanes'], ['full'])
        self.assertEqual(outside[0]['lane_exclusion']['category'], 'provider')
        ai = next(f for f in self.manifest['features'] if f['id'] == 'ai')
        self.assertIn('component.ai_provider', ai['components'])

    def test_pr_lane_passes_its_cases_without_certifying_completeness(self):
        outside = self.pr_lane()
        result = self.evaluate()
        self.assertEqual(result['status'], 'passed')
        self.assertFalse(result['validation_errors'])
        self.assertFalse(result['certifies_completeness'])
        self.assertTrue(result['coverage_gaps'])
        self.assertEqual([c['id'] for c in result['outside_lane']], outside)
        ai = next(f for f in result['features'] if f['id'] == 'ai')
        self.assertEqual((ai['status'], ai['outside_lane']), ('incomplete', ['component.ai_provider']))

    def test_pr_lane_fails_on_any_selected_case_that_did_not_pass(self):
        self.pr_lane()
        cases = self.reports[0]['cases']
        original = copy.deepcopy(cases)
        index = next(i for i, case in enumerate(cases) if case['id'] == 'component.ai_contract')
        for position in (0, index):
            for mutation in [dict(failed=1), dict(skipped=1), dict(status='blocked', tests=0), dict(status='failed')]:
                with self.subTest(case=cases[position]['id'], mutation=mutation):
                    cases[position] = original[position] | mutation
                    self.assertEqual(self.evaluate()['status'], 'failed')
                    cases[position] = copy.deepcopy(original[position])
        del cases[index]
        self.assertIn('uncompressed: missing component.ai_contract', self.evaluate()['validation_errors'])

    def test_pr_lane_fails_when_a_left_out_component_reports_a_failure(self):
        self.pr_lane()
        self.reports[0]['cases'].append(dict(id='component.ai_provider', status='failed', tests=1, failed=1, skipped=0))
        self.assertIn('uncompressed: additional failure component.ai_provider', self.evaluate()['validation_errors'])

    def test_full_lane_requires_what_the_pr_lane_leaves_out(self):
        self.provenance['lane'] = 'full'
        for report in self.reports:
            report['lane'] = 'full'
        self.reports[0]['cases'] = [case for case in self.reports[0]['cases'] if case['id'] != 'component.ai_provider']
        result = self.evaluate()
        self.assertEqual(result['status'], 'failed')
        self.assertTrue(result['certifies_completeness'])
        self.assertIn('uncompressed: missing component.ai_provider', result['validation_errors'])

    def test_a_report_from_another_lane_or_an_unknown_lane_fails(self):
        self.pr_lane()
        self.reports[1]['lane'] = 'full'
        self.assertTrue(any(error.endswith('mismatched lane') for error in self.evaluate()['validation_errors']))
        self.provenance['lane'] = 'nightly'
        for report in self.reports:
            report['lane'] = 'nightly'
        result = self.evaluate()
        self.assertEqual(result['status'], 'failed')
        self.assertIn('unknown lane: nightly', result['validation_errors'])

    def test_manifest_rejects_a_lane_exclusion_it_cannot_justify(self):
        for mutation in [dict(lanes=['pr']), dict(lanes=[]), dict(lanes=['full', 'full']), dict(lanes=['full', 'nightly']),
                         dict(lane_exclusion=dict(category='convenience', reason='slow')),
                         dict(lane_exclusion=dict(category='provider', reason='')), dict(lane_exclusion=None)]:
            with self.subTest(mutation=mutation):
                manifest = copy.deepcopy(self.manifest)
                target = next(c for c in manifest['components'] if c['id'] == 'component.ai_provider')
                target.update(mutation)
                if target['lane_exclusion'] is None:
                    del target['lane_exclusion']
                with self.assertRaises(ValueError):
                    validate_manifest(manifest)
        manifest = copy.deepcopy(self.manifest)
        manifest['components'][0]['lane_exclusion'] = dict(category='provider', reason='no lanes declared')
        with self.assertRaises(ValueError):
            validate_manifest(manifest)
        manifest['components'][0]['lanes'] = sorted(manifest['lanes'])
        with self.assertRaises(ValueError):
            validate_manifest(manifest)
        manifest = copy.deepcopy(self.manifest)
        del manifest['lanes']['full']
        with self.assertRaises(ValueError):
            validate_manifest(manifest)

    def test_summary_names_the_lane_and_what_it_does_not_certify(self):
        with tempfile.TemporaryDirectory() as directory:
            save_summary(directory, self.evaluate())
            self.assertTrue((Path(directory)/'summary.txt').read_text().startswith('PACS full validation: FAILED\n'))
        self.pr_lane()
        summary = self.evaluate()
        with tempfile.TemporaryDirectory() as directory:
            save_summary(directory, summary)
            text = (Path(directory)/'summary.txt').read_text()
        self.assertTrue(text.startswith(
            'PACS validation, lane pr: PASSED\nCompleteness: not certified by this lane; the full lane judges it '
            f'(coverage gaps: {len(summary["coverage_gaps"])}; required components outside this lane: 1).\n'))
        self.assertIn('\ncomponent.ai_provider (lanes: full; provider): ', text)


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

    def test_driver_preserves_spawn_exit_and_timeout_records(self):
        spec=importlib.util.spec_from_file_location('driver',ROOT/'scripts/run_dcmtk_validation.py')
        module=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            driver=module.Driver.__new__(module.Driver)
            driver.output=Path(directory)
            with self.assertRaises(OSError):
                driver.command(['/pacs-does-not-exist-1311'],'missing.log')
            with self.assertRaises(RuntimeError):
                driver.command([sys.executable,'-c','raise SystemExit(7)'],'exit.log')
            with self.assertRaises(RuntimeError):
                driver.command([sys.executable,'-c','import time; time.sleep(30)'],'timeout.log',timeout=.05)
            events=[json.loads(line) for line in (driver.output/'commands.jsonl').read_text().splitlines()]
            self.assertEqual([event['outcome'] for event in events],['spawn_error','exited','timeout'])
            self.assertEqual(events[1]['returncode'],7)
            self.assertEqual(events[2]['reported_exit_code'],124)


if __name__=='__main__':
    unittest.main()
