#!/usr/bin/env python3
"""Actual retrieve_scu against a controlled wire peer; not reference archive evidence."""
import argparse
import copy
import json
import re
import socket
import struct
import subprocess
import threading
import time
from pathlib import Path

import pydicom
from pydicom.dataset import Dataset
from pydicom.filebase import DicomBytesIO
from pydicom.filereader import read_dataset
from pydicom.filewriter import write_dataset

CT = '1.2.840.10008.5.1.4.1.1.2'
MR = '1.2.840.10008.5.1.4.1.1.4'
GET = '1.2.840.10008.5.1.4.1.2.2.3'
LE = '1.2.840.10008.1.2.1'
STUDY = '1.2.3.1315'


def require(value, message):
    if not value:
        raise AssertionError(message)


def item(kind, body):
    return struct.pack('>BBH', kind, 0, len(body))+body


def pdu(kind, body):
    return struct.pack('>BBI', kind, 0, len(body))+body


def exact(sock, count):
    data = bytearray()
    while len(data) < count:
        chunk = sock.recv(count-len(data))
        if not chunk:
            raise EOFError('Peer closed connection')
        data += chunk
    return bytes(data)


def receive_pdu(sock):
    kind, _, length = struct.unpack('>BBI', exact(sock, 6))
    require(length < 1048576, 'Unexpected PDU length')
    return kind, exact(sock, length)


def items(data):
    offset = 0
    while offset < len(data):
        kind, _, size = struct.unpack('>BBH', data[offset:offset+4])
        body = data[offset+4:offset+4+size]
        require(len(body) == size, 'Truncated item')
        yield kind, body
        offset += size+4


def encode_dataset(ds, implicit):
    stream = DicomBytesIO()
    stream.is_little_endian = True
    stream.is_implicit_VR = implicit
    write_dataset(stream, ds)
    return stream.getvalue()


def command(field, message_id=1, status=None, completed=None, failed=0, dataset=None,
            sop_class=GET, sop_uid=None):
    ds = Dataset()
    ds.CommandGroupLength = 0
    ds.AffectedSOPClassUID = sop_class
    ds.CommandField = field
    if field & 0x8000:
        ds.MessageIDBeingRespondedTo = message_id
    else:
        ds.MessageID = message_id
        ds.Priority = 0
    ds.CommandDataSetType = 0x0101 if dataset is None else 0x0000
    if sop_uid is not None:
        ds.AffectedSOPInstanceUID = sop_uid
    if status is not None:
        ds.Status = status
    if completed is not None:
        ds.NumberOfCompletedSuboperations = completed
        ds.NumberOfFailedSuboperations = failed
        ds.NumberOfWarningSuboperations = 0
    ds.CommandGroupLength = len(encode_dataset(ds, True))-12
    return ds


def send_message(sock, context, cmd, dataset=None):
    payload = encode_dataset(cmd, True)
    sock.sendall(pdu(4, struct.pack('>I', len(payload)+2)+bytes([context, 3])+payload))
    if dataset is not None:
        payload = encode_dataset(dataset, False)
        for offset in range(0, len(payload), 8192):
            fragment = payload[offset:offset+8192]
            flags = 2 if offset+len(fragment) == len(payload) else 0
            sock.sendall(pdu(4, struct.pack('>I', len(fragment)+2)+bytes([context, flags])+fragment))


def receive_message(sock):
    command_bytes, data_bytes = bytearray(), bytearray()
    cmd = None
    while True:
        kind, body = receive_pdu(sock)
        if kind != 4:
            return kind, None, None
        offset = 0
        while offset < len(body):
            size = struct.unpack('>I', body[offset:offset+4])[0]
            context, flags = body[offset+4:offset+6]
            payload = body[offset+6:offset+4+size]
            offset += 4+size
            if flags & 1:
                command_bytes += payload
                if flags & 2:
                    cmd = read_dataset(DicomBytesIO(bytes(command_bytes)), True, True)
                    if cmd.CommandDataSetType == 0x0101:
                        return context, cmd, None
            else:
                data_bytes += payload
                if flags & 2:
                    return context, cmd, bytes(data_bytes)


