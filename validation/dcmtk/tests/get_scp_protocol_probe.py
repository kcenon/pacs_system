#!/usr/bin/env python3
"""Controlled C-GET SCUs exercise actual qr_scp response routing (#1306).

Protocol/fault evidence; independent DCMTK acceptance is get_scp_peer_probe.py.
"""
import argparse
import concurrent.futures
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
from get_scu_protocol_probe import (CT, MR, GET, LE, command, item, items, pdu,
                                    receive_message, receive_pdu, require, send_message)

STUDY = '1.2.3.1306'
ECHO = '1.2.840.10008.1.1'
CASES = ('success', 'delayed-interleaved', 'duplicates', 'late-after-cancel',
         'concurrent', 'mixed-status', 'partial-context', 'no-storage-role',
         'syntax-mismatch', 'wrong-message-id', 'wrong-context', 'wrong-command',
         'wrong-instance', 'missing-status', 'missing-dataset', 'cancel',
         'peer-release', 'peer-abort', 'shutdown', 'shutdown-race', 'response-timeout',
         'unsolicited-response', 'unauthorized-store')


def connect(port, name, directory):
    sock = socket.create_connection(('127.0.0.1', port), 5)
    sock.settimeout(40)
    rq = struct.pack('>H', 1)+b'\0'*2+b'PACS_QR'.ljust(16)+b'GET_SCU'.ljust(16)+b'\0'*32
    rq += item(0x10, b'1.2.840.10008.3.1.1.1')
    for ident, uid in [(1, GET), (3, CT), (5, MR), (7, ECHO)]:
        if name == 'partial-context' and uid == MR:
            continue
        syntax = '1.2.840.10008.1.2.4.70' if name == 'syntax-mismatch' and uid in (CT, MR) else LE
        rq += item(0x20, bytes([ident, 0, 0, 0])+item(0x30, uid.encode())+item(0x40, syntax.encode()))
    user = item(0x51, struct.pack('>I', 16384))+item(0x52, b'1.2.3.1306')
    if name != 'no-storage-role':
        for uid in (CT, MR):
            if name == 'partial-context' and uid == MR: continue
            user += item(0x54, struct.pack('>H', len(uid))+uid.encode()+bytes([0, 1]))
    sock.sendall(pdu(1, rq+item(0x50, user)))
    kind, ac = receive_pdu(sock)
    require(kind == 2, 'Association not accepted')
    (directory/'associate-ac.bin').write_bytes(pdu(kind, ac))
    roles, contexts = {}, {}
    for kind, body in items(ac[68:]):
        if kind == 0x21: contexts[body[0]] = body[2]
        if kind == 0x50:
            for tag, value in items(body):
                if tag == 0x54:
                    length = struct.unpack('>H', value[:2])[0]
                    roles[value[2:2+length].decode()] = list(value[2+length:])
    if name == 'no-storage-role':
        require(contexts[3] != 0 and contexts[5] != 0, 'Outbound-only SOP accepted without a role')
    else:
        require(roles.get(CT) == [0, 1], 'Storage SCP role not accepted')
        if name != 'partial-context': require(roles.get(MR) == [0, 1], 'MR SCP role not accepted')
    return sock


