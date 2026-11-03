#!/usr/bin/env python3
"""Jobs, routing, sync and prefetch between pacs_server and two independently running Orthanc servers (#1323).

pacs_server runs with its REST API on. Orthanc A holds studies pacs_server retrieves, syncs and prefetches;
Orthanc B receives what pacs_server sends and forwards. Every case compares exact manifests, SOP Instance UID
to SHA-256 of the pixel data, read from the Orthanc servers through their own REST API and from pacs_server
over WADO-URI, with what the fixtures declared before any job ran. Job states come from the REST API; the
audit trail and the DIMSE counters of GET /api/v1/system/metrics are checked against the same events, also
across a graceful stop and a forced kill in the middle of a job.
"""
import argparse
from contextlib import ExitStack, contextmanager
import hashlib
import io
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

import pydicom
from pydicom.dataset import Dataset, FileMetaDataset

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
sys.path.insert(0, str(ROOT/'validation/peers/http'))
from http_peer import TokenKit
from run import Validation, require

ISSUER, AUDIENCE = 'https://issuer.workflow.test', 'pacs-workflow'
PACS_AE, SCU_AE, A_AE, B_AE = 'PACS_WF', 'WF_SCU', 'ORTHANC_A', 'ORTHANC_B'
EXPLICIT = '1.2.840.10008.1.2.1'
SECONDARY_CAPTURE = '1.2.840.10008.5.1.4.1.1.7'
JOB_TIMEOUT = 180
TERMINAL = ('completed', 'failed', 'cancelled')
LARGE = 512  # rows and columns of the objects whose transfer a case interrupts


def free_port():
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


def pixel_digest(data):
    return hashlib.sha256(pydicom.dcmread(io.BytesIO(data)).PixelData).hexdigest()


