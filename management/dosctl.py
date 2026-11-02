#!/usr/bin/env python3
"""DOS job control over mTCP. Credentials remain on the workstation."""
import argparse,ftplib,io,json,os,sys,time,secrets,re,contextlib,datetime,hashlib

from pathlib import Path
ROOT='/NET/JOBS'
DEFAULT=Path.home()/'.config/dosctl/access.json'
sys.path.insert(0,str(Path(__file__).resolve().parent))
import recipes
LAST_RESULT=None
JSON_MODE=False

def connect(args):
 c=json.loads((args.credentials or DEFAULT).read_text())
 f=ftplib.FTP();f.connect(c['host'],c.get('port',21),timeout=5);f.login(c['username'],c['password']);return f

def read(f,path):
 b=io.BytesIO();f.retrbinary('RETR '+path,b.write);return b.getvalue()

def put(f,path,data):
 f.storbinary('STOR '+path,io.BytesIO(data))
 if read(f,path)!=data:raise RuntimeError('Upload verification failed: '+path)

def text(s):
 if '\n' in s or '\r' in s:raise ValueError('Newlines are not allowed in request fields')
 return s

def state_dir(a):
 key=hashlib.sha256(str(a.credentials or DEFAULT).encode()).hexdigest()[:12]
 base=recipes.BASE;base.mkdir(parents=True,exist_ok=True,mode=0o700);os.chmod(base,0o700)
 p=base/'jobs'/key;p.mkdir(parents=True,exist_ok=True);return p

def track(a,job,state):
 p=state_dir(a);r={'id':job,'state':state,'updated_at':recipes.timestamp(),'command':a.cmd,'label':getattr(a,'label',None)}
 path=p/(job+'.json');path.write_text(json.dumps(r)+'\n')
 pending=p/'pending.json'
 if state in ('completed','failed-to-start','interrupted'):
  if pending.exists() and json.loads(pending.read_text()).get('id')==job:pending.unlink()
 else:
  temp=p/'pending.new';temp.write_text(json.dumps(r)+'\n');os.replace(temp,pending)


