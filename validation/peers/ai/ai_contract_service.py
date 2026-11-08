#!/usr/bin/env python3
"""An inference service that speaks the REST contract pacs_ai's ai_service_connector uses (#1323).

It shares no code with PACS. Each model behaves in one declared way, so every outcome the connector must
handle can be produced on demand:

  chest-cad        running on the first poll, completed on the second; writes a Comprehensive SR and a
                   Segmentation that reference the requested study, series and instance
  always-fails     failed, with a fixed error message
  never-finishes   running for ever, until cancelled
  bad-reference    completed; its SR names a study the request did not

Every request is appended to --log as one JSON line (method, path, whether the bearer token matched,
the parsed body), so a run can be checked against what the service actually received. Result objects
are written to --results-dir as <SOP Instance UID>.dcm.
"""
import argparse
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import threading
import uuid

from pydicom.dataset import Dataset, FileMetaDataset
from pydicom.sequence import Sequence

COMPREHENSIVE_SR = '1.2.840.10008.5.1.4.1.1.88.33'
SEGMENTATION = '1.2.840.10008.5.1.4.1.1.66.4'
EXPLICIT = '1.2.840.10008.1.2.1'
FAILURE_MESSAGE = 'the model crashed on this input'
MODELS = [
    dict(model_id='chest-cad', name='Chest CAD', description='finds one nodule and segments it', version='1.0.0'),
    dict(model_id='always-fails', name='Always fails', description='fails every request', version='1.0.0'),
    dict(model_id='never-finishes', name='Never finishes', description='runs until cancelled', version='1.0.0'),
    dict(model_id='bad-reference', name='Bad reference', description='reports on another study', version='1.0.0'),
]


def now():
    return datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')


def new_uid():
    return '2.25.' + str(uuid.uuid4().int)


class Results:
    """Builds result objects the way an inference service would, from the request alone."""

    def __init__(self, directory):
        self.directory = Path(directory)
        self.directory.mkdir(parents=True, exist_ok=True)

    @staticmethod
    def base(sop_class, modality, request, study):
        ds = Dataset()
        ds.file_meta = FileMetaDataset()
        ds.file_meta.MediaStorageSOPClassUID = sop_class
        ds.file_meta.MediaStorageSOPInstanceUID = new_uid()
        ds.file_meta.TransferSyntaxUID = EXPLICIT
        ds.SOPClassUID, ds.SOPInstanceUID = sop_class, ds.file_meta.MediaStorageSOPInstanceUID
        ds.StudyInstanceUID, ds.SeriesInstanceUID = study, new_uid()
        metadata = request.get('metadata') or {}
        ds.PatientID = metadata.get('patient_id', '')
        ds.PatientName = metadata.get('patient_name', '')
        ds.Modality, ds.SeriesNumber, ds.InstanceNumber = modality, '900', '1'
        ds.Manufacturer, ds.ManufacturerModelName, ds.SoftwareVersions = 'Contract AI', request['model_id'], '1.0.0'
        ds.ContentDate, ds.ContentTime = datetime.now(timezone.utc).strftime('%Y%m%d'), '120000'
        return ds

    @staticmethod
    def evidence(request, study):
        """The objects the result was made from, as PS3.3 C.17.2 Current Requested Procedure Evidence lists them."""
        instance = Dataset()
        instance.ReferencedSOPClassUID = request.get('metadata', {}).get('source_sop_class', '')
        instance.ReferencedSOPInstanceUID = request.get('metadata', {}).get('source_sop_instance', '')
        series = Dataset()
        series.SeriesInstanceUID = request.get('series_instance_uid', '')
        series.ReferencedSOPSequence = Sequence([instance])
        item = Dataset()
        item.StudyInstanceUID = study
        item.ReferencedSeriesSequence = Sequence([series])
        return Sequence([item])

    def write(self, ds):
        ds.is_little_endian, ds.is_implicit_VR = True, False
        path = self.directory/f'{ds.SOPInstanceUID}.dcm'
        ds.save_as(path, write_like_original=False)
        return ds.SOPInstanceUID

    def report(self, request, study):
        ds = self.base(COMPREHENSIVE_SR, 'SR', request, study)
        ds.CompletionFlag, ds.VerificationFlag, ds.ValueType = 'COMPLETE', 'UNVERIFIED', 'CONTAINER'
        concept = Dataset()
        concept.CodeValue, concept.CodingSchemeDesignator, concept.CodeMeaning = '126000', 'DCM', 'Imaging Measurement Report'
        ds.ConceptNameCodeSequence = Sequence([concept])
        ds.ContinuityOfContent = 'SEPARATE'
        finding = Dataset()
        finding.RelationshipType, finding.ValueType = 'CONTAINS', 'TEXT'
        name = Dataset()
        name.CodeValue, name.CodingSchemeDesignator, name.CodeMeaning = '121071', 'DCM', 'Finding'
        finding.ConceptNameCodeSequence = Sequence([name])
        finding.TextValue = 'nodule, 6 mm'
        ds.ContentSequence = Sequence([finding])
        ds.CurrentRequestedProcedureEvidenceSequence = self.evidence(request, study)
        return self.write(ds)

    def segmentation(self, request, study):
        ds = self.base(SEGMENTATION, 'SEG', request, study)
        ds.SegmentationType, ds.ContentLabel = 'BINARY', 'NODULE'
        ds.Rows, ds.Columns, ds.NumberOfFrames = 8, 8, 1
        ds.SamplesPerPixel, ds.PhotometricInterpretation = 1, 'MONOCHROME2'
        ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 1, 1, 0, 0
        segment = Dataset()
        segment.SegmentNumber, segment.SegmentLabel, segment.SegmentAlgorithmType = 1, 'nodule', 'AUTOMATIC'
        segment.SegmentAlgorithmName = request['model_id']
        ds.SegmentSequence = Sequence([segment])
        ds.ReferencedSeriesSequence = self.evidence(request, study)[0].ReferencedSeriesSequence
        ds.PixelData = bytes([0, 0, 0x18, 0x3C, 0x3C, 0x18, 0, 0])
        return self.write(ds)


