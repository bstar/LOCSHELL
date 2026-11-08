#!/usr/bin/env python3
"""Enable an accepted staged supervisor without changing NIC/profile settings."""
import datetime,json,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1]
b=R/'build'/('startup-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'));b.mkdir()
changes=[]
for name in ['NETFTP.BAT','NETMGR.BAT']:
 old=b/(name+'.before');new=b/name
 subprocess.run(['dosctl','get','/LOCSHELL/NET/'+name,str(old)],check=True)
 data=old.read_bytes();target=b'C:\\LOCSHELL\\NET\\FTPSRV.EXE'
 if data.count(target)!=1:raise RuntimeError('Unexpected startup contents; stop: '+name)
 new.write_bytes(data.replace(target,b'CALL C:\\LOCSHELL\\NET\\AGENT\\START.BAT'))
 changes.append((name,old,new))
applied=[]
try:
 for name,old,new in changes:
  subprocess.run(['dosctl','put',str(new),'/LOCSHELL/NET/'+name],check=True);applied.append((name,old))
except Exception:
 for name,old in reversed(applied):subprocess.run(['dosctl','put',str(old),'/LOCSHELL/NET/'+name],check=True)
 raise
(b/'result.json').write_text(json.dumps({'enabled':True,'files':[n for n,_,_ in changes]},indent=2)+'\n')
print('Enabled both network startup entry points. Backups:',b)
