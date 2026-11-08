from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from deferrals import audit_deferrals


class DeferralAuditTests(unittest.TestCase):
    def test_skip_is_attributed_to_exact_case_without_waiving_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'test.cpp').write_text('TEST_CASE("one") {\nSKIP("DCMTK not installed");\n}\nTEST_CASE("two") {}\n')
            cases = [dict(case_id=str(line), kind='catch2', name=name, source='test.cpp',
                          source_line=str(line), tags=tags, ctest_names=[name])
                     for name, line, tags in [('one',1,[]),('two',4,['.slow'])]]
            audit_deferrals(root, cases)
            self.assertEqual([e['kind'] for e in cases[0]['deferrals']], ['conditional-skip'])
            self.assertEqual(cases[0]['deferrals'][0]['owner_issue'], 1240)
            self.assertEqual([e['kind'] for e in cases[1]['deferrals']], ['hidden'])
            self.assertFalse(cases[0]['deferrals'][0]['waives_failure'])

    def test_quarantine_retains_unresolved_registration(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root/'.github/sanitizers/ctest-quarantine-thread.txt'
            path.parent.mkdir(parents=True)
            path.write_text('# reason\nstress::lockfree_queue memory safety\n')
            result = audit_deferrals(root, [])
            self.assertEqual(result['quarantine'][0]['case_ids'], [])
            self.assertEqual(result['quarantine'][0]['resolution'], 'not in this build')


if __name__ == '__main__':
    unittest.main()
