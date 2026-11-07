#!/usr/bin/env python3
"""Focused storage wire/restart paths; query/retrieve acceptance is separate."""
import argparse
import copy
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
    parser.add_argument('--outbound-only', action='store_true')
    v = Validation(parser.parse_args())
    v.case('fixtures', v.fixtures, phase='fixture_preparation')

    def send_to_reference(paths, expected, name, port):
        directory = v.output/name
        directory.mkdir()
        with v.server(['storescp', '-d', v.codec[3], '-aet', 'REFERENCE',
                       '-od', directory, port], port,
                      log_name=v.current+'-reference'):
            output = v.command([v.bin/'store_scu', '-v', '-aec', 'REFERENCE',
                                '127.0.0.1', port, *paths])
            require('Accepted presentation contexts:' in output, 'Missing negotiation evidence')
            accepted = output.split('Accepted presentation contexts:', 1)[1].split('\n\n', 1)[0]
            contexts = dict(re.findall(r'^\s+\[\d+\]\s+(\S+):\s+(\S+)', accepted, re.M))
            syntax = v.codec[2] or '1.2.840.10008.1.2.1'
            require(all(contexts.get(str(ds.SOPClassUID)) == syntax for ds in expected),
                    'Requested syntax was not negotiated for every storage SOP class')
            detail = v.verify(directory, expected)
            originals = {str(ds.SOPInstanceUID): ds for ds in map(pydicom.dcmread, paths)}
            for path in directory.rglob('*'):
                if path.is_file():
                    ds = pydicom.dcmread(path)
                    require(ds.PixelData == originals[str(ds.SOPInstanceUID)].PixelData,
                            'Wire transfer changed Pixel Data item bytes')
        return detail+'; negotiated syntax and exact item bytes match'

    def outbound():
        return send_to_reference([p for group in v.paths.values() for p in group],
                                 [ds for group in v.data.values() for ds in group],
                                 'outbound', 22503)

    v.case('wire.outbound', outbound, requires=['fixtures'])
    if v.args.outbound_only:
        return int(any(row['status'] != 'passed' for row in v.results))

    archive = v.output/'archive'
    server_args = [v.bin/'store_scp', '22502', 'PACS_STORE', '--storage-dir', archive]

    def incoming():
        archive.mkdir()
        with v.server(server_args, 22502) as process:
            flags = [v.codec[4]] if v.codec[4] else []
            v.command(['storescu', '-d', *flags, '-aec', 'PACS_STORE', '127.0.0.1',
                       '22502', *[p for group in v.paths.values() for p in group]])
            detail = v.verify(archive, [ds for group in v.data.values() for ds in group])
        require(process.returncode == 0, f'Receiver shutdown exit {process.returncode}')
        return detail

    v.case('wire.incoming', incoming, requires=['fixtures'])

    def new_instance(suffix, name):
        ds = copy.deepcopy(v.data['ct'][0])
        ds.StudyInstanceUID = v.run_uid+'.'+suffix
        ds.SeriesInstanceUID = ds.StudyInstanceUID+'.1'
        ds.SOPInstanceUID = ds.SeriesInstanceUID+'.1'
        ds.file_meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
        native, wire = v.output/(name+'-native.dcm'), v.output/(name+'-wire.dcm')
        ds.save_as(native)
        if v.codec[0]:
            v.command([*v.codec[0], '+un', native, wire])
        else:
            wire.write_bytes(native.read_bytes())
        return ds, wire

    def restart_forward():
        archived_paths = sorted(archive.rglob('*.dcm'))
        expected = [ds for group in v.data.values() for ds in group]
        readiness, wire = new_instance('8', 'restart-readiness')
        with v.server(server_args, 22502) as process:
            # store_scp registers Storage, not Verification. Exercise its actual
            # service after restart without replacing any of the original UIDs.
            flags = [v.codec[4]] if v.codec[4] else []
            v.command(['storescu', '-d', *flags, '-aec', 'PACS_STORE', '127.0.0.1', '22502', wire])
            v.verify(archive, expected+[readiness])
            detail = send_to_reference(archived_paths, expected,
                                      'after-restart', 22504)
        require(process.returncode == 0, f'Restarted receiver shutdown exit {process.returncode}')
        return detail

    v.case('wire.restart-forward', restart_forward, requires=['wire.incoming'])

    def secondary():
        ds, wire = new_instance('9', 'secondary')
        return send_to_reference([wire], [ds], 'separate-destination', 22505)

    v.case('wire.separate-destination', secondary, requires=['fixtures'])
    v.save()
    return int(any(row['status'] != 'passed' for row in v.results))


if __name__ == '__main__':
    raise SystemExit(main())
