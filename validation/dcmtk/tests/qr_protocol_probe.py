#!/usr/bin/env python3
"""Raw-socket Query/Retrieve protocol and fault cases against actual qr_scp (#1319).

No DICOM toolkit decides these results: the peer is this script, so it can pause,
fragment, misbehave and count every PDU. Independent-peer acceptance is qr_matrix_probe.py.
"""
import argparse
import json
from pathlib import Path
import signal
import socket
import sqlite3
import struct
import subprocess
import time

import pydicom
from pydicom.dataset import Dataset
from pydicom.filebase import DicomBytesIO
from pydicom.filereader import read_dataset

from get_scu_protocol_probe import (LE, command, encode_dataset, exact, item, items, pdu, receive_message, receive_pdu, require,
                                    send_message)

AE = 'PACS_QR'
VERIFICATION = '1.2.840.10008.1.1'
FIND_PATIENT = '1.2.840.10008.5.1.4.1.2.1.1'
FIND_STUDY = '1.2.840.10008.5.1.4.1.2.2.1'
MOVE_STUDY = '1.2.840.10008.5.1.4.1.2.2.2'
GET_STUDY = '1.2.840.10008.5.1.4.1.2.2.3'
CT = '1.2.840.10008.5.1.4.1.1.2'
BIG_ENDIAN = '1.2.840.10008.1.2.2'
IMPLICIT = '1.2.840.10008.1.2'
MANY = 60000
TERMINAL_WAIT = 30
C_FIND_RQ, C_GET_RQ, C_ECHO_RQ, C_CANCEL_RQ = 0x0020, 0x0010, 0x0030, 0x0FFF
CASES = ('large-result-is-complete', 'slow-reader-keeps-pdu-framing', 'cancel-stops-matching', 'cancel-other-message-is-ignored', 'cancel-after-final-is-harmless', 'peer-abort-midway',
         'peer-close-midway', 'shutdown-midway', 'fragmented-request', 'many-pdvs-in-one-pdu', 'duplicate-request',
         'max-pdu-is-respected', 'oversized-pdu', 'huge-declared-pdu', 'unknown-pdu-type', 'truncated-pdu', 'data-before-association',
         'rejected-abstract-syntax', 'rejected-transfer-syntax', 'request-on-rejected-context', 'sop-class-mismatch',
         'role-refused', 'missing-identifier', 'malformed-identifier', 'association-limit', 'idle-timeout', 'partial-pdu-timeout',
         'busy-association-outlives-idle-timeout', 'get-names-unreadable-object', 'wrong-called-ae', 'resources-steady')


class Peer:
    """One association opened by hand. The context map is what the SCP actually accepted."""

    def __init__(self, port, contexts=((1, FIND_STUDY, (LE,)),), max_pdu=16384, called=AE, roles=(), receive_buffer=None):
        self.socket = socket.socket()
        if receive_buffer:
            # Before connecting, so the window the server sees stays small.
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, receive_buffer)
        self.socket.settimeout(5)
        self.socket.connect(('127.0.0.1', port))
        self.socket.settimeout(TERMINAL_WAIT)
        request = struct.pack('>H', 1)+b'\0'*2+called.encode().ljust(16)+b'PROTOCOL_SCU'.ljust(16)+b'\0'*32
        request += item(0x10, b'1.2.840.10008.3.1.1.1')
        for ident, abstract, syntaxes in contexts:
            body = bytes([ident, 0, 0, 0])+item(0x30, abstract.encode())
            body += b''.join(item(0x40, syntax.encode()) for syntax in syntaxes)
            request += item(0x20, body)
        user = item(0x51, struct.pack('>I', max_pdu))+item(0x52, b'1.2.3.1319')
        for uid, scu, scp in roles:
            user += item(0x54, struct.pack('>H', len(uid))+uid.encode()+bytes([scu, scp]))
        self.socket.sendall(pdu(1, request+item(0x50, user)))
        self.kind, self.answer = receive_pdu(self.socket)
        self.results, self.roles, self.max_length = {}, {}, None
        if self.kind == 2:
            for kind, body in items(self.answer[68:]):
                if kind == 0x21:
                    self.results[body[0]] = body[2]
                if kind == 0x50:
                    for tag, value in items(body):
                        if tag == 0x51:
                            self.max_length = struct.unpack('>I', value)[0]

    def find(self, keys, message_id=7, context=1, sop_class=FIND_STUDY):
        send_message(self.socket, context, command(C_FIND_RQ, message_id, dataset=keys, sop_class=sop_class), keys)

    def next_response(self):
        """Returns (status, identifier) for one response, raising when the peer abandons the exchange."""
        context, message, payload = receive_message(self.socket)
        require(message is not None, f'Association ended with PDU type {context}')
        identifier = read_dataset(DicomBytesIO(payload), False, True) if payload else None
        return int(message.Status), identifier, message

    def drain(self):
        """Reads to the final response. Returns the identifiers and the final status."""
        identifiers = []
        while True:
            status, identifier, _ = self.next_response()
            if status in (0xFF00, 0xFF01):
                identifiers.append(identifier)
            else:
                return identifiers, status

    def cancel(self, message_id=7, context=1):
        ds = Dataset()
        ds.CommandGroupLength = 0
        ds.CommandField = C_CANCEL_RQ
        ds.MessageIDBeingRespondedTo = message_id
        ds.CommandDataSetType = 0x0101
        ds.CommandGroupLength = len(encode_dataset(ds, True))-12
        send_message(self.socket, context, ds)

    def release(self):
        self.socket.sendall(pdu(5, b'\0'*4))
        kind, _ = receive_pdu(self.socket)
        require(kind == 6, 'No A-RELEASE-RP')
        self.socket.close()

    def abort(self):
        self.socket.sendall(pdu(7, b'\0'*4))
        self.socket.close()


