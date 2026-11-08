"""Pinned scene preparation and verified, contained LOC SHELL installation."""
import argparse
import ftplib
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import operations
from layout import temporary_directory

REPOSITORY = Path(__file__).resolve().parents[1]
DESTINATION = '/LOCSHELL/ISLAND'
PRESERVE = ['VOLUME.CFG', 'VOLUME.BAK', 'VOLSET.TMP']
REQUIRED = {'SCENE.EGA','PALETTE.BIN','MOON.BIN','SKY.MSK','CLOUD.BIN','CLOUD.MOT',
            'FIRE.BIN','STAR1.BIN','STAR2.BIN','SONG2.OPL','SONG3.OPL','SONG4.OPL',
            'TWINKLE.BIN','DETAILS.BIN','SCENE.CFG','PLAYLIST.CFG','CREDITS.TXT'}


def parser(sub):
    scene = sub.add_parser('scene', help='Install a pinned screensaver pack inside LOCSHELL')
    verbs = scene.add_subparsers(dest='scene_command', required=True)
    install = verbs.add_parser('install')
    install.add_argument('name', choices=['island'])
    install.add_argument('--dry-run', action='store_true')
    install.add_argument('--select', action='store_true', help='Save Island as the selected saver')
    install.add_argument('--preview', action='store_true', help='Queue a preview after clients disconnect')


def checked(command, directory, progress=False):
    # The CLI captures diagnostics in StringIO for --json. Pass pipes to child
    # processes rather than requiring those Python streams to expose fileno().
    result=subprocess.run(command,cwd=directory,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    if progress and result.stdout:print(result.stdout,end='',file=sys.stderr)
    if result.stderr:print(result.stderr,end='',file=sys.stderr)
    if result.returncode:
        raise RuntimeError('Pinned Island preparation failed: '+(result.stderr or result.stdout or 'command exited '+str(result.returncode)).strip()[-2000:])
    return result.stdout


def prepare_pack(destination, repository=REPOSITORY):
    """Use the repository-pinned source; never reset someone's modified checkout."""
    repository = Path(repository)
    module = repository / 'third_party/dos-scenes'
    pin = checked(['git','ls-tree','HEAD','third_party/dos-scenes'],repository).split()[2]
    if not (module / 'tools/pack_island.py').is_file():
        checked(['git','submodule','update','--init','--','third_party/dos-scenes'],repository,progress=True)
    actual = checked(['git','rev-parse','HEAD'],module).strip()
    if actual != pin:
        raise RuntimeError('Scene checkout differs from the repository pin; it was not overwritten')
    checked(['git','diff','--exit-code',pin,'--','tools/pack_island.py','tools/scene_format.py','scenes/island'],module)
    checked([sys.executable,str(module/'tools/pack_island.py'),'--output',str(destination)],module,progress=True)
    files = {p.name for p in Path(destination).iterdir() if p.is_file()}
    if files != REQUIRED:
        raise RuntimeError('Incomplete or unexpected Island pack')
    return pin


def exists(api, ftp, path):
    return operations.maybe(api,ftp,path)


def install_pack(a, api, pack, pin):
    pack = Path(pack)
    if {p.name for p in pack.iterdir() if p.is_file()} != REQUIRED:
        raise ValueError('Expected the complete pinned Island pack')
    before = {}
    with api.connect(a) as ftp:
        capabilities = ftp.sendcmd('SITE AGENT').split()
        if 'layout=2' not in capabilities:
            raise RuntimeError('This server needs the self-contained LOCSHELL layout upgrade first')
        if a.select and 'saverselect=1' not in capabilities:
            raise RuntimeError('This server cannot save a remote screensaver selection')
        # Check pending jobs before any mutation; never compete with a handoff.
        if exists(api,ftp,'/LOCSHELL/NET/JOBS/LOCK/ID.TXT') is not None:
            raise RuntimeError('A managed job is pending; inspect that job before installing a scene')
        try:
            ftp.cwd('/LOCSHELL/NET/JOBS/LOCK')
        except ftplib.error_perm as error:
            if not str(error).startswith('550'):raise
        else:
            raise RuntimeError('A managed job directory is pending or incomplete')
        for name in PRESERVE:
            before[name] = exists(api,ftp,DESTINATION+'/'+name)
        if not a.dry_run:
            try:
                ftp.cwd(DESTINATION)
            except ftplib.error_perm as error:
                if not str(error).startswith('550'):
                    raise
                ftp.mkd(DESTINATION)
    with temporary_directory(prefix='island-manifest-') as temp:
        manifest = Path(temp) / 'release.json'
        files = [{'source':str(pack/name),'destination':DESTINATION+'/'+name,
                  'sha256':hashlib.sha256((pack/name).read_bytes()).hexdigest()} for name in sorted(REQUIRED)]
        manifest.write_text(json.dumps({'name':'Pinned Island screensaver '+pin[:12],
                                       'preserve':[DESTINATION+'/'+name for name in PRESERVE],
                                       'files':files},indent=2))
        release = operations.deploy(SimpleNamespace(credentials=a.credentials,manifest=manifest,dry_run=a.dry_run),api)
    result = {'state':release['state'],'scene':'island','revision':pin,'destination':DESTINATION,
              'files':len(files),'bytes':sum((pack/name).stat().st_size for name in REQUIRED),
              'release':release['id'],'volume_preserved':True,'runtime_verified':False}
    if not a.dry_run:
        with api.connect(a) as ftp:
            # Independent full payload readback, including unchanged targets.
            for row in files:
                if hashlib.sha256(api.read(ftp,row['destination'])).hexdigest()!=row['sha256']:
                    raise RuntimeError('Final scene readback mismatch; inspect release '+release['id'])
            for name,data in before.items():
                if exists(api,ftp,DESTINATION+'/'+name)!=data:
                    raise RuntimeError('Scene volume setting changed during installation; inspect release '+release['id'])
            if a.select:
                result['selection'] = ftp.sendcmd('SITE SAVER SELECT island')
            if a.preview:
                result['preview'] = ftp.sendcmd('SITE SAVER PREVIEW island')
        operations.history(api,'scene-installed',scene='island',revision=pin,release=release['id'],runtime_verified=False)
    return result


def handle(a, api):
    with temporary_directory(prefix='locshell-island-') as temp:
        pack = Path(temp) / 'pack'
        pin = prepare_pack(pack)
        return install_pack(a,api,pack,pin)
