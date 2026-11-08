"""Standard-library checks of the Query/Retrieve tables; running them is qr_matrix_probe.py's job."""
import fnmatch
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
from qr_matrix import (FIND_REFUSALS, IDENTITY, INSTANCES, MODELS, PATIENTS, REFERENCE_DIFFERENCES,
                       RETRIEVE_CASES, RETRIEVE_REFUSALS, RETURN_KEYS, SERIES, SERIES_BY_KEY, STUDIES, STUDY_BY_KEY, UNSUPPORTED_KEYS,
                       case_id, find_cases, instance_uid, requirement_cases, series_uid, study_uid)
from support import declared_support

LEVEL_KEYS = {'PATIENT': {row.key for row in PATIENTS}, 'STUDY': {row.key for row in STUDIES},
              'SERIES': {row.key for row in SERIES}, 'IMAGE': {row.key for row in INSTANCES}}


class Tables(unittest.TestCase):
    def test_case_identifiers_are_unique(self):
        ids = [case_id('find', row) for row in find_cases()]
        self.assertEqual(len(ids), len(set(ids)))
        ids = [case_id('retrieve', row) for row in RETRIEVE_CASES]
        self.assertEqual(len(ids), len(set(ids)))
        for table in (FIND_REFUSALS, RETRIEVE_REFUSALS):
            names = [row.name for row in table]
            self.assertEqual(len(names), len(set(names)))

    def test_every_model_reaches_every_level_it_has(self):
        covered = {(row.model, row.level) for row in find_cases()}
        for model, spec in MODELS.items():
            for level in spec['levels']:
                self.assertIn((model, level), covered)
        retrieved = {(row.model, row.level) for row in RETRIEVE_CASES}
        for model in ('patient-root', 'study-root'):
            for level in MODELS[model]['levels']:
                self.assertIn((model, level), retrieved)

    def test_expected_keys_exist_and_levels_are_valid(self):
        for row in find_cases():
            self.assertIn(row.level, MODELS[row.model]['levels'], row.name)
            self.assertLessEqual(set(row.expected), LEVEL_KEYS[row.level], row.name)
        for row in RETRIEVE_CASES:
            self.assertLessEqual(set(row.expected), LEVEL_KEYS['IMAGE'], row.name)

    def test_results_exclude_something_so_ignored_keys_fail(self):
        """Except for universal requests, a server returning everything must miss the expectation."""
        for row in find_cases():
            if row.name == 'universal' or set(row.keys) <= {'SpecificCharacterSet'} or row.name.startswith('unsupported-key'):
                continue
            self.assertNotEqual(set(row.expected), LEVEL_KEYS[row.level], f'{row.model} {row.level} {row.name}')

    def test_each_level_has_a_matching_and_a_nonmatching_case(self):
        for level, keys in LEVEL_KEYS.items():
            rows = [row for row in find_cases() if row.level == level]
            self.assertTrue(any(row.expected for row in rows), level)
            self.assertTrue(any(not row.expected for row in rows), level)

    def test_uids_follow_the_hierarchy(self):
        for instance in INSTANCES:
            series = SERIES_BY_KEY[instance.series]
            study = STUDY_BY_KEY[series.study]
            self.assertTrue(instance_uid(instance.key).startswith(series_uid(series.key)+'.'))
            self.assertTrue(series_uid(series.key).startswith(study_uid(study.key)+'.'))
        uids = [instance_uid(row.key) for row in INSTANCES]
        self.assertEqual(len(uids), len(set(uids)))

    def test_every_level_has_an_identity_and_return_keys(self):
        for level in LEVEL_KEYS:
            self.assertIn(IDENTITY[level], RETURN_KEYS[level])

    def test_unsupported_keys_are_not_held(self):
        self.assertFalse(UNSUPPORTED_KEYS & {key for keys in RETURN_KEYS.values() for key in keys})

    def test_reference_differences_name_real_rows(self):
        known = {case_id('find', row).replace('find.', 'find.', 1) for row in find_cases()}
        known |= {f'find.refuse.{row.name}' for row in FIND_REFUSALS}
        for operation in ('get', 'move'):
            known |= {f'{operation}.{row.model}.{row.level.lower()}.{row.name}' for row in RETRIEVE_CASES}
            known |= {f'{operation}.refuse.{row.name}' for row in RETRIEVE_REFUSALS}
        self.assertEqual(set(REFERENCE_DIFFERENCES)-known, set())
        for reason in REFERENCE_DIFFERENCES.values():
            self.assertTrue(reason)


class Requirements(unittest.TestCase):
    def setUp(self):
        self.declared = declared_support(ROOT)
        self.mapping = requirement_cases(self.declared)

    def test_every_owned_declaration_has_named_cases(self):
        owned = {case['case_id'] for case in self.declared['cases'] if case.get('owner_issue') == 1319}
        self.assertEqual(owned-set(self.mapping), set())

    def test_every_pattern_could_name_a_case(self):
        ids = [case_id('qr.find.qr-scp', row) for row in find_cases()]
        ids += [f'qr.find.qr-scp.refuse.{row.name}' for row in FIND_REFUSALS]
        for operation in ('get', 'move'):
            ids += [case_id(f'qr.{operation}.qr-scp', row) for row in RETRIEVE_CASES]
            ids += [f'qr.{operation}.qr-scp.refuse.{row.name}' for row in RETRIEVE_REFUSALS]
        for requirement, patterns in self.mapping.items():
            if 'qr.find.' in ''.join(patterns) and 'SCP' in requirement:
                matched = [case for case in ids if any(fnmatch.fnmatchcase(case, pattern) for pattern in patterns)]
                self.assertTrue(matched, requirement)

    def test_the_retired_model_is_scp_only(self):
        retired = next(sop for sop in self.declared['sop_classes'] if sop['uid'] == MODELS['patient-study-only']['find'])
        self.assertEqual(retired['roles'], ['SCP'])


if __name__ == '__main__':
    unittest.main()
