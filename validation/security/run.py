#!/usr/bin/env python3
"""Actual TLS/DIMSE, external audit receipt and recursive de-identification.

Public output and private fixture/audit evidence must be separate new directories.
Private keys are removed on exit; raw synthetic audit payloads remain private.
Missing tools/certificates and incomplete selections fail, never skip.
"""
import argparse
from contextlib import contextmanager
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import time
import unittest

from receiver import Receiver, audit, frames, require

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'validation/dcmtk/tests'))
from get_scu_protocol_probe import (LE, command, item, items, pdu,
                                    receive_message, receive_pdu, send_message)

ECHO = '1.2.840.10008.1.1'
PATIENT = 'SECURITY_SOURCE_PATIENT_1317'
STUDY = '1.2.826.0.1.3680043.10.543.1317.1'
SOURCE_UID = STUDY+'.1.1'
DICOM_CASES = ['dcmtk-mtls', 'tls12', 'tls13', 'plaintext', 'untrusted-client',
               'expired-client', 'missing-client', 'reject-tls11',
               'reject-tls12-under13', 'tls13-policy', 'recovery',
               'client-untrusted-server', 'client-expired-server',
               'client-hostname-mismatch', 'client-independent-mtls']
AUDIT_CASES = ['tls-single', 'tls-concurrent', 'udp-concurrent', 'reject-hostname',
               'reject-untrusted-server', 'reject-expired-server',
               'reject-untrusted-client', 'reject-missing-client',
               'reject-mismatched-key', 'reject-incomplete-key',
               'store-tls', 'store-udp']


def port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


def leaks(data, needles):
    return any(needle in data for needle in needles)


