#!/usr/bin/env python3
"""Install the pinned DIMSE-N peers, each verified by its digest. Usage: install.py <directory>

Both archives are plain zip files, so no pip is needed and nothing outside <directory> changes.
"""
import hashlib
import os
from pathlib import Path
import stat
import sys
import urllib.request
import zipfile

# 2.1.1 is the newest release that works with the pydicom 2.4 the rest of the validation image uses.
PEERS = (
    dict(name='pynetdicom', version='2.1.1', license='MIT',
         url='https://files.pythonhosted.org/packages/be/39/626a2e5da38dc04d5296e61f083a13f7b055c82843a911a7138424aef285/'
             'pynetdicom-2.1.1-py3-none-any.whl',
         sha256='598910d65712327e478dfd59696703143e8f2531dfb409e60c3b8820d86bae4f', into='python'),
    dict(name='dcm4che', version='5.35.2', license='MPL-1.1 OR GPL-2.0 OR LGPL-2.1',
         url='https://www.dcm4che.org/maven2/org/dcm4che/dcm4che-assembly/5.35.2/dcm4che-assembly-5.35.2-bin.zip',
         sha256='e7379ef8e584a60e35f346ba72bd6a5390b67f13aff0c7026bff856ed6019b04', into='.'),
)


def install(peer, target):
    archive = target/peer['url'].rsplit('/', 1)[1]
    if not archive.exists():
        with urllib.request.urlopen(peer['url'], timeout=300) as response:
            archive.write_bytes(response.read())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != peer['sha256']:
        archive.unlink()
        raise SystemExit(f'{peer["name"]}: digest {digest} does not match the pinned {peer["sha256"]}')
    with zipfile.ZipFile(archive) as bundle:
        bundle.extractall(target/peer['into'])
    print(f'{peer["name"]} {peer["version"]} ({peer["license"]}) installed, sha256 {digest}')


def main():
    target = Path(sys.argv[1])
    target.mkdir(parents=True, exist_ok=True)
    for peer in PEERS:
        install(peer, target)
    # zipfile does not keep mode bits, and the dcm4che launchers are shell scripts.
    for script in target.glob('dcm4che-*/bin/*'):
        if script.suffix != '.bat':
            os.chmod(script, script.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


if __name__ == '__main__':
    main()
