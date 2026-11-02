#!/usr/bin/env python3
"""Public MPPS CLIs plus a controlled malformed wire peer and read-only SQLite oracle.

This is protocol/durability regression evidence, not independent MPPS SCP conformance.
Only public N-CREATE/N-SET operations write the database.
"""
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import signal
import socket
import sqlite3
import struct
import subprocess
import time

from pydicom.dataset import Dataset
from get_scu_protocol_probe import (LE, encode_dataset, item, items, pdu,
                                    receive_message, receive_pdu, require, send_message)

MPPS = '1.2.840.10008.3.1.2.3.3'
ECHO = '1.2.840.10008.1.1'


class Probe:
    def __init__(self, args):
        self.bin = Path(args.bin_dir).resolve()
        self.output = Path(args.output).resolve()
        self.output.mkdir(parents=True, exist_ok=False)
        self.db = self.output/'pacs.db'
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            self.port = sock.getsockname()[1]
        self.commands, self.cases = [], []

    def command(self, args, expected=0):
        args = [str(a) for a in args]
        index = len(self.commands)+1
        result = subprocess.run(args, capture_output=True, text=True, timeout=20)
        (self.output/f'command-{index}.stdout').write_text(result.stdout)
        (self.output/f'command-{index}.stderr').write_text(result.stderr)
        self.commands.append(dict(argv=args, exit=result.returncode, expected_exit=expected))
        self.save()
        require(result.returncode == expected, f'Command {index}: exit {result.returncode}, expected {expected}')
        return result.stdout+result.stderr

    def save(self):
        (self.output/'results.json').write_text(json.dumps(
            dict(commands=self.commands, cases=self.cases), indent=2)+'\n')

    @contextmanager
    def server(self, label):
        args = [str(self.bin/'pacs_server'), '--port', str(self.port), '--ae-title', 'MPPS_TEST',
                '--storage-dir', str(self.output/'storage'), '--db-path', str(self.db),
                '--max-associations', '10', '--log-level', 'warning']
        with (self.output/f'server-{label}.log').open('w') as log:
            process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic()+15
                while True:
                    require(process.poll() is None, 'Server exited before readiness')
                    try:
                        with socket.create_connection(('127.0.0.1', self.port), .2):
                            break
                    except OSError:
                        require(time.monotonic() < deadline, 'Server readiness deadline exceeded')
                        time.sleep(.05)
                self.echo()
                yield
            finally:
                forced = False
                if process.poll() is None:
                    process.send_signal(signal.SIGTERM)
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        forced = True
                        process.kill()
                        process.wait()
                self.commands.append(dict(argv=args, exit=process.returncode, forced_kill=forced))
                self.save()
                require(not forced and process.returncode == 0, 'Server did not shut down cleanly')

    def echo(self):
        self.command([self.bin/'echo_scu', '-aec', 'MPPS_TEST', '127.0.0.1', self.port])

    def row(self, uid):
        with sqlite3.connect(self.db.as_uri()+'?mode=ro', uri=True) as connection:
            connection.row_factory = sqlite3.Row
            rows = connection.execute('SELECT * FROM mpps WHERE mpps_uid = ?', (uid,)).fetchall()
        require(len(rows) == 1, 'Exact UID does not identify one durable MPPS record: '+uid)
        return dict(rows[0])

    def create(self, uid):
        output = self.command([self.bin/'mpps_scu', '127.0.0.1', self.port, 'MPPS_TEST', 'create',
                               '--patient-id', 'PROTOCOL', '--modality', 'CT',
                               '--study-uid', '1.2.3.1316.99', '--mpps-uid', uid])
        require(uid in output, 'N-CREATE did not report the supplied UID')
        row = self.row(uid)
        require(row['mpps_uid'] == uid and row['status'] == 'IN PROGRESS', 'Incorrect initial state')
        require(row['modality'] == 'CT' and row['start_datetime'], 'Missing persisted creation metadata')

    def update(self, uid, status, expected=0):
        output = self.command([self.bin/'mpps_scu', '127.0.0.1', self.port, 'MPPS_TEST', 'set',
                               '--mpps-uid', uid, '--status', status], expected)
        if expected:
            # PS3.4 F.7.2.2, and what dcm4che mppsscp answers: 0110H, may no longer be updated.
            require('0x110' in output.lower(), 'Post-terminal request failed without the expected DIMSE status')

    def connect(self, label):
        sock = socket.create_connection(('127.0.0.1', self.port), 5)
        sock.settimeout(5)
        try:
            rq = struct.pack('>H', 1)+b'\0'*2+b'MPPS_TEST'.ljust(16)+b'MALFORMED'.ljust(16)+b'\0'*32
            rq += item(0x10, b'1.2.840.10008.3.1.1.1')
            for context, uid in [(1, MPPS), (3, ECHO)]:
                rq += item(0x20, bytes([context, 0, 0, 0])+item(0x30, uid.encode())+item(0x40, LE.encode()))
            rq += item(0x50, item(0x51, struct.pack('>I', 16384))+item(0x52, b'1.2.3.1316'))
            sock.sendall(pdu(1, rq))
            kind, ac = receive_pdu(sock)
            require(kind == 2, 'Association not accepted')
            (self.output/f'{label}-associate-ac.bin').write_bytes(pdu(kind, ac))
            contexts = {body[0]: body[2] for tag, body in items(ac[68:]) if tag == 0x21}
            require(contexts == {1: 0, 3: 0}, 'Required presentation contexts were not accepted')
            return sock
        except BaseException:
            sock.close()
            raise

    def malformed(self, label, uid):
        before = self.row(uid)
        cmd = Dataset()
        cmd.CommandGroupLength = 0
        cmd.RequestedSOPClassUID = MPPS
        cmd.CommandField = 0x0120
        cmd.MessageID = 77
        cmd.CommandDataSetType = 0
        cmd.RequestedSOPInstanceUID = uid
        context = 1
        if label == 'wrong-class':
            cmd.RequestedSOPClassUID = ECHO
        elif label == 'wrong-context':
            context = 3
        elif label == 'unknown-context':
            context = 9
        elif label == 'affected-class-fallback':
            del cmd.RequestedSOPClassUID
            cmd.AffectedSOPClassUID = MPPS
        elif label == 'conflicting-class':
            cmd.AffectedSOPClassUID = MPPS
        elif label == 'affected-instance-fallback':
            del cmd.RequestedSOPInstanceUID
            cmd.AffectedSOPInstanceUID = uid
        elif label == 'missing-instance':
            del cmd.RequestedSOPInstanceUID
        cmd.CommandGroupLength = len(encode_dataset(cmd, True))-12
        data = Dataset()
        data.PerformedProcedureStepStatus = 'COMPLETED'
        (self.output/f'{label}-command.bin').write_bytes(encode_dataset(cmd, True))
        with self.connect(label) as sock:
            send_message(sock, context, cmd, data)
            response_status = None
            try:
                received_context, response, _ = receive_message(sock)
                if label in ('affected-instance-fallback', 'missing-instance'):
                    require(response is not None and received_context == 1, 'Missing N-SET error response')
                    require(response.CommandField == 0x8120 and response.MessageIDBeingRespondedTo == 77,
                            'Wrong N-SET error response identity')
                    response_status = int(response.Status)
                    expected = 0xc000 if label == 'affected-instance-fallback' else 0x0120
                    require(response_status == expected, f'Incorrect N-SET error status: {response_status:#x}')
                    sock.sendall(pdu(5, b'\0'*4))
                    require(receive_pdu(sock)[0] == 6, 'Release not acknowledged')
                else:
                    require(response is None and received_context == 7, 'Malformed SOP/context did not abort')
            except (EOFError, ConnectionResetError):
                require(label not in ('affected-instance-fallback', 'missing-instance'), 'Expected a DIMSE error response')
        after = self.row(uid)
        require(after == before, 'Malformed request changed durable state: '+label)
        self.echo()
        self.cases.append(dict(case=label, passed=True, status=response_status, before=before, after=after))
        self.save()

    def run(self):
        cases = [('1.2.3.1316.1', 'COMPLETED'), ('1.2.3.1316.10', 'COMPLETED'),
                 ('1.2.3.1316.2', 'DISCONTINUED'), ('1.2.3.1316.20', 'DISCONTINUED')]
        terminal_rows = {}
        with self.server('initial'):
            for uid, status in cases:
                self.create(uid)
                self.update(uid, status)
                before = self.row(uid)
                require(before['status'] == status and before['end_datetime'], 'Terminal state not persisted')
                self.update(uid, 'DISCONTINUED' if status == 'COMPLETED' else 'COMPLETED', 1)
                require(self.row(uid) == before, 'Post-terminal request changed state')
                terminal_rows[uid] = before
                self.cases.append(dict(case='lifecycle-'+uid, passed=True, uid_length=len(uid), state=before))
                self.save()
            self.create('1.2.3.1316.30')
            for label in ('wrong-class', 'wrong-context', 'unknown-context', 'affected-class-fallback',
                          'conflicting-class', 'affected-instance-fallback', 'missing-instance'):
                self.malformed(label, '1.2.3.1316.30')
        with self.server('restart'):
            for uid, status in cases:
                require(self.row(uid) == terminal_rows[uid], 'State changed across process restart')
                self.update(uid, status, 1)
                require(self.row(uid) == terminal_rows[uid], 'Restart allowed a terminal update')
                self.cases.append(dict(case='restart-'+uid, passed=True, state=self.row(uid)))
                self.save()
        require(len(self.cases) == 15, 'Unexpected empty/incomplete case selection')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', required=True)
    parser.add_argument('--output', required=True, help='New evidence directory outside the source tree')
    probe = Probe(parser.parse_args())
    try:
        probe.run()
    except BaseException as error:
        (probe.output/'failure.txt').write_text(repr(error)+'\n')
        raise
    print(f'MPPS protocol/durability: {len(probe.cases)} passed, 0 failed, 0 skipped')


if __name__ == '__main__':
    main()
