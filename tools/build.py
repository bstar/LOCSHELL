#!/usr/bin/env python3
from pathlib import Path
import subprocess,shutil,re
R=Path(__file__).resolve().parents[1]; B=R/'build'; B.mkdir(exist_ok=True)

for obj in list(B.glob('*.o')) + list(B.glob('*.obj')): obj.unlink()
S=B/'source';shutil.copytree(R/'src',S,dirs_exist_ok=True)
# Upstream includes and makefiles assume a case-insensitive DOS filesystem.
for p in S.rglob('*'):
 if p.is_file() and p.suffix.upper() in ('.CPP','.H','.CFG'):
  s=p.read_text(errors='strict')
  s=re.sub(r'(#\s*include\s*")([^"]+)(")',lambda m:m[1]+m[2].upper()+m[3],s)
  p.write_text(s)
def run(args):subprocess.run(args,cwd=B,check=True)
for name in ('packet','arp','dns','eth','ip','tcp','tcpsockm','utils','timer','trace','udp'):
 run(['wpp',str(S/'TCPLIB'/(name.upper()+'.CPP')),'-0','-ml','-s','-ei','-zp2','-dCFG_H="FTPSRV.CFG"','-i='+str(S/'TCPINC'),'-i='+str(S/'INCLUDE'),'-i='+str(S/'APPS/FTPSRV')])
run(['wasm','-0','-ml',str(S/'TCPLIB/IPASM.ASM')])
for name in ('FTPSRV','FTPCL','FTPUSR'):
 run(['wpp',str(S/'APPS/FTPSRV'/(name+'.CPP')),'-0','-ml','-s','-ei','-zp2','-dCFG_H="FTPSRV.CFG"','-i='+str(S/'TCPINC'),'-i='+str(S/'INCLUDE')])
run(['wlink','system','dos','option','map','option','eliminate','option','stack=4096','name','FTPSRV.EXE','file',','.join(p.name for p in B.glob('*.o'))])
run(['wcl','-q','-bt=dos','-0','-ms','-os','-fe=DOSAGENT.EXE',str(R/'management/agent.c')])
