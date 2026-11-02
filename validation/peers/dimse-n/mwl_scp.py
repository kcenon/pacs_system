#!/usr/bin/env python3
"""A Modality Worklist SCP, written from PS3.4 Annex K, that records the identifiers it receives.

Usage: mwl_scp.py --aet AE --port N --items items.json --record keys.jsonl [--fail-with HEX]

Items are flat {keyword: value} objects; the Scheduled Procedure Step attributes among them go in the
sequence. It matches single values, leading or trailing '*' wild cards and date ranges, which is what
worklist_scu sends. --fail-with answers every query with that failure status.
"""
import argparse
from fnmatch import fnmatchcase
import json
from pathlib import Path

from pydicom.dataset import Dataset
from pydicom.sequence import Sequence
from pynetdicom import AE, evt

from peer_common import SYNTAXES, append_record, to_plain

MWL = '1.2.840.10008.5.1.4.31'
STEP_KEYS = ('Modality', 'ScheduledStationAETitle', 'ScheduledProcedureStepStartDate', 'ScheduledProcedureStepStartTime',
             'ScheduledProcedureStepID', 'ScheduledProcedureStepDescription')
DATASET_MISMATCH = 0xA900


def matches(wanted, value):
    wanted, value = str(wanted).strip(), str(value).strip()
    if not wanted:
        return True
    if '-' in wanted and wanted.replace('-', '').isdigit():
        low, high = wanted.split('-')
        return (not low or low <= value) and (not high or value <= high)
    return fnmatchcase(value, wanted)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aet', required=True)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--items', required=True)
    parser.add_argument('--record', required=True)
    parser.add_argument('--fail-with')
    args = parser.parse_args()
    items = json.loads(Path(args.items).read_text())

    def on_find(event):
        keys = event.identifier
        append_record(args.record, dict(calling_ae=str(event.assoc.requestor.ae_title), keys=to_plain(keys)))
        if args.fail_with:
            yield int(args.fail_with, 16), None
            return
        steps = keys.get('ScheduledProcedureStepSequence')
        if steps is None or len(steps) != 1:
            yield DATASET_MISMATCH, None  # K.6.1.2.1: the sequence is a required matching key.
            return
        wanted_step = steps[0]
        for item in items:
            flat_keys = [element for element in keys if element.VR != 'SQ']
            step_keys = [element for element in wanted_step if element.VR != 'SQ']
            if not all(matches(element.value or '', item.get(element.keyword, '')) for element in flat_keys):
                continue
            if not all(matches(element.value or '', item.get(element.keyword, '')) for element in step_keys):
                continue
            row, step = Dataset(), Dataset()
            for element in flat_keys:
                setattr(row, element.keyword, item.get(element.keyword, ''))
            for element in step_keys:
                setattr(step, element.keyword, item.get(element.keyword, ''))
            row.ScheduledProcedureStepSequence = Sequence([step])
            yield 0xFF00, row
        yield 0x0000, None

    ae = AE(ae_title=args.aet)
    ae.acse_timeout = ae.dimse_timeout = ae.network_timeout = 30
    ae.add_supported_context(MWL, list(SYNTAXES))
    print(f'listening on {args.port}', flush=True)
    ae.start_server(('127.0.0.1', args.port), evt_handlers=[(evt.EVT_C_FIND, on_find)])


if __name__ == '__main__':
    main()
