#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
R=Path(__file__).resolve().parents[1];B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve()
subprocess.run(['nix','--extra-experimental-features','nix-command flakes','develop','path:'+str(R/'nix'),'--command','wcl','-q','-bt=dos','-0','-ml','-fe='+str(B/'VGATEST.EXE'),str(R/'tools/vga_test.c'),str(B/'mathsave.o')],cwd=B,check=True)
conf=B/'vga.conf';conf.write_text('[sdl]\noutput=surface\n[dosbox]\nquit warning=false\n[cpu]\ncputype=8086\ncycles=fixed 12000\n[mixer]\nnosound=true\n[autoexec]\nmount c '+str(B)+'\nc:\nVGATEST.EXE\nVGATEST.EXE mode2\nVGATEST.EXE text\nexit\n')
for n in ['VGAPASS.TXT','MODE2PAS.TXT','FALLPASS.TXT']:(B/n).unlink(missing_ok=True)
subprocess.run([os.environ.get('DOSBOX','dosbox-x'),'-fastlaunch','-silent','-nomenu','-nogui','-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=60,check=True)
for n in ['VGAPASS.TXT','MODE2PAS.TXT','FALLPASS.TXT']:assert (B/n).exists(),n;print(n,(B/n).read_text())

# EGA has no VGA mode13h: exercise automatic detection, not a forced override.
(B/'FALLPASS.TXT').unlink()
conf.write_text(conf.read_text().replace('[dosbox]\n', '[dosbox]\nmachine=ega\n').replace('VGATEST.EXE\nVGATEST.EXE mode2\nVGATEST.EXE text','VGATEST.EXE auto'))
subprocess.run([os.environ.get('DOSBOX','dosbox-x'),'-fastlaunch','-silent','-nomenu','-nogui','-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=60,check=True)
assert (B/'FALLPASS.TXT').exists();print('PASS automatic EGA-to-text fallback')