def command_main():
 global LAST_RESULT
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--credentials',type=Path);p.add_argument('--timeout',type=float,default=None)
 sub=p.add_subparsers(dest='cmd',required=True)
 for name in ('put','get'):
  s=sub.add_parser(name);s.add_argument('source');s.add_argument('destination')
 for name in ('run','exec'):
  s=sub.add_parser(name);s.add_argument('program');s.add_argument('--cwd',default='C:\\');s.add_argument('--label');s.add_argument('arguments',nargs='*')
 for name in ('status','result'):
  s=sub.add_parser(name);s.add_argument('job',nargs='?',default='LOCK')
 sub.add_parser('jobs');sub.add_parser('resume')
 inv=sub.add_parser('inventory');inv.add_argument('--path',action='append');inv.add_argument('--all-drives',action='store_true');inv.add_argument('--hash',action='store_true')
 sub.add_parser('recipes')
 r=sub.add_parser('reports');r.add_argument('--limit',type=int,default=20)
 s=sub.add_parser('recipe');s.add_argument('name',choices=recipes.RECIPES);s.add_argument('--path',action='append');s.add_argument('--all-drives',action='store_true');s.add_argument('--deep',action='store_true');s.add_argument('--hash',action='store_true');s.add_argument('--manifest',type=Path)
 a=p.parse_args()
 if a.cmd=='inventory':a.cmd='recipe';a.name='inventory';a.deep=False;a.manifest=None
 if a.cmd in ('recipes','recipe','reports'):
  LAST_RESULT=recipes.execute(a,sys.modules[__name__])
  if not JSON_MODE:print(json.dumps(LAST_RESULT,indent=2))
  return 2 if LAST_RESULT.get('state')=='waiting' else 0
 if a.cmd=='resume':
  pending=state_dir(a)/'pending.json'
  if not pending.exists():LAST_RESULT={'state':'idle','message':'No locally tracked outstanding job'};print(LAST_RESULT['message']);return
  job=json.loads(pending.read_text())['id']
  with connect(a) as f:
   try:remote=read(f,ROOT+'/LOCK/ID.TXT').decode().strip()
   except ftplib.error_perm:remote=None
   args=['result','LOCK' if remote==job else job]
  with contextlib.ExitStack() as stack:
   old=sys.argv;sys.argv=[old[0]]+(['--credentials',str(a.credentials)] if a.credentials else [])+args
   try:return command_main()
   finally:sys.argv=old
 with connect(a) as f:
  if a.cmd=='put':
   data=Path(a.source).read_bytes()
   try:before=read(f,a.destination)
   except ftplib.error_perm:before=None
   if before is None:put(f,a.destination,data);return
   if before==data:return
   parent=a.destination.rsplit('/',1)[0];stem='U'+secrets.token_hex(4)[:7].upper()
   stage=parent+'/'+stem+'.NEW';backup=parent+'/'+stem+'.BAK'
   if f.nlst(stage) or f.nlst(backup):raise RuntimeError('Staging name collision; retry')
   put(f,stage,data);f.rename(a.destination,backup)
   try:f.rename(stage,a.destination)
   except Exception:f.rename(backup,a.destination);raise
   if read(f,a.destination)!=data:raise RuntimeError('Final readback mismatch; backup: '+backup)
   print('Updated; previous file:',backup);return
  if a.cmd=='get':
   with Path(a.destination).open('xb') as out:out.write(read(f,a.source))
   return
  if a.cmd=='jobs':
   jobs=[x for x in f.nlst(ROOT) if re.fullmatch(r'(?:.*/)?(?:LOCK|J[0-9A-F]{7})',x)];LAST_RESULT={'jobs':jobs};print('\n'.join(jobs));return
  if a.cmd in ('status','result'):
   if not (a.job=='LOCK' or (len(a.job)==8 and a.job.startswith('J') and all(x in '0123456789ABCDEF' for x in a.job[1:]))):raise ValueError('Invalid job ID')
   path=ROOT+'/'+a.job
   try:job=read(f,path+'/ID.TXT').decode().strip() if a.job=='LOCK' else a.job
   except ftplib.error_perm as e:
    if a.job=='LOCK' and str(e).startswith('550'):LAST_RESULT={'state':'idle','id':None};print('No outstanding job');return
    raise
   if a.job=='LOCK':print('Job:',job)
   LAST_RESULT={'id':job,'artifacts':{},'state':'unknown'}
   complete=True
   for n in (['STATE.TXT'] if a.cmd=='status' else ['STATE.TXT','RESULT.TXT','OUTPUT.TXT']):
    try:
     value=read(f,path+'/'+n).decode('cp437');LAST_RESULT['artifacts'][n]=value
     if n=='STATE.TXT':LAST_RESULT['state']=value.strip()
     print(value,end='')
    except ftplib.error_perm:print(n+': unavailable');complete=False
   if a.cmd=='result' and a.job=='LOCK' and complete:
    state=read(f,path+'/STATE.TXT').decode().strip()
    if state in ('completed','failed-to-start','interrupted'):
     if len(job)!=8 or job[0]!='J' or any(x not in '0123456789ABCDEF' for x in job[1:]):raise ValueError('Invalid archived job ID')
     f.rename(path,ROOT+'/'+job)
   if complete and a.cmd=='result' and LAST_RESULT['state'] in ('completed','failed-to-start','interrupted'):track(a,job,LAST_RESULT['state'])
   return
  if 'QUIT' not in f.sendcmd('SITE HELP').split():raise RuntimeError('Start C:\\NET\\AGENT\\START.BAT locally first; current server lacks SITE QUIT')
  kind='BAT' if a.cmd in ('run','inventory') else 'EXE'
  payload=None
  if a.cmd=='run':
   payload=Path(a.program).read_bytes().decode('cp437').replace('\r\n','\n').replace('\n','\r\n').encode('cp437')
  elif a.cmd=='inventory':payload=b'@ECHO OFF\r\nVER\r\nMEM /C\r\nDIR C:\\ /S /B\r\nDIR D:\\ /S /B\r\nDIR E:\\ /S /B\r\nDIR F:\\ /S /B\r\n'
  cwd=text(getattr(a,'cwd','C:\\'));program=ROOT.replace('/','\\')[0:]+'\\LOCK\\SCRIPT.BAT' if kind=='BAT' else text(a.program)
  if kind=='BAT':program='C:'+program
  argv=getattr(a,'arguments',[])
  if not re.match(r'^[A-Za-z]:\\',cwd) or not re.match(r'^[A-Za-z]:\\',program):raise ValueError('Use absolute DOS paths')
  if len(argv)>30:raise ValueError('At most 30 program arguments are supported')
  if any('\t' in x or '\r' in x or '\n' in x for x in argv):raise ValueError('Tabs/newlines in arguments unsupported')
  if kind=='BAT' and argv:raise ValueError('Batch arguments are not supported; put them in the script')
  if len((' '.join(argv)).encode('cp437'))+len(program.encode('cp437'))>110:raise ValueError('DOS command tail too long')
  if len(cwd.encode('cp437'))>78 or len(program.encode('cp437'))>126:raise ValueError('DOS path too long')
  label=text(getattr(a,'label',None) or a.cmd)
  if len(label.encode('cp437'))>90:raise ValueError('Job label too long')
  request=('\r\n'.join([kind,cwd,program,'\t'.join(argv)])+'\r\n').encode('cp437')
  # Atomic MKD serializes submitters. Failed/partial submissions remain visible.
  f.mkd(ROOT+'/LOCK');job='J'+secrets.token_hex(4)[:7].upper();base=ROOT+'/LOCK'
  track(a,job,'uploading')
  put(f,base+'/ID.TXT',job.encode());put(f,base+'/STATE.TXT',b'queued\r\n')
  put(f,base+'/LABEL.TXT',label.encode('cp437'))
  if payload is not None:put(f,base+'/SCRIPT.BAT',payload)
  put(f,base+'/REQUEST.TXT',request)
  put(f,base+'/READY.NEW',b'ready\r\n');f.rename(base+'/READY.NEW',base+'/READY')
  track(a,job,'submitted')
  print('Submitted',job,flush=True)
  try:f.sendcmd('SITE QUIT')
  except (EOFError,ConnectionError):pass
 deadline=time.monotonic()+(a.timeout if a.timeout is not None else (900 if a.cmd=='inventory' else 300))
 while time.monotonic()<deadline:
  time.sleep(1)
  try:
   with connect(a) as f:
    if read(f,base+'/ID.TXT').decode()!=job:raise RuntimeError('Job ownership changed')
    state=read(f,base+'/STATE.TXT').decode().strip()
    if state in ('completed','failed-to-start','interrupted'):
     job_output=read(f,base+'/OUTPUT.TXT').decode('cp437');print(job_output,end='')
     result=read(f,base+'/RESULT.TXT').decode().strip();print('\n'+state+'\n'+result)
     f.rename(base,ROOT+'/'+job)
     track(a,job,state);LAST_RESULT={'id':job,'state':state,'exit_code_type':result.splitlines()[0],'exit_code':int(result.splitlines()[-1]),'output':job_output}
     if state!='completed' or result.splitlines()[-1]!='0':return 1
     return 0
  except (OSError,EOFError,ftplib.Error):pass
 track(a,job,'waiting');LAST_RESULT={'id':job,'state':'waiting','cancelled':False}
 print('Timed out waiting for '+job+'; it was NOT cancelled or resubmitted. Use dosctl status/result.',file=sys.stderr);return 2

def main():
 global JSON_MODE,LAST_RESULT
 LAST_RESULT=None
 JSON_MODE=len(sys.argv)>1 and sys.argv[1]=='--json'
 if not JSON_MODE:return command_main()
 sys.argv.pop(1);output=io.StringIO();error=io.StringIO()
 try:
  with contextlib.redirect_stdout(output),contextlib.redirect_stderr(error):rc=command_main() or 0
  result=LAST_RESULT or {'state':'completed' if not rc else 'needs-attention','output':output.getvalue(),'exit_code':rc}
  if error.getvalue():result['diagnostic']=error.getvalue()
 except (OSError,ValueError,RuntimeError,ftplib.Error) as e:rc=1;result={'state':'error','error':str(e)}
 print(json.dumps(result,indent=2));return rc

if __name__=='__main__':
 try:sys.exit(main() or 0)
 except (OSError,ValueError,RuntimeError,ftplib.Error) as e:print(str(e),file=sys.stderr);sys.exit(1)
