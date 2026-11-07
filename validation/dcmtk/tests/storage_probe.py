#!/usr/bin/env python3
"""Independent DCMTK sender -> PACS storage -> DCMTK decoder regression (#1292)."""
import argparse
from pathlib import Path
import sys

import pydicom

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
    args = parser.parse_args()
    v = Validation(args)
    v.case('fixtures', v.fixtures, phase='fixture_preparation')

    def receive():
        directory = v.output/'received'
        directory.mkdir()
        paths = [p for group in v.paths.values() for p in group]
        with v.server([v.bin/'store_scp', '22502', 'PACS_STORE', '--storage-dir', directory], 22502) as process:
            flags = [v.codec[4]] if v.codec[4] else []
            v.command(['storescu', '-d', *flags, '-aec', 'PACS_STORE', '127.0.0.1', '22502', *paths])
            detail = v.verify(directory, [ds for group in v.data.values() for ds in group])
            sources = {str(ds.SOPInstanceUID): ds for ds in map(pydicom.dcmread, paths)}
            for path in directory.rglob('*.dcm'):
                received = pydicom.dcmread(path)
                source = sources[str(received.SOPInstanceUID)]
                require(received.PixelData == source.PixelData, 'Pixel item bytes or native payload changed')
                require(received['PixelData'].is_undefined_length == bool(v.codec[2]),
                        'Pixel Data length form does not match the negotiated syntax')
        require(process.returncode == 0, f'Receiver shutdown failed: {process.returncode}')
        return detail+'; exact Pixel Data item bytes and length form preserved'

    v.case('storage.receive', receive, requires=['fixtures'])
    v.save()
    return int(any(row['status'] != 'passed' for row in v.results))


if __name__ == '__main__':
    raise SystemExit(main())
