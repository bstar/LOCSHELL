#!/usr/bin/env python3
"""Actual DOS scanner paths, sizes, CRC32 and non-mutating behavior."""
from pathlib import Path
import os,subprocess,shutil,zlib
R=Path(__file__).resolve().parents[1];B=R/'build/dosinfo-test';D=B/'drive';(D/'TEST/SUB').mkdir(parents=True,exist_ok=True)
shutil.copyfile(R/'build/DOSINFO.EXE',D/'DOSINFO.EXE');payload=b'123456789';(D/'TEST/A.DAT').write_bytes(payload);(D/'TEST/SUB/B.DAT').write_bytes(payload)
c=B/'test.conf';c.write_text('[sdl]\noutput=surface\n[cpu]\ncputype=8086\ncycles=fixed 12000\n[mixer]\nnosound=true\n[autoexec]\nmount c '+str(D)+'\nc:\nDOSINFO.EXE --hash C:\\TEST > HASH.TXT\nDOSINFO.EXE --scan C:\\ABSENT > BAD.TXT\nexit\n')
subprocess.run([os.environ.get('DOSBOX',shutil.which('dosbox-x') or 'dosbox-x'),'-conf',str(c)],env={**os.environ,'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'},stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,timeout=30,check=True)
s=(D/'HASH.TXT').read_text();assert s.count(f'\t9\t{zlib.crc32(payload):08X}')==2,s;assert 'END\t2\t0' in s
assert 'ERROR' in (D/'BAD.TXT').read_text();assert (D/'TEST/A.DAT').read_bytes()==payload;print('PASS native DOS recursion, sizes, CRC32, missing path and source preservation')
