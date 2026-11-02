#!/usr/bin/env python3
"""Receive N-EVENT-REPORTs on associations the other side opens, and record what arrived.

Usage: event_receiver.py --aet AE --port N --record events.jsonl [--status HEX] [--exit-after N]

Each record is one event as this separate process received it, with the association's calling AE
Title and the roles negotiated. The only thing this peer enforces is the role rule: PS3.7 gives the
association requestor the SCU role unless Role Selection says otherwise, so an event report from a
requestor that was not granted the SCP role is refused with 0122 and recorded as such.
"""
import argparse
import threading

from pynetdicom import AE, evt

from peer_common import SYNTAXES, append_record, to_plain

STORAGE_COMMITMENT, UPS_EVENT = '1.2.840.10008.1.20.1', '1.2.840.10008.5.1.4.34.6.4'
REFUSED_SOP_CLASS = 0x0122


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--aet', required=True)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--record', required=True)
    parser.add_argument('--status', default='0000', help='N-EVENT-REPORT-RSP status, in hex')
    parser.add_argument('--exit-after', type=int, default=0, help='Stop after this many events; 0 means run until killed')
    args = parser.parse_args()
    done = threading.Event()
    received = [0]

    def on_event(event):
        context = event.context
        negotiated = next(cx for cx in event.assoc.accepted_contexts if cx.context_id == context.context_id)
        requestor_is_scp = negotiated.as_scu  # This acceptor is the SCU when the requestor took the SCP role.
        status = int(args.status, 16) if requestor_is_scp else REFUSED_SOP_CLASS
        append_record(args.record, dict(
            kind='event', calling_ae=str(event.assoc.requestor.ae_title), called_ae=str(event.assoc.acceptor.ae_title),
            requestor_scp_role=bool(requestor_is_scp), abstract_syntax=str(context.abstract_syntax),
            sop_class=str(event.request.AffectedSOPClassUID), instance=str(event.request.AffectedSOPInstanceUID),
            event_type=event.event_type, message_id=event.request.MessageID, dataset=to_plain(event.event_information),
            replied=f'{status:04X}'))
        received[0] += 1
        if args.exit_after and received[0] >= args.exit_after:
            done.set()
        return status, None

    def on_accepted(event):
        append_record(args.record, dict(kind='association', calling_ae=str(event.assoc.requestor.ae_title),
                                        contexts=sorted((cx.abstract_syntax, bool(cx.as_scu), bool(cx.as_scp))
                                                        for cx in event.assoc.accepted_contexts)))

    ae = AE(ae_title=args.aet)
    ae.acse_timeout = ae.dimse_timeout = ae.network_timeout = 30
    for uid in (STORAGE_COMMITMENT, UPS_EVENT):
        ae.add_supported_context(uid, list(SYNTAXES), scu_role=False, scp_role=True)
    server = ae.start_server(('127.0.0.1', args.port), block=False,
                             evt_handlers=[(evt.EVT_N_EVENT_REPORT, on_event), (evt.EVT_ACCEPTED, on_accepted)])
    print(f'listening on {args.port}', flush=True)
    try:
        done.wait()
    except KeyboardInterrupt:
        pass
    server.shutdown()


if __name__ == '__main__':
    main()
