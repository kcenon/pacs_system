#!/usr/bin/env python3
"""Build a pinned DCMTK test system and validate PACS sequentially. Docker only on host."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'validation/dcmtk'))
from report import digest, evaluate, save_summary, validate_manifest


def snapshot(repo, destination):
    """Include tracked changes and new nonignored files, excluding local build artifacts."""
    paths = subprocess.check_output(['git','-C',str(repo),'ls-files','-c','-o','--exclude-standard','-z']).split(b'\0')
    checksum = hashlib.sha256()
    for name in sorted(set(p.decode() for p in paths if p)):
        path = repo/name
        if not path.exists():
            continue  # Deleted tracked files are absent from the build context too.
        if path.is_symlink() or not path.is_file():
            raise ValueError(f'Unexpected source entry: {name}')
        data = path.read_bytes()
        checksum.update(name.encode()+b'\0'+hashlib.sha256(data).digest())
        dest = destination/name
        dest.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(path,dest)
    return checksum.hexdigest()


def compose_config(peer_image, runner_image, profile):
    env = dict(DICOM_PORT='11112',DICOM_TRANSFER_SYNTAX=profile,DICOM_MOVE_TRANSFER_SYNTAX=profile,
               PACS1_AE_TITLE='DCMTK_PACS',PACS2_AE_TITLE='DCMTK_PAC2',STORESCP_AE_TITLE='STORE_SCP',
               TEST_SCU_AE_TITLE='TEST_SCU',MAX_PDU_SIZE='16384',MAX_ASSOCIATIONS='16',
               MAX_STUDIES='200',MAX_BYTES_PER_STUDY='1024mb',GENERATE_TEST_DATA='false',
               GENERATE_PIXEL_DATA='true')
    services = {}
    for name,ae,vol,template in [('pacs-server','DCMTK_PACS','primary','primary'),
                                 ('pacs-server-2','DCMTK_PAC2','secondary','secondary')]:
        services[name] = dict(image=peer_image,environment=env | dict(ROLE='pacs-server',AE_TITLE=ae,
            CONFIG_TEMPLATE=f'/etc/dcmtk/dcmqrscp-{template}.cfg.template',STORAGE_DIR='/dicom/db'),
            volumes=[f'{vol}:/dicom/db'],healthcheck=dict(test=['CMD','echoscu','-aec',ae,'localhost','11112'],
                interval='2s',timeout='3s',retries=60),mem_limit='512m',cap_drop=['ALL'])
    services['storescp-receiver'] = dict(image=peer_image,environment=env | dict(ROLE='storescp',
        AE_TITLE='STORE_SCP',STORAGE_DIR='/dicom/received'),volumes=['received:/dicom/received'],
        healthcheck=dict(test=['CMD','echoscu','-aec','STORE_SCP','localhost','11112'],
                         interval='2s',timeout='3s',retries=60))
    services['mwl-server'] = dict(image=peer_image,environment=env | dict(ROLE='worklist',
        AE_TITLE='DCMTK_WLM',WLM_AE_TITLE='DCMTK_WLM',WLM_STATION_AE='MODALITY01'),
        healthcheck=dict(test=['CMD','echoscu','-aec','DCMTK_WLM','localhost','11112'],
                         interval='2s',timeout='3s',retries=60))
    services['test-client'] = dict(image=peer_image,user='root',environment=env | dict(ROLE='test-client',
        AE_TITLE='TEST_SCU',GENERATE_TEST_DATA='true',TEST_DATA_DIR='/dicom/testdata'),
        volumes=['fixtures:/dicom/testdata'],healthcheck=dict(
            test=['CMD','test','-f','/tmp/dcmtk-test-client.ready'],interval='1s',timeout='2s',retries=120))
    runner_env = env | dict(PACS_HOST='pacs-server',PACS_PORT='11112',PACS_AE_TITLE='DCMTK_PACS',
        PACS2_HOST='pacs-server-2',PACS2_PORT='11112',STORESCP_HOST='storescp-receiver',STORESCP_PORT='11112',
        STORESCP_STORAGE_DIR='/dicom/received',WLM_HOST='mwl-server',WLM_PORT='11112',
        WLM_AE_TITLE='DCMTK_WLM',WLM_STATION_AE='MODALITY01',AE_TITLE='TEST_SCU',TEST_DATA_DIR='/dicom/testdata')
    services['runner'] = dict(image=runner_image,profiles=['validation'],environment=runner_env,
        volumes=['fixtures:/dicom/testdata:ro','primary:/dicom/primary-db:ro',
                 'secondary:/dicom/secondary-db:ro','received:/dicom/received'])
    return dict(services=services,volumes={k:{} for k in ['primary','secondary','received','fixtures']})


class Driver:
    def __init__(self,args):
        self.args = args
        self.output = Path(args.output).resolve()
        if self.output.is_relative_to(ROOT):
            raise ValueError('Choose an output directory outside the source tree')
        self.output.mkdir(parents=True,exist_ok=False)
        self.reports,self.errors = [],[]
        self.manifest = json.loads((ROOT/'validation/dcmtk/features.json').read_text())
        self.lock = json.loads((ROOT/'validation/dcmtk/lock.json').read_text())
        validate_manifest(self.manifest)
        if self.manifest['profiles'] != self.lock['profiles']:
            raise ValueError('Profile manifest and peer lock differ')
        for filename in ('features.json', 'lock.json'):
            shutil.copy2(ROOT/'validation/dcmtk'/filename,self.output/filename)
        self.provenance = dict(source_revision=subprocess.check_output(
            ['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip(),
            source_digest='',peer_revision=self.lock['revision'],
            manifest_digest=digest(ROOT/'validation/dcmtk/features.json'))

    def command(self,args,log,timeout=180,allowed=(0,)):
        with (self.output/log).open('a') as stream:
            stream.write('COMMAND '+json.dumps([str(a) for a in args])+'\n')
            stream.flush()
            process = subprocess.Popen([str(a) for a in args],stdout=stream,stderr=subprocess.STDOUT,start_new_session=True)
            try: code = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid,signal.SIGKILL)
                process.wait()
                code = 124
            stream.write(f'\nEXIT {code}\n')
        if code not in allowed:
            raise RuntimeError(f'{log}: exit {code}')
        return code

    def image_label(self,image,key):
        data = json.loads(subprocess.check_output(['docker','image','inspect',image],text=True))[0]
        return (data['Config'].get('Labels') or {}).get(key)

    def profile(self,profile,peer,runner):
        print(f'Validating {profile}',flush=True)
        name = 'pacs-validation-'+uuid.uuid4().hex[:12]
        directory = self.output/profile
        directory.mkdir()
        compose = directory/'compose.json'
        compose.write_text(json.dumps(compose_config(peer,runner,profile),indent=2)+'\n')
        dc = ['docker','compose','-p',name,'-f',str(compose)]
        container = name+'-runner'
        try:
            self.command([*dc,'up','-d','--no-build','--wait','--wait-timeout','150'],f'{profile}/startup.log',180)
            self.command([*dc,'run','--no-deps','--name',container,'runner','--profile',profile,
                          '--source-revision',self.provenance['source_revision'],
                          '--source-digest',self.provenance['source_digest'],
                          *(['--interop-only'] if self.args.interop_only else [])],f'{profile}/runner.log',
                         self.args.timeout,allowed=(0,1))
        except Exception as exc:
            self.errors.append(f'{profile}: {exc}')
        finally:
            for command,log in [([*dc,'logs','--no-color'],f'{profile}/peers.log'),
                (['docker','cp',f'{container}:/results/run',str(directory/'evidence')],f'{profile}/copy.log')]:
                try: self.command(command,log)
                except Exception as exc: self.errors.append(f'{profile}: {exc}')
            if not self.args.keep:
                try: self.command([*dc,'down','--volumes','--remove-orphans','--timeout','5'],f'{profile}/cleanup.log')
                except Exception as exc: self.errors.append(f'{profile}: cleanup failed: {exc}')
            report = directory/'evidence/results.json'
            if report.exists():
                try: self.reports.append(json.loads(report.read_text()))
                except Exception as exc: self.errors.append(f'{profile}: invalid report: {exc}')

    def run(self):
        try:
            self.command(['docker','info'],'docker.log')
            with tempfile.TemporaryDirectory(prefix='pacs-validation-') as temp:
                temp = Path(temp)
                peer_source,context = temp/'dcmtk',temp/'context'
                context.mkdir()
                self.provenance['source_digest'] = snapshot(ROOT,context/'pacs_system')
                shutil.copytree(context/'pacs_system/validation/dcmtk',context/'harness')
                shutil.copy2(context/'pacs_system/scripts/run_dcmtk_validation.py',context/'driver.py')
                (context/'.dockerignore').write_text('pacs_system/validation/\npacs_system/scripts/run_dcmtk_validation.py\n')
                self.command(['git','init',str(peer_source)],'checkout.log')
                self.command(['git','-C',str(peer_source),'fetch','--depth','1',self.lock['repository'],self.lock['revision']],'checkout.log')
                self.command(['git','-C',str(peer_source),'checkout','--detach','FETCH_HEAD'],'checkout.log')
                shutil.copytree(peer_source/'tests',context/'dcmtk-tests')
                shutil.copy2(ROOT/'validation/dcmtk/Dockerfile',context/'Dockerfile')
                peer = 'pacs-validation-peer:'+self.lock['revision'][:12]
                if self.image_exists(peer) and self.image_label(peer,'pacs.validation.peer-revision')==self.lock['revision']:
                    pass
                else:
                    self.command(['docker','build','--label','pacs.validation.peer-revision='+self.lock['revision'],
                                  '-t',peer,str(peer_source)],'peer-build.log',2400)
                runner = self.args.runner_image or 'pacs-validation-runner:'+self.provenance['source_digest'][:16]
                if self.args.runner_image:
                    if self.image_label(runner,'pacs.validation.source-digest')!=self.provenance['source_digest']:
                        raise ValueError('Prebuilt runner does not match current source contents')
                    if self.image_label(runner,'pacs.validation.peer-revision')!=self.lock['revision']:
                        raise ValueError('Prebuilt runner uses a different DCMTK revision')
                else:
                    self.command(['docker','build','--build-arg','DCMTK_IMAGE='+peer,
                        '--build-arg',f'BUILD_JOBS={self.args.jobs}',
                        '--label','pacs.validation.source-digest='+self.provenance['source_digest'],
                        '--label','pacs.validation.peer-revision='+self.lock['revision'],
                        '-t',runner,str(context)],'runner-build.log',3600)
                images = json.loads(subprocess.check_output(['docker','image','inspect',peer,runner],text=True))
                (self.output/'images.json').write_text(json.dumps(images,indent=2)+'\n')
                (self.output/'provenance.json').write_text(json.dumps(self.provenance,indent=2)+'\n')
                for profile in (self.args.profiles or self.lock['profiles']):
                    self.profile(profile,peer,runner)
        except Exception as exc:
            self.errors.append(str(exc))
        summary = evaluate(self.manifest,self.reports,self.provenance)
        summary['validation_errors'] += self.errors
        if self.errors: summary['status']='failed'
        save_summary(self.output,summary)
        print((self.output/'summary.txt').read_text(),flush=True)
        print(f'Evidence: {self.output}',flush=True)
        return int(summary['status']!='passed')

    @staticmethod
    def image_exists(image):
        return subprocess.run(['docker','image','inspect',image],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode==0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',default=str(Path(tempfile.gettempdir())/('pacs-validation-'+uuid.uuid4().hex[:12])))
    parser.add_argument('--profiles',nargs='+',choices=['uncompressed','jpeg-lossless','jpeg-ls-lossless','rle-lossless'],
                        help='Diagnostic subset; omitted profiles fail the complete gate')
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--timeout',type=int,default=2400,help='Seconds allowed for each validation container')
    parser.add_argument('--interop-only',action='store_true',help='Diagnostic subset; omitted components fail the gate')
    parser.add_argument('--runner-image',help='Reuse only an image whose provenance labels match this source')
    parser.add_argument('--keep',action='store_true',help='Keep this run\'s containers and synthetic volumes for debugging')
    args = parser.parse_args()
    if args.jobs<1 or args.timeout<1:
        parser.error('--jobs and --timeout must be positive')
    if args.profiles and len(set(args.profiles))!=len(args.profiles):
        parser.error('Duplicate profiles are not allowed')
    return Driver(args).run()


if __name__=='__main__':
    raise SystemExit(main())
