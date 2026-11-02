#!/usr/bin/env python3
"""Execute real 16-bit supervisor/child under DOSBox; server is a test stub."""
from pathlib import Path
import shutil,subprocess,os
R=Path(__file__).resolve().parents[1];base=R/'build/test';fixtures=base/'drive/NET'
box=os.environ.get('DOSBOX','/home/bstar/projects/DOS/runtime/bin/dosbox-x')
for name,kind,program,expected in [('native','EXE','C:\\NET\\CHILD.EXE','37'),('missing','EXE','C:\\ABSENT.EXE','-1'),('batch','BAT','C:\\NET\\JOBS\\LOCK\\SCRIPT.BAT',None)]:
 b=R/'build'/('test-'+name);root=b/'drive';net=root/'NET';job=net/'JOBS/LOCK';job.mkdir(parents=True,exist_ok=True)
 for n in ['DOSAGENT.EXE','FAKE.EXE','CHILD.EXE']:shutil.copyfile(fixtures/n,net/n)
 (net/'ONCE').unlink(missing_ok=True)
 for n in ['CLAIMED','STATE.TXT','OUTPUT.TXT','RESULT.TXT']:(job/n).unlink(missing_ok=True)
 (job/'REQUEST.TXT').write_bytes(('\r\n'.join([kind,'C:\\NET',program,''])+'\r\n').encode())
 (job/'READY').write_text('ready');(job/'SCRIPT.BAT').write_bytes(b'@ECHO OFF\r\nECHO batch proof\r\nECHO written > C:\\PROOF.TXT\r\n')
 conf=b/'test.conf';conf.write_text('[sdl]\noutput=surface\n[cpu]\ncputype=8086\ncycles=fixed 12000\n[mixer]\nnosound=true\n[autoexec]\nmount c '+str(root)+'\nc:\nC:\\NET\\DOSAGENT.EXE C:\\NET\\FAKE.EXE\nexit\n')
 def boot():subprocess.run([box,'-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30,check=True)
 boot();result=(job/'RESULT.TXT').read_text();state=(job/'STATE.TXT').read_text().strip()
 assert state==('failed-to-start' if name=='missing' else 'completed'),state
 if expected:assert result.splitlines()[1]==expected,result
 if name=='native':assert all(x in (job/'OUTPUT.TXT').read_text() for x in ['child output','child stderr'])
 if name=='batch':assert (root/'PROOF.TXT').read_text().strip()=='written'
 before=(job/'OUTPUT.TXT').read_bytes();(net/'ONCE').unlink();boot();assert (job/'OUTPUT.TXT').read_bytes()==before,'job replayed'
 print('PASS',name,'execution, output/status and no replay')
