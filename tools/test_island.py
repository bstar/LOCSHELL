#!/usr/bin/env python3
"""Test NETFTP embedding with the bundled island scene and twinkles."""
from pathlib import Path
import os,subprocess,shutil,sys
R=Path(__file__).resolve().parents[1];B=R/'build';S=R/'third_party/dos-scenes'
shutil.rmtree(B/'ISLAND',ignore_errors=True)
subprocess.run([sys.executable,str(S/'tools/pack_island.py'),'--output',str(B/'ISLAND')],check=True)
subprocess.run(['wcl','-q','-bt=dos','-0','-ml','-i='+str(B),'-fe='+str(B/'ISLTEST.EXE'),str(R/'tools/island_test.c'),str(B/'island.o')],cwd=B,check=True)
(B/'ISLPASS.TXT').unlink(missing_ok=True)
cfg=B/'island-test.conf';cfg.write_text(f'[sdl]\noutput=surface\n[cpu]\ncore=normal\ncputype=386\nfpu=false\ncycles=fixed 3000\n[sblaster]\nsbtype=none\noplmode=opl2\n[autoexec]\nmount c {B}\nc:\nISLTEST.EXE\nexit\n')
subprocess.run([os.environ.get('DOSBOX','dosbox-x'),'-conf',str(cfg)],env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=80,check=True)
assert (B/'ISLPASS.TXT').exists(),(B/'ISLTEST.LOG').read_text()
sys.path.insert(0,str(S/'tests'));from reference import compose,decode
t,frame=map(int,(B/'ISLFRAME.TXT').read_text().split());assert decode((B/'ISLAND.RAW').read_bytes())==compose(B/'ISLAND',t,frame)
print((B/'ISLPASS.TXT').read_text().strip());print('PASS all 64000 pixels, 386 without FPU, 160KB reserved')
