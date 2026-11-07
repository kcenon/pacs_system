#!/usr/bin/env python3
"""Original archive storage/oracle paths against the pinned Compose peers."""
import argparse
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import CODECS, Validation


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--profile', choices=CODECS, required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--fixtures', default='/dicom/testdata')
    parser.add_argument('--peer-tests', default='/peer-tests')
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--source-digest', required=True)
    v = Validation(parser.parse_args())
    v.case('peer.preflight', v.preflight, phase='peer_preparation', chain='reference')
    v.case('fixtures', v.fixtures, phase='fixture_preparation')
    v.case('store.primary', lambda:v.command([
        v.bin/'store_scu', '-v', '-aec', 'DCMTK_PACS', 'pacs-server', '11112',
        *[p for paths in v.paths.values() for p in paths]]), requires=['fixtures', 'peer.preflight'])
    for modality in ('ct', 'mr', 'cr'):
        v.case('integrity.'+modality, lambda modality=modality:v.integrity(modality),
               requires=['store.primary'])
    v.case('store.secondary', v.secondary, requires=['fixtures', 'peer.preflight'])
    v.save()
    return int(any(row['status'] != 'passed' for row in v.results))


if __name__ == '__main__':
    raise SystemExit(main())