class Service:
    def __init__(self, args):
        self.args = args
        self.results = Results(args.results_dir)
        self.jobs = {}
        self.lock = threading.Lock()
        self.log = Path(args.log)

    def record(self, entry):
        with self.lock, self.log.open('a') as log:
            log.write(json.dumps(entry) + '\n')

    def submit(self, body):
        if not isinstance(body, dict) or not body.get('study_instance_uid') or not body.get('model_id'):
            return 400, dict(error='study_instance_uid and model_id are required')
        if body['model_id'] not in {model['model_id'] for model in MODELS}:
            return 400, dict(error=f'unknown model {body["model_id"]}')
        job = dict(job_id=str(uuid.uuid4()), request=body, status='pending', polls=0, created_at=now(),
                   started_at=None, completed_at=None, result_uids=[], error_message=None)
        with self.lock:
            self.jobs[job['job_id']] = job
        return 201, dict(job_id=job['job_id'], status='pending')

    def advance(self, job):
        """One poll moves a job along the path its model declares."""
        model, request = job['request']['model_id'], job['request']
        if job['status'] in ('completed', 'failed', 'cancelled'):
            return
        job['polls'] += 1
        job['started_at'] = job['started_at'] or now()
        if model == 'never-finishes' or (model == 'chest-cad' and job['polls'] < 2):
            job['status'] = 'running'
        elif model == 'always-fails':
            job.update(status='failed', error_message=FAILURE_MESSAGE, completed_at=now())
        elif model == 'chest-cad':
            study = request['study_instance_uid']
            job.update(status='completed', completed_at=now(),
                       result_uids=[self.results.report(request, study), self.results.segmentation(request, study)])
        elif model == 'bad-reference':
            job.update(status='completed', completed_at=now(), result_uids=[self.results.report(request, new_uid())])

    def status(self, job_id):
        with self.lock:
            job = self.jobs.get(job_id)
            if not job:
                return 404, dict(error=f'no job {job_id}')
            self.advance(job)
            progress = dict(pending=0, running=50).get(job['status'], 100)
            return 200, dict(job_id=job_id, status=job['status'], progress=progress, message=f'{job["status"]} after {job["polls"]} polls',
                             error_message=job['error_message'], created_at=job['created_at'], started_at=job['started_at'],
                             completed_at=job['completed_at'], result_uids=job['result_uids'])

    def cancel(self, job_id):
        with self.lock:
            job = self.jobs.get(job_id)
            if not job:
                return 404, dict(error=f'no job {job_id}')
            if job['status'] not in ('completed', 'failed'):
                job.update(status='cancelled', completed_at=now())
            return 200, dict(job_id=job_id, status=job['status'])


def handler_for(service):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass

        def reply(self, status, document):
            data = json.dumps(document).encode()
            self.send_response(status)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Connection', 'close')
            self.end_headers()
            self.wfile.write(data)

        def route(self, method, authorized, body):
            if not self.path.startswith(service.args.base):
                return 404, dict(error='unknown path')
            route = self.path[len(service.args.base):]
            if route == '/health' and method == 'GET':
                return 200, dict(status='ok')
            if not authorized:
                return 401, dict(error='a valid bearer token is required')
            if route == '/models' and method == 'GET':
                return 200, MODELS
            if route.startswith('/models/') and method == 'GET':
                found = [model for model in MODELS if model['model_id'] == route[len('/models/'):]]
                return (200, found[0]) if found else (404, dict(error='unknown model'))
            if route == '/inference' and method == 'POST':
                return service.submit(body)
            if route.startswith('/inference/') and method == 'GET':
                return service.status(route[len('/inference/'):])
            if route.startswith('/inference/') and method == 'DELETE':
                return service.cancel(route[len('/inference/'):])
            return 404, dict(error='unknown route')

        def handle_any(self, method):
            length = int(self.headers.get('Content-Length') or 0)
            raw = self.rfile.read(length) if length else b''
            try:
                body = json.loads(raw) if raw else None
            except ValueError:
                body = raw.decode(errors='replace')
            authorized = not service.args.token or self.headers.get('Authorization') == f'Bearer {service.args.token}'
            status, document = self.route(method, authorized, body)
            service.record(dict(time=now(), method=method, path=self.path, authorized=authorized, body=body,
                                status=status, reply=document))
            self.reply(status, document)

        def do_GET(self):
            self.handle_any('GET')

        def do_POST(self):
            self.handle_any('POST')

        def do_DELETE(self):
            self.handle_any('DELETE')

    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--base', default='/v1')
    parser.add_argument('--results-dir', required=True)
    parser.add_argument('--log', required=True)
    parser.add_argument('--token', default='')
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', args.port), handler_for(Service(args)))
    server.serve_forever()


if __name__ == '__main__':
    main()
