#!/usr/bin/env python3
"""pacs_ai's connector and result handler against an inference service in another process (#1323).

validation/peers/ai/ai_contract_service.py shares no code with PACS. This probe archives a source object,
starts the service, runs the hidden [ai-contract] cases of ai_tests against it, and then checks what the
service itself recorded: the request bodies, the bearer token, the polls and the cancellation, and the
UID references inside the result objects it wrote. Each Catch2 case counts only together with that
record. The cases with an actual inference provider are reported blocked: no provider is declared, and
no provider endpoint or account exists here; the contract service cannot stand in for one.
"""
import argparse
import json
import os
from pathlib import Path
import secrets
import socket
import sys
import xml.etree.ElementTree as ET

import pydicom
from pydicom.dataset import Dataset, FileMetaDataset

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
from run import Validation, require

SERVICE = ROOT/'validation/peers/ai/ai_contract_service.py'
CT_IMAGE = '1.2.840.10008.5.1.4.1.1.2'
COMPREHENSIVE_SR = '1.2.840.10008.5.1.4.1.1.88.33'
SEGMENTATION = '1.2.840.10008.5.1.4.1.1.66.4'
EXPLICIT = '1.2.840.10008.1.2.1'
CASES = {
    "a completed inference's results reach the archive tied to their source": 'ai.contract.completed',
    "a failed inference reports the service's error and no result": 'ai.contract.failed',
    'an inference that never finishes times out and is cancelled at the service': 'ai.contract.timeout-cancel',
    'a result about a study the archive does not hold is refused and leaves nothing': 'ai.contract.foreign-study',
    "a request without the service's token is refused": 'ai.contract.unauthorized',
}


def free_port():
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


