#!/usr/bin/env python3
"""Process, network, disk and index failures of pacs_server's C-STORE path, against its storage contract (#1326).

docs/DURABILITY.md states the contract. Each case starts from an empty archive, stores fixtures with DCMTK
storescu, injects one fault and restarts the server where the fault killed it. The state is then read without
PACS code: index rows straight from the SQLite file, objects and pixel digests from the files with pydicom, and
leftover temporary files from the directory. Every case checks that each object storescu saw acknowledged is
indexed and filed with its declared pixels, that no index row lacks its file, and that after the case sends
everything again each fixture is held exactly once. Faults come from faultshim.c (LD_PRELOAD): a SIGKILL just
before or just after the n-th rename into the archive, and ENOSPC for archive files while a flag file exists.
Process death here is SIGKILL: page cache survives it, so nothing below says anything about power loss.
"""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import sqlite3
import subprocess
import sys
import threading
import time

import pydicom
from pydicom.dataset import Dataset, FileMetaDataset

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
from run import Validation, require

PACS_AE, SCU_AE = 'PACS_FAULT', 'FAULT_SCU'
EXPLICIT = '1.2.840.10008.1.2.1'
SECONDARY_CAPTURE = '1.2.840.10008.5.1.4.1.1.7'


def free_port():
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


