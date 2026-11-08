"""Import an existing installation into one LOCSHELL tree without deleting it."""
import hashlib
import json
from pathlib import Path
import re
import secrets
from types import SimpleNamespace
import operations

ROOT='/LOCSHELL'


def parser(sub):
    archive=sub.add_parser('archive-import',help='Move previous application trees into LOCSHELL after switchover')
    archive.add_argument('import_id')
    command=sub.add_parser('import-installation',help='Stage an existing installation into the contained layout')
    command.add_argument('--shell',required=True,help='Previous shell directory, absolute FTP path')
    command.add_argument('--network',required=True,help='Previous network directory, absolute FTP path')
    command.add_argument('--scene',help='Previous scene directory, absolute FTP path')
    command.add_argument('--preferences',required=True,help='Previous preferences filename, within --shell')
    command.add_argument('--rename-file',action='append',default=[],help='Explicit previous filename=canonical filename mapping in startup scripts')
    command.add_argument('--package',type=Path,required=True,help='Package output directory containing package.json')
    command.add_argument('--startup',action='append',default=[],help='Explicit DOS startup file to update after staging')
    command.add_argument('--dry-run',action='store_true')


def path(value):
    value=value.rstrip('/')
    if not value.startswith('/') or any(not re.fullmatch(r'[A-Za-z0-9_~!-]{1,8}(?:\.[A-Za-z0-9_~-]{1,3})?',part) for part in value[1:].split('/')):
        raise ValueError('Use an absolute DOS 8.3 FTP path')
    return value


def maybe(api,ftp,name):return operations.maybe(api,ftp,name)


def mkdir(ftp,name):
    try:ftp.cwd(name)
    except Exception as error:
        if not str(error).startswith('550'):raise
        ftp.mkd(name)


def tree(api,ftp,root,limit=4096,role='network'):
    result={}
    def walk(directory,depth):
        if depth>16:raise RuntimeError('Import directory nesting limit reached')
        rows=[]
        if hasattr(ftp,'retrlines'):
            listing=[];ftp.retrlines('LIST '+directory,listing.append)
            for line in listing:
                fields=line.strip().split(None,8)
                if len(fields)!=9 or fields[0][0] not in 'd-':raise RuntimeError('Unexpected FTP directory listing')
                rows.append((fields[8],fields[0][0]=='d'))
        else:
            for entry in sorted(ftp.nlst(directory)):
                name=entry.replace('\\','/').rsplit('/',1)[-1]
                if name in ('','.','..'):continue
                try:ftp.cwd(directory+'/'+name)
                except api.ftplib.error_perm as error:
                    if not str(error).startswith('550'):raise
                    rows.append((name,False))
                else:rows.append((name,True))
        for name,is_directory in sorted(rows):
            if name in ('','.','..'):continue
            relative=(directory+'/'+name)[len(root)+1:].upper()
            if relative.endswith(('.BAK','.OLD','.ERR','.LOG','.OUT','.NEW','.TMP')):continue
            if role=='shell' and ('/' in relative or is_directory or relative.endswith(('.EXE','.COM','.BAT','.HLP'))):continue
            if role=='network' and is_directory and depth==0 and name.upper() not in ('AGENT','BIN','DRIVERS'):continue
            remote=path(directory+'/'+name)
            if is_directory:walk(remote,depth+1)
            else:
                if len(result)>=limit:raise RuntimeError('Import file limit reached')
                result[remote[len(root)+1:]]=api.read(ftp,remote)
    ftp.cwd(root);walk(root,0)
    return result