class AiContract(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.work = self.output/'work'
        (self.work/'results').mkdir(parents=True)
        (self.work/'pacs').mkdir()
        self.port = free_port()
        self.token = secrets.token_hex(16)  # made for this run; it opens nothing but the contract service
        self.requests = self.output/'service-requests.jsonl'
        self.source = self.work/'source.dcm'
        self.outcomes = {}

    def source_case(self):
        study = f'{self.run_uid}.1'
        ds = Dataset()
        ds.file_meta = FileMetaDataset()
        ds.file_meta.MediaStorageSOPClassUID, ds.file_meta.MediaStorageSOPInstanceUID = CT_IMAGE, f'{study}.1.1'
        ds.file_meta.TransferSyntaxUID = EXPLICIT
        ds.SOPClassUID, ds.SOPInstanceUID = CT_IMAGE, ds.file_meta.MediaStorageSOPInstanceUID
        ds.StudyInstanceUID, ds.SeriesInstanceUID = study, f'{study}.1'
        ds.PatientName, ds.PatientID, ds.PatientBirthDate, ds.PatientSex = 'AI^SOURCE', 'AI-CONTRACT', '19700101', 'O'
        ds.StudyDate, ds.StudyTime, ds.StudyID, ds.AccessionNumber = '20260501', '101112', 'AI', 'ACC-AI'
        ds.ReferringPhysicianName, ds.Modality, ds.SeriesNumber, ds.InstanceNumber = '', 'CT', '1', '1'
        ds.ImageType, ds.KVP = ['ORIGINAL', 'PRIMARY', 'AXIAL'], '120'
        ds.Rows, ds.Columns, ds.SamplesPerPixel, ds.PhotometricInterpretation = 4, 4, 1, 'MONOCHROME2'
        ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 16, 12, 11, 0
        ds.RescaleIntercept, ds.RescaleSlope = '-1024', '1'
        ds.PixelData = bytes(range(32))
        ds.is_little_endian, ds.is_implicit_VR = True, False
        ds.save_as(self.source, write_like_original=False)
        self.source_uids = dict(study=study, series=ds.SeriesInstanceUID, instance=ds.SOPInstanceUID)
        return f'CT image {ds.SOPInstanceUID} in study {study}'

    def run_contract(self):
        env = dict(os.environ, PACS_AI_SERVICE_URL=f'http://127.0.0.1:{self.port}/v1', PACS_AI_TOKEN=self.token,
                   PACS_AI_RESULTS_DIR=str(self.work/'results'), PACS_AI_SOURCE=str(self.source), PACS_AI_WORK=str(self.work/'pacs'))
        report = self.output/'ai-contract.xml'
        self.command([self.bin/'ai_tests', '[ai-contract]', '--reporter', 'console', '--reporter', f'xml::out={report}'],
                     timeout=300, env=env, allowed=tuple(range(0, 256)))
        root = ET.parse(report).getroot()
        for case in root.iter('TestCase'):
            result = case.find('OverallResult')
            failures = [' '.join((node.text or '').split()) for node in case.iter('Expression') if node.get('success') == 'false']
            self.outcomes[case.get('name')] = dict(success=result is not None and result.get('success') == 'true', failures=failures)
        unknown = sorted(set(self.outcomes)-set(CASES))
        missing = sorted(set(CASES)-set(self.outcomes))
        require(not unknown and not missing, f'ai_tests [ai-contract] ran {sorted(self.outcomes)}; missing {missing}, unexpected {unknown}')
        return f'{len(self.outcomes)} Catch2 cases ran against the service on port {self.port}'

    def logged(self):
        return [json.loads(line) for line in self.requests.read_text().splitlines()]

    def catch2(self, name):
        outcome = self.outcomes[name]
        require(outcome['success'], f'Catch2 case failed: {outcome["failures"][:3]}')

    def submissions(self, model, authorized=True):
        return [row for row in self.logged() if row['method'] == 'POST' and row['path'] == '/v1/inference'
                and isinstance(row['body'], dict) and row['body'].get('model_id') == model and row['authorized'] is authorized]

    def expected_body(self, model):
        source = pydicom.dcmread(self.source)
        return dict(study_instance_uid=self.source_uids['study'], series_instance_uid=self.source_uids['series'], model_id=model,
                    priority=0, parameters=dict(threshold='0.5'),
                    metadata=dict(patient_id=source.PatientID, patient_name=str(source.PatientName),
                                  source_sop_class=CT_IMAGE, source_sop_instance=self.source_uids['instance']))

    def references(self, ds):
        """Study, series and instance UIDs a result object says it was made from."""
        if ds.SOPClassUID == COMPREHENSIVE_SR:
            item = ds.CurrentRequestedProcedureEvidenceSequence[0]
            study, series = item.StudyInstanceUID, item.ReferencedSeriesSequence[0]
        else:
            study, series = ds.StudyInstanceUID, ds.ReferencedSeriesSequence[0]
        return study, series.SeriesInstanceUID, series.ReferencedSOPSequence[0].ReferencedSOPInstanceUID

    def completed_case(self):
        self.catch2("a completed inference's results reach the archive tied to their source")
        rows = self.submissions('chest-cad')
        require(len(rows) == 1 and rows[0]['status'] == 201, f'{len(rows)} authorized chest-cad submissions logged')
        require(rows[0]['body'] == self.expected_body('chest-cad'), f'the service received {rows[0]["body"]}')
        job = rows[0]['reply']['job_id']
        polls = [row for row in self.logged() if row['path'] == f'/v1/inference/{job}' and row['method'] == 'GET']
        require([row['reply']['status'] for row in polls] == ['running', 'completed'], f'polls: {[row["reply"]["status"] for row in polls]}')
        produced = polls[-1]['reply']['result_uids']
        expected = (self.source_uids['study'], self.source_uids['series'], self.source_uids['instance'])
        classes = []
        for uid in produced:
            ds = pydicom.dcmread(self.work/'results'/f'{uid}.dcm')
            require(ds.StudyInstanceUID == self.source_uids['study'], f'{uid} is in study {ds.StudyInstanceUID}')
            require(self.references(ds) == expected, f'{uid} refers to {self.references(ds)}')
            classes.append(ds.SOPClassUID)
        require(sorted(classes) == sorted([COMPREHENSIVE_SR, SEGMENTATION]), f'result classes {classes}')
        return f'job {job}: the service got the request as sent, answered running then completed, and both results refer to the source'

    def failed_case(self):
        self.catch2("a failed inference reports the service's error and no result")
        rows = self.submissions('always-fails')
        require(len(rows) == 1 and rows[0]['status'] == 201, f'{len(rows)} always-fails submissions logged')
        job = rows[0]['reply']['job_id']
        last = [row for row in self.logged() if row['path'] == f'/v1/inference/{job}'][-1]
        require(last['reply']['status'] == 'failed' and last['reply']['result_uids'] == [], f'last answer {last["reply"]}')
        return f'job {job} failed at the service with "{last["reply"]["error_message"]}", as the connector reported'

    def timeout_case(self):
        self.catch2('an inference that never finishes times out and is cancelled at the service')
        rows = self.submissions('never-finishes')
        require(len(rows) == 1, f'{len(rows)} never-finishes submissions logged')
        job = rows[0]['reply']['job_id']
        calls = [row for row in self.logged() if row['path'] == f'/v1/inference/{job}']
        polls = [row for row in calls if row['method'] == 'GET']
        deletes = [row for row in calls if row['method'] == 'DELETE']
        require(polls and all(row['reply']['status'] == 'running' for row in polls), 'the job was not running at every poll')
        require(len(deletes) == 1 and deletes[0]['authorized'] and deletes[0]['reply']['status'] == 'cancelled',
                f'cancellation at the service: {deletes}')
        return f'{len(polls)} polls saw it running; the timed-out job was cancelled at the service with one DELETE'

    def foreign_case(self):
        self.catch2('a result about a study the archive does not hold is refused and leaves nothing')
        rows = self.submissions('bad-reference')
        require(len(rows) == 1, f'{len(rows)} bad-reference submissions logged')
        job = rows[0]['reply']['job_id']
        last = [row for row in self.logged() if row['path'] == f'/v1/inference/{job}'][-1]
        (uid,) = last['reply']['result_uids']
        ds = pydicom.dcmread(self.work/'results'/f'{uid}.dcm')
        require(ds.StudyInstanceUID != self.source_uids['study'], 'the foreign result names the source study')
        archived = [path.name for path in (self.work/'pacs'/'archive').rglob('*') if path.is_file() and uid in path.name]
        require(not archived, f'the refused result is in the archive: {archived}')
        return f'result {uid} names study {ds.StudyInstanceUID}; refused, and no file of it is in the archive'

    def unauthorized_case(self):
        self.catch2("a request without the service's token is refused")
        refused = self.submissions('chest-cad', authorized=False)
        require(len(refused) == 1 and refused[0]['status'] == 401, f'unauthorized submissions: {refused}')
        without = [row for row in self.logged() if not row['authorized'] and row['path'] != '/v1/health']
        require(without == refused, f'requests without the token: {[(row["method"], row["path"]) for row in without]}')
        return 'the one submission without the token was answered 401; every other request carried it'

    def blocked(self, case_id, detail):
        self.results.append(dict(id=case_id, status='blocked', tests=0, failed=0, skipped=0, phase='product', chain='end-to-end',
                                 prerequisites=['an inference provider endpoint and account'], executed=False, detail=detail,
                                 blocked_by=['an inference provider endpoint and account'], failure_category='prerequisite', seconds=0))
        self.save()
        print(f'{case_id}: blocked: {detail}', flush=True)

    def run_all(self):
        self.case('ai.source', self.source_case, phase='fixture_preparation')
        args = ['python3', SERVICE, '--port', self.port, '--results-dir', self.work/'results', '--log', self.requests,
                '--token', self.token]
        with self.server(args, self.port, log_name='server-ai-contract'):
            self.case('ai.contract.run', self.run_contract, requires=['ai.source'], phase='prerequisite')
        run = ['ai.contract.run']
        self.case('ai.contract.completed', self.completed_case, requires=run)
        self.case('ai.contract.failed', self.failed_case, requires=run)
        self.case('ai.contract.timeout-cancel', self.timeout_case, requires=run)
        self.case('ai.contract.foreign-study', self.foreign_case, requires=run)
        self.case('ai.contract.unauthorized', self.unauthorized_case, requires=run)
        self.blocked('ai.actual-provider', 'No inference provider is named in the PRD or the conformance statement, and no provider '
                     'endpoint or account is configured here; the contract service does not stand in for one.')
        counts = {status: sum(1 for row in self.results if row['status'] == status) for status in ('passed', 'failed', 'blocked')}
        (self.output/'ai-summary.json').write_text(json.dumps(dict(
            counts=counts, port=self.port, source=self.source_uids,
            failures=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'failed'],
            blocked=[dict(id=row['id'], detail=row['detail']) for row in self.results if row['status'] == 'blocked']), indent=1)+'\n')
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
    return AiContract(args).run_all()


if __name__ == '__main__':
    raise SystemExit(main())
