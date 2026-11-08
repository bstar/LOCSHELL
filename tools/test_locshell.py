#!/usr/bin/env python3
"""Headless real-DOS dashboard/launch acceptance using only authored fixtures.

Run inside the pinned Nix shell, after tools/build.py --locshell. No network
access, packet driver, installed games, machine catalogue or ROM font needed.
"""
from pathlib import Path
import os
import shutil
import subprocess

R = Path(__file__).resolve().parents[1]
B = Path(os.environ.get('DOS_BUILD_DIR', R / 'build')).resolve()
O = B / 'locshell-test'
S = R / 'locshell/src/locshell'
T = R / 'locshell/tests'
C = O / 'drive-c'
D = O / 'drive-d'
APP = C / 'LOCSHELL'
GAME = D / 'GAMES/DEMO/ONE'
for path in (APP, GAME, APP / 'NET/AGENT'):
    path.mkdir(parents=True, exist_ok=True)


def run(args):
    subprocess.run(list(map(str, args)), cwd=O, check=True)


def compile(name, sources, small=False, extra=()):
    run(['wcl', '-q', '-bt=dos', '-0', '-ms' if small else '-ml', '-os',
         '-zt=256', '-k16384', '-i=' + str(S), *extra,
         '-fe=' + str(APP / (name + '.EXE')), *sources])


def write(path, text):
    path.write_bytes(text.replace('\r\n', '\n').replace('\n', '\r\n').encode('ascii'))


# Keep an earlier run from masking a missing/failing program this time.
for name in ('DASHTEST.TXT', 'RENDER.TXT', 'MEMUI.TXT', 'MODEL.TXT', 'RESULT.TXT',
             'MEMORY.TXT', 'LEARN.TXT', 'RECENT.TXT', 'RECENT.NEW', 'RECENT.BAK',
             'LEARN.DAT', 'GAME.CFG', 'LAUNCH.NEW', 'LAUNCH.REQ', 'LAUNCH.RES'):
    (APP / name).unlink(missing_ok=True)
(GAME / 'FIXTURE.TXT').unlink(missing_ok=True)

# The exact catalogue identity uses normalized LF commands, like library.c.
def identity(title, commands):
    value = 2166136261
    for byte in (title + commands).encode('ascii'):
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


entries, memory = [], []
for i in range(48):
    title = 'Example application %02d' % i
    commands = 'C:\nCD \\LOCSHELL\nFIXTURE.EXE\n'
    entries.append(title + '\n---GENRE---Demo\n---SERIES---Examples\n'
                   '---DEVELOPER---Fixture\n---PUBLISHER---Fixture\n'
                   '---YEAR---1991\n---STATUS---Utility\n---FAVORITE---\n' + commands + '\n')
    memory.append('%08x|64|0|0|-|D|Authored test fixture\n' % identity(title, commands))
write(APP / 'LIBRARY.TXT', ''.join(entries))
write(APP / 'MEMORY.TSV', ''.join(memory))
common = [S / (name + '.c') for name in ('library', 'memory', 'memory_dos', 'learn', 'launch')]
for name, source in [('DASHTEST', 'dashboard_test.c'), ('RENDER', 'render_test.c'),
                     ('MEMUI', 'memory_ui_test.c')]:
    compile(name, [*common, T / 'locshell' / source])
compile('MEMTEST', [S / 'memory.c', T / 'locshell/memory_test.c'])
compile('LEARNMOD', [S / 'learn.c', T / 'memlearn/learn_model.c'])
compile('FIXTURE', [T / 'memlearn/fixture.c'], small=True)
compile('HTEST', [T / 'memlearn/handoff_test.c', S / 'launch.c'], small=True)
shutil.copyfile(B / 'LOCHAND.EXE', APP / 'LOCHAND.EXE')
shutil.copyfile(APP / 'FIXTURE.EXE', GAME / 'CHILD.EXE')
(APP/'NET/BIN').mkdir(parents=True,exist_ok=True)
shutil.copyfile(APP/'FIXTURE.EXE',APP/'NET/BIN/PING.EXE')
run(['nasm', '-f', 'bin', T / 'memlearn/game_loader.asm', '-o', GAME / 'DEMO.COM'])

