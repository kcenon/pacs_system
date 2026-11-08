"""Standard-library checks of the DIMSE-N tables; running them against peers is dimse_n_probe.py's job."""
import ast
import fnmatch
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
from dimse_n import (MPPS_REFUSALS, MWL_ITEMS, MWL_QUERIES, PERFORMED_REQUIRED, STGCMT_REFUSALS, SUCCESS, UPS_OPERATIONS, UPS_PULL, UPS_PUSH,
                     UPS_QUERY, UPS_SET_CASES, UPS_STATES, UPS_TABLE, UPS_WATCH, change_state, request_cancel, requirement_cases, ups_create,
                     ups_event_step, ups_performed, ups_setup, watch_action)
from support import declared_support

PROBE = ROOT/'validation/dcmtk/tests/dimse_n_probe.py'
PEERS = ROOT/'validation/peers/dimse-n'


class StateTable(unittest.TestCase):
    def test_every_event_has_a_cell_for_every_state(self):
        for event, cells in UPS_TABLE.items():
            self.assertEqual(set(cells), set(UPS_STATES), event)

    def test_table_cc_1_1_2_rows(self):
        """A few cells worth stating twice: the ones an implementation most easily gets backwards."""
        self.assertEqual(UPS_TABLE['progress-correct']['SCHEDULED'], (SUCCESS, 'IN PROGRESS'))
        self.assertEqual(UPS_TABLE['progress-correct']['IN PROGRESS'], ('C302',))
        self.assertEqual(UPS_TABLE['complete-correct']['COMPLETED'], ('B306',))
        self.assertEqual(UPS_TABLE['cancel-correct']['CANCELED'], ('B304',))
        self.assertEqual(UPS_TABLE['complete-correct']['SCHEDULED'], ('C310',))
        self.assertEqual(UPS_TABLE['request-cancel']['COMPLETED'], ('C311',))
        for state in UPS_STATES[1:]:
            self.assertEqual(UPS_TABLE['create'][state], ('0111',))
            self.assertEqual(UPS_TABLE['schedule'][state], ('C303',))
        for event in ('progress-correct', 'progress-wrong', 'schedule', 'complete-correct', 'complete-wrong', 'cancel-correct', 'cancel-wrong', 'request-cancel'):
            self.assertEqual(UPS_TABLE[event]['null'], ('C307',), event)

    def test_a_wrong_transaction_never_changes_state(self):
        for event in ('progress-wrong', 'complete-wrong', 'cancel-wrong'):
            for state in UPS_STATES:
                self.assertEqual(len(UPS_TABLE[event][state]), 1, f'{event} in {state}')

    def test_setup_reaches_each_state_through_public_operations(self):
        self.assertEqual(ups_setup('1', 'null', '9'), [])
        self.assertEqual(len(ups_setup('1', 'SCHEDULED', '9')), 1)
        self.assertEqual(len(ups_setup('1', 'IN PROGRESS', '9')), 2)
        self.assertEqual(len(ups_setup('1', 'COMPLETED', '9')), 4)
        self.assertEqual(len(ups_setup('1', 'CANCELED', '9')), 4)

    def test_wrong_transaction_in_scheduled_means_none_offered(self):
        _, step = ups_event_step('complete-wrong', '1', 'SCHEDULED', '9')
        self.assertNotIn('TransactionUID', step['dataset'])
        _, step = ups_event_step('complete-wrong', '1', 'IN PROGRESS', '9')
        self.assertEqual(step['dataset']['TransactionUID'], '9.9')

    def test_final_state_attributes_are_in_place_for_correct_rows(self):
        prepared, _ = ups_event_step('complete-correct', '1', 'IN PROGRESS', '9')
        self.assertEqual(len(prepared), 1)
        prepared, _ = ups_event_step('cancel-correct', '1', 'IN PROGRESS', '9')
        self.assertEqual(len(prepared), 1)
        prepared, _ = ups_event_step('complete-correct', '1', 'COMPLETED', '9')
        self.assertEqual(prepared, [])


