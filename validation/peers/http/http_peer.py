"""What the DICOMweb probe needs beyond curl and DCMTK: strict readers of HTTP payloads and independently signed tokens.

Nothing here imports or names PACS code. The multipart reader follows RFC 2046 and PS3.18 8.6, the image readers the
PNG and JPEG file formats, and tokens are signed by the openssl command.
"""
import base64
import json
from pathlib import Path
import re
import struct
import subprocess
import zlib


class PayloadError(ValueError):
    """A payload that does not follow its format, whatever the status line said."""


def parse_response_headers(raw):
    """curl -D writes one block per response; a 100 Continue comes first when the server sends one."""
    blocks = [block for block in raw.decode('latin-1').split('\r\n\r\n') if block.strip()]
    if not blocks:
        raise PayloadError('no response headers')
    lines = blocks[-1].split('\r\n')
    status = int(lines[0].split()[1])
    headers = {}
    for line in lines[1:]:
        name, _, value = line.partition(':')
        headers.setdefault(name.strip().lower(), []).append(value.strip())
    return status, headers


def parse_media_type(value):
    """Return the lowercased type and its parameters; quoted and unquoted parameter values are equivalent."""
    pieces = re.findall(r'(?:[^;"]|"(?:[^"\\]|\\.)*")+', value)
    if not pieces:
        raise PayloadError(f'no media type in {value!r}')
    parameters = {}
    for piece in pieces[1:]:
        name, _, parameter = piece.strip().partition('=')
        parameter = parameter.strip()
        if parameter.startswith('"') and parameter.endswith('"') and len(parameter) >= 2:
            parameter = parameter[1:-1]
        parameters[name.strip().lower()] = parameter
    return pieces[0].strip().lower(), parameters


class Part:
    def __init__(self, headers, data):
        self.headers, self.data = headers, data

    def content_type(self):
        return parse_media_type(self.headers.get('content-type', ''))[0] if 'content-type' in self.headers else ''


def split_multipart(content_type, body):
    """Read multipart/related strictly: a missing type or closing delimiter is an error, not a shorter list."""
    media, parameters = parse_media_type(content_type)
    if media != 'multipart/related':
        raise PayloadError(f'not multipart/related: {content_type}')
    if 'type' not in parameters:
        raise PayloadError(f'multipart/related without the required type parameter: {content_type}')
    if not parameters.get('boundary'):
        raise PayloadError(f'multipart/related without a boundary: {content_type}')
    delimiter = b'--'+parameters['boundary'].encode()
    if body.startswith(delimiter):
        position = 0
    else:
        position = body.find(b'\r\n'+delimiter)
        if position < 0:
            raise PayloadError('no delimiter in the body')
        position += 2
    parts = []
    while True:
        after = position+len(delimiter)
        if body[after:after+2] == b'--':
            if body[after+2:].strip(b'\r\n \t'):
                raise PayloadError('data after the closing delimiter')
            return parts
        line_end = body.find(b'\r\n', after)
        if line_end < 0 or body[after:line_end].strip(b' \t'):
            raise PayloadError('malformed delimiter line')
        following = body.find(b'\r\n'+delimiter, line_end)
        if following < 0:
            raise PayloadError('no closing delimiter: the body is truncated')
        section = body[line_end+2:following]
        if section.startswith(b'\r\n'):
            head, data = b'', section[2:]
        else:
            separator = section.find(b'\r\n\r\n')
            if separator < 0:
                raise PayloadError('a part without the blank line ending its headers')
            head, data = section[:separator], section[separator+4:]
        headers = {}
        for line in head.decode('latin-1').split('\r\n'):
            if line:
                name, _, value = line.partition(':')
                headers[name.strip().lower()] = value.strip()
        parts.append(Part(headers, data))
        position = following+2


def build_multipart(parts, boundary='probe-boundary-7f3a9c'):
    """A multipart/related body from (content type, bytes) pairs; returns the Content-Type and the body."""
    body = b''.join(f'--{boundary}\r\nContent-Type: {kind}\r\n\r\n'.encode()+data+b'\r\n' for kind, data in parts)
    return f'multipart/related; type="application/dicom"; boundary={boundary}', body+f'--{boundary}--\r\n'.encode()


def paeth(left, up, corner):
    estimate = left+up-corner
    a, b, c = abs(estimate-left), abs(estimate-up), abs(estimate-corner)
    return left if a <= b and a <= c else up if b <= c else corner


