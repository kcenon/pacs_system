"""Shared by the scripted peers. Uses pydicom and pynetdicom only; nothing here touches PACS code."""
import base64
import json
from pathlib import Path

from pydicom.dataelem import RawDataElement
from pydicom.dataset import Dataset
from pydicom.multival import MultiValue
from pydicom.sequence import Sequence
from pydicom.tag import Tag

SYNTAXES = ('1.2.840.10008.1.2.1', '1.2.840.10008.1.2')


def to_dataset(values):
    """{keyword: value} to a Dataset; lists of dicts are sequences. '@tag:vr' keys state an unnamed element."""
    ds = Dataset()
    for key, value in values.items():
        if key.startswith('@'):
            # An element with these exact bytes, which pydicom would refuse to write as a typed value.
            tag, vr = key[1:].split(':')
            raw = value.encode()
            ds[Tag(int(tag, 16))] = RawDataElement(Tag(int(tag, 16)), vr, len(raw), raw, 0, False, True)
        elif key == 'PixelData':
            ds.PixelData = base64.b64decode(value)
            ds['PixelData'].VR = 'OW'
        elif isinstance(value, list) and all(isinstance(item, dict) for item in value):
            setattr(ds, key, Sequence([to_dataset(item) for item in value]))
        else:
            setattr(ds, key, value)
    return ds


def to_plain(ds):
    """A Dataset as {keyword or tag: value}, for the JSON evidence and the checks that read it."""
    plain = {}
    for element in ds:
        key = element.keyword or f'{element.tag.group:04X}{element.tag.element:04X}'
        if element.VR == 'SQ':
            plain[key] = [to_plain(item) for item in element.value]
        elif isinstance(element.value, (bytes, bytearray)):
            plain[key] = dict(bytes=len(element.value))
        elif isinstance(element.value, MultiValue):
            plain[key] = [str(item) for item in element.value]
        elif isinstance(element.value, int):
            plain[key] = element.value
        else:
            plain[key] = '' if element.value is None else str(element.value)
    return plain


def append_record(path, record):
    if path:
        with Path(path).open('a') as output:
            output.write(json.dumps(record, sort_keys=True)+'\n')
            output.flush()


def parse_peers(values):
    """AE:host:port entries to {AE: (host, port)}."""
    peers = {}
    for value in values or ():
        ae, host, port = value.split(':')
        peers[ae] = (host, int(port))
    return peers
