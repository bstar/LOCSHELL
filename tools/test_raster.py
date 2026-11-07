#!/usr/bin/env python3
"""Check the actual assembly edge renderer against an independent scalar path."""
from pathlib import Path
import os,subprocess
R=Path(__file__).resolve().parents[1];B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve()
subprocess.run(['nix','--extra-experimental-features','nix-command flakes','develop','path:'+str(R/'nix'),'--command','wcl','-q','-bt=dos','-3','-ml','-ot','-ol','-oi','-oe=100','-s','-zro','-fpi87','-fp3','-fe='+str(B/'RASTER.EXE'),str(R/'tools/raster_test.c')],cwd=B,check=True)
conf=B/'raster.conf'
conf.write_text(f'[sdl]\noutput=surface\n[dosbox]\nquit warning=false\n[cpu]\ncore=normal\ncputype=386\nfpu=387\ncycles=fixed 20000\n[mixer]\nnosound=true\n[autoexec]\nmount c {B}\nc:\nRASTER.EXE\nexit\n')
(B/'RASTPASS.TXT').unlink(missing_ok=True)
subprocess.run([os.environ.get('DOSBOX','dosbox-x'),'-fastlaunch','-silent','-nomenu','-nogui','-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=60,check=True)
assert (B/'RASTPASS.TXT').exists(),(B/'RASTER.LOG').read_text()
print((B/'RASTPASS.TXT').read_text())
