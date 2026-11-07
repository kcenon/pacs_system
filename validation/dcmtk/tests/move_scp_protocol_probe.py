#!/usr/bin/env python3
"""Actual qr_scp against controlled move requester/destination wire peers (#1304).

These deliberately malformed/refusing peers are protocol evidence, not independent
DCMTK interoperability evidence (see move_scp_peer_probe.py).
"""
import argparse
import copy
import io
import json
import signal
import socket
import struct
import subprocess
import threading
import time
from pathlib import Path

import pydicom
from get_scu_protocol_probe import (CT, MR, LE, command, encode_dataset, exact, item,
                                    items, pdu, receive_message, receive_pdu, require,
                                    send_message)

MOVE = '1.2.840.10008.5.1.4.1.2.2.2'
STUDY = '1.2.3.1304'


def run_case(args, name):
    directory = Path(args.output)/name
    directory.mkdir(parents=True)
    archive = directory/'archive'
    archive.mkdir()
    datasets = []
    for i in range(3):
        ds = copy.deepcopy(pydicom.dcmread(args.fixture))
        ds.StudyInstanceUID = STUDY
        ds.SeriesInstanceUID = STUDY+'.'+str(i+1)
        ds.SOPInstanceUID = ds.SeriesInstanceUID+'.1'
        ds.SOPClassUID = CT if i < 2 else MR
        ds.file_meta.MediaStorageSOPClassUID = ds.SOPClassUID
        ds.file_meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
        ds.PatientID = 'MOVE_PROTOCOL'
        ds.InstanceNumber = i+1
        ds.save_as(archive/(str(i)+'.dcm'))
        datasets.append(ds)
    expected_uids = {str(ds.SOPInstanceUID) for ds in datasets}
    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    destination_port = listener.getsockname()[1]
    no_destination = name in ('unknown-ae', 'connection-refused', 'missing-dataset')
    if not no_destination:
        listener.listen(1)
        listener.settimeout(8)
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    evidence = {'case': name, 'store_requests': [], 'responses': [], 'acknowledgements': []}
    errors, blocked = [], threading.Event()
    stop_peer = threading.Event()

    def destination():
        try:
            with listener.accept()[0] as sock:
                sock.settimeout(40)
                kind, rq = receive_pdu(sock)
                require(kind == 1, 'Missing destination A-ASSOCIATE-RQ')
                require(rq[4:20].rstrip() == b'DEST_SCP', 'Wrong destination AE')
                require(rq[20:36].rstrip() == b'PACS_QR', 'Wrong storage calling AE')
                (directory/'store-associate-rq.bin').write_bytes(pdu(kind, rq))
                pcs, application = [], b''
                for kind, body in items(rq[68:]):
                    if kind == 0x10:
                        application = body
                    elif kind == 0x20:
                        subs = list(items(body[4:]))
                        uid = next(value for tag, value in subs if tag == 0x30).rstrip(b'\0').decode()
                        syntaxes = [value.rstrip(b'\0').decode() for tag, value in subs if tag == 0x40]
                        require(LE in syntaxes, 'Stored syntax not proposed')
                        pcs.append((body[0], uid))
                require({uid for _, uid in pcs} == {CT, MR}, 'Missing matched SOP class context')
                if name == 'association-rejected':
                    sock.sendall(pdu(3, bytes([0, 1, 1, 1])))
                    return
                ac = rq[:68]+item(0x10, application)
                for ident, uid in pcs:
                    rejected = name == 'partial-context' and uid == MR
                    ac += item(0x21, bytes([ident, 0, 3 if rejected else 0, 0])+item(0x40, LE.encode()))
                ac += item(0x50, item(0x51, struct.pack('>I', 16384))+item(0x52, b'1.2.3.1304'))
                sock.sendall(pdu(2, ac))
                while True:
                    context, store, payload = receive_message(sock)
                    if store is None:
                        if context == 5:
                            if name == 'release-timeout':
                                require(receive_pdu(sock)[0] == 7, 'No abort after release timeout')
                            else:
                                sock.sendall(pdu(6, b'\0'*4))
                        else:
                            require(context == 7, 'Unexpected storage association termination')
                        return
                    require(store.CommandField == 1 and payload, 'Missing actual C-STORE dataset')
                    require(store.MoveOriginatorApplicationEntityTitle == 'ORIGINATOR', 'Wrong move originator AE')
                    require(store.MoveOriginatorMessageID == 77, 'Wrong move originator message ID')
                    require(store.Priority == 1, 'Move priority was not preserved')
                    ds = pydicom.dcmread(io.BytesIO(payload), force=True)
                    uid = str(ds.SOPInstanceUID)
                    require(uid in expected_uids, 'Unexpected instance')
                    original = next(ds for ds in datasets if str(ds.SOPInstanceUID) == uid)
                    require(ds.PixelData == original.PixelData, 'Pixel payload changed')
                    evidence['store_requests'].append(uid)
                    count = len(evidence['store_requests'])
                    if (name in ('cancel', 'shutdown') and count == 2) or name == 'response-timeout':
                        blocked.set()
                        require(receive_pdu(sock)[0] == 7, 'No abort while transfer was blocked')
                        return
                    status = [0, 0xB000, 0xC001][count-1] if name == 'mixed-status' else 0
                    ack = command(0x8001, int(store.MessageID), status=status,
                                  sop_class=str(store.AffectedSOPClassUID), sop_uid=uid)
                    if name == 'wrong-message-id': ack.MessageIDBeingRespondedTo += 1
                    if name == 'wrong-command': ack.CommandField = 0x8030
                    if name == 'wrong-instance': ack.AffectedSOPInstanceUID = '1.2.3.999'
                    if name == 'missing-status': del ack.Status
                    ack_context = 253 if name == 'wrong-context' else context
                    send_message(sock, ack_context, ack)
                    evidence['acknowledgements'].append({'uid': uid, 'status': status})
        except Exception as exc:
            errors.append(repr(exc))
        finally:
            stop_peer.set()
            listener.close()

    worker = None
    log = (directory/'qr-scp.log').open('w')
    process = subprocess.Popen([args.binary, str(port), 'PACS_QR', '--storage-dir', str(archive),
                                '--index-db', str(directory/'index.db'), '--peer',
                                f'DEST_SCP:127.0.0.1:{destination_port}'],
                               stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    started = time.monotonic()
    try:
        for _ in range(100):
            require(process.poll() is None, 'qr_scp exited before ready')
            try:
                requester = socket.create_connection(('127.0.0.1', port), .1)
                break
            except OSError: time.sleep(.05)
        else: raise AssertionError('qr_scp did not open port')
        if not no_destination:
            worker = threading.Thread(target=destination)
            worker.start()
        with requester:
            requester.settimeout(40)
            rq = struct.pack('>H', 1)+b'\0'*2+b'PACS_QR'.ljust(16)+b'ORIGINATOR'.ljust(16)+b'\0'*32
            rq += item(0x10, b'1.2.840.10008.3.1.1.1')
            rq += item(0x20, bytes([1, 0, 0, 0])+item(0x30, MOVE.encode())+item(0x40, LE.encode()))
            rq += item(0x50, item(0x51, struct.pack('>I', 16384))+item(0x52, b'1.2.3.1304'))
            requester.sendall(pdu(1, rq))
            require(receive_pdu(requester)[0] == 2, 'Move association not accepted')
            keys = pydicom.Dataset()
            keys.QueryRetrieveLevel = 'STUDY'
            keys.StudyInstanceUID = STUDY
            request = command(0x21, 77, dataset=None if name == 'missing-dataset' else keys, sop_class=MOVE)
            request.Priority = 1
            request.MoveDestination = 'UNKNOWN' if name == 'unknown-ae' else 'DEST_SCP'
            send_message(requester, 1, request, None if name == 'missing-dataset' else keys)
            if name in ('cancel', 'shutdown'):
                require(blocked.wait(5), 'Destination never reached second blocked C-STORE')
                cancel_started = time.monotonic()
                if name == 'shutdown':
                    process.send_signal(signal.SIGTERM)
                    process.wait(timeout=5)
                    require(process.returncode == 0, 'Shutdown during transfer failed')
                else:
                    cancel = command(0xFFF, sop_class=MOVE)
                    del cancel.MessageID
                    cancel.MessageIDBeingRespondedTo = 77
                    send_message(requester, 1, cancel)
            while name != 'shutdown':
                context, response, payload = receive_message(requester)
                require(response is not None and context == 1 and response.CommandField == 0x8021,
                        'Missing C-MOVE response')
                require(response.MessageIDBeingRespondedTo == 77, 'Wrong response message ID')
                row = {'status': int(response.Status)}
                for label, field in [('remaining', 'NumberOfRemainingSuboperations'),
                                     ('completed', 'NumberOfCompletedSuboperations'),
                                     ('failed', 'NumberOfFailedSuboperations'),
                                     ('warning', 'NumberOfWarningSuboperations')]:
                    if field in response: row[label] = int(getattr(response, field))
                if payload:
                    failed = pydicom.dcmread(io.BytesIO(payload), force=True).FailedSOPInstanceUIDList
                    row['failed_uids'] = [str(failed)] if isinstance(failed, str) else list(map(str, failed))
                evidence['responses'].append(row)
                if response.Status == 0xFF00:
                    require(sum(row[key] for key in ('remaining', 'completed', 'failed', 'warning')) == 3,
                            'Pending counters lost objects')
                    require(not payload, 'Pending response contains identifier')
                    continue
                break
            if name != 'shutdown':
                requester.sendall(pdu(5, b'\0'*4))
                require(receive_pdu(requester)[0] == 6, 'Missing requester release response')
        if worker:
            worker.join(timeout=5)
            require(not worker.is_alive(), 'Destination worker did not terminate')
        require(not errors, 'Destination errors: '+str(errors))
        if name == 'shutdown':
            require(time.monotonic()-cancel_started < 5, 'Shutdown did not interrupt blocked operation')
        else:
            final = evidence['responses'][-1]
            statuses = {'unknown-ae': 0xA801, 'missing-dataset': 0xA900,
                        'partial-context': 0xB000, 'mixed-status': 0xB000, 'cancel': 0xFE00,
                        'success': 0, 'release-timeout': 0}
            require(final['status'] == statuses.get(name, 0xA702), 'Wrong final status: '+str(final))
            if name not in ('unknown-ae', 'missing-dataset'):
                wanted = {'success': (3, 0, 0), 'release-timeout': (3, 0, 0),
                          'partial-context': (2, 1, 0), 'mixed-status': (1, 1, 1), 'cancel': (1, 1, 0)}
                require(tuple(final[k] for k in ('completed', 'failed', 'warning')) == wanted.get(name, (0, 3, 0)),
                        'Incorrect final sub-operation counts: '+str(final))
                failed_uids = set(final.get('failed_uids', []))
                require(len(failed_uids) == final['failed'], 'Failed UID list does not match count')
                if name == 'partial-context': require(failed_uids == {str(datasets[2].SOPInstanceUID)}, 'Wrong rejected SOP UID')
                if name == 'mixed-status': require(failed_uids == {evidence['store_requests'][-1]}, 'Wrong failed storage UID')
            if name == 'cancel':
                require(final['remaining'] == 1 and time.monotonic()-cancel_started < 2,
                        'C-CANCEL did not promptly stop the active transfer')
                require(len(evidence['store_requests']) == 2, 'Sent more stores after C-CANCEL')
            else:
                require('remaining' not in final, 'Final success/warning/failure contains Remaining')
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        listener.close()
        if worker: worker.join(timeout=2)
        log.close()
        evidence.update(seconds=time.monotonic()-started, process_exit=process.returncode, peer_errors=errors)
        (directory/'evidence.json').write_text(json.dumps(evidence, indent=2)+'\n')
    require(process.returncode == 0, f'qr_scp did not exit cleanly: {process.returncode}')
    require('Scan complete: 3 files indexed\n' in (directory/'qr-scp.log').read_text(), 'Public index failed')
    return evidence


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True)
    parser.add_argument('--fixture', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    Path(args.output).mkdir(parents=True, exist_ok=False)
    results = []
    cases = ['success', 'unknown-ae', 'connection-refused', 'association-rejected', 'partial-context',
             'mixed-status', 'wrong-message-id', 'wrong-context', 'wrong-command', 'wrong-instance',
             'missing-status', 'missing-dataset', 'cancel', 'shutdown', 'response-timeout', 'release-timeout']
    for name in cases:
        try:
            run_case(args, name)
            result = {'name': name, 'status': 'passed'}
        except Exception as exc:
            result = {'name': name, 'status': 'failed', 'error': repr(exc)}
        results.append(result)
        print(json.dumps(result), flush=True)
    (Path(args.output)/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    return int(any(row['status'] != 'passed' for row in results))


if __name__ == '__main__':
    raise SystemExit(main())
