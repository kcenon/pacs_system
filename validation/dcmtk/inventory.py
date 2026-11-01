"""Match discovered test identities to selected jobs and actual test results.

Catch2 JUnit names include section paths, so they cannot identify TEST_CASEs
unambiguously. Use the native XML reporter for Catch2 and JUnit for CTest.
"""
from collections import Counter
import json
from pathlib import Path
import re
from urllib.parse import quote
import xml.etree.ElementTree as ET


def identity(kind, binary, name):
    return '/'.join(quote(value, safe='') for value in (kind, binary, name))


def unique_names(records):
    names = [record['name'] for record in records]
    if any(not isinstance(name, str) or not name for name in names):
        raise ValueError('missing test name')
    duplicates = [name for name, count in Counter(names).items() if count > 1]
    if duplicates:
        raise ValueError(f'duplicate test names: {duplicates}')
    return records


def result_key(record, kind):
    if kind == 'ctest':
        return record['name']
    return json.dumps([record['name'], sorted(record.get('tags', []))], separators=(',', ':'))


def unique_catch(records):
    keys = [result_key(record, 'catch2') for record in records]
    if any(not record.get('name') for record in records) or len(keys) != len(set(keys)):
        raise ValueError('missing or duplicate Catch2 name/tag identity')
    return records


def catch_listing(text):
    root = ET.fromstring(text)
    if root.tag != 'MatchingTests':
        raise ValueError('expected Catch2 MatchingTests discovery document')
    return unique_catch([
        dict(name=case.findtext('Name'), tags=re.findall(r'\[([^]]+)\]', case.findtext('Tags', '')),
             source=case.findtext('SourceInfo/File', ''), line=case.findtext('SourceInfo/Line', ''))
        for case in root.findall('TestCase')])


def catch_results(path):
    root = ET.parse(path).getroot()
    if root.tag != 'Catch2TestRun' or root.find('OverallResultsCases') is None:
        raise ValueError('incomplete Catch2 execution document')
    results = []
    for case in root.findall('TestCase'):
        overall = case.find('OverallResult')
        if overall is None:
            raise ValueError(f'incomplete Catch2 case: {case.get("name")}')
        skipped = int(overall.get('skips', '0')) > 0
        # !mayfail must not turn failing assertions into conformance evidence.
        failed = overall.get('success') != 'true' or any(
            node.get('success') == 'false' for node in case.iter('Expression'))
        failed = failed or any(case.find('.//' + tag) is not None
                               for tag in ('Exception', 'FatalErrorCondition', 'Failure'))
        failed = failed or any(int(node.get('failures', '0')) > 0
                               or int(node.get('expectedFailures', '0')) > 0
                               for node in case.iter('OverallResults'))
        results.append(dict(name=case.get('name'),
                            tags=re.findall(r'\[([^]]+)\]', case.get('tags', '')),
                            status='failed' if failed else 'skipped' if skipped else 'passed'))
    unique_catch(results)
    overall = root.find('OverallResultsCases')
    total = sum(int(overall.get(key, '0')) for key in ('successes', 'failures', 'expectedFailures', 'skips'))
    if not results or total != len(results):
        raise ValueError('Catch2 case totals do not match executed identities')
    # Some reporters omit assertion details for allowed failures. Preserve the
    # aggregate failure as well instead of attributing it to an arbitrary case.
    assertions = root.find('OverallResults')
    if assertions is not None and int(assertions.get('expectedFailures', '0')):
        if not any(result['status'] == 'failed' for result in results):
            raise ValueError('Catch2 allowed failures have no attributable failing case')
    return results


def ctest_results(path):
    root = ET.parse(path).getroot()
    if root.tag not in ('testsuite', 'testsuites'):
        raise ValueError('expected JUnit test suite')
    results = []
    for case in root.iter('testcase'):
        failed = case.find('failure') is not None or case.find('error') is not None
        skipped = case.find('skipped') is not None or case.get('status') in ('notrun', 'disabled')
        results.append(dict(name=case.get('name'),
                            status='failed' if failed else 'skipped' if skipped else 'passed'))
    if not results:
        raise ValueError('JUnit contains no test cases')
    return unique_names(results)


