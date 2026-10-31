#!/usr/bin/env python3
"""Run PACS components and real DCMTK peers; preserve failed and absent evidence."""
import argparse
from contextlib import contextmanager
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import socket
import subprocess
import time
import uuid

import pydicom
from report import digest, junit_counts, passed, validate_manifest, write_junit

HERE = Path(__file__).resolve().parent
CODECS = {
    'uncompressed': (None, None, None, '+xe', None),
    'jpeg-lossless': (['dcmcjpeg', '+e1'], 'dcmdjpeg', '1.2.840.10008.1.2.4.70', '+xs', '-xs'),
    'jpeg-ls-lossless': (['dcmcjpls', '+el'], 'dcmdjpls', '1.2.840.10008.1.2.4.80', '+xt', '-xt'),
    'rle-lossless': (['dcmcrle'], 'dcmdrle', '1.2.840.10008.1.2.5', '+xr', '-xr'),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


class Validation:
    def __init__(self, args):
        self.args = args
        self.output = Path(args.output)
        self.output.mkdir(parents=True, exist_ok=False)
        self.bin = Path(args.build_dir)/'bin'
        self.manifest = json.loads((HERE/'features.json').read_text())
        validate_manifest(self.manifest)
        self.results = []
        self.current = 'setup'
        self.data, self.paths = {}, {}
        self.codec = CODECS[args.profile]

    def command(self, args, timeout=30, allowed=(0,), env=None):
        args = [str(x) for x in args]
        with (self.output/f'{self.current}.log').open('a') as log:
            log.write('COMMAND '+json.dumps(args)+'\n')
            log.flush()
            p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 start_new_session=True, env=env)
            try:
                stdout, stderr = p.communicate(timeout=timeout)
                rc = p.returncode
            except subprocess.TimeoutExpired:
                os.killpg(p.pid, signal.SIGKILL)
                stdout, stderr = p.communicate()
                rc = 124
            log.write(stdout.decode(errors='replace')+stderr.decode(errors='replace'))
            log.write(f'\nEXIT {rc}\n')
        require(rc in allowed, f'exit {rc}: {args[0]}; see {self.current}.log')
        return stdout.decode(errors='replace')

    def case(self, name, action, requires=()):
        self.current = name
        started = time.monotonic()
        result = dict(id=name, status='failed', tests=1, failed=1, skipped=0)
        completed = {r['id']:r for r in self.results}
        missing = [key for key in requires if not passed(completed.get(key))]
        if missing:
            result.update(status='blocked',tests=0,failed=0,
                          detail='Prerequisite failed or absent: '+', '.join(missing),seconds=0)
            self.results.append(result)
            self.save()
            print(f'{name}: blocked: {result["detail"]}',flush=True)
            return
        try:
            detail = action()
            result.update(status='passed', failed=0, detail='completed')
            if isinstance(detail, dict):
                result.update(detail)
                require(detail.get('tests', 0) > 0, detail.get('detail','No test cases executed'))
                require(not detail.get('failed') and not detail.get('skipped'),
                        detail.get('detail',f'failed/skipped cases: {detail}'))
            elif isinstance(detail, str):
                result['detail'] = detail
        except Exception as exc:
            # Preserve actual assertion/skip counts when a component supplied them.
            # A skipped suite fails the gate without inventing an assertion failure.
            result.update(status='failed', detail=str(exc))
        result['seconds'] = round(time.monotonic()-started, 3)
        self.results.append(result)
        self.save()
        print(f'{name}: {result["status"]}: {result["detail"]}', flush=True)

    def save(self):
        payload = dict(schema_version=1, profile=self.args.profile,
                       source_revision=self.args.source_revision, source_digest=self.args.source_digest,
                       peer_revision=json.loads((HERE/'lock.json').read_text())['revision'],
                       manifest_digest=digest(HERE/'features.json'), cases=self.results)
        (self.output/'results.json').write_text(json.dumps(payload,indent=2)+'\n')
        write_junit(self.output/'results.xml', self.results)

    def preflight(self):
        compressed = self.args.profile != 'uncompressed'
        # dcmqrscp forwards precompressed objects but does not encode native objects
        # on demand. Its native-fixture C-MOVE suite belongs to the native profile.
        script = 'test-transfer-syntax.sh' if compressed else 'test-all.sh'
        text = self.command(['bash', Path(self.args.peer_tests)/script], timeout=240)
        pattern = r'Transfer Syntax Results: (\d+)/(\d+) passed' if compressed else r'Total:\s+(\d+)/(\d+) passed'
        counts = re.findall(pattern, text)
        require(len(counts)==1, 'Peer aggregate summary missing/ambiguous')
        passed,total = map(int,counts[0])
        require(passed==total and total>=(16 if compressed else 88), 'Incomplete DCMTK baseline suite')
        for path in re.findall(r'Transfer syntax evidence: (/tmp/transfer-syntax-\S+)',text):
            if Path(path).is_dir():
                shutil.copytree(path,self.output/'peer-transfer-evidence')
        return dict(tests=total, failed=0, skipped=0,
                    detail=('Peer compressed store/move/decoded integrity suite' if compressed else
                            'Peer baseline; optional TLS/ad-hoc overlay cases are not counted'))

    def peer_tls(self):
        text = self.command(['bash',Path(self.args.peer_tests)/'test-tls-runtime.sh'], timeout=90)
        require('TLS Runtime Results: 3/3 passed' in text, 'Independent TLS evidence missing')
        return dict(tests=3, failed=0, skipped=0)

    def fixtures(self):
        root = '2.25.'+str(uuid.uuid4().int)
        originals = self.output/'originals'
        inputs = self.output/'inputs'
        originals.mkdir()
        inputs.mkdir()
        for group,mod in enumerate(('ct','mr','cr'),1):
            self.data[mod],self.paths[mod] = [],[]
            sources = sorted((Path(self.args.fixtures)/mod).glob('*.dcm'))
            require(len(sources)=={'ct':5,'mr':6,'cr':2}[mod], f'{mod}: fixture count differs from locked peer')
            for i,path in enumerate(sources,1):
                ds = pydicom.dcmread(path)
                ds.StudyInstanceUID = f'{root}.{group}'
                ds.SeriesInstanceUID = f'{root}.{group}.1'
                ds.SOPInstanceUID = f'{root}.{group}.1.{i}'
                ds.file_meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
                ds.PatientID = 'PACS_VALIDATE_'+mod.upper()
                native,wire = originals/f'{mod}-{i}.dcm',inputs/f'{mod}-{i}.dcm'
                ds.save_as(native)
                if self.codec[0]:
                    self.command([*self.codec[0],'+un',native,wire])
                    require(str(pydicom.dcmread(wire).file_meta.TransferSyntaxUID)==self.codec[2],
                            'Compressor did not produce the required transfer syntax')
                else:
                    wire.write_bytes(native.read_bytes())
                self.data[mod].append(ds)
                self.paths[mod].append(wire)
        (self.output/'fixture-manifest.json').write_text(json.dumps(
            {m:[self.signature(ds) for ds in values] for m,values in self.data.items()},indent=2)+'\n')
        return '13 new-UID CT/MR/CR instances with nonempty PixelData'

    @staticmethod
    def signature(ds):
        fields = ['SOPClassUID','SOPInstanceUID','StudyInstanceUID','SeriesInstanceUID','PatientID',
                  'PatientName','StudyDate','StudyTime','AccessionNumber',
                  'Modality','Rows','Columns','BitsAllocated','BitsStored','PixelRepresentation',
                  'PhotometricInterpretation','SamplesPerPixel']
        require(bool(ds.get('PixelData')), 'Missing or empty PixelData')
        return {k:str(ds.get(k,'')) for k in fields} | {
            'pixel_size':len(ds.PixelData),'pixel_sha256':hashlib.sha256(ds.PixelData).hexdigest()}

    def verify(self, directory, expected, require_syntax=True):
        actual = {}
        decoded = self.output/'decoded'/self.current
        decoded.mkdir(parents=True,exist_ok=True)
        for i,path in enumerate(sorted(Path(directory).rglob('*'))):
            if not path.is_file():
                continue
            ds = pydicom.dcmread(path)
            syntax = str(ds.file_meta.TransferSyntaxUID)
            if require_syntax and self.codec[2]:
                require(syntax==self.codec[2], f'Transfer syntax fallback: {syntax}')
            if ds.file_meta.TransferSyntaxUID.is_compressed:
                decoders = {v[2]:v[1] for v in CODECS.values() if v[2]}
                require(syntax in decoders, f'No independent decoder for {syntax}')
                dest = decoded/f'{i}.dcm'
                self.command([decoders[syntax],path,dest])
                ds = pydicom.dcmread(dest)
            uid = str(ds.SOPInstanceUID)
            require(uid not in actual, f'Duplicate received UID {uid}')
            actual[uid] = self.signature(ds)
        wanted = {str(ds.SOPInstanceUID):self.signature(ds) for ds in expected}
        require(actual==wanted, f'UID/metadata/full PixelData mismatch ({len(actual)}/{len(wanted)} objects)')
        return f'{len(actual)} exact instances and full PixelData hashes match'

    def get_oracle(self, host, ae, study, directory):
        directory.mkdir()
        self.command(['getscu','-v','-S',self.codec[3],'-to','5','-ta','5','-td','10',
                      '-aet','TEST_SCU','-aec',ae,'-od',directory,host,'11112',
                      '-k','QueryRetrieveLevel=STUDY','-k',f'StudyInstanceUID={study}'])

    def query(self, mod):
        ds = self.data[mod][0]
        output = self.command([self.bin/'query_scu','pacs-server','11112','DCMTK_PACS',
                               '--level','IMAGE','--study-uid',ds.StudyInstanceUID,
                               '--series-uid',ds.SeriesInstanceUID,'--format','json'])
        payload = json.loads(output)
        actual = [item['sopInstanceUid'] for item in payload['results']]
        expected = [str(ds.SOPInstanceUID) for ds in self.data[mod]]
        require(sorted(actual)==sorted(expected) and payload['resultCount']==len(expected),
                f'{mod}: exact query UID set/count mismatch')

    def integrity(self, mod):
        directory = self.output/('oracle-'+mod)
        self.get_oracle('pacs-server','DCMTK_PACS',self.data[mod][0].StudyInstanceUID,directory)
        return self.verify(directory,self.data[mod])

    def secondary(self):
        ds = copy.deepcopy(self.data['ct'][0])
        ds.StudyInstanceUID = '2.25.'+str(uuid.uuid4().int)
        ds.SeriesInstanceUID = ds.StudyInstanceUID+'.1'
        ds.SOPInstanceUID = ds.SeriesInstanceUID+'.1'
        ds.file_meta.MediaStorageSOPInstanceUID = ds.SOPInstanceUID
        native,wire = self.output/'secondary-native.dcm',self.output/'secondary.dcm'
        ds.save_as(native)
        if self.codec[0]: self.command([*self.codec[0],'+un',native,wire])
        else: wire.write_bytes(native.read_bytes())
        self.command([self.bin/'store_scu','-aec','DCMTK_PAC2','pacs-server-2','11112',wire])
        directory = self.output/'secondary-received'
        self.get_oracle('pacs-server-2','DCMTK_PAC2',ds.StudyInstanceUID,directory)
        return self.verify(directory,[ds])

    def move_scu(self):
        ds = self.data['ct'][0]
        before = json.loads(self.command([self.bin/'query_scu','pacs-server-2','11112','DCMTK_PAC2',
                                          '--study-uid',ds.StudyInstanceUID,'--format','json'],allowed=(0,1)))
        # query_scu documents exit 1 for an empty successful result. The required
        # empty JSON result distinguishes this precondition from a command error.
        require(before['resultCount']==0 and before['results']==[],
                'Destination contains the study before C-MOVE')
        self.command([self.bin/'retrieve_scu','pacs-server','11112','DCMTK_PACS','--mode','move',
                      '--dest-ae','DCMTK_PAC2','--study-uid',ds.StudyInstanceUID,'--no-progress'])
        directory = self.output/'moved-ct'
        self.get_oracle('pacs-server-2','DCMTK_PAC2',ds.StudyInstanceUID,directory)
        return self.verify(directory,self.data['ct'])

    def get_scu(self, mod):
        dest = self.output/('pacs-get-'+mod)
        dest.mkdir()
        self.command([self.bin/'retrieve_scu','pacs-server','11112','DCMTK_PACS','--mode','get',
                      '--study-uid',self.data[mod][0].StudyInstanceUID,'--output',dest,'--no-progress'])
        return self.verify(dest,self.data[mod])

    def mwl(self, sequences=False):
        result = json.loads(self.command([self.bin/'worklist_scu','-aec','DCMTK_WLM','-o','json',
                                          'mwl-server','11112']))
        items = result['worklistItems']
        require(result['resultCount']==3 and {i['patient']['id'] for i in items}=={'PAT001','PAT002','PAT003'},
                'MWL patient IDs/count mismatch')
        if sequences:
            for item in items:
                step = item['scheduledProcedureStep']
                require(step['stationAETitle']=='MODALITY01' and step['modality'] in ('CT','MR','CR')
                        and step['stepId'] and step['startDate'], 'Missing/malformed ScheduledProcedureStepSequence')

    @contextmanager
    def server(self, args, port):
        with (self.output/f'{self.current}-server.log').open('w') as log:
            p = subprocess.Popen([str(a) for a in args],stdout=log,stderr=subprocess.STDOUT,start_new_session=True)
            try:
                ready = False
                for _ in range(100):
                    require(p.poll() is None, f'Server exited {p.returncode}; see server log')
                    try:
                        with socket.create_connection(('127.0.0.1',port),.1):
                            ready = True
                            break
                    except OSError:
                        time.sleep(.1)
                require(ready, 'Server did not open its port')
                yield
            finally:
                if p.poll() is None:
                    os.killpg(p.pid,signal.SIGTERM)
                    try: p.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        os.killpg(p.pid,signal.SIGKILL)
                        p.wait()

    def echo_scp(self):
        with self.server([self.bin/'echo_scp','22501','PACS_ECHO'],22501):
            self.command(['echoscu','-aec','PACS_ECHO','127.0.0.1','22501'])

    def store_scp(self):
        dest = self.output/'inbound-store'
        dest.mkdir()
        with self.server([self.bin/'store_scp','22502','PACS_STORE','--storage-dir',dest],22502):
            flags = [self.codec[4]] if self.codec[4] else []
            self.command(['storescu',*flags,'-aec','PACS_STORE','127.0.0.1','22502',*self.paths['ct']])
            return self.verify(dest,self.data['ct'])

    def qr_scp(self, mode):
        # Independent DCMTK SCUs exercise the actual CLI, including its file index.
        with self.server([self.bin/'qr_scp','22503','PACS_QR','--storage-dir',self.output/'inputs',
                          '--index-db',self.output/f'{mode}.db','--peer','STORE_SCP:storescp-receiver:11112'],22503):
            ds = self.data['ct'][0]
            keys = ['-k','QueryRetrieveLevel=STUDY','-k',f'StudyInstanceUID={ds.StudyInstanceUID}']
            if mode=='query':
                dest = self.output/'query-responses'
                dest.mkdir()
                self.command(['findscu','-S','-X','-od',dest,'-aec','PACS_QR','127.0.0.1','22503',*keys])
                responses = [pydicom.dcmread(p) for p in dest.iterdir()]
                require(len(responses)==1 and str(responses[0].StudyInstanceUID)==str(ds.StudyInstanceUID),
                        'C-FIND SCP returned wrong study set')
            elif mode=='get':
                dest = self.output/'scp-get-received'
                dest.mkdir()
                self.command(['getscu','-S',self.codec[3],'-od',dest,'-aec','PACS_QR','127.0.0.1','22503',*keys])
                return self.verify(dest,self.data['ct'])
            else:
                dest = Path('/dicom/received')
                # This is a fresh, project-scoped synthetic receiver volume. Keep directories
                # because storescp caches study directories between associations.
                for path in dest.rglob('*'):
                    if path.is_file(): path.unlink()
                self.command(['movescu','-S','-aec','PACS_QR','-aem','STORE_SCP','127.0.0.1','22503',*keys])
                return self.verify(dest,self.data['ct'])

    def tls_scp(self):
        cert = self.output/'tls-certs'
        self.command(['bash','/usr/local/bin/gen-certs.sh',cert])
        secure = ['echoscu','+tls',cert/'client-key.pem',cert/'client-cert.pem',
                  '+cf',cert/'ca-cert.pem','-aec','PACS_TLS','127.0.0.1','22504']
        with self.server([self.bin/'secure_echo_scp','22504','PACS_TLS','--cert',cert/'server-cert.pem',
                          '--key',cert/'server-key.pem','--ca',cert/'ca-cert.pem'],22504):
            self.command(secure)
            self.command(['echoscu','-to','2','-ta','2','-aec','PACS_TLS','127.0.0.1','22504'],allowed=(1,))
            self.command(secure)

    def component(self, spec):
        xml = self.output/(spec['id']+'.xml')
        if spec.get('kind') == 'discovery':
            text = self.command(['ctest','--test-dir',self.args.build_dir,'--show-only=json-v1'])
            inventory = json.loads(text)
            (self.output/'ctest-inventory.json').write_text(text)
            tests = inventory['tests']
            require(bool(tests), 'CTest registered no tests')
            binaries = {s['binary'] for s in self.manifest['components'] if 'binary' in s}
            unmapped = []
            for test in tests:
                labels = next((p['value'] for p in test.get('properties',[]) if p['name']=='LABELS'),[])
                command = test.get('command',[])
                if (not command or (Path(command[0]).name not in binaries and 'cli' not in labels
                    and test['name'] not in ('pacs_factory_result_compat','storage_boundary_guard'))):
                    unmapped.append(test['name'])
            require(not unmapped, 'CTest cases outside the manifest: '+str(unmapped))
            return dict(tests=1,failed=0,skipped=0,detail=f'{len(tests)} registered CTest cases mapped to runners')
        env = os.environ.copy()
        env['PACS_TEST_CERT_DIR'] = str(self.output/'pacs-certs')
        failure = None
        try:
            command = (['ctest','--test-dir',self.args.build_dir,'--output-on-failure','--no-tests=error',
                        '--timeout','60','--output-junit',str(xml),*spec['args']] if spec.get('kind')=='ctest'
                       else [self.bin/spec['binary'],*spec['args'],'--warn','UnmatchedTestSpec',
                             '--reporter','console','--reporter',f'junit::out={xml}'])
            self.command(command,
                         timeout=spec['timeout_seconds'],env=env)
        except Exception as exc:
            failure = str(exc)
        try:
            counts = junit_counts(xml)
        except Exception as exc:
            return dict(tests=0,failed=1,skipped=0,status='failed',
                        detail=f'{failure or "No process failure"}; invalid/missing JUnit: {exc}')
        if failure:
            counts.update(status='failed', failed=max(1,counts['failed']), detail=failure)
        return counts

    def run(self):
        pins = Path('/workspace/dependency-pins.json')
        if pins.exists():
            shutil.copy2(pins,self.output/'dependency-pins.json')
        self.save()
        self.case('peer.preflight',self.preflight)
        self.case('peer.tls',self.peer_tls)
        self.case('fixtures',self.fixtures)
        self.case('echo.primary',lambda:self.command([self.bin/'echo_scu','-aec','DCMTK_PACS','pacs-server','11112']),
                  requires=['peer.preflight'])
        self.case('echo.secondary',lambda:self.command([self.bin/'echo_scu','-aec','DCMTK_PAC2','pacs-server-2','11112']),
                  requires=['peer.preflight'])
        self.case('store.primary',lambda:self.command([self.bin/'store_scu','-aec','DCMTK_PACS','pacs-server',
                  '11112',*[p for paths in self.paths.values() for p in paths]]),requires=['fixtures','echo.primary'])
        for mod in ('ct','mr','cr'):
            self.case('query.'+mod,lambda mod=mod:self.query(mod),requires=['store.primary'])
            self.case('integrity.'+mod,lambda mod=mod:self.integrity(mod),requires=['store.primary'])
        self.case('store.secondary',self.secondary,requires=['fixtures','echo.secondary'])
        self.case('move.scu',self.move_scu,requires=['store.primary','echo.secondary'])
        for mod in ('ct','mr','cr'):
            self.case('get.'+mod,lambda mod=mod:self.get_scu(mod),requires=['store.primary'])
        self.case('mwl.scu',self.mwl,requires=['peer.preflight'])
        self.case('mwl.sequence',lambda:self.mwl(True),requires=['mwl.scu'])
        self.case('echo.scp',self.echo_scp,requires=['peer.preflight'])
        self.case('store.scp',self.store_scp,requires=['fixtures','peer.preflight'])
        for mode in ('query','get','move'):
            self.case(mode+'.scp',lambda mode=mode:self.qr_scp(mode),requires=['fixtures','peer.preflight'])
        self.case('tls.scp',self.tls_scp,requires=['peer.tls'])
        if self.args.profile=='uncompressed' and not self.args.interop_only:
            self.current = 'certificate-setup'
            try:
                self.command(['bash',HERE.parent.parent/'tools/integration_tests/test_data/certs/generate_test_certs.sh',
                              self.output/'pacs-certs'])
            except Exception as exc:
                self.case('certificate-setup',lambda:require(False,str(exc)))
            for spec in self.manifest['components']:
                self.case(spec['id'],lambda spec=spec:self.component(spec))
        self.save()
        return int(any(r['status']!='passed' for r in self.results))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', default='/results/run')
    p.add_argument('--profile', choices=CODECS, default='uncompressed')
    p.add_argument('--build-dir', default='/workspace/build')
    p.add_argument('--peer-tests', default='/peer-tests')
    p.add_argument('--fixtures', default='/dicom/testdata')
    p.add_argument('--source-revision', required=True)
    p.add_argument('--source-digest', required=True)
    p.add_argument('--interop-only', action='store_true', help='Diagnostic subset; cannot pass the full gate')
    args = p.parse_args()
    return Validation(args).run()


if __name__=='__main__':
    raise SystemExit(main())
