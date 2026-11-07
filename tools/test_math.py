#!/usr/bin/env python3
"""Real-mode x87 rendering and VGA lifecycle tests, plus absent-FPU fallback."""
from pathlib import Path
import os, subprocess

R=Path(__file__).resolve().parents[1]
B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve()
subprocess.run(['nix','--extra-experimental-features','nix-command flakes','develop','path:'+str(R/'nix'),'--command','wcl','-q','-bt=dos','-0','-ml','-fe='+str(B/'MATHTEST.EXE'),str(R/'tools/math_test.c'),str(B/'mathsave.o')],cwd=B,check=True)
for cpu,fpu,marker in [('386','387','MATHPASS.TXT'),('386','false','NOFPUPAS.TXT'),('386','287','NOFPUPAS.TXT'),('386','8087','NOFPUPAS.TXT'),('286','387','NOFPUPAS.TXT')]:
    (B/marker).unlink(missing_ok=True)
    if cpu=='386' and fpu=='387':(B/'MATHSTAT.TXT').unlink(missing_ok=True)
    conf=B/'math.conf'
    conf.write_text(f'[sdl]\noutput=surface\n[dosbox]\nquit warning=false\n[cpu]\ncore=normal\ncputype={cpu}\nfpu={fpu}\ncycles=fixed 20000\n[mixer]\nnosound=true\n[autoexec]\nmount c {B}\nc:\nMATHTEST.EXE'+(' fallback' if fpu!='387' or cpu=='286' else '')+'\nexit\n')
    subprocess.run([os.environ.get('DOSBOX','dosbox-x'),'-fastlaunch','-silent','-nomenu','-nogui','-conf',str(conf)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=60,check=True)
    assert (B/marker).exists(),(B/'MATH.LOG').read_text()
    print(f'CPU={cpu} FPU={fpu}: '+(B/marker).read_text().strip())
print((B/'MATHSTAT.TXT').read_text())
assert (B/'SCULPT0.RAW').read_bytes() != (B/'SCULPT.RAW').read_bytes(), 'Sculpture did not move'

# Compare both full-resolution Mandelbrot views with an independent scalar
# escape-time reference, sampling a grid including interior/exterior points.
for filename, cx, cy, span in [('MANDEL.RAW',-.5,0.,3.4),('ZOOM.RAW',-.5804024827222598,.0435025483882599,2.278)]:
    raw=(B/filename).read_bytes()
    for y in range(0,200,7):
        for x in range(0,320,7):
            c=complex(cx+((x+.5)/320-.5)*span, cy+((y+.5)/200-.5)*span*.75)
            z=0j; count=0
            while abs(z)**2<=4 and count<128:
                z=z*z+c; count+=1
            expected=0 if count==128 else 32+(count*5)%224
            assert raw[y*320+x]==expected,(filename,x,y,raw[y*320+x],expected)
print('PASS sculpture motion and 2,668 independently checked Mandelbrot/zoom samples')
