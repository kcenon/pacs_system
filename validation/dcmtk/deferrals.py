"""Attach exception ownership to individual discovered tests without waiving them.

Source locations come from Catch2 discovery. The audit records potential SKIP
branches; only execution results establish whether a branch was actually taken.
"""
from pathlib import Path
import re


def disposition(category, owner, cause, exit_condition):
    return dict(category=category, owner_issue=owner, cause=cause,
                exit_condition=exit_condition, waives_failure=False)


def skip_disposition(text):
    if 'DCMTK not installed' in text:
        return disposition('environment', 1240, 'DCMTK executable unavailable',
                           'Run with the pinned DCMTK tools and execute the assertions.')
    if 'certificates not available' in text:
        return disposition('environment', 1317, 'Legacy TLS fixture certificates unavailable',
                           'Generate the required certificate chain before executing the TLS cases.')
    if 'TLS not properly configured' in text:
        return disposition('fixture defect', 1317, 'Legacy TLS fixture startup is not enforced',
                           'Configure the TLS endpoint and assert handshake behavior without SKIP.')
    if 'in-process' in text:
        return disposition('fixture defect', 1240, 'The fixture uses the in-process v1 server',
                           'Use the v2 TCP server and verify DIMSE with an external DCMTK process.')
    if 'no TCP transport' in text:
        return disposition('product defect', 1240, 'This build lacks association TCP transport',
                           'Build the TCP transport and execute real cross-process associations.')
    if 'C-MOVE destination' in text:
        return disposition('product defect', 1304, 'C-MOVE destination association unavailable',
                           'Deliver the exact instance set to a distinct receiver and assert final counters.')
    if 'unexpectedly in use' in text:
        return disposition('environment', 1319, 'Requested test port is occupied',
                           'Allocate an available port and complete the association assertions.')
    if 'Windows CI runners lack resources' in text:
        return disposition('environment', 1327, 'Windows concurrent pipeline resource gate',
                           'Provision a native Windows lane that executes the concurrent pipeline case.')
    return disposition('unclassified', 1313, text,
                       'Classify this exception and execute its assertions without a skip.')


def audit_deferrals(root, cases):
    root = Path(root).resolve()
    locations = {}
    for case in cases:
        if case['kind'] == 'catch2' and case.get('source_line'):
            locations.setdefault(case['source'], set()).add(int(case['source_line']))
    rows = []
    for case in cases:
        exceptions = []
        tags = case.get('tags', [])
        if '!mayfail' in tags or '!shouldfail' in tags:
            serialization = case['name'] == 'container_adapter binary serialization'
            exceptions.append(dict(kind='allowed-failure', **disposition(
                'fixture defect' if serialization else 'product defect allowance; cause unverified',
                1324 if serialization else 1327,
                'Binary roundtrip result is not asserted' if serialization else
                'Historical cross-system failure allowance; no current defect is established by this tag',
                'Assert successful deserialization and exact data equality without !mayfail.' if serialization else
                'Reproduce or retire the historical allowance after the pinned native/sanitizer matrix passes.')))
        if any(tag.startswith('.') for tag in tags):
            exceptions.append(dict(kind='hidden', **disposition(
                'environment', 1326, 'An explicit endurance/stress selection is required',
                'Execute this exact case in its required lane with recorded duration, load and cleanup.')))
        source = case.get('source')
        if isinstance(source, str) and source in locations:
            path = Path(source)
            path = path if path.is_absolute() else root/path
            try:
                path.resolve().relative_to(root)
                lines = path.read_text().splitlines()
                start = int(case['source_line'])
                end = min((line-1 for line in locations[source] if line > start), default=len(lines))
                body = '\n'.join(lines[start-1:end])
                for match in re.finditer(r'\bSKIP\((.*?)\);', body, re.DOTALL):
                    text = ' '.join(match.group(1).split())
                    exceptions.append(dict(kind='conditional-skip', source_expression=text,
                                           **skip_disposition(text)))
            except (OSError, ValueError) as exc:
                exceptions.append(dict(kind='source-unavailable', **disposition(
                    'environment', 1313, str(exc), 'Make the matching test source available for audit.')))
        # These assertions failed in the original E2E probe; never exempt them.
        if case['name'] in ('Complete MPPS workflow', 'MPPS discontinue workflow',
                            'MPPS cannot modify completed procedure'):
            exceptions.append(dict(kind='observed-product-failure', **disposition(
                'product defect', 1316, 'MPPS workflow failed in the Linux baseline',
                'Repair the workflow and pass the existing status/state/error-count assertions.')))
        case['deferrals'] = exceptions
        if exceptions:
            rows.append(dict(case_id=case['case_id'], name=case['name'], exceptions=exceptions))

    quarantine = []
    for path in sorted((root/'.github/sanitizers').glob('ctest-quarantine-*.txt')):
        for line in path.read_text().splitlines():
            name = line.strip()
            if not name or name.startswith('#'):
                continue
            matches = [case for case in cases if name in case.get('ctest_names', [])]
            known_race = name == 'stress::lockfree_queue memory safety' and path.stem.endswith('-thread')
            exception = dict(kind='sanitizer-quarantine', lane=path.stem, **disposition(
                'product defect' if known_race else 'unclassified',
                'kcenon/thread_system#761' if known_race else 1313,
                'Concurrent queue payload race tracked by the existing TSan quarantine' if known_race else
                'Quarantine requires an explicit defect owner and cause',
                'Run this exact case in its sanitizer lane without the quarantine or a matching suppression.'))
            for case in matches:
                case['deferrals'].append(exception)
            quarantine.append(dict(ctest_name=name, source=str(path.relative_to(root)),
                                   case_ids=[case['case_id'] for case in matches],
                                   resolution='resolved' if matches else 'not in this build',
                                   exception=exception))
    return dict(cases=rows, quarantine=quarantine,
                note='Potential exceptions are audited, not granted. Skipped, missing and failing executions still fail the gate.')
