#!/usr/bin/env python3
"""Native 8086 display tests; shares the actual server/supervisor display code."""
from pathlib import Path
import subprocess,os
R=Path(__file__).resolve().parents[1];B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve()
subprocess.run(['nix','--extra-experimental-features','nix-command flakes','develop','path:'+str(R/'nix'),'--command','wcl','-q','-bt=dos','-0','-ml','-fe='+str(B/'UITEST.EXE'),str(R/'tools/ui_test.c')],cwd=B,check=True)
conf=B/'ui.conf';conf.write_text('[sdl]\noutput=surface\n[dosbox]\nquit warning=false\n[cpu]\ncputype=8086\ncycles=fixed 12000\n[mixer]\nnosound=true\n[autoexec]\nmount c '+str(B)+'\nc:\nUITEST.EXE\nexit\n')
(B/'PASS.TXT').unlink(missing_ok=True)
subprocess.run([os.environ.get('DOSBOX','dosbox-x'),'-fastlaunch','-silent','-nomenu','-nogui','-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30,check=True)
assert (B/'PASS.TXT').exists();print((B/'PASS.TXT').read_text())
