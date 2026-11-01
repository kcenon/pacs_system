#!/usr/bin/env python3
"""Live fault-injection probe for #1312; requires the isolated DCMTK topology.

A zero exit verifies diagnostic scheduling, not product correctness. The normal
results report deliberately retains failed stores and any diagnostic product bugs.
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import CODECS, Validation


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', required=True)
    p.add_argument('--profile', choices=list(CODECS)[1:], required=True)
    p.add_argument('--build-dir', default='/workspace/build')
    p.add_argument('--peer-tests', default='/peer-tests')
    p.add_argument('--fixtures', default='/dicom/testdata')
    p.add_argument('--source-revision', required=True)
    p.add_argument('--source-digest', required=True)
    p.add_argument('--fail-reference-seed', action='store_true')
    args = p.parse_args()
    v = Validation(args)
    v.case('peer.preflight', v.preflight, phase='peer_preparation', chain='reference')
    v.case('fixtures', v.fixtures, phase='fixture_preparation')
    # Exercise the real compressed store command against an AE that the peer
    # refuses, rather than replacing the command with a mock success/failure.
    v.case('store.primary', lambda:v.command([
        v.bin/'store_scu','-aec','REJECT_1312','pacs-server','11112',
        *[path for paths in v.paths.values() for path in paths]]), requires=['fixtures','peer.preflight'])
    v.results[-1]['fault_injection'] = 'Peer rejects the called AE REJECT_1312'
    for mod in ('ct','mr','cr'):
        v.case('query.'+mod,lambda mod=mod:v.query(mod),requires=['store.primary'])
        v.case('get.'+mod,lambda mod=mod:v.get_scu(mod),requires=['store.primary'])
    v.case('move.scu',v.move_scu,requires=['store.primary'])

    if args.fail_reference_seed:
        def refused_seed():
            paths = v.fixture_sets['diagnostic-reference']['paths']
            v.command(['storescu',v.codec[4],'-aet','TEST_SCU','-aec','REJECT_1312',
                       'pacs-server','11112',*[path for group in paths.values() for path in group]])
        v.seed_reference = refused_seed
    v.reference_diagnostics()
    v.scp_diagnostics()
    v.save()

    rows = {row['id']:row for row in v.results}
    failures = []
    def check(condition, message):
        if not condition:
            failures.append(message)
    check(rows['store.primary']['status']=='failed' and rows['store.primary']['executed'],
          'The actual PACS store must fail under the injected rejection')
    for name in ['query.ct','query.mr','query.cr','get.ct','get.mr','get.cr','move.scu']:
        check(rows[name]['status']=='blocked' and not rows[name]['executed'] and rows[name]['tests']==0,
              name+': the original chain must remain blocked and unexecuted')
        diagnostic = rows['diagnostic.'+name]
        if args.fail_reference_seed:
            check(diagnostic['status']=='blocked' and not diagnostic['executed'] and diagnostic['tests']==0,
                  diagnostic['id']+': failed reference preparation must prevent execution')
        else:
            check(diagnostic['executed'] and diagnostic['tests']>0,
                  diagnostic['id']+': independent diagnostic did not execute')
            binary = 'query_scu' if name.startswith('query.') else 'retrieve_scu'
            check(any(c['case_id']==diagnostic['id'] and Path(c['command'][0]).name==binary
                      for c in v.command_results), diagnostic['id']+': actual product command missing')
    check(rows['diagnostic.reference.ready']['status']==('blocked' if args.fail_reference_seed else 'passed'),
          'Reference readiness status does not reflect independent object verification')
    evidence = dict(status='failed' if failures else 'passed', failures=failures,
                    scope='diagnostic scheduling only; product report remains failed',
                    injected_faults=['PACS store called AE refusal']+
                    (['DCMTK diagnostic seed called AE refusal'] if args.fail_reference_seed else []))
    (v.output/'probe-assertions.json').write_text(json.dumps(evidence,indent=2)+'\n')
    print(json.dumps(evidence,indent=2))
    return int(bool(failures))


if __name__=='__main__':
    raise SystemExit(main())
