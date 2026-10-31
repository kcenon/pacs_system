"""Evidence and release-readiness gate; Python standard library only."""
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def junit_counts(path):
    root = ET.parse(path).getroot()
    cases = list(root.iter('testcase'))
    if not cases:
        raise ValueError('JUnit contains no test cases')
    return dict(tests=len(cases),
                failed=sum(c.find('failure') is not None or c.find('error') is not None for c in cases),
                skipped=sum(c.find('skipped') is not None or c.get('status') in ('notrun', 'disabled') for c in cases))


def write_junit(path, cases):
    root = ET.Element('testsuite', name='pacs-dcmtk-validation', tests=str(len(cases)),
                      failures=str(sum(c['status'] != 'passed' for c in cases)))
    for result in cases:
        case = ET.SubElement(root, 'testcase', classname='pacs.validation', name=result['id'],
                             time=str(result.get('seconds', 0)))
        if result['status'] != 'passed':
            ET.SubElement(case, 'failure', type=result['status'],
                          message=result.get('detail', result['status'])).text = result.get('detail', '')
        ET.SubElement(case, 'system-out').text = json.dumps(result, ensure_ascii=False)
    ET.ElementTree(root).write(path, encoding='utf-8', xml_declaration=True)


def passed(result):
    return (isinstance(result, dict) and result.get('status') == 'passed'
            and type(result.get('tests')) is int and result['tests'] > 0
            and type(result.get('skipped')) is int and result['skipped'] == 0
            and type(result.get('failed')) is int and result['failed'] == 0)


def evaluate(manifest, reports, provenance):
    """Missing/empty/skipped evidence cannot certify a feature or a complete run."""
    errors, seen, evidence = [], set(), {}
    profiles = manifest['profiles']
    for report in reports:
        if not isinstance(report, dict):
            errors.append('invalid report object')
            continue
        profile = report.get('profile')
        if not isinstance(profile, str) or profile not in profiles or profile in seen:
            errors.append(f'duplicate or unknown profile: {profile}')
            continue
        seen.add(profile)
        for field, expected in provenance.items():
            if report.get(field) != expected:
                errors.append(f'{profile}: mismatched {field}')
        local = {}
        results = report.get('cases', [])
        if not isinstance(results, list):
            errors.append(f'{profile}: invalid case list')
            results = []
        for result in results:
            if not isinstance(result, dict):
                errors.append(f'{profile}: invalid case object')
                continue
            key = result.get('id')
            if not isinstance(key, str) or key in local:
                errors.append(f'{profile}: duplicate/invalid case ID: {key}')
                continue
            local[key] = result
        required = list(manifest['interop_cases'])
        if profile == 'uncompressed':
            required += [c['id'] for c in manifest['components']]
        for key in required:
            result = local.get(key)
            if result is None:
                errors.append(f'{profile}: missing {key}')
            elif not passed(result):
                errors.append(f'{profile}: {key}: {result.get("status")}: {result.get("detail", "invalid counts")}')
            evidence[f'{profile}/{key}'] = result
        for key in local.keys() - set(required):
            if local[key].get('status') != 'passed':
                errors.append(f'{profile}: additional failure {key}')
    for profile in set(profiles) - seen:
        errors.append(f'missing profile: {profile}')
    gaps = [dict(feature=f['id'], requirement=g) for f in manifest['features'] for g in f.get('gaps', [])]
    features = []
    for feature in manifest['features']:
        required = [f'uncompressed/{key}' for key in feature['components']]
        required += [f'{p}/{key}' for p in profiles for key in feature['interop']]
        bad = [k for k in required if not passed(evidence.get(k))]
        features.append(dict(id=feature['id'], status='failed' if bad else
                             'incomplete' if feature.get('gaps') or not required else 'passed',
                             missing_or_failed=bad, gaps=feature.get('gaps', [])))
    return dict(schema_version=1, status='passed' if not errors and not gaps else 'failed',
                validation_errors=errors, coverage_gaps=gaps, features=features,
                provenance=provenance)


def validate_manifest(manifest):
    components = [c['id'] for c in manifest['components']]
    interop = manifest['interop_cases']
    ids = components + interop
    if not ids or len(ids) != len(set(ids)):
        raise ValueError('case IDs must be unique and nonempty')
    if not manifest['profiles'] or len(set(manifest['profiles'])) != len(manifest['profiles']):
        raise ValueError('profiles must be nonempty and unique')
    if 'uncompressed' not in manifest['profiles']:
        raise ValueError('uncompressed component profile is mandatory')
    features = manifest['features']
    if not features or len({f['id'] for f in features}) != len(features):
        raise ValueError('feature IDs must be nonempty and unique')
    used = set()
    for f in features:
        if not f['components'] and not f['interop'] and not f.get('gaps'):
            raise ValueError(f'{f["id"]}: no evidence or explicit coverage gap')
        if set(f['components']) - set(components) or set(f['interop']) - set(interop):
            raise ValueError(f'{f["id"]}: unknown evidence ID')
        used.update(f['components'] + f['interop'])
    if set(ids) - used:
        raise ValueError(f'unmapped evidence: {set(ids)-used}')


def save_summary(output, summary):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    (output/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    lines = [f'PACS full validation: {summary["status"].upper()}', '',
             'Feature states (component tests do not establish external interoperability):']
    lines += [f'{f["id"]}: {f["status"]}' for f in summary['features']]
    lines += ['', 'Validation errors:'] + summary['validation_errors']
    lines += ['', 'Outstanding coverage requirements:']
    lines += [f'{g["feature"]}: {g["requirement"]}' for g in summary['coverage_gaps']]
    (output/'summary.txt').write_text('\n'.join(lines)+'\n')
    cases = [dict(id=f['id'], status=f['status'],
                  detail='; '.join(f['missing_or_failed'] + f['gaps'])) for f in summary['features']]
    cases += [dict(id=f'gate-{i}', status='failed', detail=e)
              for i,e in enumerate(summary['validation_errors'])]
    write_junit(output/'summary.xml', cases)