class Workflow(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.work = self.output/'work'
        self.work.mkdir()
        self.ports = {name: free_port() for name in ('pacs_dimse', 'pacs_http', 'a_dimse', 'a_http', 'b_dimse', 'b_http')}
        self.sets, self.jobs, self.nodes = {}, {}, {}
        self.stops, self.kills = [], []
        self.b_stack, self.b_starts, self.pacs_runs = None, 0, 0
        self.token, self.pacs_process, self.interrupted, self.restarted_at = None, None, None, None

    # Fixtures -------------------------------------------------------------------------------

    def write_set(self, name, patient_id, modality, series_count, per_series, size=8):
        """A study of Secondary Capture objects; returns its manifest and files."""
        study = f'{self.run_uid}.{len(self.sets)+1}'
        manifest, files = {}, []
        folder = self.work/'fixtures'/name
        folder.mkdir(parents=True)
        for series_number in range(1, series_count+1):
            series = f'{study}.{series_number}'
            for number in range(1, per_series+1):
                sop = f'{series}.{number}'
                ds = Dataset()
                ds.file_meta = FileMetaDataset()
                ds.file_meta.MediaStorageSOPClassUID, ds.file_meta.MediaStorageSOPInstanceUID = SECONDARY_CAPTURE, sop
                ds.file_meta.TransferSyntaxUID = EXPLICIT
                ds.SOPClassUID, ds.SOPInstanceUID = SECONDARY_CAPTURE, sop
                ds.StudyInstanceUID, ds.SeriesInstanceUID = study, series
                ds.PatientName, ds.PatientID, ds.PatientBirthDate, ds.PatientSex = f'WORKFLOW^{name.upper()}', patient_id, '19800101', 'O'
                ds.StudyDate, ds.StudyTime, ds.StudyID, ds.AccessionNumber = '20260501', '101112', name[:16], f'ACC-{name}'[:16]
                ds.ReferringPhysicianName, ds.StudyDescription = 'REF^DOC', f'Workflow {name}'
                ds.Modality, ds.SeriesNumber, ds.InstanceNumber, ds.ConversionType = modality, str(series_number), str(number), 'WSD'
                ds.Rows, ds.Columns, ds.SamplesPerPixel, ds.PhotometricInterpretation = size, size, 1, 'MONOCHROME2'
                ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 8, 8, 7, 0
                seed = hashlib.sha256(sop.encode()).digest()
                ds.PixelData = (seed*(size*size//len(seed)+1))[:size*size]
                path = folder/f'{series_number}-{number}.dcm'
                ds.is_little_endian, ds.is_implicit_VR = True, False
                ds.save_as(path, write_like_original=False)
                manifest[sop] = hashlib.sha256(ds.PixelData).hexdigest()
                files.append(path)
        self.sets[name] = dict(study=study, manifest=manifest, files=files, patient_id=patient_id)
        return self.sets[name]

    def fixtures(self):
        self.write_set('remote', 'WF-REMOTE', 'OT', 2, 3)              # at A: retrieved
        self.write_set('sync', 'WF-SYNC', 'OT', 1, 3)                  # at A: synchronized
        self.write_set('prior-held', 'WF-PRIOR', 'OT', 1, 2)           # at A and at pacs_server
        self.write_set('prior-missing', 'WF-PRIOR', 'OT', 1, 2)        # at A only: prefetched
        self.write_set('local', 'WF-LOCAL', 'OT', 1, 4)                # at pacs_server: stored to B
        self.write_set('partial', 'WF-PARTIAL', 'OT', 1, 2)            # at pacs_server: sent with a missing one
        self.write_set('retry', 'WF-RETRY', 'OT', 1, 3)                # at pacs_server: B down, then retried
        self.write_set('cancel', 'WF-CANCEL', 'OT', 4, 30, size=LARGE)
        self.write_set('pause', 'WF-PAUSE', 'OT', 2, 20, size=LARGE)
        self.write_set('graceful', 'WF-GRACEFUL', 'OT', 2, 20, size=LARGE)
        self.write_set('forced', 'WF-FORCED', 'OT', 2, 20, size=LARGE)
        self.write_set('routed', 'WF-ROUTED', 'CT', 1, 3)              # sent to pacs_server later: forwarded
        self.write_set('kept', 'WF-KEPT', 'MR', 1, 2)                  # sent to pacs_server later: not forwarded
        (self.output/'manifests.json').write_text(json.dumps({name: dict(study=item['study'], patient_id=item['patient_id'],
                                                                         manifest=item['manifest'])
                                                             for name, item in self.sets.items()}, indent=1)+'\n')
        return f'{sum(len(item["manifest"]) for item in self.sets.values())} objects in {len(self.sets)} studies, declared in manifests.json'

    def tokens(self):
        kit = TokenKit(self.private/'keys')
        self.jwks = self.private/'jwks.json'
        self.jwks.write_text(json.dumps(kit.jwks()))
        now = int(time.time())
        self.token = kit.token(dict(iss=ISSUER, aud=AUDIENCE, sub='workflow', iat=now, nbf=now-5, exp=now+6*3600,
                                    scope='pacs.admin dicomweb.read dicomweb.search'))
        return 'one administrator token from a key made for this run'

    def versions(self):
        rows = {}
        for name, args in (('orthanc', ['dpkg-query', '-W', 'orthanc']), ('dcmtk', ['dpkg-query', '-W', 'dcmtk']),
                           ('python', ['python3', '--version'])):
            rows[name] = self.command(args).strip()
        rows['pydicom'] = pydicom.__version__
        (self.output/'peer-versions.json').write_text(json.dumps(rows, indent=1)+'\n')
        return json.dumps(rows)

    # Servers --------------------------------------------------------------------------------

    @contextmanager
    def orthanc(self, name, ae, dimse, http, modalities, log_name):
        directory = self.work/f'orthanc-{name}'
        directory.mkdir(exist_ok=True)
        config = directory/'orthanc.json'
        config.write_text(json.dumps(dict(
            Name=f'WORKFLOW_{name.upper()}', StorageDirectory=str(directory/'storage'), IndexDirectory=str(directory/'index'),
            HttpPort=http, DicomPort=dimse, DicomAet=ae, DicomCheckCalledAet=False, RemoteAccessAllowed=False,
            AuthenticationEnabled=False, DicomAlwaysAllowStore=True, DicomAlwaysAllowFind=True, DicomAlwaysAllowGet=True,
            DicomAlwaysAllowMove=True, DicomModalities=modalities, UnknownSopClassAccepted=True, StableAge=1,
            OverwriteInstances=False, LimitFindResults=0, LimitFindInstances=0)))
        with self.server(['Orthanc', config, f'--logdir={directory}'], http, log_name=log_name) as process:
            yield process

    def start_b(self):
        self.b_starts += 1
        self.b_stack = ExitStack()
        self.b_stack.enter_context(self.orthanc('b', B_AE, self.ports['b_dimse'], self.ports['b_http'], {},
                                                f'server-orthanc-b-{self.b_starts}'))

    def stop_b(self):
        stack, self.b_stack = self.b_stack, None
        if stack:
            stack.close()

    @contextmanager
    def pacs(self, fresh=False):
        self.pacs_runs += 1
        run = self.pacs_runs
        directory = self.work/'pacs'
        if fresh and directory.exists():
            shutil.rmtree(directory)
        directory.mkdir(exist_ok=True)
        args = [self.bin/'pacs_server', '--port', self.ports['pacs_dimse'], '--ae-title', PACS_AE,
                '--storage-dir', directory/'archive', '--db-path', directory/'index.db',
                '--http-port', self.ports['pacs_http'], '--http-rest-api',
                '--oauth2-issuer', ISSUER, '--oauth2-audience', AUDIENCE, '--oauth2-jwks', self.jwks]
        with self.server(args, self.ports['pacs_dimse'], log_name=f'server-pacs-{run}') as process:
            self.wait_port(self.ports['pacs_http'])
            self.pacs_process = process
            yield process
        if run in self.kills:
            self.stops.append((f'server-pacs-{run} (killed on purpose)', process.returncode))
        else:
            self.stops.append((f'server-pacs-{run}', process.returncode))

    @staticmethod
    def wait_port(port):
        for _ in range(100):
            try:
                with socket.create_connection(('127.0.0.1', port), .1):
                    return
            except OSError:
                time.sleep(.1)
        raise ValueError(f'nothing listens on port {port}')

    @staticmethod
    def wait_closed(port, timeout=120):
        deadline = time.monotonic()+timeout
        while time.monotonic() < deadline:
            try:
                with socket.create_connection(('127.0.0.1', port), .1):
                    pass
            except OSError:
                return
            time.sleep(.1)
        raise ValueError(f'port {port} still accepts connections {timeout} s after the kill')

    # Clients --------------------------------------------------------------------------------

    def http(self, method, url, body=None, headers=None, timeout=60):
        request = urllib.request.Request(url, method=method, data=body)
        for key, value in (headers or {}).items():
            request.add_header(key, value)
        try:
            with urllib.request.urlopen(request, timeout=timeout) as reply:
                return reply.status, reply.read()
        except urllib.error.HTTPError as error:
            return error.code, error.read()

    def api(self, method, path, document=None):
        body = json.dumps(document, indent=2).encode() if document is not None else None
        headers = {'Authorization': f'Bearer {self.token}'}
        if body is not None:
            headers['Content-Type'] = 'application/json'
        status, data = self.http(method, f'http://127.0.0.1:{self.ports["pacs_http"]}/api/v1{path}', body, headers)
        try:
            parsed = json.loads(data) if data else None
        except ValueError:
            parsed = data.decode(errors='replace')
        with (self.output/'api.jsonl').open('a') as log:
            log.write(json.dumps(dict(case=self.current, method=method, path=path, request=document, status=status,
                                      reply=parsed if len(str(parsed)) < 4000 else str(parsed)[:4000]))+'\n')
        return status, parsed

    def orthanc_api(self, port, method, path, body=None, content_type=None):
        status, data = self.http(method, f'http://127.0.0.1:{port}{path}', body,
                                 {'Content-Type': content_type} if content_type else None)
        require(200 <= status < 300, f'Orthanc {method} {path} answered {status}: {data[:200]}')
        return data

    def orthanc_load(self, port, files):
        for path in files:
            self.orthanc_api(port, 'POST', '/instances', path.read_bytes(), 'application/dicom')

    def orthanc_count(self, port):
        return json.loads(self.orthanc_api(port, 'GET', '/statistics'))['CountInstances']

    def orthanc_manifest(self, port, name):
        """The objects of one fixture set an Orthanc server holds, with the SHA-256 of their pixel data."""
        found = {}
        for sop in self.sets[name]['manifest']:
            hits = [hit for hit in json.loads(self.orthanc_api(port, 'POST', '/tools/lookup', sop.encode()))
                    if hit['Type'] == 'Instance']
            require(len(hits) <= 1, f'Orthanc on {port} holds {sop} {len(hits)} times')
            if hits:
                found[sop] = pixel_digest(self.orthanc_api(port, 'GET', f'/instances/{hits[0]["ID"]}/file'))
        return found

    def pacs_manifest(self, name):
        """The objects of one fixture set pacs_server serves over WADO-URI, with the SHA-256 of their pixel data."""
        item, found = self.sets[name], {}
        for sop in item['manifest']:
            query = urllib.parse.urlencode(dict(requestType='WADO', studyUID=item['study'], seriesUID=sop.rsplit('.', 1)[0],
                                                objectUID=sop, contentType='application/dicom'))
            status, data = self.http('GET', f'http://127.0.0.1:{self.ports["pacs_http"]}/wado?{query}',
                                     headers={'Authorization': f'Bearer {self.token}', 'Accept': 'application/dicom'})
            if status == 200:
                found[sop] = pixel_digest(data)
        return found

    def pacs_instance_count(self, name):
        """Rows QIDO-RS lists for a set's study: a duplicate would be a second row."""
        status, data = self.http('GET', f'http://127.0.0.1:{self.ports["pacs_http"]}/dicomweb/studies/{self.sets[name]["study"]}/instances',
                                 headers={'Authorization': f'Bearer {self.token}', 'Accept': 'application/dicom+json'})
        require(status in (200, 204), f'QIDO-RS answered {status}')
        rows = json.loads(data) if status == 200 and data else []
        uids = [row['00080018']['Value'][0] for row in rows]
        require(len(uids) == len(set(uids)), f'QIDO-RS lists a SOP Instance twice: {len(uids)} rows')
        return len(uids)

    def store_to_pacs(self, *names):
        files = [path for name in names for path in self.sets[name]['files']]
        self.command(['storescu', '-aet', SCU_AE, '-aec', PACS_AE, '127.0.0.1', self.ports['pacs_dimse'], *files], timeout=600)
        return len(files)

    def audit_total(self, event_type, patient_id, outcome=None):
        query = dict(event_type=event_type, patient_id=patient_id, limit=1)
        if outcome:
            query['outcome'] = outcome
        status, page = self.api('GET', f'/audit/logs?{urllib.parse.urlencode(query)}')
        require(status == 200, f'audit answered {status}: {page}')
        return page['pagination']['total']

    def store_counters(self):
        status, document = self.api('GET', '/system/metrics')
        require(status == 200, f'GET /system/metrics answered {status}: {document}')
        store = document['dimse_operations']['c_store']
        return dict(success=store['success'], failure=store['failure'], images=document['data_transfer']['images_stored'],
                    associations=document['associations']['total_established'])

    def job(self, job_id):
        status, job = self.api('GET', f'/jobs/{job_id}')
        require(status == 200, f'GET /jobs/{job_id} answered {status}: {job}')
        return job

    def wait_job(self, job_id, wanted=TERMINAL, timeout=JOB_TIMEOUT):
        deadline = time.monotonic()+timeout
        while True:
            job = self.job(job_id)
            if job['status'] in wanted:
                return job
            require(time.monotonic() < deadline, f'job {job_id} still {job["status"]} after {timeout} s: {job}')
            time.sleep(.2)

    def wait_progress(self, job_id, at_least, timeout=60):
        deadline = time.monotonic()+timeout
        while True:
            job = self.job(job_id)
            if job['progress']['completed_items'] >= at_least or job['status'] in TERMINAL:
                return job
            require(time.monotonic() < deadline, f'job {job_id} made no progress in {timeout} s: {job}')
            time.sleep(.02)

    def create_job(self, label, document, start=True):
        status, created = self.api('POST', '/jobs', document)
        require(status == 201, f'POST /jobs answered {status}: {created}')
        require(created['status'] == 'pending', f'a new job is {created["status"]}, not pending')
        job_id = created['job_id']
        self.jobs[label] = job_id
        if start:
            status, started = self.api('POST', f'/jobs/{job_id}/start')
            require(status == 200 and started['status'] in ('queued', 'running', *TERMINAL),
                    f'POST /jobs/{job_id}/start answered {status}: {started}')
        return job_id

    # Cases ----------------------------------------------------------------------------------

    def peers_case(self):
        names = ('remote', 'sync', 'prior-held', 'prior-missing')
        for name in names:
            self.orthanc_load(self.ports['a_http'], self.sets[name]['files'])
        held = self.orthanc_count(self.ports['a_http'])
        require(held == sum(len(self.sets[name]['manifest']) for name in names), f'Orthanc A holds {held} objects')
        require(self.orthanc_count(self.ports['b_http']) == 0, 'Orthanc B is not empty')
        return f'Orthanc A holds {held} objects, Orthanc B none'

    def seed_case(self):
        sent = self.store_to_pacs('prior-held', 'local', 'partial', 'retry', 'cancel', 'pause', 'graceful', 'forced')
        for name in ('prior-held', 'local', 'partial', 'retry'):
            require(self.pacs_manifest(name) == self.sets[name]['manifest'], f'pacs_server does not serve the {name} set as sent')
        return f'{sent} objects stored over C-STORE; the small sets are served back with equal pixels'

    def nodes_case(self):
        for name, ae, port in (('a', A_AE, self.ports['a_dimse']), ('b', B_AE, self.ports['b_dimse'])):
            status, node = self.api('POST', '/remote-nodes', dict(name=f'Orthanc {name.upper()}', ae_title=ae, host='127.0.0.1',
                                                                  port=port, supports_find=True, supports_move=True,
                                                                  supports_get=False, supports_store=True))
            require(status == 201, f'POST /remote-nodes answered {status}: {node}')
            self.nodes[name] = node['node_id']
            status, verified = self.api('POST', f'/remote-nodes/{self.nodes[name]}/verify')
            require(status == 200 and verified.get('success') is True, f'verify {name}: {status} {verified}')
            status, node = self.api('GET', f'/remote-nodes/{self.nodes[name]}')
            require(status == 200 and node['status'] == 'online', f'node {name} after C-ECHO: {status} {node}')
        return f'nodes {self.nodes} answered C-ECHO and read back online'

    def retrieve_case(self):
        item = self.sets['remote']
        before = self.store_counters()
        audit_before = self.audit_total('C_STORE', item['patient_id'], 'SUCCESS')
        job = self.wait_job(self.create_job('retrieve', dict(type='retrieve', source_node_id=self.nodes['a'], study_uid=item['study'])))
        require(job['status'] == 'completed', f'retrieve job ended {job["status"]}: {job.get("error_message")}')
        found = self.pacs_manifest('remote')
        require(found == item['manifest'], f'pacs_server serves {len(found)} of {len(item["manifest"])} retrieved objects, or other pixels')
        require(self.pacs_instance_count('remote') == len(item['manifest']), 'QIDO-RS lists another number of objects')
        audit = self.audit_total('C_STORE', item['patient_id'], 'SUCCESS')-audit_before
        after = self.store_counters()
        moved = len(item['manifest'])
        require(audit == moved, f'audit trail: {audit} successful C_STORE rows for {moved} retrieved objects')
        require(after['success']-before['success'] == moved and after['images']-before['images'] == moved,
                f'metrics moved from {before} to {after} for {moved} stored objects')
        return f'{moved} objects with equal pixels; {audit} C_STORE audit rows and +{moved} stored images in the metrics'

    def retrieve_repeat_case(self):
        """pacs_server refuses a SOP Instance it holds (duplicate policy reject), answering 0x0111. Orthanc
        counts that answer as a completed sub-operation, so the job completes: the job reports what the
        source reported, and the refusals are in the audit trail and the metrics. Nothing is stored twice."""
        item = self.sets['remote']
        before = self.store_counters()
        refused_before = self.audit_total('C_STORE', item['patient_id'], 'FAILURE')
        job = self.wait_job(self.create_job('retrieve-repeat', dict(type='retrieve', source_node_id=self.nodes['a'], study_uid=item['study'])))
        moved = len(item['manifest'])
        require(job['status'] == 'completed', f'the repeated retrieve ended {job["status"]}: {job.get("error_message")}')
        progress = job['progress']
        require((progress['total_items'], progress['completed_items'], progress['failed_items']) == (moved, moved, 0),
                f'progress {progress}, the source reported {moved} completed sub-operations')
        require(self.pacs_instance_count('remote') == moved, 'the repeated retrieve changed what pacs_server lists')
        require(self.pacs_manifest('remote') == item['manifest'], 'the repeated retrieve changed what pacs_server serves')
        refused = self.audit_total('C_STORE', item['patient_id'], 'FAILURE')-refused_before
        after = self.store_counters()
        require(refused == moved, f'audit trail: {refused} refused C_STORE rows for {moved} offered duplicates')
        require(after['failure']-before['failure'] == moved and after['images'] == before['images'],
                f'metrics moved from {before} to {after} for {moved} refused duplicates')
        return f'job completed as the source reported; nothing stored twice; {refused} refusals audited and counted as failures'

    def store_case(self, name, again=False):
        item = self.sets[name]
        before = self.orthanc_count(self.ports['b_http'])
        job = self.wait_job(self.create_job(f'store-{name}{"-again" if again else ""}', dict(
            type='store', destination_node_id=self.nodes['b'], instance_uids=sorted(item['manifest']))))
        require(job['status'] == 'completed', f'store job ended {job["status"]}: {job.get("error_message")}')
        progress = job['progress']
        require(progress['completed_items'] == len(item['manifest']) and progress['failed_items'] == 0, f'progress {progress}')
        sent = self.orthanc_manifest(self.ports['b_http'], name)
        require(sent == item['manifest'], f'Orthanc B holds {len(sent)} of {len(item["manifest"])} sent objects, or other pixels')
        added = self.orthanc_count(self.ports['b_http'])-before
        expected = 0 if again else len(item['manifest'])
        require(added == expected, f'Orthanc B gained {added} objects, expected {expected}')
        return f'{len(sent)} objects at Orthanc B with equal pixels, {added} new'

    def missing_object_case(self):
        item = self.sets['partial']
        missing = f'{self.run_uid}.999.1'
        before = self.orthanc_count(self.ports['b_http'])
        job = self.wait_job(self.create_job('partial', dict(type='store', destination_node_id=self.nodes['b'],
                                                            instance_uids=[*sorted(item['manifest']), missing])))
        require(job['status'] == 'failed', f'a job naming an object pacs_server does not hold ended {job["status"]}')
        require(missing in (job.get('error_message') or ''), f'the error does not name the missing object: {job.get("error_message")}')
        require(self.orthanc_manifest(self.ports['b_http'], 'partial') == item['manifest'], 'the objects it holds were not all sent')
        require(self.orthanc_count(self.ports['b_http'])-before == len(item['manifest']), 'Orthanc B gained another number of objects')
        progress = job['progress']
        require(progress['completed_items'] == len(item['manifest']) and progress['failed_items'] == 1, f'progress {progress}')
        return f'failed and says why ({job["error_message"][:100]}); the {len(item["manifest"])} held objects arrived'

    def unknown_node_case(self):
        job = self.wait_job(self.create_job('unknown-node', dict(type='store', destination_node_id='no-such-node',
                                                                 instance_uids=sorted(self.sets['local']['manifest']))))
        require(job['status'] == 'failed' and 'no-such-node' in (job.get('error_message') or ''), f'job to an unknown node: {job}')
        return f'failed: {job["error_message"]}'

    def retry_case(self):
        """With Orthanc B down the job fails; with B back, retry runs it again and everything arrives once."""
        item = self.sets['retry']
        self.stop_b()
        try:
            job_id = self.create_job('retry', dict(type='store', destination_node_id=self.nodes['b'], instance_uids=sorted(item['manifest'])))
            failed = self.wait_job(job_id)
            require(failed['status'] == 'failed', f'with Orthanc B down the job ended {failed["status"]}')
        finally:
            self.start_b()
        status, verified = self.api('POST', f'/remote-nodes/{self.nodes["b"]}/verify')
        require(status == 200 and verified.get('success') is True, f'verify B after its restart: {verified}')
        before = self.orthanc_count(self.ports['b_http'])
        status, retried = self.api('POST', f'/jobs/{job_id}/retry')
        require(status == 200, f'retry answered {status}: {retried}')
        job = self.wait_job(job_id)
        require(job['status'] == 'completed', f'the retried job ended {job["status"]}: {job.get("error_message")}')
        require(job['retry_count'] == 1, f'retry_count {job["retry_count"]} after one retry')
        require(self.orthanc_manifest(self.ports['b_http'], 'retry') == item['manifest'], 'Orthanc B does not hold the retried set as sent')
        added = self.orthanc_count(self.ports['b_http'])-before
        require(added == len(item['manifest']), f'Orthanc B gained {added} objects on the retry')
        return f'failed while B was down ({failed["error_message"][:80]}); the retry completed, {added} objects arrived once'

    def cancel_case(self):
        item = self.sets['cancel']
        job_id = self.create_job('cancel', dict(type='store', destination_node_id=self.nodes['b'], instance_uids=sorted(item['manifest'])))
        running = self.wait_progress(job_id, 5)
        require(running['status'] not in TERMINAL, f'the job ended {running["status"]} before it could be cancelled')
        status, cancelled = self.api('POST', f'/jobs/{job_id}/cancel')
        require(status == 200, f'cancel answered {status}: {cancelled}')
        job = self.wait_job(job_id)
        arrived = len(self.orthanc_manifest(self.ports['b_http'], 'cancel'))
        require(job['status'] == 'cancelled', f'the cancelled job ended {job["status"]}')
        require(0 < arrived < len(item['manifest']), f'{arrived} of {len(item["manifest"])} objects arrived: cancellation not exercised')
        require(arrived == job['progress']['completed_items'], f'progress says {job["progress"]["completed_items"]} sent, Orthanc B holds {arrived}')
        status, again = self.api('POST', f'/jobs/{job_id}/cancel')
        require(status == 409, f'cancelling a cancelled job answered {status}')
        return f'cancelled after {arrived} of {len(item["manifest"])} objects; progress and destination agree'

    def pause_case(self):
        item = self.sets['pause']
        job_id = self.create_job('pause', dict(type='store', destination_node_id=self.nodes['b'], instance_uids=sorted(item['manifest'])))
        self.wait_progress(job_id, 3)
        status, paused = self.api('POST', f'/jobs/{job_id}/pause')
        require(status == 200 and paused['status'] == 'paused', f'pause answered {status}: {paused}')
        time.sleep(1)
        held = self.job(job_id)['progress']['completed_items']
        time.sleep(2)
        later = self.job(job_id)
        require(later['status'] == 'paused' and later['progress']['completed_items'] == held,
                f'paused job moved from {held} to {later["progress"]["completed_items"]} objects')
        arrived_paused = len(self.orthanc_manifest(self.ports['b_http'], 'pause'))
        require(arrived_paused == held, f'Orthanc B holds {arrived_paused} objects while progress says {held}')
        status, resumed = self.api('POST', f'/jobs/{job_id}/resume')
        require(status == 200, f'resume answered {status}: {resumed}')
        job = self.wait_job(job_id)
        require(job['status'] == 'completed', f'the resumed job ended {job["status"]}')
        require(self.orthanc_manifest(self.ports['b_http'], 'pause') == item['manifest'], 'Orthanc B does not hold the set as sent')
        return f'held at {held} of {len(item["manifest"])} objects for 2 s while paused, then completed'

    def routing_case(self):
        routed = self.sets['routed']
        status, rule = self.api('POST', '/routing/rules', dict(
            name='CT to Orthanc B', description='forward every CT object', enabled=True, priority=10,
            conditions=[dict(field='modality', pattern='CT')], actions=[dict(destination_node_id=self.nodes['b'])]))
        require(status == 201, f'POST /routing/rules answered {status}: {rule}')
        status, enabled = self.api('POST', '/routing/enable')
        require(status == 200, f'routing enable answered {status}: {enabled}')
        before = self.orthanc_count(self.ports['b_http'])
        self.store_to_pacs('routed', 'kept')
        deadline = time.monotonic()+JOB_TIMEOUT
        while self.orthanc_manifest(self.ports['b_http'], 'routed') != routed['manifest'] and time.monotonic() < deadline:
            time.sleep(.5)
        require(self.orthanc_manifest(self.ports['b_http'], 'routed') == routed['manifest'], 'Orthanc B does not hold every CT object as sent')
        require(not self.orthanc_manifest(self.ports['b_http'], 'kept'), 'an MR object no rule names reached Orthanc B')
        require(self.orthanc_count(self.ports['b_http'])-before == len(routed['manifest']), 'Orthanc B gained another number of objects')
        status, listed = self.api('GET', '/jobs?limit=100')
        require(status == 200, f'GET /jobs answered {status}')
        forwards = [job for job in listed['jobs'] if job['type'] == 'store' and job.get('instance_uids')
                    and job['instance_uids'][0] in routed['manifest']]
        require(len(forwards) == len(routed['manifest']) and all(job['status'] == 'completed' for job in forwards),
                f'forward jobs: {[(job["job_id"], job["status"]) for job in forwards]}')
        return f'{len(routed["manifest"])} CT objects forwarded by {len(forwards)} completed jobs; no MR object'

    def sync_case(self, again=False):
        item = self.sets['sync']
        stored_before = self.audit_total('C_STORE', item['patient_id'], 'SUCCESS')
        job = self.wait_job(self.create_job('sync-again' if again else 'sync', dict(
            type='sync', source_node_id=self.nodes['a'], patient_id=item['patient_id'])))
        require(job['status'] == 'completed', f'sync job ended {job["status"]}: {job.get("error_message")}')
        progress = job['progress']
        require(progress['total_items'] == 1, f'the sync found {progress["total_items"]} studies, not 1')
        require((progress['skipped_items'], progress['completed_items']) == ((1, 0) if again else (0, 1)), f'progress {progress}')
        require(self.pacs_manifest('sync') == item['manifest'], 'pacs_server does not serve the synchronized study as sent')
        stored = self.audit_total('C_STORE', item['patient_id'], 'SUCCESS')-stored_before
        require(stored == (0 if again else len(item['manifest'])), f'{stored} objects stored by this sync')
        return f'study {"already held, nothing moved" if again else "retrieved"}: {stored} C_STORE audit rows'

    def prefetch_case(self, again=False):
        held, missing = self.sets['prior-held'], self.sets['prior-missing']
        stored_before = self.audit_total('C_STORE', missing['patient_id'], 'SUCCESS')
        job = self.wait_job(self.create_job('prefetch-again' if again else 'prefetch', dict(
            type='prefetch', source_node_id=self.nodes['a'], patient_id=missing['patient_id'])))
        require(job['status'] == 'completed', f'prefetch job ended {job["status"]}: {job.get("error_message")}')
        progress = job['progress']
        require(progress['total_items'] == 2, f'the prefetch found {progress["total_items"]} studies, not 2')
        require((progress['skipped_items'], progress['completed_items']) == ((2, 0) if again else (1, 1)), f'progress {progress}')
        require(self.pacs_manifest('prior-missing') == missing['manifest'], 'pacs_server does not serve the prefetched study as sent')
        require(self.pacs_instance_count('prior-held') == len(held['manifest']), 'the held prior study changed')
        stored = self.audit_total('C_STORE', missing['patient_id'], 'SUCCESS')-stored_before
        require(stored == (0 if again else len(missing['manifest'])), f'{stored} objects stored by this prefetch')
        return 'both priors held, nothing moved' if again else 'the held prior skipped, the missing one retrieved'

    def health_case(self):
        """Status, totals and counters against what this run did and what the disk holds."""
        held = sum(len(item['manifest']) for item in self.sets.values())
        studies = len(self.sets)
        series = sum(len({sop.rsplit('.', 1)[0] for sop in item['manifest']}) for item in self.sets.values())
        status, metrics = self.api('GET', '/system/metrics')
        require(status == 200, f'metrics answered {status}')
        store = metrics['dimse_operations']['c_store']
        refused = len(self.sets['remote']['manifest'])
        require((store['success'], store['failure'], metrics['data_transfer']['images_stored']) == (held, refused, held),
                f'C-STORE counters {store}, images {metrics["data_transfer"]["images_stored"]}: expected {held} stored and {refused} refused')
        time.sleep(6)  # the status is cached for 5 s
        status, health = self.api('GET', '/system/status')
        require(status == 200 and health.get('operational') is True, f'status: {status} {health}')
        totals = health['metrics']
        require((totals['total_instances'], totals['total_studies'], totals['total_series']) == (held, studies, series),
                f'status totals {totals}: the run stored {held} objects in {studies} studies and {series} series')
        require((totals['successful_stores'], totals['failed_stores']) == (store['success'], store['failure']),
                f'status store counts {totals} differ from the metrics {store}')
        require(health['associations']['total'] == metrics['associations']['total_established'] > 0,
                f'associations: status {health["associations"]}, metrics {metrics["associations"]}')
        disk = os.statvfs(self.work/'pacs'/'archive')
        used = 100*(1-disk.f_bavail/disk.f_blocks)
        reported = health['storage']['usage_percent']
        require(abs(reported-used) < 1, f'storage usage reported {reported}%, the file system says {used:.2f}%')
        expected = 'degraded' if reported >= 95 else 'healthy'
        require(health['status'] == expected and health['healthy'] is (expected == 'healthy'),
                f'status {health["status"]}, healthy {health["healthy"]} at {reported}% used')
        return (f'{held} objects in {studies} studies and {series} series, {store["success"]} stored and {store["failure"]} refused '
                f'C-STOREs agree between status and metrics; storage {reported}% used, status {health["status"]}')

    def interrupt_case(self, name, kill):
        """Start a store job and stop pacs_server in the middle of it; the next run must finish it."""
        item = self.sets[name]
        job_id = self.create_job(name, dict(type='store', destination_node_id=self.nodes['b'], instance_uids=sorted(item['manifest'])))
        running = self.wait_progress(job_id, 3)
        require(running['status'] not in TERMINAL, f'the job ended {running["status"]} before the stop')
        self.interrupted = dict(job_id=job_id, sent=running['progress']['completed_items'])
        if kill:
            self.kills.append(self.pacs_runs)
            os.killpg(self.pacs_process.pid, signal.SIGKILL)
            self.pacs_process.wait()
            # A killed process holds its listening sockets until the kernel has unmapped its memory, which takes
            # seconds for a sanitizer's address space; the next run must not take them for its own.
            for port in (self.ports['pacs_dimse'], self.ports['pacs_http']):
                self.wait_closed(port)
        return f'job {job_id} stopped by {"SIGKILL" if kill else "SIGTERM"} after {running["progress"]["completed_items"]} of {len(item["manifest"])} objects'

    def resumed_case(self, name):
        item, job_id = self.sets[name], self.interrupted['job_id']
        first = self.job(job_id)
        require(first['status'] in ('queued', 'running', 'completed'), f'after the restart the job is {first["status"]}')
        job = self.wait_job(job_id)
        require(job['status'] == 'completed', f'the resumed job ended {job["status"]}: {job.get("error_message")}')
        require(job['completed_at'] >= self.restarted_at,
                f'the job completed at {job["completed_at"]}, before the restart at {self.restarted_at}: nothing was interrupted')
        require(self.orthanc_manifest(self.ports['b_http'], name) == item['manifest'], 'Orthanc B does not hold the set once, as sent')
        return (f'stopped after {self.interrupted["sent"]} of {len(item["manifest"])} objects; after the restart the job was '
                f'{first["status"]} and completed at {job["completed_at"]}; Orthanc B holds each object once')

    def restart_state_case(self):
        listed, offset = {}, 0
        while True:
            status, page = self.api('GET', f'/jobs?limit=100&offset={offset}')
            require(status == 200, f'GET /jobs answered {status}')
            listed.update({job['job_id']: job['status'] for job in page['jobs']})
            offset += 100
            if offset >= page['total']:
                break
        missing = [label for label, job_id in self.jobs.items() if job_id not in listed]
        require(not missing, f'jobs lost across the restart: {missing}')
        for label in ('retrieve', 'partial', 'cancel', 'pending'):
            job = self.job(self.jobs[label])
            require(job['status'] == listed[self.jobs[label]], f'{label}: listed {listed[self.jobs[label]]}, read {job["status"]}')
        require(listed[self.jobs['pending']] == 'pending', f'a job never started is {listed[self.jobs["pending"]]} after the restart')
        require(listed[self.jobs['partial']] == 'failed' and listed[self.jobs['cancel']] == 'cancelled', 'terminal jobs changed state')
        return f'all {len(self.jobs)} jobs listed and readable after the restart; the unstarted one still pending'

    def audit_kept_case(self):
        moved = len(self.sets['remote']['manifest'])
        kept = self.audit_total('C_STORE', self.sets['remote']['patient_id'], 'SUCCESS')
        require(kept == moved, f'{kept} successful C_STORE rows for the retrieved study after the restarts, expected {moved}')
        return f'the audit rows written before the graceful stop and the forced kill are still there ({kept})'

    def stopped_cleanly(self):
        unclean = [f'{name} exited {code}' for name, code in self.stops if code != 0 and 'killed on purpose' not in name]
        killed = [code for name, code in self.stops if 'killed on purpose' in name]
        require(self.stops and not unclean, f'pacs_server exits on SIGTERM: {self.stops}')
        require(killed == [-signal.SIGKILL]*len(self.kills), f'killed runs: {killed}')
        return f'{len(self.stops)-len(killed)} pacs_server runs exited 0 on SIGTERM; {len(killed)} killed on purpose'

    def run_all(self):
        self.case('workflow.fixtures', self.fixtures, phase='fixture_preparation')
        self.case('workflow.tokens', self.tokens, phase='prerequisite')
        self.case('workflow.versions', self.versions, phase='prerequisite')
        a_modalities = {'PACS': [PACS_AE, '127.0.0.1', self.ports['pacs_dimse']]}
        nodes = ['workflow.nodes']
        with ExitStack() as peers:
            peers.enter_context(self.orthanc('a', A_AE, self.ports['a_dimse'], self.ports['a_http'], a_modalities, 'server-orthanc-a'))
            self.start_b()
            peers.callback(self.stop_b)
            self.case('workflow.peers', self.peers_case, requires=['workflow.fixtures'], phase='prerequisite')
            with self.pacs(fresh=True):
                self.case('workflow.seed', self.seed_case, requires=['workflow.peers', 'workflow.tokens'])
                self.case('workflow.nodes', self.nodes_case, requires=['workflow.seed'])
                self.case('workflow.retrieve', self.retrieve_case, requires=nodes)
                self.case('workflow.retrieve-repeat-refused', self.retrieve_repeat_case, requires=['workflow.retrieve'])
                self.case('workflow.store', lambda: self.store_case('local'), requires=nodes)
                self.case('workflow.store-again', lambda: self.store_case('local', again=True), requires=['workflow.store'])
                self.case('workflow.missing-object', self.missing_object_case, requires=nodes)
                self.case('workflow.unknown-node', self.unknown_node_case, requires=nodes)
                self.case('workflow.retry', self.retry_case, requires=nodes)
                self.case('workflow.cancel', self.cancel_case, requires=nodes)
                self.case('workflow.pause-resume', self.pause_case, requires=nodes)
                self.case('workflow.routing', self.routing_case, requires=nodes)
                self.case('workflow.sync', self.sync_case, requires=nodes)
                self.case('workflow.sync-again', lambda: self.sync_case(again=True), requires=['workflow.sync'])
                self.case('workflow.prefetch', self.prefetch_case, requires=nodes)
                self.case('workflow.prefetch-again', lambda: self.prefetch_case(again=True), requires=['workflow.prefetch'])
                self.case('workflow.health', self.health_case, requires=nodes)
                self.case('workflow.pending-created', lambda: self.create_job('pending', dict(
                    type='store', destination_node_id=self.nodes['b'], instance_uids=sorted(self.sets['local']['manifest'])),
                    start=False) and 'created and never started', requires=nodes)
                self.case('workflow.graceful-stop', lambda: self.interrupt_case('graceful', kill=False), requires=nodes)
            self.restarted_at = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
            with self.pacs():
                self.case('workflow.graceful-resumed', lambda: self.resumed_case('graceful'), requires=['workflow.graceful-stop'])
                self.case('workflow.restart-state', self.restart_state_case,
                          requires=['workflow.pending-created', 'workflow.cancel', 'workflow.missing-object', 'workflow.retrieve'])
                self.case('workflow.forced-kill', lambda: self.interrupt_case('forced', kill=True), requires=nodes)
            self.restarted_at = time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())
            with self.pacs():
                self.case('workflow.forced-resumed', lambda: self.resumed_case('forced'), requires=['workflow.forced-kill'])
                self.case('workflow.audit-kept', self.audit_kept_case, requires=['workflow.retrieve'])
        self.case('workflow.stops-on-sigterm', self.stopped_cleanly)
        counts = {status: sum(1 for row in self.results if row['status'] == status) for status in ('passed', 'failed', 'blocked')}
        (self.output/'workflow-summary.json').write_text(json.dumps(dict(
            counts=counts, jobs=self.jobs, nodes=self.nodes, ports=self.ports, stops=self.stops,
            failures=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'failed'],
            blocked=[row['id'] for row in self.results if row['status'] == 'blocked']), indent=1)+'\n')
        shutil.rmtree(self.private, ignore_errors=True)
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
    return Workflow(args).run_all()


if __name__ == '__main__':
    raise SystemExit(main())
