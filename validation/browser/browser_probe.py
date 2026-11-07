#!/usr/bin/env python3
"""The web app in a real browser against the real pacs_server (#1322).

pacs_server runs with its REST API on and OAuth 2.0 tokens required; objects reach it through C-STORE from
DCMTK storescu and STOW-RS from curl, so the data the pages show was stored through supported APIs. The
values each page must show are declared here, before any page is opened, and the browser tests also ask
the server directly and through DIMSE, so a page that agrees only with itself cannot pass. Nothing
intercepts the API.
"""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import time

import pydicom
from pydicom.dataset import Dataset, FileMetaDataset

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'validation/dcmtk'))
sys.path.insert(0, str(ROOT/'validation/peers/http'))
from http_peer import TokenKit, build_multipart
from run import Validation, require

WEB = ROOT/'web'
ISSUER, AUDIENCE = 'https://issuer.browser.test', 'pacs-web'
PACS_AE, SCU_AE = 'PACS_BROWSER', 'BROWSER_SCU'
EXPLICIT = '1.2.840.10008.1.2.1'
SECONDARY_CAPTURE = '1.2.840.10008.5.1.4.1.1.7'
DICOMWEB_SCOPES = 'dicomweb.read dicomweb.search dicomweb.write'

# What the pages must show. Patient C carries a Korean name in ISO_IR 192, and B and C arrive over STOW-RS.
PATIENTS = [
    dict(key='a', patient_id='BRW-A', name='BROWSER^ALPHA', birth_date='19800102', sex='F', protocol='C-STORE', studies=[
        dict(study_id='SA1', date='20260110', description='Chest follow-up', modality='CT', series=[
            dict(number=1, modality='CT', description='Axial', instances=2),
            dict(number=2, modality='CT', description='Coronal', instances=1)]),
        dict(study_id='SA2', date='20260212', description='Knee', modality='MR', series=[
            dict(number=1, modality='MR', description='Sagittal', instances=1)])]),
    dict(key='b', patient_id='BRW-B', name='BROWSER^BRAVO', birth_date='19751230', sex='M', protocol='STOW-RS', studies=[
        dict(study_id='SB1', date='20260305', description='Abdomen', modality='OT', series=[
            dict(number=1, modality='OT', description='Screens', instances=3)])]),
    dict(key='c', patient_id='BRW-C', name='홍^길동', birth_date='19900315', sex='O', protocol='STOW-RS', charset='ISO_IR 192', studies=[
        dict(study_id='SC1', date='20260401', description='초음파', modality='US', series=[
            dict(number=1, modality='US', description='Liver', instances=1)])]),
]


def free_port():
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