def rewrite(data,mapping,aliases=()):
    # Only callers' explicitly chosen DOS path prefixes are changed; binary
    # files, images, memory observations and game content are never rewritten.
    text=data.decode('cp437')
    for old,new in sorted(mapping,key=lambda item:-len(item[0])):
        source='C:'+old.replace('/','\\')
        target='C:'+new.replace('/','\\')
        text=re.sub(re.escape(source)+r'(?=\\|[\s;]|$)',lambda match:target,text,flags=re.I)
        text=re.sub(r'(?<![A-Za-z0-9_:\\])'+re.escape(source[2:])+r'(?=\\|[\s;]|$)',lambda match:target[2:],text,flags=re.I)
    for old,new in aliases:
        text=re.sub(r'(?<![A-Za-z0-9_.])'+re.escape(old)+r'(?=[\s\";]|$)',lambda match:new,text,flags=re.I)
    return text.encode('cp437')


def handle(a,api):
    if a.cmd=='archive-import':return archive_import(a,api)
    sources=[(path(a.shell),ROOT),(path(a.network),ROOT+'/NET')]
    if a.scene:sources.append((path(a.scene),ROOT+'/ISLAND'))
    if any(old.upper()==ROOT or old.upper().startswith(ROOT+'/') for old,new in sources):
        raise ValueError('Import expects previous directories outside the new application tree')
    old_roots=[old.upper() for old,new in sources]
    if any(left==right or left.startswith(right+'/') or right.startswith(left+'/') for i,left in enumerate(old_roots) for right in old_roots[i+1:]):
        raise ValueError('Previous application directories must be separate, non-overlapping trees')
    if not re.fullmatch(r'[A-Za-z0-9_~-]{1,8}\.CFG',a.preferences,re.I):raise ValueError('Invalid preferences filename')
    package=a.package.resolve();description=json.loads((package/'package.json').read_text());payload={}
    for name,digest in description['files'].items():
        target=path(ROOT+'/'+name.replace('\\','/'));data=(package/'LOCSHELL'/name).read_bytes()
        if hashlib.sha256(data).hexdigest()!=digest:raise ValueError('Package hash mismatch: '+name)
        payload[target]=data
    if b'TEST.KEY' in payload.get(ROOT+'/LOCSHELL.EXE',b''):raise ValueError('Production package required')
    if not all(ROOT+'/'+name in payload for name in ['LOCSHELL.EXE','LOCHAND.EXE','LOCSHELL.BAT','LCSMAIN.BAT']):raise ValueError('Incomplete package')
    aliases=[(a.preferences.rsplit('.',1)[0]+'.BAT','LOCSHELL.BAT')]
    for item in getattr(a,'rename_file',[]):
        parts=item.split('=')
        if len(parts)!=2:raise ValueError('Use old filename=new filename')
        path('/'+parts[0]);path('/'+parts[1]);aliases.append(tuple(parts))
    protected={ROOT+'/LIBRARY.TXT',ROOT+'/LOCSHELL.CFG',ROOT+'/MEMORY.TSV',ROOT+'/LEARN.DAT',ROOT+'/RECENT.TXT'}
    import_id='I'+secrets.token_hex(4)[:7].upper();out=api.recipes.BASE/'imports'/import_id
    snapshot={};critical={};startup={}
    with api.connect(a) as ftp:
        # An import is prepared alongside the live installation, never over it.
        try:ftp.cwd(ROOT)
        except api.ftplib.error_perm as error:
            if not str(error).startswith('550'):raise
        else:raise RuntimeError('LOCSHELL already exists; inspect it instead of merging installations blindly')
        try:ftp.cwd(a.network.rstrip('/')+'/JOBS/LOCK')
        except api.ftplib.error_perm as error:
            if not str(error).startswith('550'):raise
        else:raise RuntimeError('An outstanding or incomplete job directory exists; inspect it before import')
        for suffix in ['/JOBS/LOCK/ID.TXT','/AGENT/LOCAL.REQ','/AGENT/LOCAL.ACT']:
            if maybe(api,ftp,a.network.rstrip('/')+suffix) is not None:raise RuntimeError('Resolve the outstanding job/launch before importing')
        if maybe(api,ftp,a.shell.rstrip('/')+'/LEARN.REQ') is not None:raise RuntimeError('Resolve the pending learning run first')
        for old,new in sources:
            contents=tree(api,ftp,old,role='shell' if new==ROOT else 'scene' if new==ROOT+'/ISLAND' else 'network');snapshot[old]=contents
            for name,data in contents.items():
                target=new+'/'+name.upper()
                if new==ROOT:
                    if '/' in name or name.upper().endswith(('.EXE','.COM','.BAT','.HLP')):continue
                    if name.upper().startswith(a.preferences.upper().rsplit('.',1)[0]+'.'):
                        target=ROOT+'/LOCSHELL.'+name.rsplit('.',1)[1]
                    if name.upper() in ['LIBRARY.TXT','MEMORY.TSV','LEARN.DAT','RECENT.TXT',a.preferences.upper()]:critical[old+'/'+name]=data
                    if name.upper()=='LIBRARY.TXT' and rewrite(data,sources)!=data:
                        raise RuntimeError('Catalogue launch paths need an explicit merge before import; original preserved')
                elif new==ROOT+'/NET':
                    if '/LOCK/' in '/'+name.upper()+'/':raise RuntimeError('An outstanding job directory exists')
                    # Completed old job evidence is archived; do not make it active.
                    if name.upper().startswith(('JOBS/','TASKS/')):continue
                # Keep current scene assets/volume from an existing installation.
                if target in payload and new!=ROOT+'/ISLAND' and target not in protected:continue
                if new==ROOT+'/NET' and name.upper().endswith(('.BAT','.CFG','.INI','.TXT')) and b'\0' not in data:
                    data=rewrite(data,sources,aliases if name.upper().endswith('.BAT') else ())
                if target==ROOT+'/LOCSHELL.CFG':
                    first,separator,rest=data.partition(b'\n')
                    previous=a.preferences.rsplit('.',1)[0].upper().encode('ascii')
                    if separator and re.sub(rb'[^A-Z]',b'',first.upper()).startswith(previous):data=b'LOC SHELL 1\r\n'+rest
                payload[target]=data
        for filename in a.startup:
            filename=path(filename);old=api.read(ftp,filename);new=rewrite(old,sources,aliases)
            if new!=old:startup[filename]=(old,new)
        estimate=sum(len(data) for data in payload.values())
        if hasattr(ftp,'sendcmd'):
            free=re.search(r'has (\d+) free bytes',ftp.sendcmd('SITE DISKFREE C'))
            if not free or int(free[1])<estimate+1024*1024:raise RuntimeError('Insufficient or unverified C: space for contained staging')
        report={'state':'dry-run' if a.dry_run else 'prepared','id':import_id,'files':len(payload),'bytes':estimate,
                'sources':[old for old,new in sources],'destination':ROOT,'startup_files':list(startup),
                'critical_sha256':{name:hashlib.sha256(data).hexdigest() for name,data in critical.items()},
                'source_files_deleted':False,'requires_local_start':'C:\\LOCSHELL\\LOCSHELL.BAT'}
        if a.dry_run:return report
        out.mkdir(parents=True,mode=0o700)
        operations.save(out/'import.json',report)
        # Imported active files are backed up on the host before any remote mutation.
        # Full previous trees remain untouched and are archived after switchover.
        for index,(old,contents) in enumerate(snapshot.items()):
            for name,data in contents.items():
                local=out/'before'/str(index)/name;local.parent.mkdir(parents=True,exist_ok=True);local.write_bytes(data)
        operations.save(out/'source-map.json',{'sources':report['sources'],'preferences':a.preferences})
        for name,data in critical.items():
            if api.read(ftp,name)!=data:raise RuntimeError('Source state changed during preparation; nothing installed')
        directories={ROOT,ROOT+'/NET',ROOT+'/NET/JOBS',ROOT+'/NET/DRIVERS',ROOT+'/BACKUP',ROOT+'/BACKUP/'+import_id,ROOT+'/GAMES'}
        for target in payload:
            parent=target.rsplit('/',1)[0]
            while parent and parent!='/':directories.add(parent);parent=parent.rsplit('/',1)[0]
        for directory in sorted(directories,key=lambda name:(name.count('/'),name)):mkdir(ftp,directory)
    releases=[]
    for start in range(0,len(payload),256):
        batch=list(sorted(payload.items()))[start:start+256];files=[]
        for index,(target,data) in enumerate(batch):
            local=out/'payload'/('%04d.bin'%(start+index));local.parent.mkdir(exist_ok=True);local.write_bytes(data)
            files.append({'source':str(local),'destination':target,'sha256':hashlib.sha256(data).hexdigest()})
        manifest=out/('batch-%03d.json'%(start//256));operations.save(manifest,{'name':'Contained LOC SHELL import '+import_id,'files':files})
        release=operations.deploy(SimpleNamespace(credentials=a.credentials,manifest=manifest,dry_run=False),api)
        releases.append(release['id']);report['releases']=releases;operations.save(out/'import.json',report)
    # Update only named bootstrap files, after the complete new payload exists.
    # Their transaction artifacts are inside LOCSHELL as well.
    if startup:
        files=[]
        with api.connect(a) as ftp:
            for index,(name,(old,new)) in enumerate(startup.items()):
                if api.read(ftp,name)!=old:raise RuntimeError('Startup changed during import; payload ready, bootstrap not replaced')
                local=out/('startup-%03d.bat'%index);local.write_bytes(new)
                files.append({'source':str(local),'destination':name,'sha256':hashlib.sha256(new).hexdigest()})
        manifest=out/'startup.json';operations.save(manifest,{'name':'LOC SHELL bootstrap '+import_id,'artifacts':ROOT+'/BACKUP/'+import_id,'files':files})
        release=operations.deploy(SimpleNamespace(credentials=a.credentials,manifest=manifest,dry_run=False),api);releases.append(release['id'])
    with api.connect(a) as ftp:
        for name,data in payload.items():
            if api.read(ftp,name)!=data:raise RuntimeError('Import final readback mismatch: '+name)
    report.update(state='ready-for-local-start',releases=releases,runtime_verified=False,artifacts=str(out))
    operations.save(out/'import.json',report);operations.history(api,'installation-imported',**report)
    return report


def archive_import(a,api):
    if not re.fullmatch(r'I[0-9A-F]{7}',a.import_id):raise ValueError('Invalid import ID')
    journal=api.recipes.BASE/'imports'/a.import_id/'import.json'
    record=json.loads(journal.read_text())
    if record['state']!='ready-for-local-start':raise RuntimeError('Import is not waiting for switchover')
    with api.connect(a) as ftp:
        if 'layout=2' not in ftp.sendcmd('SITE AGENT').split():raise RuntimeError('Start the contained LOC SHELL locally first')
        if maybe(api,ftp,ROOT+'/NET/JOBS/LOCK/ID.TXT') is not None:raise RuntimeError('A new-layout job is pending')
        for name,digest in record.get('critical_sha256',{}).items():
            location=name
            for moved in record.get('archive_moves',[]):
                if moved['state']=='archived' and name.startswith(moved['source']+'/'):location=moved['target']+name[len(moved['source']):]
            data=maybe(api,ftp,location)
            if data is None or hashlib.sha256(data).hexdigest()!=digest:
                raise RuntimeError('Previous user state changed after import; reconcile before archiving '+name)
        moves=record.setdefault('archive_moves',[])
        for index,old in enumerate(record['sources']):
            target=ROOT+'/BACKUP/'+a.import_id+'/TREE%u'%index
            if any(move['source']==old and move['state']=='archived' for move in moves):continue
            move={'source':old,'target':target,'state':'about-to-rename'};moves.append(move);operations.save(journal,record)
            # Whole-directory rename preserves all history, images and byte data.
            # An unsupported rename fails visibly; no copy/delete fallback discards sources.
            ftp.rename(old,target)
            move['state']='archived';operations.save(journal,record)
    record['state']='archived';record['runtime_verified']=False;operations.save(journal,record)
    operations.history(api,'installation-archived',import_id=a.import_id,destination=ROOT)
    return record