def ended_or_aborted(sock, seconds=10):
    """True when the SCP sent A-ABORT or closed, which are the only acceptable refusals here."""
    sock.settimeout(seconds)
    try:
        while True:
            kind, _ = receive_pdu(sock)
            if kind == 7:
                return True
            require(kind != 4, 'SCP answered with data instead of refusing')
    except EOFError:
        return True
    except ConnectionError:
        return True
    except socket.timeout:
        return False


def patient_keys(patient_id=''):
    ds = Dataset()
    ds.QueryRetrieveLevel = 'PATIENT'
    ds.PatientID = patient_id
    ds.PatientName = ''
    return ds


def study_keys(study_uid=''):
    ds = Dataset()
    ds.QueryRetrieveLevel = 'STUDY'
    ds.StudyInstanceUID = study_uid
    return ds


class Server:
    def __init__(self, args, root, extra=(), patients=0, big_object=False, second_object=False):
        self.args, self.root = args, root
        self.archive = root/'archive'
        self.archive.mkdir(parents=True)
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            self.port = reservation.getsockname()[1]
        self.command = [args.binary, str(self.port), AE, '--storage-dir', str(self.archive), '--index-db', str(root/'index.db'),
                        *map(str, extra)]
        self.big = None
        if big_object:
            self.big = bytes(range(256))*512
            write_object(self.archive/'big.dcm', self.big)
        if second_object:
            write_object(self.archive/'second.dcm', bytes(range(256))*512, instance=2)
        subprocess.run([*self.command, '--scan-only'], check=True, capture_output=True, timeout=60)
        if patients:
            with sqlite3.connect(root/'index.db') as connection:
                connection.executemany('INSERT INTO patients (patient_id, patient_name, birth_date, sex) VALUES (?, ?, ?, ?)',
                                       ((f'BULK{i:06}', f'BULK^{i:06}', '19700101', 'O') for i in range(patients)))
        self.log = (root/'qr-scp.log').open('w')
        self.process = subprocess.Popen(self.command, stdout=self.log, stderr=subprocess.STDOUT, start_new_session=True)
        for _ in range(100):
            require(self.process.poll() is None, 'qr_scp exited before ready')
            try:
                with socket.create_connection(('127.0.0.1', self.port), .1):
                    return
            except OSError:
                time.sleep(.05)
        raise AssertionError('qr_scp did not open its port')

    def alive_and_serving(self):
        """The real proof that a fault left the server usable: a complete new exchange."""
        require(self.process.poll() is None, f'qr_scp died with exit {self.process.returncode}')
        peer = Peer(self.port, ((1, VERIFICATION, (LE,)),))
        require(peer.results.get(1) == 0, 'Verification context not accepted')
        send_message(peer.socket, 1, command(C_ECHO_RQ, 9, sop_class=VERIFICATION))
        status, _, _ = peer.next_response()
        require(status == 0, f'C-ECHO status {status:04X}')
        peer.release()

    def stop(self, seconds=10):
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGTERM)
            try:
                self.process.wait(timeout=seconds)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                raise AssertionError(f'qr_scp did not stop within {seconds} s') from None
        self.log.close()
        require(self.process.returncode == 0, f'qr_scp exit {self.process.returncode}')