class Requests(unittest.TestCase):
    def test_requests_name_ups_push_on_their_own_context(self):
        self.assertEqual((change_state('1', 'IN PROGRESS', '2')['class'], change_state('1', 'IN PROGRESS', '2')['meta']), (UPS_PUSH, UPS_PULL))
        self.assertEqual((request_cancel('1')['class'], request_cancel('1')['meta'], request_cancel('1')['action']), (UPS_PUSH, UPS_PUSH, 2))
        step = watch_action('1', 3, 'AE', True)
        self.assertEqual((step['class'], step['meta'], step['dataset']['DeletionLock']), (UPS_PUSH, UPS_WATCH, 'TRUE'))

    def test_n_create_has_the_required_values_and_an_empty_lock(self):
        values = ups_create()
        for keyword in ('ProcedureStepState', 'ScheduledProcedureStepPriority', 'ProcedureStepLabel', 'ScheduledProcedureStepStartDateTime', 'InputReadinessState'):
            self.assertTrue(values[keyword], keyword)
        self.assertEqual(values['TransactionUID'], '')
        self.assertNotIn('ProcedureStepLabel', ups_create(label=None))

    def test_performed_item_can_lose_each_required_attribute(self):
        for keyword in PERFORMED_REQUIRED:
            self.assertIn(keyword, ups_performed()['UnifiedProcedureStepPerformedProcedureSequence'][0])
            self.assertNotIn(keyword, ups_performed(keyword)['UnifiedProcedureStepPerformedProcedureSequence'][0])

    def test_operations_by_context(self):
        self.assertEqual(UPS_OPERATIONS[UPS_PUSH], {'n_create', 'n_get', 'cancel'})
        self.assertIn('state', UPS_OPERATIONS[UPS_PULL])
        self.assertNotIn('state', UPS_OPERATIONS[UPS_WATCH])
        self.assertEqual(UPS_OPERATIONS[UPS_QUERY], {'n_get', 'c_find'})

    def test_tables_have_unique_names(self):
        for table in (UPS_SET_CASES, MPPS_REFUSALS, MWL_QUERIES, STGCMT_REFUSALS):
            names = [row[0] for row in table]
            self.assertEqual(len(names), len(set(names)))

    def test_worklist_queries_name_patients_that_exist(self):
        known = {item['PatientID'] for item in MWL_ITEMS}
        for name, _, expected in MWL_QUERIES:
            self.assertLessEqual(set(expected), known, name)


class Requirements(unittest.TestCase):
    def setUp(self):
        self.declared = declared_support(ROOT)
        self.mapping = requirement_cases(self.declared)
        self.owned = {case['case_id'] for case in self.declared['cases'] if case['owner_issue'] == 1320}

    def test_every_owned_case_has_named_cases(self):
        self.assertTrue(self.owned)
        self.assertEqual(self.owned-set(self.mapping), set())
        for case_id in self.owned:
            self.assertTrue(self.mapping[case_id], case_id)

    def test_owned_cases_are_assigned_to_an_executing_job(self):
        for case in self.declared['cases']:
            if case['owner_issue'] == 1320:
                self.assertEqual(case['assignment_status'], 'assigned', case['case_id'])

    def test_the_statement_declares_each_ups_class_with_its_roles(self):
        roles = {row['uid']: row['roles'] for row in self.declared['sop_classes']}
        self.assertEqual(roles['1.2.840.10008.5.1.4.34.6.1'], ['SCP', 'SCU'])
        self.assertEqual(roles['1.2.840.10008.5.1.4.34.6.2'], ['SCP', 'SCU'])
        self.assertEqual(roles['1.2.840.10008.5.1.4.34.6.3'], ['SCP', 'SCU'])
        self.assertEqual(roles['1.2.840.10008.5.1.4.34.6.4'], ['SCP'])
        self.assertEqual(roles['1.2.840.10008.5.1.4.34.6.5'], ['SCP'])

    def test_requirement_patterns_name_cases_the_probe_can_produce(self):
        source = PROBE.read_text()
        prefixes = {'dimse_n.mpps', 'dimse_n.mwl', 'dimse_n.stgcmt', 'dimse_n.ups', 'dimse_n.print'}
        for requirement, patterns in self.mapping.items():
            for pattern in patterns:
                self.assertTrue(any(pattern.startswith(prefix) for prefix in prefixes), f'{requirement}: {pattern}')
                head = pattern.split('.*')[0].rsplit('.', 1)[-1].replace('*', '')
                self.assertTrue(head in source or fnmatch.fnmatch(pattern, 'dimse_n.*.*'), pattern)


class Independence(unittest.TestCase):
    def test_peers_share_no_pacs_code(self):
        """A peer that imports or runs PACS code would not be independent evidence."""
        for path in PEERS.glob('*.py'):
            tree = ast.parse(path.read_text())
            modules = set()
            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    modules |= {alias.name.split('.')[0] for alias in node.names}
                elif isinstance(node, ast.ImportFrom):
                    modules.add((node.module or '').split('.')[0])
            allowed = {'argparse', 'base64', 'datetime', 'fnmatch', 'hashlib', 'json', 'os', 'pathlib', 'queue', 'stat', 'sys', 'threading',
                       'time', 'urllib', 'zipfile', 'pydicom', 'pynetdicom', 'peer_common'}
            self.assertLessEqual(modules, allowed, path.name)
            text = path.read_text()
            for name in ('pacs_server', 'build/bin', 'kcenon'):
                self.assertNotIn(name, text, path.name)

    def test_the_pins_state_a_digest(self):
        text = (PEERS/'install.py').read_text()
        self.assertEqual(text.count('sha256='), 2)
        self.assertIn("version='5.35.2'", text)
        self.assertIn("version='2.1.1'", text)


if __name__ == '__main__':
    unittest.main()
