"""One bounded DOS scan per step; checkpoint and cancel between jobs."""
import json,secrets,subprocess,sys,time,re,fcntl,contextlib
from pathlib import Path
from types import SimpleNamespace
from operations import save,history,now

def checkpoint(path,r):
 with (path.parent/'state.lock').open('a') as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  if path.exists():r['cancel_requested']=r.get('cancel_requested',False) or json.loads(path.read_text()).get('cancel_requested',False)
  save(path,r)

def path_for(api,ident):
 if not re.fullmatch('T[0-9A-F]{7}',ident):raise ValueError('Invalid flow ID')
 return api.recipes.BASE/'flows'/ident/'flow.json'
def invoke(a,args):
 command=[sys.executable,str(Path(__file__).with_name('dosctl.py')),'--json']
 if a.credentials:command+=['--credentials',str(a.credentials)]
 p=subprocess.run(command+args,capture_output=True,text=True);r=json.loads(p.stdout)
 if p.returncode and args[0]=='result' and (str(r.get('error',''))[:3] in ('421','425','426') or r.get('state') in ('unreachable','busy')):return {'state':'waiting','diagnostic':r}
 if p.returncode and r.get('state') not in ('waiting','completed'):raise RuntimeError(json.dumps(r))
 return r

def start(a,api):
 paths=a.path or ['C:\\']
 if any(not re.fullmatch(r'[C-Fc-f]:\\[^*?\r\n\t]*',x) or len(x)>75 for x in paths):raise ValueError('Invalid scan root')
 ident='T'+secrets.token_hex(4)[:7].upper();record={'id':ident,'kind':'scan','state':'ready','created_at':now(),'paths':paths,'limit':a.batch,'step':0,'files':0,'cancel_requested':False,'scope':api.state_dir(a).name}
 with api.connect(a) as f:
  try:f.mkd('/NET/TASKS')
  except api.ftplib.error_perm:f.cwd('/NET/TASKS');f.cwd('/')
  f.mkd('/NET/TASKS/'+ident);api.put(f,'/NET/TASKS/'+ident+'/ROOTS.TXT',('\r\n'.join(paths)+'\r\n').encode('cp437'))
 save(path_for(api,ident),record);history(api,'flow-created',flow=ident,paths=paths);return record

def step(a,api):
 path=path_for(api,a.flow);r=json.loads(path.read_text())
 if r.get('scope',api.state_dir(a).name)!=api.state_dir(a).name:raise ValueError('Flow belongs to a different credential profile')
 if r['state'] in ('completed','cancelled','failed','uncertain'):return r
 if r.get('cancel_requested') and not r.get('job'):r['state']='cancelled';checkpoint(path,r);return r
 if not r.get('job'):
  label=f"Flow {r['id']} step {r['step']+1} ({r['files']} files)"
  if r['state']=='submitting':
   matches=[]
   for p in api.state_dir(a).glob('J*.json'):
    v=json.loads(p.read_text())
    if v.get('label')==label:matches.append(v)
   if len(matches)!=1:r['state']='uncertain';checkpoint(path,r);raise RuntimeError('Submission ownership uncertain; no replay performed')
   r['job']=matches[0]['id'];checkpoint(path,r)
  else:
   r['state']='submitting';checkpoint(path,r)
   if r['cancel_requested']:r['state']='cancelled';checkpoint(path,r);return r
   result=invoke(a,['exec','C:\\NET\\AGENT\\DOSINFO.EXE','--detach','--label',label,'--cwd','C:\\NET\\TASKS\\'+r['id'],'--','--chunk',str(r['limit'])])
   r['job']=result['id'];r['state']='waiting';checkpoint(path,r)
 # Retrieve the same job; result never launches a new command.
 deadline=time.monotonic()+(a.timeout or 300)
 while True:
  try:
   with api.connect(a) as f:
    try:owner=api.read(f,api.ROOT+'/LOCK/ID.TXT').decode().strip()
    except api.ftplib.error_perm:owner=None
   result=invoke(a,['result','LOCK' if owner==r['job'] else r['job']])
   if result.get('state') in ('completed','failed-to-start','interrupted'):break
  except (OSError,api.ftplib.Error):pass
  if time.monotonic()>=deadline:r['state']='waiting';checkpoint(path,r);return r
  time.sleep(1)
 output=result.get('artifacts',{}).get('OUTPUT.TXT','');code=result.get('artifacts',{}).get('RESULT.TXT','').splitlines()
 (path.parent/f"step-{r['step']+1:05}.txt").write_text(output)
 m=re.search(r'^CHUNK\t(\d+)\t(\d+)\t(\d+)\t(\d+)\t(\d+)',output,re.M)
 if result['state']!='completed' or not code or code[-1]!='0' or not m or int(m[1])!=r['step']+1:
  r['state']='failed';r['error']='Chunk result incomplete/nonzero; not replayed';checkpoint(path,r);return r
 r['step']=int(m[1]);r['files']=int(m[3]);r['last_job']=r.pop('job');r['state']='completed' if m[2]=='1' else 'ready';r['updated_at']=now()
 # Preserve a cancellation request written concurrently by another CLI.
 latest=json.loads(path.read_text());r['cancel_requested']=latest.get('cancel_requested',False)
 if r['cancel_requested'] and r['state']!='completed':r['state']='cancelled'
 checkpoint(path,r);history(api,'flow-step',flow=r['id'],step=r['step'],files=r['files'],state=r['state']);return r

def handle(a,api):
 if a.flow_command=='list':return {'flows':[json.loads(p.read_text()) for p in sorted((api.recipes.BASE/'flows').glob('*/flow.json'),key=lambda x:x.stat().st_mtime,reverse=True)]}
 if a.flow_command=='start':
  if not 16<=a.batch<=4096:raise ValueError('Batch must be16..4096 entries')
  return start(a,api)
 path=path_for(api,a.flow)
 if a.flow_command=='cancel':
  r=json.loads(path.read_text());r['cancel_requested']=True
  if not r.get('job') and r['state']=='ready':r['state']='cancelled'
  checkpoint(path,r);history(api,'flow-cancel-request',flow=a.flow);return r
 if a.flow_command=='status':return json.loads(path.read_text())
 if a.flow_command=='result':return export(a,api)
 with (path.parent/'runner.lock').open('a') as lock:
  try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
  except BlockingIOError:raise RuntimeError('Another process is already advancing this flow')
  if a.flow_command=='step':return step(a,api)
  while True:
   r=step(a,api)
   if r['state']!='ready':return r

def export(a,api):
 path=path_for(api,a.flow);r=json.loads(path.read_text());rows={};errors=[]
 for p in sorted(path.parent.glob('step-*.txt')):
  records,errs,_=api.recipes.parse_scan(p.read_text());errors+=errs
  for row in records:rows[row['path']]=row
 artifact=path.parent/'inventory.json';save(artifact,list(rows.values()))
 return {**r,'partial':r['state']!='completed','inventory':str(artifact),'records':len(rows),'errors':errors}

def scan_records(a,api,paths,report,out):
 args=SimpleNamespace(credentials=a.credentials,timeout=a.timeout,path=paths,batch=512)
 record=start(args,api);args.flow=record['id'];args.flow_command='run';report['flow']=record['id'];report['state']='waiting';save(out/'report.json',report)
 record=handle(args,api);result=export(args,api);report['flow_progress']=result
 report['state']='completed' if record['state']=='completed' else record['state']
 return json.loads(Path(result['inventory']).read_text()),result['errors'],{'files':record['files'],'errors':len(result['errors'])} if record['state']=='completed' else None
