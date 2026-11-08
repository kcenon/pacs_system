#!/usr/bin/env python3
"""Independent DCMTK getscu -> public qr_scp/import, exact C-GET delivery (#1306)."""
import argparse
import re
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
    v = Validation(parser.parse_args())
    v.case('fixtures', v.fixtures, phase='fixture_preparation')
    args = [v.bin/'qr_scp', '22503', 'PACS_QR', '--storage-dir', v.output/'inputs',
            '--index-db', v.output/'index.db']

    def imported():
        require('Scan complete: 13 files indexed\n' in v.command([*args, '--scan-only']),
                'Public import did not index all objects without errors')
        return '13 instances imported through the public CLI'

    v.case('import', imported, requires=['fixtures'])
    for modality in ('ct', 'mr', 'cr'):
        def retrieve(modality=modality):
            directory = v.output/('received-'+modality)
            directory.mkdir()
            with v.server(args, 22503, log_name=v.current+'-pacs') as process:
                v.command(['getscu', '-d', '-S', v.codec[3], '-to', '5', '-ta', '5', '-td', '10',
                           '-aet', 'GET_SCU', '-aec', 'PACS_QR', '-od', directory,
                           '127.0.0.1', '22503', '-k', 'QueryRetrieveLevel=STUDY',
                           '-k', 'StudyInstanceUID='+v.data[modality][0].StudyInstanceUID])
            require(process.returncode == 0, f'PACS shutdown exit {process.returncode}')
            detail = v.verify(directory, v.data[modality])
            log = (v.output/(v.current+'.log')).read_text()
            for label, wanted in [('Completed', len(v.data[modality])), ('Failed', 0), ('Warning', 0)]:
                require(re.findall(label+r' Suboperations\s*:\s*(\d+)', log)[-1:] == [str(wanted)],
                        'Wrong final '+label+' count')
            originals = {str(ds.SOPInstanceUID): ds for ds in map(pydicom.dcmread, v.paths[modality])}
            for path in directory.rglob('*'):
                if path.is_file():
                    ds = pydicom.dcmread(path)
                    require(ds.PixelData == originals[str(ds.SOPInstanceUID)].PixelData,
                            'Stored item/native pixel bytes changed')
            return detail+'; final counters and exact item bytes match'
        v.case('get.'+modality, retrieve, requires=['import'])
    v.save()
    return int(any(row['status'] != 'passed' for row in v.results))


if __name__ == '__main__':
    raise SystemExit(main())
