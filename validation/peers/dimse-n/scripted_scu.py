#!/usr/bin/env python3
"""Run listed DIMSE requests on one association and report what the peer really answered.

Usage: scripted_scu.py <spec.json> <result.json>

It judges nothing: statuses, returned attributes and events received on the same association go to
the result file, and the caller compares them with the standard's tables. Exit 0 only means the
script ran; an association that was refused is a reported fact, not a crash.
"""
import json
from pathlib import Path
import sys
import time

from pydicom.tag import Tag
from pynetdicom import AE, build_role, evt

from peer_common import SYNTAXES, to_dataset, to_plain


def status_of(status):
    """pynetdicom gives an empty Dataset when the peer did not answer in time."""
    return f'{status.Status:04X}' if status and 'Status' in status else None


def answered(status, dataset=None):
    row = dict(status=status_of(status))
    if status and 'ErrorComment' in status:
        row['comment'] = str(status.ErrorComment)
    if status and 'AffectedSOPInstanceUID' in status:
        row['affected_instance'] = str(status.AffectedSOPInstanceUID)
    if dataset is not None:
        row['dataset'] = to_plain(dataset)
    return row


def resolve(value, rows):
    """Instance UIDs the peer chose: '@image_box:N' is the Nth reference in the latest N-CREATE response."""
    if not isinstance(value, str) or not value.startswith('@image_box:'):
        return value
    for row in reversed(rows):
        boxes = row.get('dataset', {}).get('ReferencedImageBoxSequence')
        if boxes:
            return boxes[int(value.split(':')[1])]['ReferencedSOPInstanceUID']
    raise ValueError('no image box to refer to')


def run_step(assoc, step, rows):
    op, sop_class, meta = step['op'], step.get('class'), step.get('meta')
    instance = resolve(step.get('instance'), rows)
    data = to_dataset(step['dataset']) if 'dataset' in step else None
    message_id = step.get('message_id', 1)
    if op == 'sleep':
        time.sleep(step['seconds'])
        return dict(status='slept')
    if op == 'n_create':
        return answered(*assoc.send_n_create(data, sop_class, instance, msg_id=message_id, meta_uid=meta))
    if op == 'n_set':
        return answered(*assoc.send_n_set(data, sop_class, instance, msg_id=message_id, meta_uid=meta))
    if op == 'n_get':
        tags = [Tag(int(tag, 16)) for tag in step.get('tags', [])]
        return answered(*assoc.send_n_get(tags, sop_class, instance, msg_id=message_id, meta_uid=meta))
    if op == 'n_action':
        return answered(*assoc.send_n_action(data, step['action'], sop_class, instance, msg_id=message_id, meta_uid=meta))
    if op == 'n_delete':
        return answered(assoc.send_n_delete(sop_class, instance, msg_id=message_id, meta_uid=meta))
    if op == 'c_find':
        rows, final = [], None
        for status, identifier in assoc.send_c_find(data, step['model'], msg_id=message_id):
            final = status_of(status)
            if identifier is not None:
                rows.append(to_plain(identifier))
        return dict(status=final, rows=rows)
    raise ValueError(f'unknown step {op}')


def main():
    spec = json.loads(Path(sys.argv[1]).read_text())
    ae = AE(ae_title=spec.get('calling_ae', 'SCRIPTED_SCU'))
    ae.acse_timeout = ae.dimse_timeout = ae.network_timeout = spec.get('timeout', 30)
    for uid in spec['contexts']:
        ae.add_requested_context(uid, list(SYNTAXES))
    events = []

    def on_event(event):
        events.append(dict(event_type=event.event_type, instance=event.request.AffectedSOPInstanceUID,
                           sop_class=event.request.AffectedSOPClassUID, dataset=to_plain(event.event_information)))
        return spec.get('event_reply', 0), None

    assoc = ae.associate(spec['host'], spec['port'], ae_title=spec['called_ae'],
                         ext_neg=[build_role(uid, scu_role=True, scp_role=True) for uid in spec.get('roles', [])],
                         evt_handlers=[(evt.EVT_N_EVENT_REPORT, on_event)])
    result = dict(established=assoc.is_established, rejected=assoc.is_rejected, aborted=assoc.is_aborted,
                  steps=[], events=events)
    if assoc.is_established:
        result['accepted'] = sorted(cx.abstract_syntax for cx in assoc.accepted_contexts)
        for step in spec['steps']:
            if not assoc.is_established:
                result['steps'].append(dict(status=None, comment='association ended before this step'))
                continue
            try:
                result['steps'].append(run_step(assoc, step, result['steps']))
            except (ValueError, RuntimeError) as exc:
                result['steps'].append(dict(status=None, comment=f'{type(exc).__name__}: {exc}'))
        deadline = time.monotonic()+spec.get('wait_seconds', 0)
        while time.monotonic() < deadline and len(events) < spec.get('wait_events', 0) and assoc.is_established:
            time.sleep(.05)
        result['alive_at_end'] = assoc.is_established
        if assoc.is_established:
            if spec.get('abort'):
                assoc.abort()
            else:
                assoc.release()
    Path(sys.argv[2]).write_text(json.dumps(result, indent=1, sort_keys=True)+'\n')


if __name__ == '__main__':
    main()
