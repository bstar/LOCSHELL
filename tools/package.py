#!/usr/bin/env python3
"""Assemble one DOS application tree, including the pinned Island pack."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile
R=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(R/'management'))
import scenes
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--build',type=Path,default=R/'build')
p.add_argument('--output',type=Path,default=R/'build/package')
p.add_argument('--without-island',action='store_true')
a=p.parse_args();b=a.build.resolve();output=a.output.resolve()
if output.exists():p.error('Choose a new output directory; existing files were not replaced')
root=output/'LOCSHELL'
needed=['LOCSHELL.EXE','LOCSOLO.EXE','LOCHAND.EXE','MEMWATCH.COM','MEMLEARN.EXE','LOCSHELL.BAT','LCSMAIN.BAT','CLASSIC.BAT','LOCSHELL.TXT','DOSAGENT.EXE','DOSINFO.EXE','FTPSRV.EXE']
missing=[name for name in needed if not (b/name).is_file()]
if missing:p.error('Run tools/build.py --locshell first; missing '+', '.join(missing))
utilities=b/'suite'
expected=['DHCP.EXE','DNSTEST.EXE','FTP.EXE','HTGET.EXE','HTTPSERV.EXE','IRCJR.EXE','NC.EXE','NETDRIVE.EXE','PING.EXE','PKTTOOL.EXE','SNTP.EXE','SPDTEST.EXE','TELNET.EXE','ND_CACHE.EXE','NETDRIVE.SYS']
if any(not (utilities/name).is_file() for name in expected):p.error('Run tools/build_suite.py with the same DOS_BUILD_DIR first')
if b'TEST.KEY' in (b/'LOCSHELL.EXE').read_bytes():p.error('A production package cannot contain emulator test hooks')
# Stage locally; a preparation failure never leaves a half-created release.
output.parent.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='locshell-package-',dir=output.parent) as temp:
    staged=Path(temp)/'package';tree=staged/'LOCSHELL'
    for path in (tree,tree/'NET/AGENT',tree/'NET/BIN',tree/'NET/JOBS',tree/'NET/DRIVERS',tree/'BACKUP',tree/'GAMES'):
        path.mkdir(parents=True,exist_ok=True)
    for name in needed:
        shutil.copyfile(b/name,tree/('NET/AGENT/'+name if name in ['DOSAGENT.EXE','DOSINFO.EXE','FTPSRV.EXE'] else name))
    shutil.copyfile(b/'FTPSRV.EXE',tree/'NET/FTPSRV.EXE')
    for name in expected:shutil.copyfile(utilities/name,tree/'NET/BIN'/name)
    (tree/'LEGAL').mkdir()
    shutil.copyfile(R/'LICENSE',tree/'LICENSE.TXT')
    shutil.copyfile(R/'NOTICE.md',tree/'NOTICE.TXT')
    shutil.copyfile(R/'third_party/watcom-heap/LICENSE',tree/'LEGAL/WATCOM.TXT')
    shutil.copyfile(R/'third_party/dos-scenes/LICENSE',tree/'LEGAL/SCENES.TXT')
    pin=None
    if not a.without_island:pin=scenes.prepare_pack(tree/'ISLAND')
    (tree/'LIBRARY.TXT').write_bytes(b'')
    text='LOC SHELL\r\n\r\nStart C:\\LOCSHELL\\LOCSHELL.BAT after your packet driver and network address\r\nare ready. Keep MTCP.CFG and its private account file in C:\\LOCSHELL\\NET.\r\nDo not overwrite an existing catalogue or preferences with this empty template.\r\nGame/configuration instructions: https://github.com/bstar/LOCSHELL\r\n'
    (tree/'INSTALL.TXT').write_bytes(text.encode('ascii'))
    files={str(path.relative_to(tree)).replace('\\','/'):hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(tree.rglob('*')) if path.is_file()}
    (staged/'package.json').write_text(json.dumps({'application':'LOC SHELL','root':'C:\\LOCSHELL','scene_revision':pin,'files':files},indent=2)+'\n')
    with zipfile.ZipFile(staged/'LOCSHELL.ZIP','w',zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(tree.rglob('*')):archive.write(path,path.relative_to(staged))
    shutil.move(str(staged),str(output))
print(json.dumps({'root':str(root),'zip':str(output/'LOCSHELL.ZIP'),'scene_revision':pin,'files':len(files)},indent=2))