def run_case(args, name, expected_exit):
    directory = Path(args.output)/name
    directory.mkdir(parents=True)
    downloads = directory/'downloads'
    datasets = []
    for index in (1, 2):
        ds = copy.deepcopy(pydicom.dcmread(args.fixture))
        ds.StudyInstanceUID = STUDY
        ds.SeriesInstanceUID = STUDY+'.1'
        ds.SOPInstanceUID = ds.SeriesInstanceUID+'.'+str(index)
        ds.file_meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
        ds.PatientID = '../../outside' if name == 'safe-paths' else 'PROTOCOL'
        datasets.append(ds)
    if name == 'wrong-existing':
        stale = copy.deepcopy(datasets[0])
        stale.PixelData = bytes([stale.PixelData[0] ^ 0xFF])+stale.PixelData[1:]
        path = downloads/'PROTOCOL'/STUDY/(STUDY+'.1')/(str(stale.SOPInstanceUID)+'.dcm')
        path.parent.mkdir(parents=True)
        stale.save_as(path)

    listener = socket.socket()
    listener.bind(('127.0.0.1', 0))
    listener.listen(1)
    listener.settimeout(5)
    evidence = {'case': name}
    errors = []

    def peer():
        try:
            with listener.accept()[0] as sock:
                sock.settimeout(4)
                kind, rq = receive_pdu(sock)
                require(kind == 1, 'Missing A-ASSOCIATE-RQ')
                (directory/'associate-rq.bin').write_bytes(pdu(kind, rq))
                pcs, roles, application = [], {}, b''
                for kind, body in items(rq[68:]):
                    if kind == 0x10:
                        application = body.rstrip(b'\0')
                    elif kind == 0x20:
                        abstract = next(v for t, v in items(body[4:]) if t == 0x30).rstrip(b'\0').decode()
                        pcs.append((body[0], abstract))
                    elif kind == 0x50:
                        for subkind, subbody in items(body):
                            if subkind == 0x54:
                                size = struct.unpack('>H', subbody[:2])[0]
                                require(size+4 == len(subbody), 'Role UID contains undeclared padding')
                                roles[subbody[2:2+size].decode()] = list(subbody[2+size:])
                require(roles.get(CT) == [0, 1], 'Storage SCP role was not offered')
                evidence['offered_roles'] = roles
                contexts = {uid: ident for ident, uid in pcs}
                ac = rq[:68]+item(0x10, application)
                for ident, uid in pcs:
                    ac += item(0x21, bytes([ident, 0, 0 if uid in (GET, CT) else 3, 0])+item(0x40, LE.encode()))
                user_info = item(0x51, struct.pack('>I', 16384))+item(0x52, b'1.2.3.1315')
                if name != 'no-role-response':
                    user_info += item(0x54, struct.pack('>H', len(CT))+CT.encode()+bytes([0, 0 if name == 'role-denied' else 1]))
                packet = pdu(2, ac+item(0x50, user_info))
                (directory/'associate-ac.bin').write_bytes(packet)
                sock.sendall(packet)
                context, get, _ = receive_message(sock)
                if name in ('role-denied', 'no-role-response'):
                    require(get is None, 'Client sent C-GET without an accepted Storage SCP role')
                    return
                require(get.CommandField == 0x10 and context == contexts[GET], 'Missing C-GET request')
                if name == 'timeout':
                    require(receive_pdu(sock)[0] == 7, 'Client did not abort at its timeout')
                    return
                count = 2 if name in ('partial-roles', 'omitted-count', 'safe-paths') else 1
                if name in ('cancel', 'wrong-message'):
                    count = 0
                completed, failed = 0, 0
                for index, ds in enumerate(datasets[:count]):
                    sent_dataset = None if name == 'missing-dataset' else ds
                    sop_class = MR if name == 'unnegotiated-store' else CT
                    cmd = command(1, 20+index, dataset=sent_dataset, sop_class=sop_class, sop_uid=ds.SOPInstanceUID)
                    send_message(sock, contexts[CT], cmd, sent_dataset)
                    ack_context, ack, _ = receive_message(sock)
                    require(ack.CommandField == 0x8001 and ack.MessageIDBeingRespondedTo == 20+index,
                            'Missing/corrupt C-STORE acknowledgement')
                    require(ack_context == contexts[CT], 'Wrong C-STORE response context')
                    expect_failure = name in ('missing-dataset', 'unnegotiated-store', 'wrong-existing')
                    require((ack.Status != 0) == expect_failure, 'Incorrect C-STORE status')
                    completed += int(ack.Status == 0)
                    failed += int(ack.Status != 0)
                status = 0xFE00 if name == 'cancel' else 0xB000 if failed or name == 'warning' else 0
                cmd = command(0x8010, 2 if name == 'wrong-message' else 1, status=status,
                              completed=None if name == 'omitted-count' else completed+(name == 'false-count'),
                              failed=failed+(name == 'warning'))
                send_message(sock, contexts[GET], cmd)
                kind, _ = receive_pdu(sock)
                if kind == 5:
                    sock.sendall(pdu(6, b'\0'*4))
                else:
                    require(kind == 7, 'Expected release or abort')
                evidence['acknowledged'] = completed
                evidence['failed_stores'] = failed
        except Exception as exc:
            errors.append(repr(exc))
        finally:
            listener.close()

    thread = threading.Thread(target=peer)
    thread.start()
    started = time.monotonic()
    result = subprocess.run([args.binary, '127.0.0.1', str(listener.getsockname()[1]), 'PROBE',
                             '--mode', 'get', '--study-uid', STUDY, '--output', str(downloads),
                             '--timeout', '1', '--no-progress', '--verbose'],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=6)
    thread.join(5)
    (directory/'client.log').write_bytes(result.stdout)
    evidence.update(exit=result.returncode, seconds=round(time.monotonic()-started, 3), peer_errors=errors)
    (directory/'result.json').write_text(json.dumps(evidence, indent=2)+'\n')
    require(not thread.is_alive() and not errors, str(evidence))
    require(result.returncode == expected_exit, str(evidence))
    if expected_exit == 0:
        files = list(downloads.rglob('*.dcm'))
        require(len(files) == 2, 'Missing actual persisted objects')
        actual = {str(ds.SOPInstanceUID): ds for ds in map(pydicom.dcmread, files)}
        for ds in datasets:
            require(actual[str(ds.SOPInstanceUID)].PixelData == ds.PixelData, 'Pixel mismatch')
        require(re.search(rb'Completed:\s+2\b', result.stdout), 'Wrong completed summary')
        require(re.search(rb'Received:\s+2\b', result.stdout), 'Wrong received summary')
    print(name, result.returncode, 'passed', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='/workspace/build/bin/retrieve_scu')
    parser.add_argument('--fixture', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    Path(args.output).mkdir(parents=True, exist_ok=False)
    for name, code in [('partial-roles', 0), ('no-role-response', 2), ('role-denied', 2),
                       ('false-count', 2), ('warning', 1), ('cancel', 2), ('timeout', 2),
                       ('wrong-message', 2), ('unnegotiated-store', 2), ('missing-dataset', 2),
                       ('omitted-count', 0), ('wrong-existing', 2), ('safe-paths', 0)]:
        run_case(args, name, code)


if __name__ == '__main__':
    main()
