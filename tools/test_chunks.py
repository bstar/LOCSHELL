#!/usr/bin/env python3
from pathlib import Path
import os,shutil,subprocess,re
R=Path(__file__).resolve().parents[1];B=R/'build/chunk-test';D=B/'drive';D.mkdir(parents=True,exist_ok=True)
shutil.copyfile(R/'build/DOSINFO.EXE',D/'DOSINFO.EXE');(D/'DATA/SUB').mkdir(parents=True,exist_ok=True)
for i in range(43):(D/'DATA'/('F%03d.TXT'%i)).write_text(str(i))
(D/'DATA/SUB/INNER.TXT').write_text('inner');(D/'ROOTS.TXT').write_bytes(b'C:\\DATA\r\n')
for name in ['CURSOR.BIN','CURSOR.BAK','CURSOR.NEW','QUEUE.TXT']:(D/name).unlink(missing_ok=True)
lines=['mount c '+str(D),'c:']+[f'DOSINFO.EXE --chunk 16 > O{i}.TXT' for i in range(1,8)]+['exit']
c=B/'test.conf';c.write_text('[sdl]\noutput=surface\n[cpu]\ncputype=8086\ncycles=fixed 12000\n[mixer]\nnosound=true\n[autoexec]\n'+'\n'.join(lines)+'\n')
subprocess.run([os.environ.get('DOSBOX',shutil.which('dosbox-x') or 'dosbox-x'),'-conf',str(c)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30,check=True)
seen=[];done=False
for i in range(1,8):
 s=(D/f'O{i}.TXT').read_text();m=re.search(r'CHUNK\t(\d+)\t(\d+)\t(\d+)\t(\d+)\t(\d+)',s);assert m,s
 assert int(m[1])==i and int(m[4])<=16 and m[5]=='0';seen+=re.findall(r'^F\t([^\t]+)\t',s,re.M);done|=m[2]=='1'
assert done and len(seen)==44 and len(set(seen))==44,(len(seen),len(set(seen)))
print('PASS: bounded native chunks, persistent DTA continuation, 44 files exactly once, nested directory and completion')