class Faults(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.work = self.output/'work'
        self.work.mkdir()
        self.port = free_port()
        self.shim = self.output/'faultshim.so'
        self.sets = {}
        self.timings = {}

    # Fixtures ------------------------------------------------------------------------------

    def write_set(self, name, count, size):
        study = f'{self.run_uid}.{len(self.sets)+1}'
        folder = self.work/'fixtures'/name
        folder.mkdir(parents=True)
        manifest, files = {}, []
        for number in range(1, count+1):
            sop = f'{study}.1.{number}'
            ds = Dataset()
            ds.file_meta = FileMetaDataset()
            ds.file_meta.MediaStorageSOPClassUID, ds.file_meta.MediaStorageSOPInstanceUID = SECONDARY_CAPTURE, sop
            ds.file_meta.TransferSyntaxUID = EXPLICIT
            ds.SOPClassUID, ds.SOPInstanceUID = SECONDARY_CAPTURE, sop
            ds.StudyInstanceUID, ds.SeriesInstanceUID = study, f'{study}.1'
            ds.PatientName, ds.PatientID, ds.PatientBirthDate, ds.PatientSex = f'FAULT^{name.upper()}', f'FAULT-{name}', '19800101', 'O'
            ds.StudyDate, ds.StudyTime, ds.StudyID, ds.AccessionNumber, ds.ReferringPhysicianName = '20260501', '101112', name[:16], name[:16], ''
            ds.Modality, ds.SeriesNumber, ds.InstanceNumber, ds.ConversionType = 'OT', '1', str(number), 'WSD'
            ds.Rows, ds.Columns, ds.SamplesPerPixel, ds.PhotometricInterpretation = size, size, 1, 'MONOCHROME2'
            ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 8, 8, 7, 0
            seed = hashlib.sha256(sop.encode()).digest()
            ds.PixelData = (seed*(size*size//len(seed)+1))[:size*size]
            ds.is_little_endian, ds.is_implicit_VR = True, False
            path = folder/f'{number:03d}.dcm'
            ds.save_as(path, write_like_original=False)
            manifest[sop] = hashlib.sha256(ds.PixelData).hexdigest()
            files.append(path)
        self.sets[name] = dict(study=study, manifest=manifest, files=files)

    def fixtures(self):
        for name in ('before-rename', 'after-rename', 'disk-full', 'index-locked'):
            self.write_set(name, 12, 8)
        for name in ('server-killed', 'client-killed'):
            self.write_set(name, 40, 512)
        (self.output/'manifests.json').write_text(json.dumps({name: dict(study=item['study'], manifest=item['manifest'])
                                                             for name, item in self.sets.items()}, indent=1)+'\n')
        return f'{sum(len(item["manifest"]) for item in self.sets.values())} objects in {len(self.sets)} studies'

    def build_shim(self):
        self.command(['gcc', '-shared', '-fPIC', '-O2', '-o', self.shim, Path(__file__).with_name('faultshim.c'), '-ldl'])
        return f'{self.shim.name} built'

    # Server and client ---------------------------------------------------------------------

    def directory(self, name):
        return self.work/'archives'/name

    @contextmanager
    def pacs(self, name, faults=None, run=1):
        """pacs_server on the case's archive; faults go to the shim. Yields the process; records its exit."""
        base = self.directory(name)
        (base/'archive').mkdir(parents=True, exist_ok=True)
        env = dict(os.environ)
        if faults is not None:
            env.update(LD_PRELOAD=str(self.shim), PACS_FAULT_ROOT=str((base/'archive').resolve()), **faults)
            # An ASan build refuses to start when another library is preloaded ahead of its runtime.
            env['ASAN_OPTIONS'] = ':'.join(filter(None, [env.get('ASAN_OPTIONS'), 'verify_asan_link_order=0']))
        args = [str(self.bin/'pacs_server'), '--port', str(self.port), '--ae-title', PACS_AE,
                '--storage-dir', str((base/'archive').resolve()), '--db-path', str((base/'index.db').resolve())]
        started = time.monotonic()
        with (self.output/f'server-{name}-{run}.log').open('w') as log:
            process = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, env=env, start_new_session=True)
            try:
                deadline = time.monotonic()+20
                while True:
                    require(process.poll() is None, f'pacs_server exited {process.returncode} while starting')
                    try:
                        with socket.create_connection(('127.0.0.1', self.port), .1):
                            break
                    except OSError:
                        require(time.monotonic() < deadline, 'pacs_server did not open its port')
                        time.sleep(.05)
                self.timings[f'{name}-{run}-ready-seconds'] = round(time.monotonic()-started, 3)
                yield process
            finally:
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait()
                self.wait_closed()

    def wait_closed(self):
        deadline = time.monotonic()+60
        while time.monotonic() < deadline:
            try:
                with socket.create_connection(('127.0.0.1', self.port), .1):
                    pass
            except OSError:
                return
            time.sleep(.1)
        raise ValueError(f'port {self.port} still open 60 s after the server ended')

    def storescu(self, files, log_name):
        with (self.output/f'{log_name}.log').open('w') as log:
            # --no-halt: by default storescu stops at the first refused object and never offers the rest.
            return subprocess.Popen(['storescu', '-v', '--no-halt', '-aet', SCU_AE, '-aec', PACS_AE, '127.0.0.1', str(self.port),
                                     *[str(path) for path in files]], stdout=log, stderr=subprocess.STDOUT)

    def outcomes(self, name, log_name):
        """file -> the status storescu printed for it; a file without one was never acknowledged."""
        by_path = {str(path): sop for path, sop in zip(self.sets[name]['files'], self.sets[name]['manifest'])}
        result, current = {}, None
        for line in (self.output/f'{log_name}.log').read_text(errors='replace').splitlines():
            if 'Sending file:' in line:
                current = by_path.get(line.split('Sending file:', 1)[1].strip())
            elif 'Received Store Response' in line and current:
                result[current] = line.split('Received Store Response', 1)[1].strip(' ()')
                current = None
        return result

    def send(self, name, files, log_name, timeout=300):
        process = self.storescu(files, log_name)
        try:
            process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        return process.returncode, self.outcomes(name, log_name)

    # State, read without PACS code ---------------------------------------------------------

    def state(self, name):
        base = self.directory(name)
        indexed = set()
        if (base/'index.db').exists():
            with sqlite3.connect(f'file:{base/"index.db"}?mode=ro', uri=True, timeout=30) as db:
                indexed = {row[0] for row in db.execute('SELECT sop_uid FROM instances')}
        filed, temporary = {}, []
        for path in (base/'archive').rglob('*'):
            if not path.is_file():
                continue
            if '.tmp.' in path.name:
                temporary.append(str(path.relative_to(base)))
            elif path.suffix == '.dcm':
                ds = pydicom.dcmread(path)
                require(ds.SOPInstanceUID not in filed, f'{ds.SOPInstanceUID} is filed twice')
                filed[ds.SOPInstanceUID] = hashlib.sha256(ds.PixelData).hexdigest()
        return dict(indexed=indexed, filed=filed, temporary=sorted(temporary))

    def consistent(self, name, state, acknowledged):
        manifest = self.sets[name]['manifest']
        require(acknowledged <= state['indexed'], f'acknowledged but not indexed: {sorted(acknowledged-state["indexed"])}')
        require(state['indexed'] <= set(state['filed']), f'index rows without a file: {sorted(state["indexed"]-set(state["filed"]))}')
        wrong = [sop for sop, digest in state['filed'].items() if manifest.get(sop) != digest]
        require(not wrong, f'filed with other pixels or not a fixture: {wrong}')

    def resend_all(self, name, run):
        """Send every fixture again on a clean server: each must end held exactly once."""
        manifest = self.sets[name]['manifest']
        with self.pacs(name, run=run):
            code, outcomes = self.send(name, self.sets[name]['files'], f'{name}-resend')
        state = self.state(name)
        require(state['indexed'] == set(manifest), f'after sending again, indexed {len(state["indexed"])} of {len(manifest)}; '
                                                   f'missing {sorted(set(manifest)-state["indexed"])[:3]}')
        require(state['filed'] == manifest, 'after sending again the files are not the fixtures, once each')
        require(not state['temporary'], f'temporary files remain: {state["temporary"]}')
        return code, outcomes

    # Cases ---------------------------------------------------------------------------------

    def killed_at_rename(self, name, variable):
        manifest = self.sets[name]['manifest']
        fifth = list(manifest)[4]
        with self.pacs(name, {variable: '5'}) as process:
            self.send(name, self.sets[name]['files'], f'{name}-faulted')
            process.wait(timeout=30)
            # 137 is the injected SIGKILL as a wrapper that starts the server reports it (the TSan retry wrapper).
            require(process.returncode in (-signal.SIGKILL, 128+signal.SIGKILL),
                    f'pacs_server ended {process.returncode}, not by the injected SIGKILL')
        acknowledged = {sop for sop, status in self.outcomes(name, f'{name}-faulted').items() if status.startswith('Success')}
        dead = self.state(name)
        require(acknowledged == set(list(manifest)[:4]), f'acknowledged {len(acknowledged)} objects before the kill, expected the first 4')
        self.consistent(name, dead, acknowledged)
        with self.pacs(name, run=2):
            restarted = self.state(name)
        self.consistent(name, restarted, acknowledged)
        code, outcomes = self.resend_all(name, run=3)
        refused = sorted(sop for sop, status in outcomes.items() if not status.startswith('Success'))
        require(set(refused) == acknowledged, f'refused when sent again: {len(refused)}, expected the {len(acknowledged)} acknowledged')
        return dict(acknowledged=len(acknowledged), fifth=fifth, dead=dict(indexed=len(dead['indexed']), filed=len(dead['filed']),
                    temporary=dead['temporary']), after_restart=dict(indexed=len(restarted['indexed']), filed=len(restarted['filed']),
                    temporary=restarted['temporary']), fifth_when_sent_again=outcomes.get(fifth))

    def before_rename_case(self):
        found = self.killed_at_rename('before-rename', 'PACS_FAULT_KILL_BEFORE_RENAME')
        require(len(found['dead']['temporary']) == 1, f'temporary files after the kill: {found["dead"]["temporary"]}')
        require(not found['after_restart']['temporary'], f'a restart left temporary files: {found["after_restart"]["temporary"]}')
        (self.output/'before-rename.json').write_text(json.dumps(found, indent=1)+'\n')
        return (f'killed with object 5 written to a temporary file: 4 acknowledged objects intact, the temporary file '
                f'removed at the restart, all 12 held once after sending again')

    def after_rename_case(self):
        found = self.killed_at_rename('after-rename', 'PACS_FAULT_KILL_AFTER_RENAME')
        require(found['dead']['filed'] == 5 and found['dead']['indexed'] == 4, f'state after the kill: {found["dead"]}')
        require(found['fifth_when_sent_again'] and found['fifth_when_sent_again'].startswith('Success'),
                f'object 5, filed but never indexed, was answered {found["fifth_when_sent_again"]!r} when sent again')
        (self.output/'after-rename.json').write_text(json.dumps(found, indent=1)+'\n')
        return 'killed after object 5 was filed and before it was indexed: sending it again stored and indexed it; all 12 held once'

    def killed_during(self, name, kill_server):
        """Kill the server or the client in the middle of a batch; returns what storescu saw acknowledged."""
        manifest = self.sets[name]['manifest']
        archive = self.directory(name)/'archive'
        with self.pacs(name) as process:
            client = self.storescu(self.sets[name]['files'], f'{name}-faulted')
            # The client's log may be buffered until it exits, so the server's archive says when 5 are in.
            deadline = time.monotonic()+60
            while sum(1 for _ in archive.rglob('*.dcm')) < 5 and client.poll() is None and time.monotonic() < deadline:
                time.sleep(.01)
            require(client.poll() is None, 'the batch ended before the kill: nothing was interrupted')
            if kill_server:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
                client.wait(timeout=60)
            else:
                client.kill()
                client.wait()
                time.sleep(1)
                require(process.poll() is None, f'pacs_server ended {process.returncode} when its client vanished')
                self.command(['echoscu', '-aet', SCU_AE, '-aec', PACS_AE, '127.0.0.1', self.port])
        acknowledged = {sop for sop, status in self.outcomes(name, f'{name}-faulted').items() if status.startswith('Success')}
        state = self.state(name)
        self.consistent(name, state, acknowledged)
        require(0 < len(acknowledged) < len(manifest), f'{len(acknowledged)} of {len(manifest)} acknowledged: nothing was interrupted')
        orphans = sorted(set(state['filed'])-state['indexed'])
        self.resend_all(name, run=2)
        return acknowledged, state, orphans

    def server_killed_case(self):
        acknowledged, state, orphans = self.killed_during('server-killed', kill_server=True)
        return (f'SIGKILL after {len(acknowledged)} acknowledged objects: all indexed and filed with their pixels; '
                f'{len(state["indexed"])} indexed, {len(orphans)} filed but not indexed, {len(state["temporary"])} temporary; '
                f'sending again held each of 40 once')

    def client_killed_case(self):
        acknowledged, state, orphans = self.killed_during('client-killed', kill_server=False)
        require(not orphans and not state['temporary'], f'the server left {orphans} unindexed and {state["temporary"]} temporary')
        return (f'the client was killed after {len(acknowledged)} acknowledged objects: the server kept serving C-ECHO, held exactly '
                f'what was acknowledged or completed, left no partial file; sending again held each of 40 once')

    def disk_full_case(self):
        name = 'disk-full'
        flag = self.work/'disk-full.flag'
        flag.write_text('full\n')
        files = self.sets[name]['files'][:3]
        with self.pacs(name, {'PACS_FAULT_FULL_FLAG': str(flag)}) as process:
            code, refused = self.send(name, files, f'{name}-full')
            require(process.poll() is None, 'pacs_server ended on a full disk')
            full = self.state(name)
            flag.unlink()
            code, stored = self.send(name, files, f'{name}-space')
        require(len(refused) == 3 and not any(status.startswith('Success') for status in refused.values()),
                f'with the disk full storescu saw {refused}')
        require(not full['indexed'] and not full['filed'] and not full['temporary'], f'a full disk left {full}')
        require(len(stored) == 3 and all(status.startswith('Success') for status in stored.values()), f'with space again: {stored}')
        self.consistent(name, self.state(name), set(stored))
        return f'3 objects refused ({sorted(set(refused.values()))}) with nothing left behind; stored once space was back'

    def index_locked_case(self):
        name = 'index-locked'
        files = self.sets[name]['files']
        with self.pacs(name) as process:
            code, first = self.send(name, files[:1], f'{name}-first')
            require(list(first.values()) == ['Success'], f'first object: {first}')
            locked, release = threading.Event(), threading.Event()

            def hold():
                with sqlite3.connect(str(self.directory(name)/'index.db'), timeout=30, isolation_level=None) as db:
                    db.execute('BEGIN EXCLUSIVE')
                    locked.set()
                    release.wait(120)
                    db.execute('ROLLBACK')
            holder = threading.Thread(target=hold)
            holder.start()
            require(locked.wait(30), 'could not lock the index')
            try:
                started = time.monotonic()
                code, refused = self.send(name, files[1:3], f'{name}-locked')
                waited = round(time.monotonic()-started, 1)
                busy = self.state_while_locked(name)
            finally:
                release.set()
                holder.join()
            require(process.poll() is None, 'pacs_server ended while its index was locked')
            code, stored = self.send(name, files[1:3], f'{name}-unlocked')
        require(len(refused) == 2 and not any(status.startswith('Success') for status in refused.values()), f'locked: {refused}')
        require(set(busy['filed']) == set(first) and not busy['temporary'], f'the locked index left files: {busy}')
        require(all(status.startswith('Success') for status in stored.values()) and len(stored) == 2, f'unlocked: {stored}')
        self.consistent(name, self.state(name), set(first) | set(stored))
        return f'2 objects refused after {waited} s with the index locked, their files removed; stored once it was free'

    def state_while_locked(self, name):
        """Files only: the index cannot be read under its exclusive lock."""
        base = self.directory(name)
        filed = {pydicom.dcmread(path).SOPInstanceUID: None for path in (base/'archive').rglob('*.dcm')}
        temporary = [str(path) for path in (base/'archive').rglob('*') if '.tmp.' in path.name]
        return dict(filed=filed, temporary=temporary)

    def run_all(self):
        self.case('reliability.fixtures', self.fixtures, phase='fixture_preparation')
        self.case('reliability.shim', self.build_shim, phase='prerequisite')
        ready = ['reliability.fixtures', 'reliability.shim']
        self.case('reliability.kill-before-rename', self.before_rename_case, requires=ready)
        self.case('reliability.kill-after-rename', self.after_rename_case, requires=ready)
        self.case('reliability.server-killed-mid-batch', self.server_killed_case, requires=['reliability.fixtures'])
        self.case('reliability.client-killed-mid-batch', self.client_killed_case, requires=['reliability.fixtures'])
        self.case('reliability.disk-full', self.disk_full_case, requires=ready)
        self.case('reliability.index-locked', self.index_locked_case, requires=['reliability.fixtures'])
        counts = {status: sum(1 for row in self.results if row['status'] == status) for status in ('passed', 'failed', 'blocked')}
        (self.output/'reliability-summary.json').write_text(json.dumps(dict(
            counts=counts, timings=self.timings,
            failures=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'failed'],
            blocked=[row['id'] for row in self.results if row['status'] == 'blocked']), indent=1)+'\n')
        shutil.rmtree(self.work/'fixtures', ignore_errors=True)
        print(json.dumps(counts))
        return int(counts['failed'] > 0 or counts['blocked'] > 0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--source-revision', required=True)
    parser.add_argument('--source-digest', required=True)
    args = parser.parse_args()
    args.profile = 'uncompressed'
    return Faults(args).run_all()


if __name__ == '__main__':
    raise SystemExit(main())
