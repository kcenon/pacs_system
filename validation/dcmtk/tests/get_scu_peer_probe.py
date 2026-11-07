#!/usr/bin/env python3
"""Independent seed/readiness -> PACS C-GET against the pinned archive (#1315)."""
import argparse
import re
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import CODECS, Validation, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--profile', choices=CODECS, required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--fixtures', default='/dicom/testdata')
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--source-digest', required=True)
    v = Validation(parser.parse_args())
    v.case('fixtures', v.fixtures, phase='fixture_preparation')

    def seed():
        flags = [v.codec[4]] if v.codec[4] else []
        v.command(['storescu', '-d', *flags, '-aet', 'TEST_SCU', '-aec', 'DCMTK_PACS',
                   'pacs-server', '11112', *[p for group in v.paths.values() for p in group]])

    v.case('reference.seed', seed, requires=['fixtures'], phase='reference_preparation')
    for modality in ('ct', 'mr', 'cr'):
        v.case('reference.ready.'+modality, lambda modality=modality:v.integrity(modality),
               requires=['reference.seed'], phase='reference_preparation')

        def retrieve(modality=modality):
            destination = v.output/('pacs-get-'+modality)
            output = v.command([v.bin/'retrieve_scu', 'pacs-server', '11112', 'DCMTK_PACS',
                                '--mode', 'get', '--calling-ae', 'TEST_SCU',
                                '--study-uid', v.data[modality][0].StudyInstanceUID,
                                '--output', destination, '--timeout', '10', '--no-progress', '--verbose'])
            expected = len(v.data[modality])
            for label in ('Completed', 'Received'):
                values = re.findall(label+r':\s+(\d+)\b', output)
                require(values == [str(expected)], label+' does not match the expected instance count')
            sop_class = str(v.data[modality][0].SOPClassUID)
            require('Storage role '+sop_class+': requestor SCU=0 SCP=1' in output,
                    'Storage SCP role was not accepted for '+modality)
            return v.verify(destination, v.data[modality])

        v.case('get.'+modality, retrieve, requires=['reference.ready.'+modality])
    v.save()
    return int(any(row['status'] != 'passed' for row in v.results))


if __name__ == '__main__':
    raise SystemExit(main())
