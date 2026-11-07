"""Independent UDP / RFC 5425 receiver. Raw audit messages stay in private evidence."""
import hashlib
import socket
import ssl
import threading
import xml.etree.ElementTree as ET
from datetime import datetime


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def frames(data):
    """Decode complete octet-counted messages; never accept partial/extra bytes."""
    result = []
    while data:
        prefix, separator, remainder = data.partition(b' ')
        require(separator and prefix.isdigit() and not prefix.startswith(b'0'), 'Invalid syslog frame length')
        size = int(prefix)
        require(0 < size <= len(remainder), 'Truncated syslog frame')
        result.append(remainder[:size])
        data = remainder[size:]
    return result


def audit(message, start, end):
    header, separator, xml = message.partition(b'\xef\xbb\xbf')
    require(separator, 'Missing UTF-8 syslog BOM')
    parts = header.decode().split()
    require(len(parts) == 7 and parts[0] == '<86>1' and parts[3:] ==
            ['pacs-security-probe', '-', 'IHE+RFC-3881', '-'], 'Incorrect RFC 5424 header')
    require(parts[2] == 'pacs-security-validation', 'Incorrect syslog origin')
    root = ET.fromstring(xml)
    require(root.tag == 'AuditMessage', 'Incorrect audit XML root')
    event = root.find('EventIdentification')
    require(event is not None, 'Missing event identity')
    for value in (parts[1], event.get('EventDateTime')):
        parsed = datetime.fromisoformat(value.replace('Z', '+00:00'))
        require(parsed.tzinfo is not None, 'Timestamp has no time zone')
        require(start - 1 <= parsed.timestamp() <= end + 1, 'Timestamp does not match the operation interval')
    require(root.find('AuditSourceIdentification').get('AuditSourceID') == 'SECURITY_PROBE',
            'Incorrect audit source')
    return root, event


class Receiver:
    def __init__(self, protocol, private, label, cert='server', ca='ca', maximum=None):
        self.protocol, self.private, self.label = protocol, private, label
        self.packets, self.handshakes, self.errors = [], [], []
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM if protocol == 'udp' else socket.SOCK_STREAM)
        self.listener.bind(('127.0.0.1', 0))
        self.port = self.listener.getsockname()[1]
        self.listener.settimeout(.2)
        self.stopped = threading.Event()
        self.context = None
        if protocol == 'tls':
            self.context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            self.context.minimum_version = ssl.TLSVersion.TLSv1_2
            if maximum:
                self.context.maximum_version = maximum
            self.context.load_cert_chain(private/(cert+'.crt'), private/(cert+'.key'))
            self.context.load_verify_locations(private/(ca+'.crt'))
            self.context.verify_mode = ssl.CERT_REQUIRED
            self.listener.listen()
        self.thread = threading.Thread(target=self.run)
        self.thread.start()

    def run(self):
        try:
            while True:
                try:
                    if self.protocol == 'udp':
                        self.packets.append(self.listener.recvfrom(65536)[0])
                        continue
                    connection, _ = self.listener.accept()
                except socket.timeout:
                    if self.stopped.is_set():
                        break
                    continue
                data = bytearray()
                try:
                    with connection:
                        connection.settimeout(8)
                        with self.context.wrap_socket(connection, server_side=True) as tls:
                            certificate = tls.getpeercert(binary_form=True)
                            self.handshakes.append(dict(version=tls.version(), cipher=tls.cipher(),
                                                        peer_sha256=hashlib.sha256(certificate).hexdigest()))
                            while True:
                                chunk = tls.recv(65536)
                                if not chunk:
                                    break
                                data.extend(chunk)
                            tls.unwrap().close()
                except (ssl.SSLError, OSError) as error:
                    self.errors.append(str(error))
                finally:
                    # Preserve bytes already received even if the peer resets the
                    # connection. Count/framing checks still reject missing data.
                    if data:
                        self.packets.append(bytes(data))
        finally:
            self.listener.close()

    def finish(self):
        self.stopped.set()
        self.thread.join(10)
        require(not self.thread.is_alive(), 'External audit receiver did not stop')
        for index, packet in enumerate(self.packets):
            (self.private/f'{self.label}-{index}.received').write_bytes(packet)
        return [message for packet in self.packets for message in
                (frames(packet) if self.protocol == 'tls' else [packet])]

    def evidence(self):
        return dict(handshakes=self.handshakes, errors=self.errors,
                    received_octets=sum(map(len, self.packets)),
                    payload_sha256=[hashlib.sha256(packet).hexdigest() for packet in self.packets])
