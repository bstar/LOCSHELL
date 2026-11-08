"""Discoverable diagnostics for DOS machines; no repair or deletion operations."""
import datetime,ftplib,hashlib,json,re,subprocess,sys,os,importlib.util
from pathlib import Path
RECIPES={
 'doctor':('FTP','Connectivity, server capabilities, pending job and free C: space'),
 'memory':('job','Fresh MEM /C; FTP is absent during collection'),
 'inventory':('job','Scoped filesystem inventory; C: root by default, --all-drives explicit'),
 'storage-check':('job','Capacity/free space; --deep lists large files and duplicate candidates'),
 'logs':('FTP','Recent FTP/boot logs, tail-limited and saved locally'),
 'deployment-check':('FTP','Compare installed files with an explicit local SHA256 manifest'),
}
from layout import STATE
BASE=Path(os.environ.get('DOSCTL_STATE_DIR',STATE))
CUSTOM={}
def load_custom():
 directory=Path(os.environ.get('DOSCTL_CUSTOM_RECIPES',Path(__file__).resolve().parents[1]/'custom_recipes'))
 for path in sorted(directory.glob('*.py')):
  if path.name.startswith('_'):continue
  spec=importlib.util.spec_from_file_location('dosctl_custom_'+path.stem,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
  if not hasattr(module,'RECIPES') or not callable(getattr(module,'execute',None)):raise ValueError('Custom recipe must expose RECIPES and execute: '+str(path))
  for name,description in module.RECIPES.items():
   if name in RECIPES:raise ValueError('Duplicate custom recipe: '+name)
   RECIPES[name]=description;CUSTOM[name]=module.execute
load_custom()

def timestamp():return datetime.datetime.now(datetime.timezone.utc).isoformat()
def norm(s):return s.replace('/','\\').upper().rstrip('\\')
def artifact_dir():
 BASE.mkdir(parents=True,exist_ok=True,mode=0o700);os.chmod(BASE,0o700)
 p=BASE/'reports'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%fZ');p.mkdir(parents=True,mode=0o700);return p

def parse_scan(output):
 records=[];errors=[];end=None
 for line in output.splitlines():
  a=line.split('\t')
  if a[0]=='F' and len(a)>=3:records.append(dict(path=norm(a[1]),directory=False,size=int(a[2]),crc32=a[3] if len(a)>3 else None))
  elif a[0]=='D' and len(a)>1:records.append(dict(path=norm(a[1]),directory=True,size=0))
  elif a[0]=='ERROR':errors.append(a[1:])
  elif a[0]=='END' and len(a)==3:end={'files':int(a[1]),'errors':int(a[2])}
 return records,errors,end


def execute(a,api):
 if a.cmd=='reports':
  records=[]
  for p in sorted((BASE/'reports').glob('*/report.json'),reverse=True)[:max(0,min(a.limit,100))]:
   try:r=json.loads(p.read_text());records.append({k:r.get(k) for k in ['recipe','state','collected_at','artifacts','jobs']})
   except (OSError,ValueError):continue
  return {'cached':True,'reports':records}
 if a.cmd=='recipes':return {'recipes':[{'name':k,'transport':v[0],'description':v[1],'default_timeout_seconds':900 if v[0]=='job' else None,'changes_user_files':bool(v[2]) if len(v)>2 else False,'source':'custom' if k in CUSTOM else 'builtin'} for k,v in RECIPES.items()]}
 name=a.name
 if a.path and a.all_drives:raise ValueError('Choose --path or --all-drives')
 for path in a.path or []:
  if name!='logs' and not re.fullmatch(r'[C-Fc-f]:\\[^*?\r\n\t]*',path):raise ValueError('Use absolute DOS directory paths')
 if a.hash and name not in ('storage-check','inventory'):raise ValueError('--hash is for inventory/storage-check')
 if name in CUSTOM:return CUSTOM[name](a,api)
 if name=='deployment-check' and not a.manifest:raise ValueError('deployment-check requires --manifest')
 out=artifact_dir();report={'recipe':name,'collected_at':timestamp(),'artifacts':str(out),'findings':[],'state':'completed'}
 with api.connect(a) as f:
  report['host']=getattr(f,'host','unknown')
  def get(path,required=False):
   try:b=api.read(f,path)
   except ftplib.error_perm as e:
    if not str(e).startswith('550'):raise
    if required:report['findings'].append({'kind':'missing-file','path':path})
    return None
   filename=path.strip('/').replace('/','_');(out/filename).write_bytes(b);return b
  def snapshot(paths):
   result={}
   for p in paths:
    b=get(p,True)
    if b is not None:result[p]={'sha256':hashlib.sha256(b).hexdigest(),'bytes':len(b),'text':b.decode('cp437')}
   return result
  if name=='doctor':
   report['capabilities']=f.sendcmd('SITE HELP');report['agent_capabilities']=f.sendcmd('SITE AGENT');report['pending_job']=None
   if 'layout=2' not in report['agent_capabilities'].split():report['findings'].append({'kind':'layout-upgrade-required'})
   b=get('/LOCSHELL/NET/JOBS/LOCK/ID.TXT')
   if b is not None:report['pending_job']={'id':b.decode().strip(),'state':(get('/LOCSHELL/NET/JOBS/LOCK/STATE.TXT') or b'unknown').decode().strip()}
   report['disk_free']=f.sendcmd('SITE DISKFREE C')
   if 'QUIT' not in report['capabilities'].split():report['findings'].append({'kind':'missing-SITE-QUIT'})
  elif name=='deployment-check':
   manifest=json.loads(a.manifest.read_text());report['checks']=[]
   for path,digest in manifest['files'].items():
    if not path.startswith('/') or any(c in path for c in '\r\n') or not re.fullmatch('[a-fA-F0-9]{64}',digest):raise ValueError('Invalid deployment manifest path/digest')
    digest=digest.lower()
    b=get(path,True);actual=hashlib.sha256(b).hexdigest() if b is not None else None
    report['checks'].append({'path':path,'expected':digest,'actual':actual,'matches':actual==digest})
    if actual!=digest:report['findings'].append({'kind':'hash-mismatch','path':path})
  elif name=='logs':
   report['logs']={}
   for path in (a.path or ['/LOCSHELL/NET/FTP.LOG','/LOCSHELL/NET/JOBS/LAST.TXT']):
    b=get(path)
    if b is not None:report['logs'][path]='\n'.join(b.decode('cp437').splitlines()[-100:])
 if RECIPES[name][0]=='job':
  def job(program,arguments,label):
   cmd=[sys.executable,str(Path(__file__).with_name('dosctl.py'))]
   if a.credentials:cmd+=['--credentials',str(a.credentials)]
   cmd+=['--timeout',str(a.timeout or 900),'exec','--label',name.replace('-',' ').title(),program,'--']+arguments
   p=subprocess.run(cmd,capture_output=True,text=True,encoding='utf-8');(out/(label+'.txt')).write_text(p.stdout+p.stderr)
   match=re.search(r'^Submitted (J[0-9A-F]{7})$',p.stdout,re.M)
   if match:report.setdefault('jobs',[]).append(match[1])
   if p.returncode==2:report['state']='waiting';report['findings'].append({'kind':'wait-timeout','message':'Job not cancelled or resubmitted. Use dosctl resume/result.'});return None
   if p.returncode:report['findings'].append({'kind':'job-failure','code':p.returncode,'output':p.stdout+p.stderr})
   return p.stdout
  if name=='memory':
   report['memory_context']='FTP stopped; supervisor and packet driver resident';report['output']=job('C:\\DOS\\MEM.EXE',['/C'],'memory')
   report['memory']={}
   for key,pattern in [('conventional_free_bytes',r'Conventional\s+[\d,]+\s+[\d,]+\s+([\d,]+)'),('upper_free_bytes',r'Upper\s+[\d,]+\s+[\d,]+\s+([\d,]+)'),('largest_program_bytes',r'Largest executable program size\s+([\d,]+)'),('largest_umb_bytes',r'Largest free upper memory block\s+([\d,]+)')]:
    m=re.search(pattern,report['output'] or '')
    if m:report['memory'][key]=int(m[1].replace(',',''))
  elif name=='storage-check':
   report['coverage']='Fresh capacity/free-space check; file analysis requires --deep or --hash'
   output=job('C:\\LOCSHELL\\NET\\AGENT\\DOSINFO.EXE',['--disks'],'disks')
   report['drives']=[]
   if output:
    for d,total,free in re.findall(r'^DISK\t([C-F])\t(\d+)\t(\d+)',output,re.M):
     report['drives'].append({'drive':d,'total_bytes':int(total),'free_bytes':int(free)})
     if int(free)<int(total)//10:report['findings'].append({'kind':'low-space','drive':d,'threshold_percent':10})
  if report['state']=='completed' and (name=='inventory' or (name=='storage-check' and (a.deep or a.hash))):
   paths=a.path or (['C:\\','D:\\','E:\\','F:\\'] if a.all_drives else ['C:\\'])
   for p in paths:
    if not re.fullmatch(r'[C-Fc-f]:\\[^*?\r\n\t]*',p):raise ValueError('Use absolute C:/D:/E:/F: DOS directories, no wildcards')
   if not a.hash:
    import flows
    rows,errors,end=flows.scan_records(a,api,paths,report,out);output='flow'
   else:output=job('C:\\LOCSHELL\\NET\\AGENT\\DOSINFO.EXE',['--hash']+paths,'scan');rows,errors,end=parse_scan(output or '')
   if output:
    report['scan']={'paths':paths,'summary':end,'errors':errors,'hash':'CRC32 candidate fingerprint only' if a.hash else None};(out/'inventory.json').write_text(json.dumps(rows,indent=2))
    if end is None:report['findings'].append({'kind':'incomplete-scan'})
    report['findings'] += [{'kind':'scan-error','details':x} for x in errors]
    if name=='storage-check':
     fs=[r for r in rows if not r['directory']];report['largest_files']=sorted(fs,key=lambda r:r['size'],reverse=True)[:30]
     groups={}
     for r in fs:
      if r['size']:groups.setdefault((r['size'],r.get('crc32')) if a.hash else (r['size'],r['path'].rsplit('\\',1)[-1]),[]).append(r['path'])
     report['duplicate_candidates']=[{'key':list(k),'paths':v} for k,v in groups.items() if len(v)>1];report['limits']=['Candidates only; compare full contents before deletion. CRC32 collisions are possible.']
     report['job_backup_bytes']=sum(r['size'] for r in fs if r['path'].startswith('C:\\LOCSHELL\\NET\\JOBS\\') or r['path'].endswith(('.BAK','.NEW')))
 if report['findings'] and report['state']=='completed':report['state']='needs-attention'
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');return report