def read_png(data):
    """Decode a non-interlaced PNG; returns its size, depth, channel count and raw samples."""
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise PayloadError('no PNG signature')
    position, compressed, header, ended = 8, b'', None, False
    while position < len(data):
        length, kind = struct.unpack('>I4s', data[position:position+8])
        chunk = data[position+8:position+8+length]
        if len(chunk) != length or zlib.crc32(kind+chunk) != struct.unpack('>I', data[position+8+length:position+12+length])[0]:
            raise PayloadError(f'bad PNG chunk {kind!r}')
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', chunk)
        elif kind == b'IDAT':
            compressed += chunk
        elif kind == b'IEND':
            ended = True
        position += 12+length
    if not header or not ended:
        raise PayloadError('PNG without IHDR or IEND')
    width, height, depth, color, _, _, interlace = header
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    if interlace or depth not in (8, 16):
        raise PayloadError(f'unexpected PNG layout: depth {depth}, interlace {interlace}')
    step = channels*depth//8
    stride = width*step
    raw = zlib.decompress(compressed)
    if len(raw) != (stride+1)*height:
        raise PayloadError(f'PNG data of {len(raw)} bytes for {width}x{height}')
    samples, previous = bytearray(), bytes(stride)
    for row in range(height):
        kind, line = raw[row*(stride+1)], bytearray(raw[row*(stride+1)+1:(row+1)*(stride+1)])
        for index in range(stride):
            left = line[index-step] if index >= step else 0
            up = previous[index]
            corner = previous[index-step] if index >= step else 0
            line[index] = (line[index]+(0, left, up, (left+up)//2, paeth(left, up, corner))[kind]) & 255
        samples += line
        previous = bytes(line)
    return dict(width=width, height=height, depth=depth, channels=channels, samples=bytes(samples))


def read_jpeg(data):
    """Read a JPEG's frame header and check that it ends where a JPEG ends; decoding is left to DCMTK."""
    if data[:2] != b'\xff\xd8':
        raise PayloadError('no JPEG SOI')
    if data[-2:] != b'\xff\xd9':
        raise PayloadError('no JPEG EOI')
    position = 2
    while position+4 <= len(data):
        if data[position] != 0xFF:
            raise PayloadError(f'no JPEG marker at {position}')
        marker = data[position+1]
        length = struct.unpack('>H', data[position+2:position+4])[0]
        if 0xC0 <= marker <= 0xCF and marker not in (0xC4, 0xC8, 0xCC):
            height, width, components = struct.unpack('>xHHB', data[position+4:position+10])
            return dict(width=width, height=height, components=components, marker=marker)
        position += 2+length
    raise PayloadError('no JPEG frame header')


def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b'=').decode()


def der_length(size):
    if size < 128:
        return bytes([size])
    encoded = size.to_bytes((size.bit_length()+7)//8, 'big')
    return bytes([0x80 | len(encoded)])+encoded


def der_tlv(tag, value):
    return bytes([tag])+der_length(len(value))+value


def der_integer(value):
    encoded = value.to_bytes((value.bit_length()+8)//8, 'big')
    return der_tlv(2, encoded)


def der_ecdsa_to_raw(signature):
    """JWS wants r and s as 32 bytes each; openssl writes an ASN.1 SEQUENCE of two INTEGERs."""
    if signature[0] != 0x30:
        raise PayloadError('not a DER ECDSA signature')
    position = 2 if signature[1] < 128 else 2+(signature[1] & 0x7F)
    values = []
    for _ in range(2):
        size = signature[position+1]
        values.append(int.from_bytes(signature[position+2:position+2+size], 'big'))
        position += 2+size
    return b''.join(value.to_bytes(32, 'big') for value in values)


class TokenKit:
    """Keys made by openssl, a JWKS built from their public values, and tokens signed by openssl."""
    RSA_KID, EC_KID, OTHER_KID = 'probe-rsa', 'probe-ec', 'probe-not-published'

    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)
        self.rsa = self.directory/'rsa.pem'
        self.ec = self.directory/'ec.pem'
        self.other = self.directory/'other.pem'
        for path, options in ((self.rsa, ['-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048']),
                              (self.ec, ['-algorithm', 'EC', '-pkeyopt', 'ec_paramgen_curve:P-256']),
                              (self.other, ['-algorithm', 'RSA', '-pkeyopt', 'rsa_keygen_bits:2048'])):
            self.openssl('genpkey', *options, '-out', path)

    @staticmethod
    def openssl(*args, stdin=None):
        return subprocess.run(['openssl', *map(str, args)], input=stdin, check=True, capture_output=True).stdout

    def rsa_public(self, key):
        modulus = int(self.openssl('rsa', '-in', key, '-noout', '-modulus').decode().strip().split('=')[1], 16)
        text = self.openssl('rsa', '-in', key, '-noout', '-text').decode()
        exponent = int(re.search(r'publicExponent: (\d+)', text)[1])
        return modulus, exponent

    def jwk(self, key, kid):
        modulus, exponent = self.rsa_public(key)
        return dict(kty='RSA', kid=kid, use='sig', alg='RS256', n=b64url(modulus.to_bytes((modulus.bit_length()+7)//8, 'big')),
                    e=b64url(exponent.to_bytes((exponent.bit_length()+7)//8, 'big')))

    def jwks(self):
        point = self.openssl('pkey', '-in', self.ec, '-pubout', '-outform', 'DER')[-65:]
        return dict(keys=[self.jwk(self.rsa, self.RSA_KID),
                          dict(kty='EC', crv='P-256', kid=self.EC_KID, use='sig', alg='ES256', x=b64url(point[1:33]), y=b64url(point[33:]))])

    def signature(self, message, alg, key):
        signed = self.openssl('dgst', '-sha256', '-sign', key, stdin=message)
        return der_ecdsa_to_raw(signed) if alg == 'ES256' else signed

    def token(self, claims, alg='RS256', key=None, kid=None):
        key = key or (self.ec if alg == 'ES256' else self.rsa)
        kid = kid if kid is not None else (self.EC_KID if alg == 'ES256' else self.RSA_KID)
        header = dict(alg=alg, typ='JWT', kid=kid)
        message = (b64url(json.dumps(header).encode())+'.'+b64url(json.dumps(claims).encode())).encode()
        return message.decode()+'.'+b64url(self.signature(message, alg, key))

    def unsigned(self, claims):
        """alg none: no signature at all."""
        header = dict(alg='none', typ='JWT')
        return b64url(json.dumps(header).encode())+'.'+b64url(json.dumps(claims).encode())+'.'

    def hmac_with_public_key(self, claims):
        """The classic confusion: HS256 keyed with the bytes of the published RSA key."""
        header = dict(alg='HS256', typ='JWT', kid=self.RSA_KID)
        message = (b64url(json.dumps(header).encode())+'.'+b64url(json.dumps(claims).encode())).encode()
        public = self.openssl('pkey', '-in', self.rsa, '-pubout')
        mac = self.openssl('dgst', '-sha256', '-mac', 'HMAC', '-macopt', 'hexkey:'+public.hex(), '-binary', stdin=message)
        return message.decode()+'.'+b64url(mac)

    def verify_with_jwks(self, token):
        """Rebuild the public key from the JWKS alone and let openssl verify the token: the qualification of this kit."""
        header_text, payload, signature = token.split('.')
        header = json.loads(base64.urlsafe_b64decode(header_text+'='*(-len(header_text) % 4)))
        jwk = next(row for row in self.jwks()['keys'] if row['kid'] == header['kid'])
        raw = base64.urlsafe_b64decode(signature+'='*(-len(signature) % 4))
        decode = lambda value: int.from_bytes(base64.urlsafe_b64decode(value+'='*(-len(value) % 4)), 'big')
        if jwk['kty'] == 'RSA':
            body = der_tlv(0x30, der_integer(decode(jwk['n']))+der_integer(decode(jwk['e'])))
            algorithm = der_tlv(0x30, bytes.fromhex('06092a864886f70d0101010500'))
        else:
            point = b'\x04'+decode(jwk['x']).to_bytes(32, 'big')+decode(jwk['y']).to_bytes(32, 'big')
            body = point
            algorithm = der_tlv(0x30, bytes.fromhex('06072a8648ce3d020106082a8648ce3d030107'))
            raw = der_tlv(0x30, der_integer(int.from_bytes(raw[:32], 'big'))+der_integer(int.from_bytes(raw[32:], 'big')))
        spki = der_tlv(0x30, algorithm+der_tlv(3, b'\x00'+body))
        public, sigfile = self.directory/'verify.der', self.directory/'verify.sig'
        public.write_bytes(spki)
        sigfile.write_bytes(raw)
        done = subprocess.run(['openssl', 'dgst', '-sha256', '-keyform', 'DER', '-verify', str(public), '-signature', str(sigfile)],
                              input=f'{header_text}.{payload}'.encode(), capture_output=True)
        return done.returncode == 0