def write_object(path, pixels, instance=1):
    ds = Dataset()
    ds.SOPClassUID, ds.SOPInstanceUID = CT, f'1.2.3.1319.77.1.{instance}'
    ds.StudyInstanceUID, ds.SeriesInstanceUID = '1.2.3.1319.77', '1.2.3.1319.77.1'
    ds.PatientID, ds.PatientName, ds.Modality = 'BIG', 'BIG^OBJECT', 'CT'
    ds.SeriesNumber, ds.InstanceNumber = 1, instance
    ds.Rows, ds.Columns, ds.SamplesPerPixel, ds.PhotometricInterpretation = 256, 512, 1, 'MONOCHROME2'
    ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 8, 8, 7, 0
    ds.PixelData = pixels
    ds.file_meta = pydicom.dataset.FileMetaDataset()
    ds.file_meta.MediaStorageSOPClassUID, ds.file_meta.MediaStorageSOPInstanceUID = CT, ds.SOPInstanceUID
    ds.file_meta.TransferSyntaxUID = LE
    ds.is_little_endian, ds.is_implicit_VR = True, False
    ds.save_as(path, write_like_original=False)


# Cases: each returns a one-line result and raises when the expectation fails -----------------

def capture_stream(peer, chunk, pause, quiet=3):
    """Everything the SCP sends until it stays silent. Parsing comes later, so it cannot slow the reader."""
    stream = bytearray()
    peer.socket.settimeout(quiet)
    started = time.monotonic()
    last = started
    try:
        while True:
            data = peer.socket.recv(chunk)
            if not data:
                break
            stream += data
            last = time.monotonic()
            if pause:
                time.sleep(pause)
    except socket.timeout:
        pass
    return bytes(stream), last-started


def parse_find_stream(stream):
    """Walk the bytes as PDUs. Returns the matches and final status, or where framing first breaks."""
    offset, matches, final, command_bytes, data_bytes = 0, [], None, bytearray(), bytearray()
    while offset < len(stream):
        if offset+6 > len(stream):
            return matches, final, f'{len(stream)-offset} bytes left after the last whole PDU'
        kind, reserved, length = struct.unpack('>BBI', stream[offset:offset+6])
        if kind != 4 or reserved != 0 or length > 1048576 or offset+6+length > len(stream):
            return matches, final, f'PDU type {kind}, reserved {reserved}, length {length} at byte {offset} of {len(stream)}'
        at, end = offset+6, offset+6+length
        start, offset = offset, end
        # Interleaved bytes can still look like a PDU header, and only fail once read as content.
        try:
            while at < end:
                size = struct.unpack('>I', stream[at:at+4])[0]
                flags = stream[at+5]
                payload = stream[at+6:at+4+size]
                at += 4+size
                if flags & 1:
                    command_bytes += payload
                    if flags & 2:
                        message = read_dataset(DicomBytesIO(bytes(command_bytes)), True, True)
                        command_bytes.clear()
                        if int(message.Status) not in (0xFF00, 0xFF01):
                            final = int(message.Status)
                else:
                    data_bytes += payload
                    if flags & 2:
                        matches.append(str(read_dataset(DicomBytesIO(bytes(data_bytes)), False, True).PatientID))
                        data_bytes.clear()
        except Exception as error:
            return matches, final, f'unreadable content in the PDU at byte {start} of {len(stream)}: {type(error).__name__}'
    return matches, final, None


def large_result_is_complete(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    peer.find(patient_keys('BULK*'), sop_class=FIND_PATIENT)
    stream, elapsed = capture_stream(peer, 1048576, 0)
    matches, final, broken = parse_find_stream(stream)
    require(broken is None, f'Framing broke: {broken}')
    require(final == 0, f'Final status {final}')
    require(sorted(matches) == [f'BULK{i:06}' for i in range(MANY)], f'{len(set(matches))} distinct of {MANY} matches')
    peer.release()
    return f'all {MANY} matches exact and in framing, {len(stream)} bytes in {elapsed:.2f} s'


def slow_reader_keeps_framing(server):
    # A reader slower than the SCP fills the socket buffers, so a write can complete only in part. On
    # network_system 093691d9 as published the next write then starts inside it, and this case fails;
    # the patch the checkout action carries queues the writes (docs/SOUP.md).
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),), receive_buffer=65536)
    peer.find(patient_keys('BULK*'), sop_class=FIND_PATIENT)
    stream, elapsed = capture_stream(peer, 4096, 0.002)
    matches, final, broken = parse_find_stream(stream)
    require(broken is None, f'Framing broke with a slow reader: {broken}, after {len(matches)} of {MANY} matches')
    require(final == 0, f'Final status {final}')
    require(sorted(matches) == [f'BULK{i:06}' for i in range(MANY)], f'{len(set(matches))} distinct of {MANY} matches')
    peer.release()
    return f'all {MANY} matches exact and in framing with a slow reader, {len(stream)} bytes in {elapsed:.2f} s'


