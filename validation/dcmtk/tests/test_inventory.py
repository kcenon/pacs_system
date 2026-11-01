import copy
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
from inventory import (attach_results, build_inventory, catch_listing, catch_results,
                       ctest_results, evaluate_inventory)
from support import declared_support


def test_record(name, tags=()):
    return dict(name=name, tags=list(tags), source='tests/example.cpp', line='10')


def example_inventory(extra=()):
    manifest = dict(components=[dict(id='component.example',binary='example',args=[])],
                    features=[dict(id='feature',components=['component.example'])])
    tests = [test_record('one')]+list(extra)
    registered = dict(tests=[dict(name=t['name'],command=['/build/example',t['name']]) for t in tests])
    return build_inventory(manifest,registered,{'example':tests},
                           {'component.example':[tests[0]]},{'os':'test'})


class InventoryTests(unittest.TestCase):
    def test_missing_test_fails_even_when_binary_was_run(self):
        inv = example_inventory()
        result = evaluate_inventory(inv,{'component.example':[]})
        self.assertEqual(result['status'],'failed')
        self.assertIn('test not executed',' '.join(result['errors']))

    def test_hidden_case_is_explicit_unverified_and_cannot_pass(self):
        inv = example_inventory([test_record('hidden',['.slow'])])
        results = attach_results(inv,'component.example',[dict(name='one',tags=[],status='passed')])
        evaluation = evaluate_inventory(inv,{'component.example':results})
        self.assertEqual(evaluation['assigned'],1)
        self.assertEqual(evaluation['discovered'],2)
        self.assertEqual(len(evaluation['unassigned']),1)
        self.assertEqual(inv['cases'][1]['assignment_status'],'unverified')

    def test_exact_case_set_passes(self):
        inv = example_inventory()
        results = attach_results(inv,'component.example',[dict(name='one',tags=[],status='passed')])
        evaluation = evaluate_inventory(inv,{'component.example':results})
        self.assertEqual(evaluation['status'],'passed')
        self.assertEqual(evaluation['executed'],1)

    def test_unknown_duplicate_skipped_or_mismatched_results_fail(self):
        inv = example_inventory()
        result = attach_results(inv,'component.example',[dict(name='one',tags=[],status='passed')])[0]
        for results in [[result,result], [dict(result,name='other')],
                        [dict(result,case_id='invented')],[dict(result,status='skipped')]]:
            with self.subTest(results=results):
                self.assertEqual(evaluate_inventory(inv,{'component.example':results})['status'],'failed')

    def test_same_catch_name_different_tags_are_distinct(self):
        text = '<MatchingTests><TestCase><Name>x</Name><Tags>[a]</Tags></TestCase>'\
               '<TestCase><Name>x</Name><Tags>[b]</Tags></TestCase></MatchingTests>'
        records = catch_listing(text)
        manifest = dict(components=[dict(id='job',binary='example')],
                        features=[dict(id='feature',components=['job'])])
        inv = build_inventory(manifest,{'tests':[]},{'example':records},{'job':records},{})
        self.assertEqual(len({r['case_id'] for r in inv['cases']}),2)
        results = attach_results(inv,'job',[dict(r,status='passed') for r in records])
        self.assertEqual(evaluate_inventory(inv,{'job':results})['status'],'passed')

    def test_duplicate_ctest_registrations_are_preserved_and_flagged(self):
        manifest = dict(components=[dict(id='job',kind='ctest')],
                        features=[dict(id='feature',components=['job'])])
        registered = dict(tests=[dict(name='x',command=['/tool']),dict(name='x',command=['/other'])])
        inv = build_inventory(manifest,registered,{}, {'job':[]},{})
        self.assertEqual(len(inv['cases']),2)
        self.assertEqual(len(inv['ctest_registrations']),2)
        self.assertIn('duplicate CTest',' '.join(inv['discovery_errors']))

    def test_build_discovery_never_reuses_disabled_feature_inventory(self):
        first = example_inventory([test_record('optional')])
        second = example_inventory()
        self.assertEqual(len(first['cases']),2)
        self.assertEqual(len(second['cases']),1)

    def test_native_xml_preserves_slashes_in_test_names(self):
        text = '<Catch2TestRun><TestCase name="name/with/slash" tags="[unit]">'\
               '<Section name="nested"><OverallResults successes="1" failures="0"/></Section>'\
               '<OverallResult success="true" skips="0"/></TestCase>'\
               '<OverallResultsCases successes="1" failures="0" expectedFailures="0"/></Catch2TestRun>'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'results.xml'
            path.write_text(text)
            self.assertEqual(catch_results(path)[0]['name'],'name/with/slash')
            path.write_text(text.replace('failures="0"','failures="1"',1))
            self.assertEqual(catch_results(path)[0]['status'],'failed')

    def test_allowed_failure_is_not_a_pass(self):
        text = '<Catch2TestRun><TestCase name="x" tags="[!mayfail]">'\
               '<Expression success="false"/><OverallResult success="true" skips="0"/></TestCase>'\
               '<OverallResults expectedFailures="1"/>'\
               '<OverallResultsCases expectedFailures="1"/></Catch2TestRun>'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'x.xml'
            path.write_text(text)
            self.assertEqual(catch_results(path)[0]['status'],'failed')

    def test_valid_skipped_case_is_preserved_as_skipped(self):
        text = '<Catch2TestRun><TestCase name="x" tags="[unit]">'\
               '<OverallResult success="true" skips="1"/></TestCase>'\
               '<OverallResultsCases successes="0" failures="0" skips="1"/></Catch2TestRun>'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'x.xml'
            path.write_text(text)
            self.assertEqual(catch_results(path)[0]['status'],'skipped')

    def test_incomplete_native_xml_or_duplicate_junit_names_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'x.xml'
            path.write_text('<Catch2TestRun><TestCase name="x"/></Catch2TestRun>')
            with self.assertRaises(ValueError): catch_results(path)
            path.write_text('<testsuite><testcase name="x"/><testcase name="x"/></testsuite>')
            with self.assertRaises(ValueError): ctest_results(path)

    def test_support_keeps_big_endian_files_and_unsupported_codecs_separate(self):
        scope = declared_support(ROOT)
        cases = {case['case_id']:case for case in scope['cases']}
        be = 'support.syntax.1.2.840.10008.1.2.2.'
        self.assertEqual(cases[be+'network']['condition'],'negative')
        self.assertEqual(cases[be+'file_or_codec']['condition'],'positive')
        for uid in ['1.2.840.10008.1.2.1.99','1.2.840.10008.1.2.4.107',
                    '1.2.840.10008.1.2.4.110','1.2.840.10008.1.2.11']:
            for boundary in ('network','file_or_codec'):
                self.assertEqual(cases[f'support.syntax.{uid}.{boundary}']['condition'],'negative')
        for case in scope['cases']:
            expected = 'assigned' if case['owner_issue'] in (1318, 1319) else 'unverified'
            self.assertEqual(case['assignment_status'], expected, case['case_id'])


if __name__ == '__main__':
    unittest.main()
