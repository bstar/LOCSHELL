"""Durable operations and verifiable release transactions. GPL-3.0-or-later."""
import datetime,hashlib,json,os,secrets,fnmatch,re
from pathlib import Path

class ReleaseFailure(RuntimeError):
 def __init__(self,record,journal,cause):
  self.release_id=record['id'];self.journal=str(journal);self.recovery_state=record['state']
  super().__init__(f"Release {self.release_id}: {cause}; recovery={self.recovery_state}; journal={journal}")

def now():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def save(path,data):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
 temp=path.with_suffix('.new');
 with temp.open('w') as f:json.dump(data,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
 os.replace(temp,path)
 fd=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
 try:os.fsync(fd)
 finally:os.close(fd)
def history(api,kind,**fields):
 base=api.recipes.BASE;base.mkdir(parents=True,exist_ok=True,mode=0o700);os.chmod(base,0o700)
 row={'at':now(),'kind':kind,**fields}
 with (base/'history.jsonl').open('a') as f:f.write(json.dumps(row)+'\n');f.flush();os.fsync(f.fileno())
 return row

def maybe(api,f,path):
 try:
  if hasattr(f,'size'):f.size(path)
  return api.read(f,path)
 except api.ftplib.error_perm as e:
  if str(e).startswith('550'):return None
  raise

def deploy(a,api):
 manifest=json.loads(a.manifest.read_text());release='R'+secrets.token_hex(4)[:7].upper();base=api.recipes.BASE/'releases'/release
 while base.exists():release='R'+secrets.token_hex(4)[:7].upper();base=api.recipes.BASE/'releases'/release
 if not isinstance(manifest.get('files'),list) or not 1<=len(manifest['files'])<=256:raise ValueError('Release needs1..256 files')
 record={'id':release,'name':manifest.get('name','application'),'state':'validating','created_at':now(),'files':[],'runtime_verified':False,'preserve_patterns':manifest.get('preserve',[]),'scope':api.state_dir(a).name if hasattr(api,'state_dir') else 'default'}
 seen=set()
 with api.connect(a) as f:
  record['host']=getattr(f,'host','unknown')
  for item in manifest['files']:
   target=item['destination']
   if not target.startswith('/') or any(x in target for x in ['\r','\n','..']):raise ValueError('Use absolute FTP paths without traversal')
   if any(not re.fullmatch(r'[A-Za-z0-9_~!-]{1,8}(?:\.[A-Za-z0-9_~-]{1,3})?',p) or p.split('.')[0].upper() in ['NUL','CON','PRN','AUX','COM1','COM2','COM3','COM4','LPT1','LPT2','LPT3'] for p in target.strip('/').split('/')):raise ValueError('Use DOS8.3 filesystem targets, not devices')
   if target.upper() in seen:raise ValueError('Duplicate destination')
   seen.add(target.upper())
   if any(fnmatch.fnmatchcase(target.upper(),p.upper()) for p in manifest.get('preserve',[])):continue
   data=Path(item['source']).read_bytes();digest=hashlib.sha256(data).hexdigest()
   if digest!=item['sha256'].lower():raise ValueError('Source hash mismatch: '+target)
   old=maybe(api,f,target);row={'target':target,'sha256':digest,'before_sha256':hashlib.sha256(old).hexdigest() if old is not None else None,'existed':old is not None,'state':'unchanged' if old==data else 'pending'}
   record['files'].append(row)
   if a.dry_run:continue
   base.mkdir(parents=True,exist_ok=True,mode=0o700)
   (base/('source-'+str(len(record['files'])))).write_bytes(data)
   if old is not None:(base/('before-'+str(len(record['files'])))).write_bytes(old)
   row['local_source']=str(base/('source-'+str(len(record['files']))))
  if a.dry_run:record['state']='dry-run';return record
  journal=base/'release.json';save(journal,record);history(api,'release-prepared',release=release,name=record['name'])
  try:
   artifact_directory=manifest.get('artifacts') or '/LOCSHELL/BACKUP'
   if hasattr(f,'cwd'):
    try:f.cwd(artifact_directory)
    except api.ftplib.error_perm as error:
     if not str(error).startswith('550'):raise
     f.mkd(artifact_directory)
   for i,row in enumerate(record['files']):
    if row['state']=='unchanged':continue
    parent=manifest.get('artifacts') or '/LOCSHELL/BACKUP'
    if not parent.startswith('/') or any(not re.fullmatch(r'[A-Za-z0-9_~-]{1,8}',part) for part in parent.strip('/').split('/')):raise ValueError('Invalid release artifact directory')
    stem=release[:5]+f'{i:03X}'
    row.update(stage=parent+'/'+stem+'.NEW',backup=parent+'/'+stem+'.BAK',aside=parent+'/'+stem+'.OUT')
    if any(maybe(api,f,row[k]) is not None for k in ['stage','backup','aside']):raise RuntimeError('Release staging name collision')
    api.put(f,row['stage'],Path(row['local_source']).read_bytes());row['state']='staged';save(journal,record)
   record['state']='committing';save(journal,record)
   for row in record['files']:
    if row['state']=='unchanged':continue
    current=maybe(api,f,row['target']);actual=hashlib.sha256(current).hexdigest() if current is not None else None
    if actual!=row['before_sha256']:raise RuntimeError('Target changed since staging: '+row['target'])
    row['state']='about-to-rename';save(journal,record)
    if row['existed']:f.rename(row['target'],row['backup'])
    row['state']='backed-up';save(journal,record)
    f.rename(row['stage'],row['target']);row['state']='installed';save(journal,record)
    if hashlib.sha256(api.read(f,row['target'])).hexdigest()!=row['sha256']:raise RuntimeError('Installed hash mismatch')
   record['state']='installed';record['verified_at']=now();save(journal,record);history(api,'release-installed',release=release,name=record['name'],host=record['host'],manifest=str(journal),files=[{'target':x['target'],'backup':x.get('backup'),'state':x['state']} for x in record['files']],runtime_verified=False)
  except Exception as e:
   record['state']='needs-recovery';record['error']=str(e);save(journal,record)
   # Roll back only the exact bytes owned by this transaction.
   try:rollback(f,api,record,journal)
   except Exception as recovery:
    record['recovery_error']=str(recovery);save(journal,record);history(api,'release-needs-recovery',release=release,manifest=str(journal),error=str(recovery))
   raise ReleaseFailure(record,journal,e) from e
 return record

def rollback(f,api,record,journal):
 # Preflight every file first: never overwrite a subsequent independent change.
 for row in record['files']:
  if row['state'] in ('pending','staged','unchanged','restored'):continue
  current=maybe(api,f,row['target']);backup=maybe(api,f,row.get('backup',''))
  actual=hashlib.sha256(current).hexdigest() if current is not None else None
  if actual not in (None,row['sha256'],row['before_sha256']):raise RuntimeError('Recovery blocked by changed target: '+row['target'])
  if row['existed'] and backup is not None and hashlib.sha256(backup).hexdigest()!=row['before_sha256']:raise RuntimeError('Recovery backup changed')
  if row['existed'] and backup is None and actual!=row['before_sha256']:raise RuntimeError('Recovery backup missing')
 for row in reversed(record['files']):
  if row['state'] in ('pending','staged','unchanged','restored'):continue
  current=maybe(api,f,row['target']);actual=hashlib.sha256(current).hexdigest() if current is not None else None
  if actual==row['before_sha256'] and row['existed']:row['state']='restored';save(journal,record);continue
  if current is not None:
   if maybe(api,f,row['aside']) is not None:raise RuntimeError('Recovery aside already exists')
   f.rename(row['target'],row['aside'])
  if row['existed']:f.rename(row['backup'],row['target'])
  restored=maybe(api,f,row['target']);actual=hashlib.sha256(restored).hexdigest() if restored is not None else None
  if actual!=row['before_sha256']:raise RuntimeError('Restored readback mismatch: '+row['target'])
  row['state']='restored';save(journal,record)
 record['state']='restored';record['restored_at']=now();save(journal,record);history(api,'release-restored',release=record['id'])

def restore(a,api):
 if len(a.release)!=8 or a.release[0]!='R' or any(c not in '0123456789ABCDEF' for c in a.release[1:]):raise ValueError('Invalid release ID')
 path=api.recipes.BASE/'releases'/a.release/'release.json';record=json.loads(path.read_text())
 if record.get('scope',api.state_dir(a).name if hasattr(api,'state_dir') else 'default')!=(api.state_dir(a).name if hasattr(api,'state_dir') else 'default'):raise ValueError('Release belongs to a different credential profile')
 with api.connect(a) as f:rollback(f,api,record,path)
 return record