def cancel_stops_matching(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    peer.find(patient_keys('BULK*'), sop_class=FIND_PATIENT)
    status, identifier, _ = peer.next_response()
    require(status == 0xFF00 and identifier is not None, f'First response status {status:04X}')
    peer.cancel()
    identifiers, final = peer.drain()
    require(final == 0xFE00, f'Final status {final:04X}, expected FE00')
    sent = len(identifiers)+1
    require(sent < MANY, f'All {MANY} matches arrived despite the cancel')
    peer.find(patient_keys('BULK000001'), message_id=8, sop_class=FIND_PATIENT)
    identifiers, final = peer.drain()
    require(final == 0 and [str(ds.PatientID) for ds in identifiers] == ['BULK000001'], 'Association unusable after cancel')
    peer.release()
    return f'{sent} of {MANY} matches, final FE00, association still serves'


def cancel_other_message(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    peer.find(patient_keys('BULK00000?'), sop_class=FIND_PATIENT)
    peer.cancel(message_id=99)
    identifiers, final = peer.drain()
    require(final == 0 and len(identifiers) == 10, f'{len(identifiers)} matches, final {final:04X}')
    peer.release()
    return 'cancel naming another message changed nothing: 10 matches, final 0000'


def cancel_after_final(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    peer.find(patient_keys('BULK000001'), sop_class=FIND_PATIENT)
    identifiers, final = peer.drain()
    require(final == 0 and len(identifiers) == 1, 'First query wrong')
    peer.cancel()
    peer.find(patient_keys('BULK000002'), message_id=8, sop_class=FIND_PATIENT)
    identifiers, final = peer.drain()
    require(final == 0 and [str(ds.PatientID) for ds in identifiers] == ['BULK000002'], 'Late cancel hit the next operation')
    peer.release()
    return 'a late cancel did not reach the next operation'


def serving_within(server, seconds):
    """Retries, since an association slot returns only after the server has seen the peer leave."""
    deadline = time.monotonic()+seconds
    while True:
        try:
            server.alive_and_serving()
            return
        except (AssertionError, OSError, EOFError):
            require(server.process.poll() is None, 'qr_scp died')
            require(time.monotonic() < deadline, f'No association slot came back within {seconds} s')
            time.sleep(.2)


def peer_leaves_midway(server, leave):
    # Startup's readiness connection may still hold the only slot.
    serving_within(server, 15)
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    peer.find(patient_keys('BULK*'), sop_class=FIND_PATIENT)
    status, _, _ = peer.next_response()
    require(status == 0xFF00, f'First response status {status:04X}')
    leave(peer)
    serving_within(server, 15)
    return 'the only association slot came back and the server serves'


def shutdown_midway(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    peer.find(patient_keys('BULK*'), sop_class=FIND_PATIENT)
    status, _, _ = peer.next_response()
    require(status == 0xFF00, f'First response status {status:04X}')
    started = time.monotonic()
    server.stop()
    elapsed = time.monotonic()-started
    peer.socket.close()
    return f'SIGTERM with a peer that stopped reading: exit 0 in {elapsed:.2f} s'


def fragmented_request(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    keys = patient_keys('BULK000003')
    message = encode_dataset(command(C_FIND_RQ, 7, dataset=keys, sop_class=FIND_PATIENT), True)
    identifier = encode_dataset(keys, False)
    for payload, kind in ((message, 1), (identifier, 0)):
        for offset in range(0, len(payload), 5):
            piece = payload[offset:offset+5]
            flags = kind | (2 if offset+len(piece) == len(payload) else 0)
            peer.socket.sendall(pdu(4, struct.pack('>I', len(piece)+2)+bytes([1, flags])+piece))
    identifiers, final = peer.drain()
    require(final == 0 and [str(ds.PatientID) for ds in identifiers] == ['BULK000003'], f'Final {final:04X}')
    peer.release()
    return 'a request split into 5-byte PDUs gave the exact result'


def many_pdvs(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    keys = patient_keys('BULK000004')
    body = b''
    for payload, kind in ((encode_dataset(command(C_FIND_RQ, 7, dataset=keys, sop_class=FIND_PATIENT), True), 1),
                          (encode_dataset(keys, False), 0)):
        for offset in range(0, len(payload), 3):
            piece = payload[offset:offset+3]
            flags = kind | (2 if offset+len(piece) == len(payload) else 0)
            body += struct.pack('>I', len(piece)+2)+bytes([1, flags])+piece
    peer.socket.sendall(pdu(4, body))
    identifiers, final = peer.drain()
    require(final == 0 and [str(ds.PatientID) for ds in identifiers] == ['BULK000004'], f'Final {final:04X}')
    peer.release()
    return 'one PDU holding many 3-byte PDVs gave the exact result'


def duplicate_request(server):
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    keys = patient_keys('BULK00000?')
    peer.find(keys, sop_class=FIND_PATIENT)
    peer.find(keys, sop_class=FIND_PATIENT)
    first, final_one = peer.drain()
    second, final_two = peer.drain()
    expected = sorted(f'BULK00000{i}' for i in range(10))
    for identifiers, final in ((first, final_one), (second, final_two)):
        require(final == 0 and sorted(str(ds.PatientID) for ds in identifiers) == expected, f'Final {final:04X}')
    peer.release()
    return 'two requests with one Message ID gave two complete, ordered result sets'


def max_pdu_respected(server):
    peer = Peer(server.port, ((1, GET_STUDY, (LE,)), (3, CT, (LE,))), max_pdu=4096, roles=((CT, 0, 1),))
    require(peer.results.get(3) == 0, 'Storage context not accepted')
    keys = study_keys('1.2.3.1319.77')
    send_message(peer.socket, 1, command(C_GET_RQ, 7, dataset=keys, sop_class=GET_STUDY), keys)
    largest, data, store, final = 0, bytearray(), None, None
    while final is None:
        kind, body = receive_pdu(peer.socket)
        require(kind == 4, f'PDU type {kind} during C-GET')
        largest = max(largest, len(body))
        require(len(body) <= 4096, f'P-DATA-TF of {len(body)} bytes exceeds the negotiated 4096')
        offset = 0
        while offset < len(body):
            size = struct.unpack('>I', body[offset:offset+4])[0]
            context, flags = body[offset+4:offset+6]
            payload = body[offset+6:offset+4+size]
            offset += 4+size
            if flags & 1:
                message = read_dataset(DicomBytesIO(payload), True, True)
                if message.CommandField == 1:
                    store = message
                elif message.CommandField == 0x8010 and message.Status != 0xFF00:
                    final = int(message.Status)
            else:
                data += payload
                if flags & 2:
                    send_message(peer.socket, context, command(0x8001, int(store.MessageID), status=0,
                                                               sop_class=str(store.AffectedSOPClassUID),
                                                               sop_uid=str(store.AffectedSOPInstanceUID)))
    require(final == 0, f'Final status {final:04X}')
    received = read_dataset(DicomBytesIO(bytes(data)), True, True) if store and peer.results[3] == 0 else None
    require(received is not None and bytes(received.PixelData) == server.big, 'Reassembled pixel data differs')
    peer.release()
    return f'largest P-DATA-TF {largest} bytes under a 4096 limit; {len(server.big)} pixel bytes exact'


def oversized_pdu(server):
    peer = Peer(server.port, ((1, FIND_STUDY, (LE,)),))
    limit = peer.max_length or 16384
    # A single PDV one megabyte past what the SCP said it can receive.
    piece = b'\0'*(limit+1048576)
    peer.socket.sendall(pdu(4, struct.pack('>I', len(piece)+2)+bytes([1, 3])+piece))
    require(ended_or_aborted(peer.socket), 'SCP neither aborted nor closed')
    server.alive_and_serving()
    return f'a P-DATA-TF past the advertised {limit} bytes ended the association; the server serves'


def huge_declared_pdu(server):
    peer = Peer(server.port, ((1, FIND_STUDY, (LE,)),))
    peer.socket.sendall(struct.pack('>BBI', 4, 0, 0xFFFFFFF0)+b'\0'*32)
    peer.socket.close()
    time.sleep(.2)
    server.alive_and_serving()
    rss = int(next(line for line in Path(f'/proc/{server.process.pid}/status').read_text().splitlines()
                   if line.startswith('VmRSS')).split()[1])
    require(rss < 1048576, f'Resident size {rss} KiB after a 4 GiB length claim')
    return f'a 4 GiB length claim allocated nothing large (RSS {rss} KiB); the server serves'


def unknown_pdu_type(server):
    peer = Peer(server.port, ((1, FIND_STUDY, (LE,)),))
    peer.socket.sendall(pdu(0x7F, b'\0'*8))
    require(ended_or_aborted(peer.socket), 'SCP neither aborted nor closed')
    server.alive_and_serving()
    return 'an unknown PDU type ended the association; the server serves'


def truncated_pdu(server):
    peer = Peer(server.port, ((1, FIND_STUDY, (LE,)),))
    peer.socket.sendall(struct.pack('>BBI', 4, 0, 1000)+b'\0'*10)
    peer.socket.close()
    server.alive_and_serving()
    return 'EOF inside a PDU left the server serving'


def data_before_association(server):
    sock = socket.create_connection(('127.0.0.1', server.port), 5)
    keys = study_keys('')
    message = encode_dataset(command(C_FIND_RQ, 7, dataset=keys, sop_class=FIND_STUDY), True)
    sock.sendall(pdu(4, struct.pack('>I', len(message)+2)+bytes([1, 3])+message))
    require(ended_or_aborted(sock), 'SCP neither aborted nor closed')
    sock.close()
    server.alive_and_serving()
    return 'P-DATA before association got no response data'


def rejected_abstract(server):
    peer = Peer(server.port, ((1, '1.2.3.4.5.6.7', (LE,)), (3, FIND_STUDY, (LE,))))
    require(peer.results.get(1) == 3, f'Unknown abstract syntax result {peer.results.get(1)}, expected 3')
    require(peer.results.get(3) == 0, 'Known context was not accepted beside it')
    peer.release()
    return 'unknown abstract syntax: result 3, known context accepted'


def rejected_transfer(server):
    peer = Peer(server.port, ((1, FIND_STUDY, (BIG_ENDIAN,)), (3, FIND_STUDY, (IMPLICIT,))))
    require(peer.results.get(1) == 4, f'Big Endian result {peer.results.get(1)}, expected 4')
    require(peer.results.get(3) == 0, 'Implicit VR context was not accepted beside it')
    peer.release()
    return 'unsupported transfer syntax: result 4, supported context accepted'


def request_on_rejected(server):
    peer = Peer(server.port, ((1, '1.2.3.4.5.6.7', (LE,)), (3, FIND_STUDY, (LE,))))
    peer.find(study_keys(''), context=1)
    require(ended_or_aborted(peer.socket), 'A request on a rejected context did not end the association')
    server.alive_and_serving()
    return 'a request on a rejected context got no data'


def sop_class_mismatch(server):
    peer = Peer(server.port, ((1, FIND_STUDY, (LE,)), (3, MOVE_STUDY, (LE,))))
    # The command names the FIND class on the context negotiated for MOVE.
    peer.find(study_keys(''), context=3, sop_class=FIND_STUDY)
    try:
        status, identifier, _ = peer.next_response()
    except (AssertionError, EOFError, ConnectionError):
        server.alive_and_serving()
        return 'association ended instead of answering'
    require(identifier is None and status not in (0, 0xFF00, 0xFF01), f'Status {status:04X} for a class mismatch')
    peer.release()
    return f'refused with status {status:04X}, no identifier'


def role_refused(server):
    peer = Peer(server.port, ((1, FIND_STUDY, (LE,)),), roles=((FIND_STUDY, 0, 1),))
    if peer.results.get(1) != 0:
        peer.release()
        return f'context not accepted for a peer refusing the SCU role (result {peer.results.get(1)})'
    peer.find(study_keys(''))
    require(ended_or_aborted(peer.socket), 'A request from a peer without the SCU role was served')
    server.alive_and_serving()
    return 'a request from a peer without the SCU role got no data'


def missing_identifier(server):
    peer = Peer(server.port)
    send_message(peer.socket, 1, command(C_FIND_RQ, 7, sop_class=FIND_STUDY))
    status, identifier, _ = peer.next_response()
    require(status == 0xA900 and identifier is None, f'Status {status:04X}, expected A900')
    peer.release()
    return 'status A900, no identifier'


def malformed_identifier(server):
    peer = Peer(server.port)
    keys = study_keys('')
    send_message(peer.socket, 1, command(C_FIND_RQ, 7, dataset=keys, sop_class=FIND_STUDY))
    # The command promised a data set; this one claims a length it does not have.
    garbage = b'\x20\x00\x0d\x00UI\xff\x7f123'
    peer.socket.sendall(pdu(4, struct.pack('>I', len(garbage)+2)+bytes([1, 2])+garbage))
    try:
        status, identifier, _ = peer.next_response()
    except (AssertionError, EOFError, ConnectionError):
        server.alive_and_serving()
        return 'association ended instead of answering'
    require(identifier is None and status not in (0, 0xFF00, 0xFF01), f'Status {status:04X} for malformed data')
    peer.release()
    return f'refused with status {status:04X}, no identifier'


def association_limit(server):
    serving_within(server, 15)
    first, second = Peer(server.port), Peer(server.port)
    require(first.kind == 2 and second.kind == 2, 'Associations inside the limit were not accepted')
    try:
        third = Peer(server.port)
        outcome = f'A-ASSOCIATE PDU type {third.kind}'
        require(third.kind == 3, f'Association past the limit got {outcome}')
        result, source, reason = third.answer[1], third.answer[2], third.answer[3]
        require((result, source, reason) == (2, 3, 2), f'Rejection fields {(result, source, reason)}, expected transient/local limit')
        third.socket.close()
    except (EOFError, ConnectionError) as error:
        raise AssertionError(f'Past the limit the connection just closed ({error}); PS3.8 asks for A-ASSOCIATE-RJ') from None
    first.find(study_keys(''))
    identifiers, final = first.drain()
    require(final == 0 and not identifiers, 'An association inside the limit stopped working')
    first.release()
    second.release()
    time.sleep(.5)
    server.alive_and_serving()
    return 'third association rejected as transient local limit; slots returned after release'


def idle_timeout(server):
    peer = Peer(server.port)
    started = time.monotonic()
    require(ended_or_aborted(peer.socket, 15), 'Idle association still open after 15 s')
    elapsed = time.monotonic()-started
    require(elapsed >= .9, f'Closed after {elapsed:.2f} s, before the 1 s idle timeout')
    server.alive_and_serving()
    return f'idle association ended after {elapsed:.2f} s under a 1 s timeout'


def partial_pdu_timeout(server):
    peer = Peer(server.port)
    peer.socket.sendall(struct.pack('>BBI', 4, 0, 1000)+b'\0'*10)
    started = time.monotonic()
    require(ended_or_aborted(peer.socket, 15), 'Association holding a partial PDU still open after 15 s')
    server.alive_and_serving()
    return f'partial PDU ended the association after {time.monotonic()-started:.2f} s under a 1 s timeout'


def busy_association(server):
    # A requester waiting for a slow retrieve sends nothing, yet it is not idle.
    peer = Peer(server.port, ((1, GET_STUDY, (LE,)), (3, CT, (LE,))), roles=((CT, 0, 1),))
    keys = study_keys('1.2.3.1319.77')
    send_message(peer.socket, 1, command(C_GET_RQ, 7, dataset=keys, sop_class=GET_STUDY), keys)
    final = None
    while final is None:
        context, message, _ = receive_message(peer.socket)
        require(message is not None, f'Association ended during the retrieve with PDU type {context}')
        if message.CommandField == 1:
            time.sleep(3)
            send_message(peer.socket, context, command(0x8001, int(message.MessageID), status=0,
                                                       sop_class=str(message.AffectedSOPClassUID),
                                                       sop_uid=str(message.AffectedSOPInstanceUID)))
        elif message.Status != 0xFF00:
            final = int(message.Status)
    require(final == 0, f'Final status {final:04X}')
    peer.release()
    return 'a 3 s wait inside a C-GET survived a 1 s idle timeout, and the retrieve finished'


def get_names_unreadable_object(server):
    (server.archive/'second.dcm').unlink()
    peer = Peer(server.port, ((1, GET_STUDY, (LE,)), (3, CT, (LE,))), roles=((CT, 0, 1),))
    keys = study_keys('1.2.3.1319.77')
    send_message(peer.socket, 1, command(C_GET_RQ, 7, dataset=keys, sop_class=GET_STUDY), keys)
    stored = []
    while True:
        context, message, payload = receive_message(peer.socket)
        require(message is not None, f'Association ended during the retrieve with PDU type {context}')
        if message.CommandField == 1:
            stored.append(str(message.AffectedSOPInstanceUID))
            send_message(peer.socket, context, command(0x8001, int(message.MessageID), status=0,
                                                       sop_class=str(message.AffectedSOPClassUID),
                                                       sop_uid=str(message.AffectedSOPInstanceUID)))
        elif message.Status != 0xFF00:
            break
    require(int(message.Status) == 0xB000, f'Final status {int(message.Status):04X}, expected B000')
    require((int(message.NumberOfCompletedSuboperations), int(message.NumberOfFailedSuboperations)) == (1, 1), 'Wrong counters')
    require(stored == ['1.2.3.1319.77.1.1'], f'Stored {stored}')
    listed = read_dataset(DicomBytesIO(payload), False, True).FailedSOPInstanceUIDList
    require(str(listed) == '1.2.3.1319.77.1.2', f'Failed SOP Instance UID List {listed}')
    peer.release()
    return 'final B000 names exactly the unreadable object; the readable one was stored'


def wrong_called_ae(server):
    peer = Peer(server.port, called='NOT_THIS_AE')
    require(peer.kind == 3, f'PDU type {peer.kind}, expected A-ASSOCIATE-RJ')
    require(peer.answer[3] == 7, f'Rejection reason {peer.answer[3]}, expected called AE not recognized')
    peer.socket.close()
    server.alive_and_serving()
    return 'A-ASSOCIATE-RJ: called AE title not recognized'


def resources_steady(server):
    def counts():
        status = Path(f'/proc/{server.process.pid}/status').read_text()
        threads = int(next(line for line in status.splitlines() if line.startswith('Threads')).split()[1])
        return len(list(Path(f'/proc/{server.process.pid}/fd').iterdir())), threads

    # Open whatever the first query opens lazily, or it would count as growth below.
    peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
    peer.find(patient_keys('BULK000001'), sop_class=FIND_PATIENT)
    peer.drain()
    peer.release()
    for _ in range(5):
        server.alive_and_serving()
    time.sleep(1)
    before = counts()
    for index in range(30):
        peer = Peer(server.port, ((1, FIND_PATIENT, (LE,)),))
        if index % 3 == 0:
            peer.find(patient_keys('BULK00000?'), sop_class=FIND_PATIENT)
            peer.next_response()
            peer.abort()
        elif index % 3 == 1:
            peer.socket.close()
        else:
            peer.release()
    deadline = time.monotonic()+10
    while counts()[0] > before[0] and time.monotonic() < deadline:
        time.sleep(.2)
    after = counts()
    require(after[0] <= before[0], f'File descriptors {before[0]} grew to {after[0]} after 30 associations')
    require(after[1] <= before[1], f'Threads {before[1]} grew to {after[1]} after 30 associations')
    return f'after 30 aborted, dropped and released associations: fds {before[0]}->{after[0]}, threads {before[1]}->{after[1]}'


SETUP = {'large-result-is-complete': dict(patients=MANY), 'slow-reader-keeps-pdu-framing': dict(patients=MANY),
         'cancel-stops-matching': dict(patients=MANY), 'cancel-other-message-is-ignored': dict(patients=100),
         'cancel-after-final-is-harmless': dict(patients=10), 'peer-abort-midway': dict(patients=MANY, extra=('--max-assoc', 1)),
         'peer-close-midway': dict(patients=MANY, extra=('--max-assoc', 1)), 'shutdown-midway': dict(patients=MANY),
         'fragmented-request': dict(patients=10), 'many-pdvs-in-one-pdu': dict(patients=10), 'duplicate-request': dict(patients=100),
         'max-pdu-is-respected': dict(big_object=True), 'association-limit': dict(extra=('--max-assoc', 2)),
         'idle-timeout': dict(extra=('--timeout', 1)), 'partial-pdu-timeout': dict(extra=('--timeout', 1)),
         'busy-association-outlives-idle-timeout': dict(big_object=True, extra=('--timeout', 1)),
         'get-names-unreadable-object': dict(big_object=True, second_object=True),
         'resources-steady': dict(patients=10)}
ACTIONS = {'large-result-is-complete': large_result_is_complete, 'slow-reader-keeps-pdu-framing': slow_reader_keeps_framing,
           'cancel-stops-matching': cancel_stops_matching, 'cancel-other-message-is-ignored': cancel_other_message,
           'cancel-after-final-is-harmless': cancel_after_final,
           'peer-abort-midway': lambda server: peer_leaves_midway(server, Peer.abort),
           'peer-close-midway': lambda server: peer_leaves_midway(server, lambda peer: peer.socket.close()),
           'shutdown-midway': shutdown_midway, 'fragmented-request': fragmented_request, 'many-pdvs-in-one-pdu': many_pdvs,
           'duplicate-request': duplicate_request, 'max-pdu-is-respected': max_pdu_respected, 'oversized-pdu': oversized_pdu,
           'huge-declared-pdu': huge_declared_pdu, 'unknown-pdu-type': unknown_pdu_type, 'truncated-pdu': truncated_pdu,
           'data-before-association': data_before_association, 'rejected-abstract-syntax': rejected_abstract,
           'rejected-transfer-syntax': rejected_transfer, 'request-on-rejected-context': request_on_rejected,
           'sop-class-mismatch': sop_class_mismatch, 'role-refused': role_refused, 'missing-identifier': missing_identifier,
           'malformed-identifier': malformed_identifier, 'association-limit': association_limit, 'idle-timeout': idle_timeout,
           'partial-pdu-timeout': partial_pdu_timeout, 'busy-association-outlives-idle-timeout': busy_association,
           'get-names-unreadable-object': get_names_unreadable_object,
           'wrong-called-ae': wrong_called_ae, 'resources-steady': resources_steady}


def run_case(args, name):
    root = Path(args.output)/name
    server = Server(args, root, **SETUP.get(name, {}))
    try:
        return ACTIONS[name](server)
    finally:
        server.stop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='/workspace/build/bin/qr_scp')
    parser.add_argument('--output', required=True)
    parser.add_argument('--case', choices=CASES)
    args = parser.parse_args()
    results = []
    for name in (args.case,) if args.case else CASES:
        started = time.monotonic()
        try:
            row = dict(case=name, status='passed', detail=run_case(args, name))
        except Exception as error:  # A failed case must not stop the others from being evaluated.
            row = dict(case=name, status='failed', detail=f'{type(error).__name__}: {error}')
        row['seconds'] = round(time.monotonic()-started, 2)
        results.append(row)
        print(json.dumps(row), flush=True)
    Path(args.output, 'summary.json').write_text(json.dumps(results, indent=2)+'\n')
    return int(any(row['status'] != 'passed' for row in results))


if __name__ == '__main__':
    raise SystemExit(main())
