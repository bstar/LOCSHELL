#!/usr/bin/env python3
"""Execute real 16-bit supervisor/child under DOSBox; server is a test stub."""
from pathlib import Path
import shutil,subprocess,os
R=Path(__file__).resolve().parents[1];B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve();base=B/'test';fixtures=base/'drive/LOCSHELL/NET'
fixtures.mkdir(parents=True,exist_ok=True)
(base/'fake.c').write_text(r'''#include <stdio.h>
int main(void){FILE *f=fopen("C:\\LOCSHELL\\NET\\ONCE","r");if(f){fclose(f);return 1;}f=fopen("C:\\LOCSHELL\\NET\\ONCE","w");fclose(f);return 0;}'''+"\n")
(base/'child.c').write_text(r'''#include <stdio.h>
int main(void){puts("child output");fprintf(stderr,"child stderr\n");return 37;}'''+"\n")
# Reuse the supplied SDK inside Nix/CI instead of fetching a second toolchain.
compiler=['wcl'] if shutil.which('wcl') else ['nix','--extra-experimental-features','nix-command flakes','develop','path:'+str(R/'nix'),'--command','wcl']
for source,name in [('fake.c','FAKE.EXE'),('child.c','CHILD.EXE')]:
 subprocess.run(compiler+['-q','-bt=dos','-0','-ms','-fe='+str(fixtures/name),str(base/source)],cwd=base,check=True)
for name in ['DOSAGENT.EXE','DOSINFO.EXE']:shutil.copyfile(B/name,fixtures/name)
box=os.environ.get('DOSBOX',shutil.which('dosbox-x') or 'dosbox-x')
for name,kind,program,expected in [('native','EXE','C:\\LOCSHELL\\NET\\CHILD.EXE','37'),('missing','EXE','C:\\ABSENT.EXE','-1'),('batch','BAT','C:\\LOCSHELL\\NET\\JOBS\\LOCK\\SCRIPT.BAT',None),('identity','EXE','C:\\LOCSHELL\\NET\\DOSINFO.EXE','0')]:
 b=B/('test-'+name);root=b/'drive';net=root/'LOCSHELL/NET';job=net/'JOBS/LOCK';job.mkdir(parents=True,exist_ok=True)
 for n in ['DOSAGENT.EXE','DOSINFO.EXE','FAKE.EXE','CHILD.EXE']:shutil.copyfile(fixtures/n,net/n)
 (net/'ONCE').unlink(missing_ok=True)
 for n in ['CLAIMED','STATE.TXT','OUTPUT.TXT','RESULT.TXT']:(job/n).unlink(missing_ok=True)
 (job/'REQUEST.TXT').write_bytes(('\r\n'.join([kind,'C:\\LOCSHELL\\NET',program,'--identity' if name=='identity' else ''])+'\r\n').encode())
 (job/'READY').write_text('ready');(job/'SCRIPT.BAT').write_bytes(b'@ECHO OFF\r\nECHO batch proof\r\nECHO written > C:\\PROOF.TXT\r\n')
 conf=b/'test.conf';conf.write_text('[sdl]\noutput=surface\n[dosbox]\nquit warning=false\n[cpu]\ncputype=8086\ncycles=fixed 12000\n[mixer]\nnosound=true\n[autoexec]\nmount c '+str(root)+'\nc:\nSET MACHINE=TESTBOX\nSET CONFIG=N\nC:\\LOCSHELL\\NET\\DOSAGENT.EXE C:\\LOCSHELL\\NET\\FAKE.EXE\nexit\n')
 def boot():subprocess.run([box,'-fastlaunch','-silent','-nomenu','-nogui','-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30,check=True)
 boot();result=(job/'RESULT.TXT').read_text();state=(job/'STATE.TXT').read_text().strip()
 assert state==('failed-to-start' if name=='missing' else 'completed'),state
 if expected:assert result.splitlines()[1]==expected,result
 if name=='native':assert all(x in (job/'OUTPUT.TXT').read_text() for x in ['child output','child stderr'])
 if name=='identity':assert all(x in (job/'OUTPUT.TXT').read_text() for x in ['ENV\tMACHINE\tTESTBOX','ENV\tCONFIG\tN','ENV\tDOSAGENT_VERSION\t2'])
 if name=='batch':assert (root/'PROOF.TXT').read_text().strip()=='written'
 before=(job/'OUTPUT.TXT').read_bytes();(net/'ONCE').unlink();boot();assert (job/'OUTPUT.TXT').read_bytes()==before,'job replayed'
 print('PASS',name,'execution, output/status and no replay')