write(APP / 'TEST.BAT', r'''@ECHO OFF
C:
CD \LOCSHELL
SET CONFIG=N
MEMTEST.EXE > MEMORY.TXT
LEARNMOD.EXE > LEARN.TXT
DASHTEST.EXE
RENDER.EXE
MEMUI.EXE
IF EXIST RECENT.TXT DEL RECENT.TXT
HTEST.EXE > MODEL.TXT
HTEST.EXE RUN
LOCHAND.EXE /CLAIM
IF ERRORLEVEL 2 GOTO FAIL
IF NOT ERRORLEVEL 1 GOTO FAIL
LOCHAND.EXE /ENTER
CALL C:\LOCSHELL\RUN.BAT
C:\LOCSHELL\LOCHAND.EXE /RETURN 0
C:
CD \LOCSHELL
LOCHAND.EXE /RESUME
HTEST.EXE TOOL
LOCHAND.EXE /CLAIM
IF ERRORLEVEL 2 GOTO FAIL
IF NOT ERRORLEVEL 1 GOTO FAIL
LOCHAND.EXE /ENTER
LOCHAND.EXE /RESUME
HTEST.EXE SETUP
LOCHAND.EXE /CLAIM
IF ERRORLEVEL 2 GOTO FAIL
IF NOT ERRORLEVEL 1 GOTO FAIL
LOCHAND.EXE /ENTER
LOCHAND.EXE /RESUME
HTEST.EXE MISSING
LOCHAND.EXE /CLAIM
IF NOT ERRORLEVEL 4 GOTO FAIL
TYPE LAUNCH.RES > MISSING.TXT
HTEST.EXE RUN
ECHO REM altered >> RUN.BAT
LOCHAND.EXE /CLAIM
IF NOT ERRORLEVEL 4 GOTO FAIL
TYPE LAUNCH.RES > HASH.TXT
HTEST.EXE RESTART
LOCHAND.EXE /CLAIM
IF ERRORLEVEL 4 GOTO FAIL
IF NOT ERRORLEVEL 3 GOTO FAIL
LOCHAND.EXE /RESUME
ECHO PASS > RESULT.TXT
GOTO END
:FAIL
ECHO FAIL > RESULT.TXT
:END
''')
conf = O / 'test.conf'
conf.write_text('[sdl]\noutput=surface\n[dosbox]\nquit warning=false\nmemsize=4\n'
                '[cpu]\ncore=normal\ncputype=386\ncycles=fixed 120000\n'
                '[autoexec]\nmount c "' + str(C) + '"\nmount d "' + str(D) +
                '"\nc:\nCALL C:\\LOCSHELL\\TEST.BAT\nexit\n')
with (O / 'dosbox.log').open('w') as log:
    subprocess.run([os.environ.get('DOSBOX', 'dosbox-x'), '-fastlaunch', '-silent',
                    '-nomenu', '-nogui', '-conf', str(conf)], cwd=O,
                   env={**os.environ, 'SDL_VIDEODRIVER': 'dummy', 'SDL_AUDIODRIVER': 'dummy'},
                   stdout=log, stderr=subprocess.STDOUT, check=True, timeout=50)
for name, expected in [('MEMORY.TXT', 'MEMORY_MODEL_OK'), ('LEARN.TXT', 'LEARN_MODEL_OK'),
                       ('DASHTEST.TXT', 'DASHBOARD_OK'), ('RENDER.TXT', 'PASS'),
                       ('MEMUI.TXT', 'MEMORY_UI_OK'), ('MODEL.TXT', 'HANDOFF_MODEL_OK'),
                       ('RESULT.TXT', 'PASS')]:
    text = (APP / name).read_text()
    assert expected in text and 'FAIL' not in text, (name, text)
    print(text.strip())
assert 'directory missing' in (APP / 'MISSING.TXT').read_text()
assert 'commands changed' in (APP / 'HASH.TXT').read_text()
assert 'retained_shell=0 retained_agent=0' in (GAME / 'FIXTURE.TXT').read_text()
recent = (APP / 'RECENT.TXT').read_text().splitlines()
assert len(recent) == 3 and recent[1].startswith('T 00000009 ') and recent[2].startswith('A 12345678 '), recent
print('RECENT_HANDOFF_OK game/tool /ENTER only; setup/rejected/restart excluded')
print('LOCSHELL_OK authored catalogue, live UI snapshots, memory gates, COM child dispatch/return')