class BrowserFlows(Validation):
    def __init__(self, args):
        super().__init__(args)
        self.work = self.output/'work'
        self.work.mkdir()
        self.http_port, self.dimse_port, self.web_port = free_port(), free_port(), free_port()
        self.expected = dict(patients=[], dimse=dict(host='127.0.0.1', port=self.dimse_port, ae=PACS_AE, calling=SCU_AE))
        self.paths = {}

    # Preparation ---------------------------------------------------------------------------

    def tools(self):
        versions = {}
        for name, argv in (('node', ['node', '--version']), ('npm', ['npm', '--version']), ('storescu', ['storescu', '--version']),
                           ('findscu', ['findscu', '--version']), ('curl', ['curl', '--version']), ('openssl', ['openssl', 'version'])):
            done = subprocess.run(argv, capture_output=True, text=True)
            require(done.returncode == 0, f'{name} is not usable: {done.stderr[-200:]}')
            versions[name] = (done.stdout or done.stderr).strip().splitlines()[0]
        versions['pydicom'] = pydicom.__version__
        lock = json.loads((WEB/'package-lock.json').read_text())
        versions['@playwright/test'] = lock['packages']['node_modules/@playwright/test']['version']
        (self.output/'tool-versions.json').write_text(json.dumps(versions, indent=1)+'\n')
        return f'{len(versions)} tools; Playwright {versions["@playwright/test"]}'

    def tokens(self):
        """Tokens signed with keys made for this run alone. The keys stay in the private folder, which is removed."""
        kit = TokenKit(self.private/'keys')
        self.paths['jwks'] = self.private/'jwks.json'
        self.paths['jwks'].write_text(json.dumps(kit.jwks()))
        now = int(time.time())

        def claims(scope, **changes):
            values = dict(iss=ISSUER, aud=AUDIENCE, sub=f'browser-{scope.split()[0] if scope else "none"}', iat=now, nbf=now-5, exp=now+3*3600, scope=scope)
            values.update(changes)
            return values

        tokens = dict(
            admin=kit.token(claims(f'pacs.admin {DICOMWEB_SCOPES}', sub='browser-admin')),
            writer=kit.token(claims('pacs.write', sub='browser-writer')),
            reader=kit.token(claims('pacs.read', sub='browser-reader')),
            none=kit.token(claims('profile', sub='browser-none')),
            # Well past the validator's 60 s of clock skew.
            expired=kit.token(claims('pacs.read', sub='browser-expired', iat=now-7200, nbf=now-7200, exp=now-3600)),
            wrong_audience=kit.token(claims('pacs.read', sub='browser-elsewhere', aud='another-service')),
            forged=kit.token(claims('pacs.admin', sub='browser-forged'), key=kit.other, kid=kit.RSA_KID))
        self.paths['tokens'] = self.private/'tokens.json'
        self.paths['tokens'].write_text(json.dumps(tokens))
        self.admin_token = tokens['admin']
        return f'{len(tokens)} tokens from a key made for this run'

    def fixtures(self):
        directory = self.work/'fixtures'
        directory.mkdir()
        files = {'C-STORE': [], 'STOW-RS': []}
        for number, patient in enumerate(PATIENTS, 1):
            declared = dict(patient_id=patient['patient_id'], name=patient['name'], birth_date=patient['birth_date'], sex=patient['sex'],
                            protocol=patient['protocol'], studies=[])
            for study_number, study in enumerate(patient['studies'], 1):
                study_uid = f'{self.run_uid}.{number}.{study_number}'
                declared_study = dict(study_uid=study_uid, study_id=study['study_id'], date=study['date'], description=study['description'],
                                      modality=study['modality'], series=[])
                for series in study['series']:
                    series_uid = f'{study_uid}.{series["number"]}'
                    sops = []
                    for instance in range(1, series['instances']+1):
                        sop = f'{series_uid}.{instance}'
                        path = directory/f'{patient["key"]}-{study["study_id"]}-{series["number"]}-{instance}.dcm'
                        self.write_object(path, patient, study, series, study_uid, series_uid, sop, instance)
                        files[patient['protocol']].append(path)
                        sops.append(sop)
                    declared_study['series'].append(dict(series_uid=series_uid, number=series['number'], modality=series['modality'],
                                                         description=series['description'], sop_instance_uids=sops))
                declared['studies'].append(declared_study)
            self.expected['patients'].append(declared)
        self.files = files
        self.paths['expected'] = self.output/'expected.json'
        self.paths['expected'].write_text(json.dumps(self.expected, indent=1, ensure_ascii=False)+'\n')
        count = sum(len(paths) for paths in files.values())
        return f'{count} objects for {len(PATIENTS)} patients, declared in expected.json'

    def write_object(self, path, patient, study, series, study_uid, series_uid, sop, instance):
        ds = Dataset()
        ds.file_meta = FileMetaDataset()
        ds.file_meta.MediaStorageSOPClassUID, ds.file_meta.MediaStorageSOPInstanceUID = SECONDARY_CAPTURE, sop
        ds.file_meta.TransferSyntaxUID = EXPLICIT
        if patient.get('charset'):
            ds.SpecificCharacterSet = patient['charset']
        ds.SOPClassUID, ds.SOPInstanceUID, ds.StudyInstanceUID, ds.SeriesInstanceUID = SECONDARY_CAPTURE, sop, study_uid, series_uid
        ds.PatientName, ds.PatientID, ds.PatientBirthDate, ds.PatientSex = patient['name'], patient['patient_id'], patient['birth_date'], patient['sex']
        ds.StudyDate, ds.StudyTime, ds.StudyID, ds.AccessionNumber = study['date'], '101112', study['study_id'], f'ACC-{study["study_id"]}'
        ds.StudyDescription, ds.ReferringPhysicianName = study['description'], 'REF^DOC'
        ds.Modality, ds.SeriesNumber, ds.SeriesDescription, ds.InstanceNumber = series['modality'], str(series['number']), series['description'], str(instance)
        ds.ConversionType = 'WSD'
        ds.Rows, ds.Columns, ds.SamplesPerPixel, ds.PhotometricInterpretation = 4, 4, 1, 'MONOCHROME2'
        ds.BitsAllocated, ds.BitsStored, ds.HighBit, ds.PixelRepresentation = 8, 8, 7, 0
        ds.PixelData = bytes(range(16))
        ds.is_little_endian, ds.is_implicit_VR = True, False
        ds.save_as(path, write_like_original=False)

    # Servers --------------------------------------------------------------------------------

    @contextmanager
    def pacs(self):
        directory = self.work/'pacs'
        directory.mkdir()
        self.paths['database'] = directory/'index.db'
        args = [self.bin/'pacs_server', '--port', self.dimse_port, '--ae-title', PACS_AE, '--storage-dir', directory/'archive',
                '--db-path', self.paths['database'], '--http-port', self.http_port, '--http-rest-api',
                '--oauth2-issuer', ISSUER, '--oauth2-audience', AUDIENCE, '--oauth2-jwks', self.paths['jwks']]
        with self.server(args, self.http_port, log_name='server-pacs') as process:
            yield process
        # A server that ignores SIGTERM is killed by server(); the case below says so instead of letting that pass.
        self.stop_code = process.returncode

    @contextmanager
    def preview(self):
        env = dict(os.environ, PACS_API_URL=f'http://127.0.0.1:{self.http_port}')
        args = ['npx', 'vite', 'preview', '--host', '127.0.0.1', '--port', str(self.web_port), '--strictPort']
        with (self.output/'web-preview.log').open('w') as log:
            process = subprocess.Popen(args, cwd=WEB, env=env, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            try:
                for _ in range(300):
                    require(process.poll() is None, 'vite preview exited; see web-preview.log')
                    try:
                        socket.create_connection(('127.0.0.1', self.web_port), .2).close()
                        break
                    except OSError:
                        time.sleep(.1)
                else:
                    raise ValueError('vite preview did not open its port')
                yield process
            finally:
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGTERM)
                    process.wait(timeout=10)

    # Cases ----------------------------------------------------------------------------------

    def seed(self):
        stored = self.files['C-STORE']
        self.command(['storescu', '-aet', SCU_AE, '-aec', PACS_AE, '127.0.0.1', self.dimse_port, *stored], timeout=120)
        content_type, body = build_multipart([('application/dicom', path.read_bytes()) for path in self.files['STOW-RS']])
        (self.work/'stow-body').write_bytes(body)
        self.command(['curl', '-sS', '--fail-with-body', '-o', self.work/'stow-reply.json', '-X', 'POST',
                      '-H', f'Authorization: Bearer {self.admin_token}', '-H', f'Content-Type: {content_type}',
                      '-H', 'Accept: application/dicom+json', '--data-binary', f'@{self.work/"stow-body"}',
                      f'http://127.0.0.1:{self.http_port}/dicomweb/studies'], timeout=120)
        return f'{len(stored)} objects over C-STORE, {len(self.files["STOW-RS"])} over STOW-RS'

    def web_build(self):
        if not (WEB/'node_modules').exists():
            self.command(['npm', 'ci', '--no-audit', '--no-fund'], timeout=900, cwd=WEB)
        self.command(['npm', 'run', 'lint'], timeout=300, cwd=WEB)
        self.command(['npm', 'run', 'build'], timeout=600, cwd=WEB)
        return 'lint and production build passed'

    def flows(self):
        report = self.output/'playwright-report.json'
        env = dict(os.environ, PACS_WEB_URL=f'http://127.0.0.1:{self.web_port}', PACS_API_URL=f'http://127.0.0.1:{self.http_port}',
                   PACS_TOKENS_FILE=str(self.paths['tokens']), PACS_EXPECTED_FILE=str(self.paths['expected']),
                   PACS_DB_PATH=str(self.paths['database']), PACS_BROWSER_OUTPUT=str(self.output))
        self.command(['npx', 'playwright', 'test', '--config', 'tests/e2e/playwright.config.ts'], timeout=1800, cwd=WEB,
                     env=env, allowed=(0, 1))
        require(report.exists(), 'Playwright wrote no report')
        self.report = json.loads(report.read_text())
        tests = list(self.collect(self.report))
        require(tests, 'The browser suite selected no test')
        return dict(tests=len(tests), failed=0, skipped=0, detail=f'{len(tests)} browser tests ran; each is its own case below')

    def collect(self, report):
        for suite in report.get('suites', []):
            yield from self.collect_suite(suite, [])

    def collect_suite(self, suite, path):
        title = [*path, suite.get('title', '')] if suite.get('title') and not suite['title'].endswith('.ts') else path
        for spec in suite.get('specs', []):
            for test in spec.get('tests', []):
                yield dict(file=suite.get('file') or spec.get('file'), title=spec['title'], path=title, tags=spec.get('tags', []),
                           results=test.get('results', []), status=test.get('status'))
        for child in suite.get('suites', []):
            yield from self.collect_suite(child, title)

    def browser_cases(self):
        for test in self.collect(getattr(self, 'report', {})):
            area = Path(test['file']).stem.replace('.spec', '')
            condition = 'negative' if 'negative' in test['tags'] else 'positive'
            slug = ''.join(ch if ch.isalnum() else '-' for ch in test['title'].lower()).strip('-')
            while '--' in slug:
                slug = slug.replace('--', '-')

            def verdict(test=test):
                last = test['results'][-1] if test['results'] else {}
                error = (last.get('error') or {}).get('message', '')
                attachments = [item.get('path') for item in last.get('attachments', []) if item.get('path')]
                require(last.get('status') == 'passed', f'{last.get("status")}: {error[:400]} artifacts: {attachments}')
                return f'{last.get("duration", 0)} ms'

            self.case(f'browser.{area}.{condition}.{slug}', verdict, requires=['browser.flows'])

    def stopped_cleanly(self):
        code = getattr(self, 'stop_code', None)
        require(code == 0, f'pacs_server exited {code} on SIGTERM')
        return 'pacs_server exited 0 on SIGTERM'

    def requirement_results(self):
        status = {row['id']: row['status'] for row in self.results}
        results = {}
        for requirement, prefix in (('requirement.frontend.positive', '.positive.'), ('requirement.frontend.negative', '.negative.')):
            matched = [case for case in status if case.startswith('browser.') and prefix in case]
            results[requirement] = dict(cases=len(matched), passed=sum(status[case] == 'passed' for case in matched),
                                        status='unexecuted' if not matched else 'passed' if all(status[case] == 'passed' for case in matched) else 'failed')
        return results

    def run_all(self):
        self.case('browser.tools', self.tools, phase='prerequisite')
        self.case('browser.tokens', self.tokens, requires=['browser.tools'], phase='prerequisite')
        self.case('browser.fixtures', self.fixtures, requires=['browser.tools'], phase='fixture_preparation')
        self.case('browser.web-build', self.web_build, requires=['browser.tools'], phase='prerequisite')
        if all(row['status'] == 'passed' for row in self.results):
            try:
                with self.pacs():
                    self.case('browser.seed', self.seed, requires=['browser.fixtures', 'browser.tokens'])
                    with self.preview():
                        self.case('browser.flows', self.flows, requires=['browser.seed', 'browser.web-build'])
            except ValueError as error:
                self.case('browser.servers', lambda: require(False, str(error)), phase='prerequisite')
            self.case('browser.pacs-stops-on-sigterm', self.stopped_cleanly, requires=['browser.seed'])
        self.browser_cases()
        counts = {status: sum(1 for row in self.results if row['status'] == status) for status in ('passed', 'failed', 'blocked')}
        (self.output/'browser-summary.json').write_text(json.dumps(dict(
            counts=counts, requirement_results=self.requirement_results(),
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
    return BrowserFlows(args).run_all()


if __name__ == '__main__':
    raise SystemExit(main())