def build_inventory(manifest, ctest, catch_all, selections, configuration):
    """Selections are produced by each runner itself, never by emulating filters."""
    records, aliases, jobs, errors = {}, {}, {}, []
    components = {spec['id']: spec for spec in manifest['components']
                  if spec.get('kind') not in ('discovery', 'corpus', 'qr-matrix')}
    owners = {job: [feature['id'] for feature in manifest['features'] if job in feature['components']]
              for job in components}
    for binary, tests in catch_all.items():
        unique_catch(tests)
        if not tests:
            errors.append(f'{binary}: no Catch2 tests discovered')
        features = sorted({feature for job, spec in components.items()
                           if spec.get('binary') == binary for feature in owners[job]})
        for test in tests:
            key = identity('catch2', binary, result_key(test, 'catch2'))
            records[key] = dict(case_id=key, name=test['name'], kind='catch2', binary=binary,
                                tags=test['tags'], source=test['source'], source_line=test.get('line'), feature_ids=features,
                                jobs=[], ctest_names=[], configuration=configuration,
                                support_status='component', sop_class_uids=[], roles=[], syntaxes=[],
                                fixture=test['source'], oracle='Catch2 assertions including allowed failures',
                                prerequisites=[], expected_outcome='passed', execution_lane='component',
                                evidence_schema='Catch2 XML v3')
    counts = Counter(test['name'] for test in ctest['tests'])
    errors += [f'duplicate CTest registration: {name} ({count})'
               for name,count in counts.items() if count > 1]
    registrations = []
    for index, test in enumerate(ctest['tests'], 1):
        command = test.get('command', [])
        binary = Path(command[0]).name if command else ''
        # CatchAddTests escapes characters special to Catch's test filter grammar.
        name = re.sub(r'\\([\[\],\\])', r'\1', command[1]) if len(command) > 1 else ''
        candidates = [key for key,record in records.items()
                      if record['kind'] == 'catch2' and record['binary'] == binary and record['name'] == name]
        if candidates:
            for key in candidates:
                if test['name'] not in records[key]['ctest_names']:
                    records[key]['ctest_names'].append(test['name'])
        else:
            key = identity('ctest', str(index) if counts[test['name']] > 1 else '', test['name'])
            records[key] = dict(case_id=key, name=test['name'], kind='ctest', binary=binary,
                                tags=[], source=command, feature_ids=[], jobs=[],
                                ctest_names=[test['name']], configuration=configuration,
                                support_status='component', sop_class_uids=[], roles=[], syntaxes=[],
                                fixture=command, oracle='CTest process result and JUnit status',
                                prerequisites=[], expected_outcome='passed', execution_lane='component',
                                evidence_schema='CTest JUnit')
            candidates = [key]
        aliases.setdefault(test['name'], set()).update(candidates)
        registrations.append(dict(registration_id=index, name=test['name'], case_ids=candidates))
    for job, spec in components.items():
        selected = selections.get(job)
        if selected is None:
            errors.append(f'{job}: missing selection discovery')
            selected = []
        is_ctest = spec.get('kind') == 'ctest'
        (unique_names if is_ctest else unique_catch)(selected)
        mapping = {}
        for test in selected:
            name = result_key(test, 'ctest' if is_ctest else 'catch2')
            candidates = aliases.get(name, set()) if is_ctest else {
                identity('catch2', spec['binary'], name)}
            if len(candidates) != 1:
                errors.append(f'{job}: ambiguous or absent selection: {name}')
                continue
            key = next(iter(candidates))
            if key not in records:
                errors.append(f'{job}: selection not in discovery: {name}')
                continue
            mapping[name] = key
            records[key]['jobs'].append(job)
            records[key]['feature_ids'] = sorted(set(records[key]['feature_ids'] + owners[job]))
        if not mapping:
            errors.append(f'{job}: empty test selection')
        jobs[job] = dict(case_ids=sorted(mapping.values()), result_names=mapping,
                         kind='ctest' if is_ctest else 'catch2')
    for record in records.values():
        record['assignment_status'] = 'assigned' if record['jobs'] else 'unverified'
        if not record['jobs']:
            record['unverified_reason'] = 'Discovered test excluded from every current execution filter'
            record['execution_lane'] = 'full-required-unassigned'
    return dict(schema_version=1, configuration=configuration, cases=list(records.values()),
                jobs=jobs, discovery_errors=errors, ctest_registered=len(ctest['tests']),
                ctest_registrations=registrations)


def attach_results(inventory, job, results):
    """Do not discard unexpected names: they must fail the set comparison."""
    mapping = inventory['jobs'][job]['result_names']
    kind = inventory['jobs'][job]['kind']
    return [dict(result, case_id=mapping.get(result_key(result, kind)), job=job) for result in results]


def evaluate_inventory(inventory, executions):
    if not isinstance(inventory, dict) or inventory.get('schema_version') != 1:
        return dict(status='failed', errors=['missing/invalid individual test inventory'])
    errors = list(inventory.get('discovery_errors', []))
    records = inventory.get('cases', [])
    ids = [record['case_id'] for record in records]
    if not ids or len(ids) != len(set(ids)):
        errors.append('empty or duplicate discovered case IDs')
    unassigned = [record['case_id'] for record in records if not record['jobs']]
    errors += [f'unassigned test: {key}' for key in unassigned]
    executed = set()
    for job, expected in inventory.get('jobs', {}).items():
        actual = executions.get(job)
        if not isinstance(actual, list):
            errors.append(f'{job}: missing individual execution results')
            continue
        names = [result_key(result, expected['kind']) for result in actual]
        if len(names) != len(set(names)):
            errors.append(f'{job}: duplicate executed test names')
        expected_names = expected['result_names']
        for name in set(expected_names) - set(names):
            errors.append(f'{job}: test not executed: {name}')
        for result in actual:
            name = result_key(result, expected['kind'])
            key = expected_names.get(name)
            if key is None or result.get('case_id') != key:
                errors.append(f'{job}: unknown or mismatched result identity: {name}')
                continue
            executed.add(key)
            if result.get('status') != 'passed':
                errors.append(f'{job}: {name}: {result.get("status")}')
    return dict(status='failed' if errors else 'passed', discovered=len(ids),
                assigned=len(ids)-len(unassigned), executed=len(executed),
                unassigned=unassigned, unexecuted=sorted(set(ids)-executed), errors=errors)
