#!/usr/bin/env python3
"""Build every shipped mTCP utility with its upstream memory model/features."""
from pathlib import Path
import re,subprocess,shutil,json,os
R=Path(__file__).resolve().parents[1];B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve();src=B/'source';out=B/'suite';out.mkdir(exist_ok=True)
if not src.is_dir():raise SystemExit('Run tools/build.py first')
# Upstream also uses angle-bracket includes for its own DOS headers.
local_headers={p.name.upper() for p in src.rglob('*.H')}
for p in src.rglob('*'):
 if p.suffix.upper() not in ('.CPP','.H','.CFG'):continue
 s=p.read_text();s=re.sub(r'(#\s*include\s*<)([^>]+)(>)',lambda m:m[1]+(m[2].upper() if m[2].upper() in local_headers else m[2])+m[3],s);p.write_text(s)
report=[]
for app in ['DHCP','DNSTEST','FTP','HTGET','HTTPSERV','IRCJR','NC','NETDRV','PING','PKTTOOL','SNTP','SPDTEST','TELNET']:
 d=out/app;d.mkdir(exist_ok=True);a=src/'APPS'/app;make=(a/'MAKEFILE').read_text()
 def val(key,default=''):
  m=re.search(r'^'+key+r'\s*=\s*(.*)',make,re.M);return m[1] if m else default
 defs=val('extra_opts').split();model=val('memory_model');cfg=next(a.glob('*.CFG')).name
 names=(val('tcpobjs')+' '+val('objs','netdrv.obj')).replace('$(extra_objs)',val('extra_objs')).split()
 # Match upstream optional definitions and memory model, with compact code.
 opts=['-0',model,'-s','-ei','-zp2','-os','-dCFG_H="'+cfg+'"','-i='+str(src/'TCPINC'),'-i='+str(src/'INCLUDE'),'-i='+str(a),*defs]
 for name in names:
  stem=Path(name).stem.upper();candidates=[a/(stem+'.CPP'),src/'TCPLIB'/(stem+'.CPP'),src/'INCLUDE'/(stem+'.CPP')]
  if stem=='IPASM':cmd=['wasm','-0',model,str(src/'TCPLIB/IPASM.ASM')]
  else:
   f=next((p for p in candidates if p.exists()),None)
   if f is None:raise SystemExit(f'Missing source {app}: {stem}')
   cmd=['wpp',str(f),*opts]
  subprocess.run(cmd,cwd=d,check=True,stdout=subprocess.DEVNULL)
 name='NETDRIVE' if app=='NETDRV' else app
 subprocess.run(['wlink','system','dos','option','eliminate','option','stack=8192','name',name+'.EXE','file',','.join(Path(n).stem.upper()+'.o' for n in names)],cwd=d,check=True,stdout=subprocess.DEVNULL)
 shutil.copyfile(d/(name+'.EXE'),out/(name+'.EXE'));report.append({'utility':name,'bytes':(out/(name+'.EXE')).stat().st_size})
 print('Built',name,flush=True)
(out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
# NetDrive's DOS device driver is supplied, never installed in CONFIG.SYS here.
nd=src/'APPS/NETDRV';work=out/'NETDRV'
for p in nd.iterdir():
 if p.suffix.upper() in ('.ASM','.H'):shutil.copyfile(p,work/p.name)
subprocess.run(['wasm','-q','-fo=NETDRIVE.o','SIMPLE.ASM'],cwd=work,check=True)
subprocess.run(['wlink','format','raw','bin','name','NETDRIVE.SYS','file','NETDRIVE.o'],cwd=work,check=True,stdout=subprocess.DEVNULL)
shutil.copyfile(work/'NETDRIVE.SYS',out/'NETDRIVE.SYS')
subprocess.run(['wpp','-0','-ms','-s','-os','-ei','-zp2','-dCFG_H="NETDRV.CFG"','-i='+str(nd),'-i='+str(src/'TCPINC'),'-i='+str(src/'INCLUDE'),str(nd/'ND_CACHE.CPP')],cwd=work,check=True,stdout=subprocess.DEVNULL)
subprocess.run(['wlink','system','dos','option','eliminate','name','ND_CACHE.EXE','file','ND_CACHE.o'],cwd=work,check=True,stdout=subprocess.DEVNULL)
shutil.copyfile(work/'ND_CACHE.EXE',out/'ND_CACHE.EXE')
