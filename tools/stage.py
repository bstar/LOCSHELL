#!/usr/bin/env python3
"""Stage side by side; never replace the running server or boot files."""
from pathlib import Path
import ftplib,json,io,hashlib
R=Path(__file__).resolve().parents[1]
c=json.loads((Path.home()/'.local/share/locshell/access.json').read_text())
with ftplib.FTP() as f:
 f.connect(c['host'],21,timeout=10);f.login(c['username'],c['password'])
 for d in ['/LOCSHELL/NET/AGENT','/LOCSHELL/NET/JOBS']:
  try:f.mkd(d)
  except ftplib.error_perm:f.cwd(d);f.cwd('/')
 payload={n:(R/'build'/n).read_bytes() for n in ['FTPSRV.EXE','DOSAGENT.EXE']}
 payload['START.BAT']=b'@ECHO OFF\r\nSET MTCPCFG=C:\\LOCSHELL\\NET\\MTCP.CFG\r\nC:\\LOCSHELL\\NET\\AGENT\\DOSAGENT.EXE C:\\LOCSHELL\\NET\\AGENT\\FTPSRV.EXE\r\n'
 for n,data in payload.items():
  path='/LOCSHELL/NET/AGENT/'+n
  if f.nlst(path):raise RuntimeError('Refusing to overwrite '+path)
  f.storbinary('STOR '+path,io.BytesIO(data));b=io.BytesIO();f.retrbinary('RETR '+path,b.write);assert b.getvalue()==data
  print(path,hashlib.sha256(data).hexdigest())
