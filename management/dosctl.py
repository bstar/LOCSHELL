#!/usr/bin/env python3
"""DOS job control over mTCP. Credentials remain on the workstation."""
import argparse,ftplib,io,json,os,sys,time,secrets,re
from pathlib import Path
ROOT='/NET/JOBS'
DEFAULT=Path.home()/'.config/dosctl/access.json'
LEGACY=Path('/home/bstar/projects/pcplus-recomp/build/network-bootstrap-01/private/ftp-access.json')

def connect(args):
 c=json.loads((args.credentials or (DEFAULT if DEFAULT.exists() else LEGACY)).read_text())
 f=ftplib.FTP();f.connect(c['host'],c.get('port',21),timeout=5);f.login(c['username'],c['password']);return f

def read(f,path):
 b=io.BytesIO();f.retrbinary('RETR '+path,b.write);return b.getvalue()

def put(f,path,data):
 f.storbinary('STOR '+path,io.BytesIO(data))
 if read(f,path)!=data:raise RuntimeError('Upload verification failed: '+path)

def text(s):
 if '\n' in s or '\r' in s:raise ValueError('Newlines are not allowed in request fields')
 return s

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--credentials',type=Path);p.add_argument('--timeout',type=float,default=None)
 sub=p.add_subparsers(dest='cmd',required=True)
 for name in ('put','get'):
  s=sub.add_parser(name);s.add_argument('source');s.add_argument('destination')
 for name in ('run','exec'):
  s=sub.add_parser(name);s.add_argument('program');s.add_argument('--cwd',default='C:\\');s.add_argument('arguments',nargs='*')
 for name in ('status','result'):
  s=sub.add_parser(name);s.add_argument('job',nargs='?',default='LOCK')
 sub.add_parser('jobs');sub.add_parser('inventory')
 a=p.parse_args()
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
  if a.cmd=='jobs':print('\n'.join(f.nlst(ROOT)));return
  if a.cmd in ('status','result'):
   if not (a.job=='LOCK' or (len(a.job)==8 and a.job.startswith('J') and all(x in '0123456789ABCDEF' for x in a.job[1:]))):raise ValueError('Invalid job ID')
   path=ROOT+'/'+a.job
   job=read(f,path+'/ID.TXT').decode().strip() if a.job=='LOCK' else a.job
   if a.job=='LOCK':print('Job:',job)
   complete=True
   for n in (['STATE.TXT'] if a.cmd=='status' else ['STATE.TXT','RESULT.TXT','OUTPUT.TXT']):
    try:print(read(f,path+'/'+n).decode('cp437'),end='')
    except ftplib.error_perm:print(n+': unavailable');complete=False
   if a.cmd=='result' and a.job=='LOCK' and complete:
    state=read(f,path+'/STATE.TXT').decode().strip()
    if state in ('completed','failed-to-start','interrupted'):
     if len(job)!=8 or job[0]!='J' or any(x not in '0123456789ABCDEF' for x in job[1:]):raise ValueError('Invalid archived job ID')
     f.rename(path,ROOT+'/'+job)
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
  request=('\r\n'.join([kind,cwd,program,'\t'.join(argv)])+'\r\n').encode('cp437')
  # Atomic MKD serializes submitters. Failed/partial submissions remain visible.
  f.mkd(ROOT+'/LOCK');job='J'+secrets.token_hex(4)[:7].upper();base=ROOT+'/LOCK'
  put(f,base+'/ID.TXT',job.encode());put(f,base+'/STATE.TXT',b'queued\r\n')
  if payload is not None:put(f,base+'/SCRIPT.BAT',payload)
  put(f,base+'/REQUEST.TXT',request)
  put(f,base+'/READY.NEW',b'ready\r\n');f.rename(base+'/READY.NEW',base+'/READY')
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
     print(read(f,base+'/OUTPUT.TXT').decode('cp437'),end='')
     result=read(f,base+'/RESULT.TXT').decode().strip();print('\n'+state+'\n'+result)
     f.rename(base,ROOT+'/'+job)
     if state!='completed' or result.splitlines()[-1]!='0':return 1
     return 0
  except (OSError,EOFError,ftplib.Error):pass
 print('Timed out waiting for '+job+'; it was NOT cancelled or resubmitted. Use dosctl status/result.',file=sys.stderr);return 2

if __name__=='__main__':
 try:sys.exit(main() or 0)
 except (OSError,ValueError,RuntimeError,ftplib.Error) as e:print(str(e),file=sys.stderr);sys.exit(1)
