"""Evidence and release-readiness gate; Python standard library only."""
import hashlib
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from inventory import evaluate_inventory


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


# The only reasons a component may stay out of a lane (#1328): a pull request run has no provider account, cannot
# wait for an endurance run, and runs on one platform. A lane that leaves any component out certifies nothing about
# completeness; only "full" does.
LANE_EXCLUSION_CATEGORIES = ('provider', 'endurance', 'platform')


def components_in_lane(manifest, lane):
    """IDs of the components a lane runs. A component without "lanes" belongs to every lane."""
    return [spec['id'] for spec in manifest['components'] if lane in spec.get('lanes', manifest['lanes'])]


def evaluate(manifest, reports, provenance):
    """Missing/empty/skipped evidence cannot certify a feature or a complete run.

    provenance['lane'] (default "full") selects the components required in the uncompressed profile. Every lane
    requires all interop cases in every profile and fails on any selected case that did not pass. Only the full
    lane also fails on declared coverage gaps; another lane reports them, and the components it left out, as not
    certified."""
    errors, seen, evidence, inventories = [], set(), {}, {}
    profiles = manifest['profiles']
    lane = provenance.get('lane', 'full')
    if lane not in manifest['lanes']:
        errors.append(f'unknown lane: {lane}')
    selected = set(components_in_lane(manifest, lane))
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
            required += [c['id'] for c in manifest['components'] if c['id'] in selected]
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
        if profile == 'uncompressed' and manifest.get('individual_test_inventory'):
            try:
                assessment = evaluate_inventory(report.get('test_inventory'),
                    {key: value.get('case_results') for key,value in local.items()})
            except (KeyError, TypeError, ValueError, AttributeError) as exc:
                assessment = dict(status='failed',errors=[f'invalid inventory evidence: {exc}'])
            inventories[profile] = assessment
            errors += [f'{profile}: inventory: {error}' for error in assessment['errors']]
    for profile in set(profiles) - seen:
        errors.append(f'missing profile: {profile}')
    gaps = [dict(feature=f['id'], requirement=g) for f in manifest['features'] for g in f.get('gaps', [])]
    outside = [dict(id=spec['id'], lanes=spec.get('lanes', sorted(manifest['lanes'])), **spec.get('lane_exclusion', {}))
               for spec in manifest['components'] if spec['id'] not in selected]
    features = []
    for feature in manifest['features']:
        required = [f'uncompressed/{key}' for key in feature['components'] if key in selected]
        required += [f'{p}/{key}' for p in profiles for key in feature['interop']]
        unselected = [key for key in feature['components'] if key not in selected]
        bad = [k for k in required if not passed(evidence.get(k))]
        features.append(dict(id=feature['id'], status='failed' if bad else
                             'incomplete' if feature.get('gaps') or unselected or not required else 'passed',
                             missing_or_failed=bad, gaps=feature.get('gaps', []), outside_lane=unselected))
    certifies = lane == 'full'
    return dict(schema_version=1, status='passed' if not errors and (not gaps or not certifies) else 'failed',
                lane=lane, certifies_completeness=certifies, outside_lane=outside,
                validation_errors=errors, coverage_gaps=gaps, features=features,
                provenance=provenance, test_inventories=inventories)


def validate_manifest(manifest):
    if manifest.get('individual_test_inventory'):
        requirements = manifest.get('support_requirements', [])
        if not requirements or len({r['case_id'] for r in requirements}) != len(requirements):
            raise ValueError('support requirement IDs must be nonempty and unique')
        fields = ('case_id','feature_id','support_status','condition','expected_outcome',
                  'oracle','fixture','execution_lane','owner_issue','evidence_schema')
        features_by_id = {f['id'] for f in manifest['features']}
        for requirement in requirements:
            if any(not requirement.get(field) for field in fields):
                raise ValueError('support requirement missing an evaluation or execution contract')
            if requirement['feature_id'] not in features_by_id:
                raise ValueError('support requirement references unknown feature')
        for feature in features_by_id:
            conditions = {r['condition'] for r in requirements if r['feature_id'] == feature}
            if not {'positive','negative'} <= conditions:
                raise ValueError(f'{feature}: positive and negative requirements are mandatory')
    lanes = manifest.get('lanes')
    if (not isinstance(lanes, dict) or 'full' not in lanes
            or any(not isinstance(text, str) or not text for text in lanes.values())):
        raise ValueError('lanes must describe every lane and include "full"')
    for spec in manifest['components']:
        if 'lanes' not in spec:
            if 'lane_exclusion' in spec:
                raise ValueError(f'{spec["id"]}: lane_exclusion without lanes')
            continue
        declared = spec['lanes']
        if (not isinstance(declared, list) or not declared or len(set(declared)) != len(declared)
                or set(declared) - set(lanes)):
            raise ValueError(f'{spec["id"]}: lanes must be a nonempty list of declared lanes')
        if 'full' not in declared:
            raise ValueError(f'{spec["id"]}: every component belongs to the full lane')
        exclusion = spec.get('lane_exclusion')
        if set(lanes) - set(declared):
            if (not isinstance(exclusion, dict) or exclusion.get('category') not in LANE_EXCLUSION_CATEGORIES
                    or not isinstance(exclusion.get('reason'), str) or not exclusion['reason']):
                raise ValueError(f'{spec["id"]}: a component left out of a lane needs lane_exclusion with a '
                                 f'category in {LANE_EXCLUSION_CATEGORIES} and a reason')
        elif exclusion is not None:
            raise ValueError(f'{spec["id"]}: lane_exclusion on a component in every lane')
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
    lane, outside = summary.get('lane', 'full'), summary.get('outside_lane', [])
    if summary.get('certifies_completeness', True):
        lines = [f'PACS full validation: {summary["status"].upper()}']
    else:
        lines = [f'PACS validation, lane {lane}: {summary["status"].upper()}',
                 f'Completeness: not certified by this lane; the full lane judges it '
                 f'({len(summary["coverage_gaps"])} coverage gaps, {len(outside)} required components outside this lane).']
    lines += ['', 'Feature states (component tests do not establish external interoperability):']
    lines += [f'{f["id"]}: {f["status"]}' for f in summary['features']]
    lines += ['', 'Validation errors:'] + summary['validation_errors']
    lines += ['', 'Outstanding coverage requirements:']
    lines += [f'{g["feature"]}: {g["requirement"]}' for g in summary['coverage_gaps']]
    if outside:
        lines += ['', f'Required outside lane {lane}:']
        lines += [f'{c["id"]} (lanes: {", ".join(c["lanes"])}; {c.get("category")}): {c.get("reason")}' for c in outside]
    (output/'summary.txt').write_text('\n'.join(lines)+'\n')
    cases = [dict(id=f['id'], status=f['status'],
                  detail='; '.join(f['missing_or_failed'] + f['gaps'])) for f in summary['features']]
    cases += [dict(id=f'gate-{i}', status='failed', detail=e)
              for i,e in enumerate(summary['validation_errors'])]
    write_junit(output/'summary.xml', cases)