class Matrix:
    def __init__(self, args):
        self.bin = Path(args.bin_dir).resolve()
        self.output = Path(args.output).resolve()
        self.private = Path(args.private_output).resolve() if args.private_output else Path(tempfile.mkdtemp(prefix='pacs-security-'))
        require(self.output != self.private and self.output not in self.private.parents and
                self.private not in self.output.parents, 'Public and private evidence must be separate')
        self.output.mkdir(parents=True, exist_ok=False)
        if args.private_output:
            self.private.mkdir(parents=True, mode=0o700, exist_ok=False)
        self.private.chmod(0o700)
        self.commands, self.results = [], {}
        self.needles = [value.encode() for value in (PATIENT, STUDY, 'SOURCE^PATIENT', 'SOURCE-ID',
                        'SOURCE-ACCESS', 'SOURCE-PRIVATE', 'SOURCE-CREATOR', 'SOURCE-PREAMBLE',
                        'SOURCE-AE', 'SOURCE-PROC', 'SECURITY_CREDENTIAL_CANARY_1317',
                        'SYNTHETIC_AUDIT_SUBJECT_', '-----BEGIN PRIVATE KEY-----',
                        '-----BEGIN RSA PRIVATE KEY-----', '-----BEGIN EC PRIVATE KEY-----')]
        self.current = 'setup'
        spec = importlib.util.spec_from_file_location('recursive', ROOT/'tools/integration_tests/scripts/test_recursive_anonymization.py')
        self.deid = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.deid)
        self.needles.extend(value.encode() for value in (self.deid.STUDY, self.deid.SERIES, self.deid.IMAGE, self.deid.OTHER))
        self.deid_names = unittest.defaultTestLoader.getTestCaseNames(self.deid.RecursiveAnonymization)
        self.inventory = (['dicom.'+name for name in DICOM_CASES] + ['audit.'+name for name in AUDIT_CASES]
                          + ['deid.'+name for name in self.deid_names] + ['logs.scanner-regression', 'logs.public-clean'])
        declarations = json.loads((Path(__file__).parent/'cases.json').read_text())['cases']
        require([case['id'] for case in declarations] == self.inventory, 'Security inventory differs from executable cases')
        (self.output/'inventory.json').write_text(json.dumps(declarations, indent=2)+'\n')
        for name in self.inventory:
            self.results[name] = dict(id=name, status='blocked', detail='Not executed', tests=0, failed=0, skipped=0)
        self.save()

    def save(self):
        (self.output/'results.json').write_text(json.dumps(dict(schema_version=1, private_evidence=str(self.private),
            cases=list(self.results.values()), commands=self.commands), indent=2)+'\n')

    def case(self, name, action):
        self.current = name
        started = time.time()
        try:
            evidence = action() or {}
            self.results[name] = dict(id=name, status='passed', tests=1, failed=0, skipped=0, evidence=evidence)
        except Exception as error:
            # Never include source identifiers or raw audit XML in a public exception.
            detail = str(error)
            if leaks(detail.encode(), self.needles):
                detail = 'Failure contains a configured source identifier; inspect private evidence'
            self.results[name] = dict(id=name, status='failed', tests=1, failed=1, skipped=0, detail=detail)
        self.results[name]['seconds'] = round(time.time()-started, 3)
        self.save()

    def command(self, args, private=False, timeout=20):
        args = list(map(str, args))
        index = len(self.commands)+1
        process = subprocess.run(args, capture_output=True, timeout=timeout)
        (self.private/f'command-{index}.json').write_text(json.dumps(dict(argv=args, exit=process.returncode), indent=2)+'\n')
        public_args = ['<private argument sha256='+hashlib.sha256(arg.encode()).hexdigest()+'>'
                       if leaks(arg.encode(), self.needles) else arg for arg in args]
        record = dict(case=self.current, argv=public_args, exit=process.returncode, private_logs=private)
        self.commands.append(record)
        for suffix, data in [('stdout', process.stdout), ('stderr', process.stderr)]:
            name = f'command-{index}.{suffix}'
            # Raw output is retained privately before checking its publication eligibility.
            (self.private/name).write_bytes(data)
            record[suffix+'_sha256'] = hashlib.sha256(data).hexdigest()
            if not private:
                require(not leaks(data, self.needles), 'Command output contains a configured source identifier or secret')
                (self.output/name).write_bytes(data)
        self.save()
        return process

    def setup(self):
        for binary in ['openssl', 'echoscu', 'storescu', 'storescp', 'dump2dcm', 'dcm2xml', 'dcmdump']:
            require(shutil.which(binary), 'Missing required independent tool: '+binary)
        for binary in ['secure_echo_scp', 'secure_echo_scu', 'pacs_security_probe', 'dcm_anonymize']:
            require((self.bin/binary).is_file(), 'Missing required production binary: '+binary)
        result = self.command(['bash', ROOT/'tools/integration_tests/test_data/certs/generate_test_certs.sh', self.private])
        require(result.returncode == 0, 'Certificate setup failed')
        require((self.private/'expired.crt').exists(), 'Expired-certificate fixture is required')
        for name, ca, days, dns in [('wrong-host', 'ca', 2, 'wrong.invalid'),
                                    ('expired-server', 'ca', -1, 'localhost'),
                                    ('other-client', 'other_ca', 2, 'localhost')]:
            config = self.private/(name+'.cnf')
            config.write_text('basicConstraints=CA:FALSE\nkeyUsage=digitalSignature,keyEncipherment\n'
                              'extendedKeyUsage=serverAuth,clientAuth\nsubjectAltName=DNS:'+dns+
                              (',IP:127.0.0.1' if dns == 'localhost' else '')+'\n')
            commands = [
                ['openssl', 'req', '-new', '-newkey', 'rsa:2048', '-nodes', '-keyout', self.private/(name+'.key'),
                 '-out', self.private/(name+'.csr'), '-subj', '/CN='+dns]]
            if days < 0:
                signing = self.private/'signing'
                signing.mkdir()
                (signing/'index').touch()
                (signing/'serial').write_text('1000\n')
                (signing/'ca.cnf').write_text('[ca]\ndefault_ca=test_ca\n[test_ca]\n'+
                    f'database="{signing}/index"\nserial="{signing}/serial"\nnew_certs_dir="{signing}"\n'+
                    f'certificate="{self.private}/ca.crt"\nprivate_key="{self.private}/ca.key"\n'+
                    'default_md=sha256\npolicy=subject_policy\n[subject_policy]\ncommonName=supplied\n')
                commands.append(['openssl', 'ca', '-batch', '-notext', '-config', signing/'ca.cnf',
                    '-in', self.private/(name+'.csr'), '-out', self.private/(name+'.crt'),
                    '-startdate', '20000101000000Z', '-enddate', '20010101000000Z', '-extfile', config])
            else:
                commands.append(['openssl', 'x509', '-req', '-in', self.private/(name+'.csr'), '-CA', self.private/(ca+'.crt'),
                 '-CAkey', self.private/(ca+'.key'), '-CAcreateserial', '-days', str(days),
                 '-extfile', config, '-out', self.private/(name+'.crt')])
            for args in commands:
                require(self.command(args).returncode == 0, 'Additional certificate setup failed')
        for key in self.private.glob('*.key'):
            key.chmod(0o600)
            self.needles.extend(line for line in key.read_bytes().splitlines() if len(line) >= 48)
        for name in ('expired', 'expired-server'):
            result = self.command(['openssl', 'verify', '-CAfile', self.private/'ca.crt', self.private/(name+'.crt')])
            require(result.returncode != 0 and b'certificate has expired' in result.stderr, 'Fixture must be expired')
        self.certificates = {}
        for path in sorted(self.private.glob('*.crt')):
            der = ssl.PEM_cert_to_DER_cert(path.read_text())
            self.certificates[path.stem] = hashlib.sha256(der).hexdigest()
            require(self.command(['openssl', 'x509', '-in', path, '-noout', '-subject', '-issuer', '-dates',
                                  '-fingerprint', '-sha256']).returncode == 0, 'Certificate identity inspection failed')
        (self.output/'certificates.json').write_text(json.dumps(self.certificates, indent=2)+'\n')
        self.command(['openssl', 'version', '-a'])
        self.command(['echoscu', '--version'])
        binaries = {name: hashlib.sha256((self.bin/name).read_bytes()).hexdigest()
                    for name in ['secure_echo_scp', 'secure_echo_scu', 'pacs_security_probe', 'dcm_anonymize']}
        (self.output/'environment.json').write_text(json.dumps(dict(python=sys.version, openssl=ssl.OPENSSL_VERSION,
            binary_sha256=binaries, source_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [Path(__file__), ROOT/'validation/security/receiver.py', ROOT/'validation/security/security_probe.cpp']}), indent=2)+'\n')

    @contextmanager
    def server(self, args, listen_port, label):
        args = list(map(str, args))
        path = self.private/(label+'.server.log')
        with path.open('wb') as log:
            process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                deadline = time.monotonic()+12
                while True:
                    require(process.poll() is None, 'Server exited before readiness: '+label)
                    try:
                        with socket.create_connection(('127.0.0.1', listen_port), .2):
                            break
                    except OSError:
                        require(time.monotonic() < deadline, 'Server did not listen: '+label)
                        time.sleep(.05)
                yield path
            finally:
                forced = False
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        forced = True
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                self.commands.append(dict(case=self.current, argv=args, exit=process.returncode,
                                          forced_kill=forced, private_logs=True,
                                          stdout_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
                self.save()
                expected = (0, -signal.SIGTERM) if Path(args[0]).name == 'storescp' else (0,)
                require(not forced and process.returncode in expected, 'Server shutdown failed: '+label)

    def scp_args(self, listen_port, certificate='server', version='1.2'):
        return [self.bin/'secure_echo_scp', listen_port, 'SECURITY_SCP', '--cert', self.private/(certificate+'.crt'),
                '--key', self.private/(certificate+'.key'), '--ca', self.private/'ca.crt', '--tls-version', version]

    def scu_args(self, listen_port, ca='ca'):
        return [self.bin/'secure_echo_scu', '127.0.0.1', listen_port, 'SECURITY_SCP',
                '--cert', self.private/'client.crt', '--key', self.private/'client.key',
                '--ca', self.private/(ca+'.crt'), '--timeout', '5000', '--calling-ae', 'SECURITY_SCU']

    def dcmtk_args(self, binary, listen_port):
        return [binary, '-d', '-aet', 'SECURITY_SCU', '-aec', 'SECURITY_SCP', '+tls', self.private/'client.key',
                self.private/'client.crt', '-pw', '+cf', self.private/'ca.crt', '-to', '5', '-ta', '5', '-td', '5',
                '127.0.0.1', listen_port]

    def echo(self, listen_port, version=ssl.TLSVersion.TLSv1_3, client='client'):
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
        context.minimum_version = context.maximum_version = version
        context.load_verify_locations(self.private/'ca.crt')
        if client:
            context.load_cert_chain(self.private/(client+'.crt'), self.private/(client+'.key'))
        if version == ssl.TLSVersion.TLSv1_1:
            context.set_ciphers('DEFAULT:@SECLEVEL=0')
        with socket.create_connection(('127.0.0.1', listen_port), 5) as connection:
            with context.wrap_socket(connection, server_hostname='127.0.0.1') as tls:
                handshake = dict(version=tls.version(), cipher=tls.cipher(),
                                 peer_sha256=hashlib.sha256(tls.getpeercert(binary_form=True)).hexdigest())
                require(handshake['peer_sha256'] == self.certificates['server'], 'Unexpected DICOM server certificate identity')
                rq = struct.pack('>H', 1)+b'\0'*2+b'SECURITY_SCP'.ljust(16)+b'SECURITY_SCU'.ljust(16)+b'\0'*32
                rq += item(0x10, b'1.2.840.10008.3.1.1.1')
                rq += item(0x20, b'\x01\0\0\0'+item(0x30, ECHO.encode())+item(0x40, LE.encode()))
                rq += item(0x50, item(0x51, struct.pack('>I', 16384))+item(0x52, b'1.2.3.1317'))
                tls.sendall(pdu(1, rq))
                kind, accepted = receive_pdu(tls)
                require(kind == 2, 'Association was not accepted')
                require([body[2] for tag, body in items(accepted[68:]) if tag == 0x21] == [0], 'Echo context rejected')
                send_message(tls, 1, command(0x0030, message_id=73, sop_class=ECHO))
                received_context, response, data = receive_message(tls)
                require(received_context == 1 and response is not None and response.CommandField == 0x8030 and
                        response.MessageIDBeingRespondedTo == 73 and response.Status == 0 and data is None,
                        'Incorrect C-ECHO response identity/status')
                tls.sendall(pdu(5, b'\0'*4))
                require(receive_pdu(tls)[0] == 6, 'Association release not acknowledged')
                return dict(handshake=handshake, dimse_status=0, message_id=73)

    def rejected_echo(self, listen_port, reason, **kwargs):
        try:
            self.echo(listen_port, **kwargs)
        except ssl.SSLError as error:
            require(reason in str(error), 'Unexpected TLS rejection reason: '+str(error))
            return dict(tls_error=str(error), required_reason=reason)
        raise AssertionError('Expected TLS rejection did not occur')

    def dcmtk_echo(self, listen_port):
        result = self.command(self.dcmtk_args('echoscu', listen_port))
        text = (result.stdout+result.stderr).decode()
        require(result.returncode == 0 and 'Received Echo Response (Success)' in text, 'DCMTK C-ECHO failed')
        require('Protocol    : TLSv1.' in text and 'Ciphersuite :' in text and 'Peer X.509v3 Certificate' in text,
                'Missing DCMTK handshake/certificate evidence')
        return dict(dimse_status=0, independent_peer='DCMTK echoscu')

    def plaintext(self, listen_port, server_log):
        offset = server_log.stat().st_size
        result = self.command(['echoscu', '-v', '-aet', 'SECURITY_SCU', '-aec', 'SECURITY_SCP',
                               '-to', '3', '-ta', '3', '-td', '3', '127.0.0.1', listen_port])
        require(result.returncode != 0, 'Plaintext DIMSE was accepted')
        time.sleep(.1)
        error = server_log.read_text()[offset:]
        require('wrong version number' in error, 'Plaintext failed without the expected server TLS error')
        return dict(exit=result.returncode, server_tls_reason='wrong version number')

    def rejected_server(self, certificate='server', ca='ca', reason='certificate verify failed'):
        listen_port = port()
        with self.server(self.scp_args(listen_port, certificate), listen_port, self.current):
            result = self.command(self.scu_args(listen_port, ca))
            text = (result.stdout+result.stderr).decode()
            require(result.returncode == 1 and reason in text, 'PACS client did not reject the expected TLS identity/trust failure')
        return dict(exit=result.returncode, required_reason=reason)

    def independent_server(self):
        listen_port = port()
        storage = self.private/'dcmtk-store'
        storage.mkdir()
        args = ['storescp', '-v', '-aet', 'SECURITY_SCP', '+tls', self.private/'server.key', self.private/'server.crt',
                '-pw', '+cf', self.private/'ca.crt', '-od', storage, listen_port]
        with self.server(args, listen_port, self.current):
            result = self.command(self.scu_args(listen_port))
            require(result.returncode == 0 and b'C-ECHO successful!' in result.stdout, 'PACS client C-ECHO to DCMTK failed')
        return dict(exit=0, independent_peer='DCMTK storescp')

    def dicom(self):
        listen_port = port()
        with self.server(self.scp_args(listen_port), listen_port, 'dicom-main') as log:
            self.case('dicom.dcmtk-mtls', lambda: self.dcmtk_echo(listen_port))
            self.case('dicom.tls12', lambda: self.echo(listen_port, ssl.TLSVersion.TLSv1_2))
            self.case('dicom.tls13', lambda: self.echo(listen_port))
            self.case('dicom.plaintext', lambda: self.plaintext(listen_port, log))
            for label, client, reason in [('untrusted-client', 'other-client', 'UNKNOWN_CA'),
                                          ('expired-client', 'expired', 'CERTIFICATE_EXPIRED'),
                                          ('missing-client', None, 'CERTIFICATE_REQUIRED')]:
                self.case('dicom.'+label, lambda client=client, reason=reason:
                          self.rejected_echo(listen_port, reason, client=client))
            self.case('dicom.reject-tls11', lambda: self.rejected_echo(
                listen_port, 'PROTOCOL_VERSION', version=ssl.TLSVersion.TLSv1_1))
            self.case('dicom.recovery', lambda: self.dcmtk_echo(listen_port))
        listen_port = port()
        with self.server(self.scp_args(listen_port, version='1.3'), listen_port, 'dicom-tls13-policy'):
            self.case('dicom.reject-tls12-under13', lambda: self.rejected_echo(
                listen_port, 'PROTOCOL_VERSION', version=ssl.TLSVersion.TLSv1_2))
            self.case('dicom.tls13-policy', lambda: self.echo(listen_port))
        self.case('dicom.client-untrusted-server', lambda: self.rejected_server(
            ca='other_ca', reason='self-signed certificate in certificate chain'))
        self.case('dicom.client-expired-server', lambda: self.rejected_server(
            certificate='expired-server', reason='certificate has expired'))
        self.case('dicom.client-hostname-mismatch', lambda: self.rejected_server(
            certificate='wrong-host', reason='IP address mismatch'))
        self.case('dicom.client-independent-mtls', self.independent_server)

    def audit_send(self, protocol='tls', count=1, certificate='server', ca='ca', client='client',
                   key='client', reason=None, maximum=None):
        receiver = Receiver(protocol, self.private, self.current, cert=certificate, maximum=maximum)
        start = time.time()
        try:
            result = self.command([self.bin/'pacs_security_probe', 'send', protocol, '127.0.0.1', receiver.port,
                self.private/(ca+'.crt'), self.private/(client+'.crt') if client else '-',
                self.private/(key+'.key') if key else '-', count])
        finally:
            messages = receiver.finish()
        end = time.time()
        evidence = receiver.evidence()
        evidence.update(exit=result.returncode, messages=len(messages))
        (self.output/(self.current+'.receiver.json')).write_text(json.dumps(evidence, indent=2)+'\n')
        if reason:
            require(result.returncode == 1 and not messages, 'Invalid audit TLS configuration delivered a message or reported success')
            require(reason in result.stderr.decode(), 'Unexpected audit TLS failure reason')
            evidence['required_reason'] = reason
        else:
            require(result.returncode == 0 and len(messages) == count, 'External audit count does not match successful sends')
            require(not receiver.errors, 'Audit receiver reported an unclean TLS shutdown')
            if protocol == 'tls':
                require(len(receiver.handshakes) == 1 and receiver.handshakes[0]['peer_sha256'] == self.certificates[client],
                        'Unexpected audit sender certificate identity')
            users = set()
            for message in messages:
                root, event = audit(message, start, end)
                require(event.find('EventID').get('csd-code') == '110114' and event.get('EventActionCode') == 'E' and
                        event.get('EventOutcomeIndicator') == '0', 'Incorrect authentication probe event')
                users.add(root.find('ActiveParticipant').get('UserID'))
            require(users == {'SYNTHETIC_AUDIT_SUBJECT_'+str(i) for i in range(count)}, 'Lost/duplicate/interleaved audit event')
            evidence['identity_set_verified'] = True
        return evidence

    def audit_store(self, protocol):
        receiver = Receiver(protocol, self.private, self.current)
        storage = self.private/('storage-'+protocol)
        listen_port = port()
        fixture = self.private/'store.dump'
        fixture.write_text(f'(0008,0016) UI [1.2.840.10008.5.1.4.1.1.2]\n(0008,0018) UI [{SOURCE_UID}]\n'
            f'(0010,0010) PN [SOURCE^PATIENT]\n(0010,0020) LO [{PATIENT}]\n(0020,000d) UI [{STUDY}]\n'
            f'(0020,000e) UI [{STUDY}.1]\n(0008,0060) CS [CT]\n(0028,0002) US 1\n'
            '(0028,0004) CS [MONOCHROME2]\n(0028,0010) US 1\n(0028,0011) US 1\n'
            '(0028,0100) US 16\n(0028,0101) US 12\n(0028,0102) US 11\n(0028,0103) US 0\n(7fe0,0010) OW 002a\n')
        source = self.private/'store.dcm'
        require(self.command(['dump2dcm', fixture, source], private=True).returncode == 0, 'Storage fixture failed')
        args = [self.bin/'pacs_security_probe', 'serve', listen_port, self.private/'server.crt', self.private/'server.key',
                self.private/'ca.crt', protocol, '127.0.0.1', receiver.port, self.private/'ca.crt',
                self.private/'client.crt', self.private/'client.key', storage]
        intervals, stored_hash = [], None
        try:
            with self.server(args, listen_port, self.current):
                for success in (True, False):
                    start = time.time()
                    result = self.command(self.dcmtk_args('storescu', listen_port)+[source], private=True)
                    end = time.time()
                    text = (result.stdout+result.stderr).decode()
                    require((result.returncode == 0) == success, 'Unexpected DCMTK store exit')
                    expected = '0x0000: Success' if success else '0xa700:'
                    require(expected in text, 'Unexpected C-STORE response status')
                    files = list(storage.rglob('*.dcm'))
                    require(len(files) == 1, 'C-STORE did not preserve exactly one object')
                    current_hash = hashlib.sha256(files[0].read_bytes()).hexdigest()
                    if success:
                        stored_hash = current_hash
                    else:
                        require(current_hash == stored_hash, 'Rejected duplicate changed stored bytes')
                    intervals.append((start, end))
                files = list(storage.rglob('*.dcm'))
                require(len(files) == 1, 'Duplicate rejection did not preserve exactly one stored object')
                result = self.command(['dcm2xml', '+M', files[0]], private=True)
                require(result.returncode == 0 and SOURCE_UID.encode() in result.stdout and PATIENT.encode() in result.stdout,
                        'Independent stored-object inspection failed')
        finally:
            messages = receiver.finish()
        require(len(messages) == 2, 'Actual success/failure did not emit exactly two external audit events')
        require(not receiver.errors, 'Audit store receiver reported a transport error')
        if protocol == 'tls':
            require(len(receiver.handshakes) == 1 and receiver.handshakes[0]['peer_sha256'] == self.certificates['client'],
                    'Unexpected native service audit sender identity')
        for message, interval, outcome in zip(messages, intervals, ('0', '8')):
            root, event = audit(message, *interval)
            require(event.find('EventID').get('csd-code') == '110104' and event.get('EventActionCode') == 'C' and
                    event.get('EventOutcomeIndicator') == outcome, 'Audit operation/outcome does not match C-STORE')
            require({node.get('UserID') for node in root.findall('ActiveParticipant')} == {'SECURITY_SCU', 'SECURITY_SCP'},
                    'Audit AE identity does not match actual association')
            require({node.get('ParticipantObjectID') for node in root.findall('ParticipantObjectIdentification')} ==
                    {STUDY, PATIENT}, 'Audit object identity does not match independently supplied data')
        return dict(**receiver.evidence(), messages=2, outcomes=[0, 8], event_code='110104',
                    stored_objects=1, identities_verified=True, source='native storage_scp audit hook')

    def audits(self):
        self.case('audit.tls-single', self.audit_send)
        self.case('audit.tls-concurrent', lambda: self.audit_send(count=16))
        self.case('audit.udp-concurrent', lambda: self.audit_send(protocol='udp', count=16))
        for label, kwargs in [
            ('reject-hostname', dict(certificate='wrong-host', reason='IP address mismatch')),
            ('reject-untrusted-server', dict(ca='other_ca', reason='self-signed certificate in certificate chain')),
            ('reject-expired-server', dict(certificate='expired-server', reason='certificate has expired')),
            ('reject-untrusted-client', dict(client='other-client', key='other-client', reason='unknown ca', maximum=ssl.TLSVersion.TLSv1_2)),
            ('reject-missing-client', dict(client=None, key=None, reason='handshake failure', maximum=ssl.TLSVersion.TLSv1_2)),
            ('reject-mismatched-key', dict(key='server', reason='Failed to load client key')),
            ('reject-incomplete-key', dict(key=None, reason='must be configured together'))]:
            self.case('audit.'+label, lambda kwargs=kwargs: self.audit_send(**kwargs))
        for protocol in ('tls', 'udp'):
            self.case('audit.store-'+protocol, lambda protocol=protocol: self.audit_store(protocol))

    def anonymization(self):
        self.deid.BINARY = self.bin/'dcm_anonymize'
        self.deid.EVIDENCE = self.private/'anonymization'
        self.needles.extend(value.encode() for value in self.deid.IDENTIFIERS)
        for name in self.deid_names:
            def action(name=name):
                # Reuse #1300's assertions and corpus. Raw dumps include deliberate retained IDs.
                test = self.deid.RecursiveAnonymization(name)
                with (self.private/(name+'.log')).open('w') as log:
                    result = unittest.TextTestRunner(stream=log, verbosity=2).run(unittest.TestSuite([test]))
                require(result.wasSuccessful() and result.testsRun == 1 and not result.skipped,
                        'Recursive anonymization failed; inspect private corpus/log')
                return dict(independent_oracle='DCMTK dcm2xml/dcmdump', cases=1,
                            evidence_sha256=hashlib.sha256((self.private/(name+'.log')).read_bytes()).hexdigest())
            self.case('deid.'+name, action)

    def scanner_regression(self):
        for needle in self.needles:
            require(leaks(b'before '+needle+b' after', self.needles), 'Sensitive-output scanner missed configured canary')
        require(not leaks(b'ordinary safe public report', self.needles), 'Sensitive-output scanner rejected safe report')
        for malformed in (b'4 abc', b'0 ', b'abc payload', b'3 abcgarbage'):
            try:
                frames(malformed)
            except AssertionError:
                continue
            raise AssertionError('Audit oracle accepted malformed/truncated framing')
        return dict(canaries=len(self.needles), rejected_frame_mutations=4)

    def public_clean(self):
        files = [path for path in self.output.rglob('*') if path.is_file()]
        require(files, 'Empty public evidence cannot establish confidentiality')
        for path in files:
            require(not leaks(path.read_bytes(), self.needles), 'Public artifact contains a configured identifier or secret: '+path.name)
        return dict(scanned_files=len(files), configured_canaries=len(self.needles),
                    scope='public artifacts and designated anonymizer diagnostics; raw synthetic payloads remain private')

    def run(self):
        try:
            self.setup()
            for section in (self.dicom, self.audits, self.anonymization):
                try:
                    section()
                except Exception as error:
                    self.case(section.__name__+'.setup', lambda error=error: (_ for _ in ()).throw(error))
            self.case('logs.scanner-regression', self.scanner_regression)
            self.case('logs.public-clean', self.public_clean)
        except Exception as error:
            self.case('setup', lambda: (_ for _ in ()).throw(error))
        finally:
            # Audit payloads and source objects remain available with mode 0700;
            # the disposable signing/client private keys are never published.
            for path in self.private.glob('*.key'):
                path.unlink()
            self.save()
        passed = sum(result['status'] == 'passed' for result in self.results.values())
        print(json.dumps(dict(passed=passed, failed=sum(r['status'] == 'failed' for r in self.results.values()),
                              blocked=sum(r['status'] == 'blocked' for r in self.results.values()), skipped=0)))
        return 0 if passed == len(self.inventory) == len(self.results) else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bin-dir', required=True)
    parser.add_argument('--output', required=True, help='New PUBLIC evidence directory; safe to upload after clean scan')
    parser.add_argument('--private-output', help='Separate new PRIVATE directory, mode 0700 (default: system temporary directory)')
    args = parser.parse_args()
    return Matrix(args).run()


if __name__ == '__main__':
    sys.exit(main())
