#!/usr/bin/env python3
"""Check the actual DOS CRT, including deliberate metadata corruption."""
from pathlib import Path
import os, subprocess
R=Path(__file__).resolve().parents[1];B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve()
subprocess.run(['nix','--extra-experimental-features','nix-command flakes','develop','path:'+str(R/'nix'),'--command','wcl','-q','-bt=dos','-0','-ml','-k12000','-fe='+str(B/'HEAPTEST.EXE'),str(R/'tools/heap_test.c'),str(B/'heapwalk.o')],cwd=B,check=True)
log=B/'HEAPTEST.LOG';log.unlink(missing_ok=True)
cfg=B/'heap-test.conf';cfg.write_text(f'[dosbox]\nquit warning=false\n[autoexec]\nmount c {B}\nc:\nHEAPTEST.EXE\nexit\n')
subprocess.run([os.environ.get('DOSBOX','dosbox-x'),'-fastlaunch','-silent','-nomenu','-nogui','-conf',str(cfg)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=20,check=True)
report=log.read_text();assert report.startswith('failures=0 '),report
print('PASS',report.strip())
