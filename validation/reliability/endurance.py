#!/usr/bin/env python3
"""Steady C-STORE and C-FIND load on pacs_server against the limits endurance.json declares (#1326).

Load, duration, warm-up and limits are read from the configuration, which is versioned with the code, before
anything runs. Every operation is timed and kept; the server's resident memory and open descriptors are
sampled from /proc. Limits are judged on what follows the warm-up. A run shorter than the configured
duration reports status "diagnostic": its numbers are evidence of a trend, not a pass. Exit 0 only for a full
run within every limit, 1 for a violation, 3 for a diagnostic run.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import time
import uuid

from pydicom.dataset import Dataset, FileMetaDataset

HERE = Path(__file__).resolve().parent
PACS_AE, SCU_AE = 'PACS_SOAK', 'SOAK_SCU'
SECONDARY_CAPTURE = '1.2.840.10008.5.1.4.1.1.7'


def free_port():
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    return ordered[min(len(ordered)-1, int(round(fraction*(len(ordered)-1))))]


def batch(folder, number, rows):
    """objects_per_batch new objects of one study, written for one storescu call."""
    study = f'2.25.{uuid.uuid4().int}'
    folder.mkdir(parents=True, exist_ok=True)
    files = []
    for index in range(number):
        sop = f'{study}.1.{index+1}'
        ds = Dataset()
        ds.file_meta = FileMetaDataset()
        ds.file_meta.MediaStorageSOPClassUID, ds.file_meta.MediaStorageSOPInstanceUID = SECONDARY_CAPTURE, sop
        ds.file_meta.TransferSyntaxUID = '1.2.840.10008.1.2.1'
        ds.SOPClassUID, ds.SOPInstanceUID, ds.StudyInstanceUID, ds.SeriesInstanceUID = SECONDARY_CAPTURE, sop, study, f'{study}.1'
        ds.PatientName, ds.PatientID, ds.StudyDate, ds.Modality = 'SOAK^LOAD', f'SOAK-{study[-8:]}', '20260501', 'OT'
        ds.Rows = ds.Columns = rows
        ds.SamplesPerPixel, ds.PhotometricInterpretation, ds.BitsAllocated, ds.BitsStored, ds.HighBit = 1, 'MONOCHROME2', 8, 8, 7
        ds.PixelRepresentation, ds.PixelData = 0, os.urandom(rows*rows)
        ds.is_little_endian, ds.is_implicit_VR = True, False
        path = folder/f'{index}.dcm'
        ds.save_as(path, write_like_original=False)
        files.append(path)
    return study, files


class Soak:
    def __init__(self, args):
        self.args = args
        self.config = json.loads(Path(args.config).read_text())
        self.duration = args.duration or self.config['duration_seconds']
        self.warmup = args.warmup if args.warmup is not None else self.config['warmup_seconds']
        self.output = Path(args.output)
        self.output.mkdir(parents=True, exist_ok=False)
        self.port = free_port()
        self.operations, self.samples = [], []

    def timed(self, kind, args):
        started = time.monotonic()
        result = subprocess.run([str(a) for a in args], capture_output=True, timeout=120)
        seconds = time.monotonic()-started
        self.operations.append(dict(kind=kind, at=round(started-self.started, 3), seconds=round(seconds, 4),
                                    exit=result.returncode))
        if result.returncode != 0:
            with (self.output/'failed-operations.log').open('ab') as log:
                log.write(f'--- {kind} at {started-self.started:.1f} s exit {result.returncode}\n'.encode())
                log.write(result.stdout[-4000:]+result.stderr[-4000:])
        return result.returncode

    def sample(self, pid):
        status = Path(f'/proc/{pid}/status').read_text()
        rss_kb = int(next(line.split()[1] for line in status.splitlines() if line.startswith('VmRSS:')))
        descriptors = len(os.listdir(f'/proc/{pid}/fd'))
        self.samples.append(dict(at=round(time.monotonic()-self.started, 1), rss_mb=round(rss_kb/1024, 2), fds=descriptors))

    def run(self):
        load, limits = self.config['load'], self.config['limits']
        work = self.output/'work'
        args = [Path(self.args.build_dir)/'bin'/'pacs_server', '--port', self.port, '--ae-title', PACS_AE,
                '--storage-dir', work/'archive', '--db-path', work/'index.db']
        work.mkdir()
        with (self.output/'server.log').open('w') as log:
            server = subprocess.Popen([str(a) for a in args], stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            for _ in range(200):
                try:
                    with socket.create_connection(('127.0.0.1', self.port), .1):
                        break
                except OSError:
                    time.sleep(.05)
            self.started = time.monotonic()
            store_every = 60/load['store_batches_per_minute']
            find_every = 60/load['finds_per_minute']
            next_store, next_find, next_sample = 0.0, find_every/2, 0.0
            studies = []
            while (now := time.monotonic()-self.started) < self.duration:
                if server.poll() is not None:
                    raise SystemExit(f'pacs_server ended {server.returncode} at {now:.0f} s')
                if now >= next_sample:
                    self.sample(server.pid)
                    next_sample += self.config['sample_seconds']
                if now >= next_store:
                    study, files = batch(work/'batch', load['objects_per_batch'], load['object_rows'])
                    self.timed('store', ['storescu', '-aet', SCU_AE, '-aec', PACS_AE, '127.0.0.1', self.port, *files])
                    shutil.rmtree(work/'batch')
                    studies.append(study)
                    next_store += store_every
                if now >= next_find and studies:
                    self.timed('find', ['findscu', '-S', '-aet', SCU_AE, '-aec', PACS_AE, '127.0.0.1', self.port,
                                        '-k', '0008,0052=STUDY', '-k', f'0020,000D={studies[len(studies)//2]}', '-k', '0010,0020'])
                    next_find += find_every
                time.sleep(.05)
            self.sample(server.pid)
        finally:
            if server.poll() is None:
                os.killpg(server.pid, signal.SIGTERM)
                server.wait(timeout=30)
        return self.judge(limits, server.returncode)

    def judge(self, limits, exit_code):
        after = [row for row in self.samples if row['at'] >= self.warmup] or self.samples[-1:]
        base = after[0]
        stores = [row['seconds'] for row in self.operations if row['kind'] == 'store' and row['at'] >= self.warmup]
        finds = [row['seconds'] for row in self.operations if row['kind'] == 'find' and row['at'] >= self.warmup]
        measured = dict(
            rss_growth_mb_after_warmup=round(max(row['rss_mb'] for row in after)-base['rss_mb'], 2),
            fd_growth_after_warmup=max(row['fds'] for row in after)-base['fds'],
            store_batch_p95_seconds=percentile(stores, .95), find_p95_seconds=percentile(finds, .95),
            failed_operations=sum(1 for row in self.operations if row['exit'] != 0))
        violations = [f'{key} {measured[key]} > {limit}' for key, limit in limits.items()
                      if measured[key] is not None and measured[key] > limit]
        if exit_code != 0:
            violations.append(f'pacs_server exited {exit_code} on SIGTERM')
        full = self.duration >= self.config['duration_seconds']
        status = ('failed' if violations else 'passed') if full else 'diagnostic'
        summary = dict(status=status, configured_seconds=self.config['duration_seconds'], ran_seconds=self.duration,
                       warmup_seconds=self.warmup, load=self.config['load'], limits=limits, measured=measured,
                       violations=violations, operations=len(self.operations), samples=len(self.samples),
                       server_exit=exit_code)
        (self.output/'endurance-summary.json').write_text(json.dumps(summary, indent=1)+'\n')
        for name, rows in (('operations.csv', self.operations), ('samples.csv', self.samples)):
            with (self.output/name).open('w', newline='') as handle:
                writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)
        shutil.rmtree(self.output/'work'/'archive', ignore_errors=True)
        print(json.dumps(dict(status=status, measured=measured, violations=violations)))
        return 0 if status == 'passed' else 1 if violations else 3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--build-dir', default='/workspace/build')
    parser.add_argument('--config', default=str(HERE/'endurance.json'))
    parser.add_argument('--duration', type=int, help='seconds; less than the configured duration makes the run diagnostic')
    parser.add_argument('--warmup', type=int, help='seconds of warm-up for a diagnostic run')
    return Soak(parser.parse_args()).run()


if __name__ == '__main__':
    raise SystemExit(main())