def query(sock, name, datasets, evidence, process, previous=None):
    keys = pydicom.Dataset()
    keys.QueryRetrieveLevel = 'STUDY'
    keys.StudyInstanceUID = STUDY
    request = command(0x10, 77, dataset=None if name == 'missing-dataset' else keys, sop_class=GET)
    request.Priority = 1
    send_message(sock, 1, request, None if name == 'missing-dataset' else keys)
    expected = {str(ds.SOPInstanceUID): ds for ds in datasets}
    stores, rows, ids, acks = [], [], [], []
    echo_queued = False
    start = time.monotonic()
    last = None
    while True:
        context, msg, payload = receive_message(sock)
        require(msg is not None, 'Association ended before terminal C-GET response')
        if msg.CommandField == 1:
            require(context in (3, 5) and payload, 'Missing C-STORE dataset/context')
            require('MoveOriginatorApplicationEntityTitle' not in msg and 'MoveOriginatorMessageID' not in msg,
                    'C-GET incorrectly sends Move Originator fields')
            require(msg.Priority == 1, 'C-GET priority not retained')
            ds = pydicom.dcmread(io.BytesIO(payload), force=True)
            uid = str(ds.SOPInstanceUID)
            require(uid in expected and uid not in stores, 'Unexpected or duplicate object')
            require(str(msg.AffectedSOPInstanceUID) == uid and str(msg.AffectedSOPClassUID) == str(ds.SOPClassUID),
                    'Command/dataset identity mismatch')
            require(ds.PixelData == expected[uid].PixelData, 'Pixel data changed')
            ident = int(msg.MessageID)
            require(ident not in ids and (not previous or ident not in previous['ids']), 'Store MessageID reused')
            ids.append(ident)
            stores.append(uid)
            if name in ('cancel', 'late-after-cancel', 'peer-release', 'peer-abort', 'shutdown', 'shutdown-race') and len(stores) == 2:
                last = (context, command(0x8001, ident, status=0, sop_class=str(ds.SOPClassUID), sop_uid=uid))
                if name in ('shutdown', 'shutdown-race'):
                    process.send_signal(signal.SIGTERM)
                    process.wait(timeout=5)
                    require(process.returncode == 0, 'Server shutdown failed')
                    break
                if name in ('peer-release', 'peer-abort'):
                    sock.sendall(pdu(5 if name == 'peer-release' else 7, b'\0'*4))
                    if name == 'peer-release': require(receive_pdu(sock)[0] == 6, 'Release did not interrupt wait')
                    break
                cancel = command(0xFFF, sop_class=GET)
                del cancel.MessageID
                cancel.MessageIDBeingRespondedTo = 77
                send_message(sock, 1, cancel)
                continue
            if name == 'response-timeout': continue
            if name == 'delayed-interleaved':
                if not echo_queued:
                    send_message(sock, 7, command(0x30, 88, sop_class=ECHO))
                    echo_queued = True
                time.sleep(.15)
            if name == 'concurrent': time.sleep(.05)
            status = [0, 0xB000, 0xC001][len(stores)-1] if name == 'mixed-status' else 0
            ack = command(0x8001, ident, status=status, sop_class=str(ds.SOPClassUID), sop_uid=uid)
            if previous and len(stores) == 1: send_message(sock, *previous['late'])
            if name == 'wrong-message-id': ack.MessageIDBeingRespondedTo += 100
            if name == 'wrong-command': ack.CommandField = 0x8030
            if name == 'wrong-instance': ack.AffectedSOPInstanceUID = '1.2.3.999'
            if name == 'missing-status': del ack.Status
            send_message(sock, 7 if name == 'wrong-context' else context, ack)
            if name == 'duplicates': send_message(sock, context, ack)
            acks.append({'id': ident, 'status': status, 'uid': uid})
        else:
            require(context == 1 and msg.CommandField == 0x8010 and msg.MessageIDBeingRespondedTo == 77,
                    'Wrong C-GET response correlation')
            row = {'status': int(msg.Status)}
            for label, field in [('remaining', 'NumberOfRemainingSuboperations'),
                                 ('completed', 'NumberOfCompletedSuboperations'),
                                 ('failed', 'NumberOfFailedSuboperations'), ('warning', 'NumberOfWarningSuboperations')]:
                if field in msg: row[label] = int(getattr(msg, field))
            if payload:
                uids = pydicom.dcmread(io.BytesIO(payload), force=True).FailedSOPInstanceUIDList
                row['failed_uids'] = [str(uids)] if isinstance(uids, str) else list(map(str, uids))
            rows.append(row)
            if msg.Status == 0xFF00:
                require(sum(row[k] for k in ('remaining', 'completed', 'failed', 'warning')) == 3, 'Incorrect pending totals')
                require(not payload, 'Pending response contains identifier')
                continue
            break
    elapsed = time.monotonic()-start
    evidence.append({'scenario': name, 'store_requests': stores, 'store_message_ids': ids,
                     'responses': rows, 'acknowledgements': acks, 'seconds': elapsed})
    if name in ('peer-release', 'peer-abort', 'shutdown', 'shutdown-race'):
        require(elapsed < 5, 'Lifecycle transition did not wake the service')
        return
    final = rows[-1]
    status = {'success': 0, 'delayed-interleaved': 0, 'duplicates': 0, 'concurrent': 0,
              'partial-context': 0xB000, 'mixed-status': 0xB000, 'missing-dataset': 0xA900,
              'cancel': 0xFE00, 'late-after-cancel': 0xFE00}.get(name, 0xA702)
    require(final['status'] == status, 'Wrong terminal status: '+str(final))
    if name != 'missing-dataset':
        counts = {'success': (3, 0, 0), 'delayed-interleaved': (3, 0, 0), 'duplicates': (3, 0, 0),
                  'concurrent': (3, 0, 0), 'partial-context': (2, 1, 0), 'mixed-status': (1, 1, 1),
                  'cancel': (1, 1, 0), 'late-after-cancel': (1, 1, 0)}.get(name, (0, 3, 0))
        require(tuple(final[k] for k in ('completed', 'failed', 'warning')) == counts, 'Wrong counters: '+str(final))
        require(len(set(final.get('failed_uids', []))) == final['failed'], 'Failed UID list mismatch')
        require(set(final.get('failed_uids', [])) <= expected.keys(), 'Unknown failed UID')
        if status == 0xFE00:
            require(final['remaining'] == 1 and elapsed < 5, 'Cancel did not leave exactly one unstarted object')
        else:
            require('remaining' not in final, 'Remaining count in non-cancel terminal response')
    if name == 'response-timeout': require(28 < elapsed < 36 and len(stores) == 1, 'Timeout multiplied by match count')
    if echo_queued:
        context, echo, payload = receive_message(sock)
        require(context == 7 and echo.CommandField == 0x8030 and echo.MessageIDBeingRespondedTo == 88 and echo.Status == 0,
                'Interleaved request was lost or consumed as a storage acknowledgement')
    return {'ids': ids, 'late': last}


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
        ds.PatientID = 'GET_PROTOCOL'
        ds.InstanceNumber = i+1
        ds.save_as(archive/(str(i)+'.dcm'))
        datasets.append(ds)
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    evidence = []
    stop_churn = threading.Event()
    churn_threads, connections = [], []
    log = (directory/'qr-scp.log').open('w')
    process = subprocess.Popen([args.binary, str(port), 'PACS_QR', '--storage-dir', str(archive),
                                '--index-db', str(directory/'index.db')], stdout=log,
                               stderr=subprocess.STDOUT, start_new_session=True)
    try:
        for _ in range(100):
            require(process.poll() is None, 'qr_scp exited before ready')
            try:
                with socket.create_connection(('127.0.0.1', port), .1): break
            except OSError: time.sleep(.05)
        else: raise AssertionError('qr_scp did not open port')
        if name == 'concurrent':
            barrier = threading.Barrier(4)
            def query_worker(index):
                own = directory/str(index)
                own.mkdir()
                with connect(port, name, own) as sock:
                    barrier.wait(timeout=5)
                    query(sock, name, datasets, evidence, process)
                    sock.sendall(pdu(5, b'\0'*4))
                    require(receive_pdu(sock)[0] == 6, 'Missing release')
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
                list(pool.map(query_worker, range(4)))
            require(len(evidence) == 4, 'Missing concurrent operation')
        else:
            with connect(port, name, directory) as sock:
                if name in ('unsolicited-response', 'unauthorized-store'):
                    cmd = command(0x8001, 99, status=0, sop_class=CT, sop_uid=str(datasets[0].SOPInstanceUID))
                    if name == 'unauthorized-store': cmd = command(1, 99, dataset=datasets[0], sop_class=CT, sop_uid=str(datasets[0].SOPInstanceUID))
                    send_message(sock, 3, cmd, datasets[0] if name == 'unauthorized-store' else None)
                    if name == 'unauthorized-store':
                        require(receive_pdu(sock)[0] == 7, 'Unauthorized request did not abort')
                    else:
                        # Unowned replies are discarded without poisoning the
                        # next operation or accumulating in a receive queue.
                        query(sock, 'success', datasets, evidence, process)
                        sock.sendall(pdu(5, b'\0'*4))
                        require(receive_pdu(sock)[0] == 6, 'Missing release')
                else:
                    if name == 'shutdown-race':
                        def churn():
                            while not stop_churn.is_set():
                                try:
                                    with socket.create_connection(('127.0.0.1', port), .1) as transient:
                                        transient.sendall(b'\x01') # EOF inside a partial PDU
                                        connections.append(1)
                                except OSError:
                                    pass
                                stop_churn.wait(.001)
                        churn_threads = [threading.Thread(target=churn) for _ in range(4)]
                        for worker in churn_threads: worker.start()
                        deadline = time.monotonic()+2
                        while len(connections) < 8 and time.monotonic() < deadline: time.sleep(.001)
                        require(len(connections) >= 8, 'Connection churn never began')
                    previous = query(sock, name, datasets, evidence, process)
                    if name == 'late-after-cancel': query(sock, 'success', datasets, evidence, process, previous)
                    if name not in ('shutdown', 'shutdown-race', 'peer-release', 'peer-abort'):
                        sock.sendall(pdu(5, b'\0'*4))
                        require(receive_pdu(sock)[0] == 6, 'Missing release response')
        return evidence
    finally:
        stop_churn.set()
        for worker in churn_threads: worker.join(timeout=3)
        if churn_threads:
            evidence.append({'connections_during_shutdown': len(connections), 'workers_joined': all(not t.is_alive() for t in churn_threads)})
        if process.poll() is None:
            process.send_signal(signal.SIGTERM)
            try: process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        log.close()
        (directory/'results.json').write_text(json.dumps(evidence, indent=2)+'\n')
        require(process.returncode == 0, f'qr_scp exit {process.returncode}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='/workspace/build/bin/qr_scp')
    parser.add_argument('--fixture', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--case', choices=CASES)
    args = parser.parse_args()
    results = []
    for name in (args.case,) if args.case else CASES:
        try:
            run_case(args, name)
            row = {'case': name, 'status': 'passed'}
        except Exception as exc:
            row = {'case': name, 'status': 'failed', 'error': repr(exc)}
        results.append(row)
        print(json.dumps(row), flush=True)
    Path(args.output, 'summary.json').write_text(json.dumps(results, indent=2)+'\n')
    return int(any(row['status'] != 'passed' for row in results))


if __name__ == '__main__': raise SystemExit(main())
