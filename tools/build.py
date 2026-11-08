#!/usr/bin/env python3
from pathlib import Path
import subprocess,shutil,re,os,argparse
R=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Build classic NETFTP or the integrated LOC SHELL and launch helpers.')
parser.add_argument('--locshell',action='store_true',help='Build LOCSHELL.EXE from the bundled frontend')
args=parser.parse_args()
B=Path(os.environ.get('DOS_BUILD_DIR',R/'build')).resolve(); B.mkdir(parents=True,exist_ok=True)

for obj in list(B.glob('*.o')) + list(B.glob('*.obj')): obj.unlink()
shell_source=os.environ.get('DOS_SHELL_SOURCE') or (str(R/'locshell/src/locshell') if args.locshell else None)
S=B/'source';shutil.copytree(R/'src',S,dirs_exist_ok=True)
# Upstream includes and makefiles assume a case-insensitive DOS filesystem.
for p in S.rglob('*'):
 if p.is_file() and p.suffix.upper() in ('.CPP','.H','.CFG'):
  s=p.read_text(errors='strict')
  s=re.sub(r'(#\s*include\s*")([^"]+)(")',lambda m:m[1]+m[2].upper()+m[3],s)
  p.write_text(s)
if shell_source:shutil.copyfile(Path(shell_source)/'launch.h',S/'APPS/FTPSRV/LOCLAUNCH.H')
if shell_source:shutil.copyfile(Path(shell_source)/'network.h',S/'APPS/FTPSRV/LOCNET.H')
if shell_source:shutil.copyfile(Path(shell_source)/'style.h',B/'LOCSTYLE.H')
if shell_source:shutil.copyfile(Path(shell_source)/'profile.h',B/'LOCPROF.H')
shutil.copyfile(R/'management/NETVIEWS.H',S/'APPS/FTPSRV/NETVIEWS.H')
shutil.copyfile(R/'management/SHELLBACK.CPP',S/'APPS/FTPSRV/SHELLBACK.CPP')
shutil.copyfile(R/'management/SHELLTEST.H', S/'APPS/FTPSRV/SHELLTEST.H')
shutil.copyfile(R/'management/DASHBOARD.H', S/'APPS/FTPSRV/DASHBOARD.H')
if shell_source:
 header=S/'APPS/FTPSRV/DASHBOARD.H'
 header.write_text(header.read_text().replace('static unsigned short daSaved[4000];','static unsigned short far daSaved[4000];').replace('static unsigned char daVgaState[4096],daFont[8192];','static unsigned char daVgaState[4096]; static unsigned char far daFont[8192];'))

shutil.copyfile(R/'management/MATHSAVE.H', S/'APPS/FTPSRV/MATHSAVE.H')
shutil.copyfile(R/'management/ISLANDSAV.H', S/'APPS/FTPSRV/ISLANDSAV.H')
SCENE=Path(os.environ.get('DOS_SCENES_SOURCE',R/'third_party/dos-scenes/src'))
if not (SCENE/'runtime.c').is_file():
 raise SystemExit('Initialize the dos-scenes submodule or set DOS_SCENES_SOURCE to its src directory.')
# Apply the tested compact cloud/XMS music integration to a build-local copy.
# The pinned upstream submodule and any supplied checkout stay untouched.
scene_stage=B/'scenes'
if scene_stage.exists(): shutil.rmtree(scene_stage)
shutil.copytree(SCENE,scene_stage/'src')
if 'DOS_SCENES_COMPACT_MUSIC' not in (scene_stage/'src/runtime.c').read_text():
 subprocess.run(['patch','--batch','--forward','-p1','-i',str(R/'third_party/dos-scenes-compact.patch')],cwd=scene_stage,check=True)
SCENE=scene_stage/'src'
shutil.copyfile(SCENE/'dos_scenes.h', S/'APPS/FTPSRV/DOSSCENE.H')
shutil.copyfile(SCENE/'dos_scenes.h', B/'DOSSCENE.H')
def run(args):subprocess.run(args,cwd=B,check=True)
for name in ('packet','arp','dns','eth','ip','tcp','tcpsockm','utils','timer','trace','udp'):
 run(['wpp',str(S/'TCPLIB'/(name.upper()+'.CPP')),'-0','-ml','-s','-ei','-zp2','-dCFG_H="FTPSRV.CFG"','-i='+str(S/'TCPINC'),'-i='+str(S/'INCLUDE'),'-i='+str(S/'APPS/FTPSRV')])
run(['wasm','-0','-ml',str(S/'TCPLIB/IPASM.ASM')])
for name in ('FTPSRV','FTPCL','FTPUSR'):
 run(['wpp',str(S/'APPS/FTPSRV'/(name+'.CPP')),*(['-dLOC_SHELL_EMBEDDED'] if shell_source else []),*(['-dLOC_SHELL_TEST'] if os.environ.get('DOS_SHELL_TEST') else []),'-0','-ml','-s','-ei','-zp2','-dCFG_H="FTPSRV.CFG"','-i='+str(S/'TCPINC'),'-i='+str(S/'INCLUDE')])
run(['wcc','-q','-0','-ml','-os','-s','-fo=heapwalk.o',str(R/'third_party/watcom-heap/heapwalk.c')])
run(['wcc','-q','-3','-ml','-ot','-ol','-oi','-oe=100','-s','-zro','-fpi87','-fp3',str(R/'management/mathsave.c')])
run(['wcc','-q','-0','-ml','-os','-s','-dDOS_SCENES_EMBEDDED',*(['-dDOS_SCENES_COMPACT_CLOUD','-dDOS_SCENES_COMPACT_MUSIC'] if shell_source else []),'-fo=island.o',str(SCENE/'runtime.c')])
if shell_source:
 for name in ('library','memory','memory_dos','learn','launch','locshell'):
  run(['wcc','-q','-0','-ml','-os','-s','-dDS_NETWORK_EMBEDDED','-zt=256','-fo=ds_'+name+'.o',str(Path(shell_source)/(name+'.c'))])
network_objects=[p.name for p in B.glob('*.o') if not p.name.startswith('ds_') and p.stem.upper()!='FTPSRV']
run(['wlink','system','dos' ,'option','map','option','eliminate','option','stack='+('16384' if shell_source else '4096'),'name',('LOCSHELL.EXE' if shell_source else 'FTPSRV.EXE'),'file',','.join(p.name for p in B.glob('*.o'))])
run(['wcl','-q','-bt=dos','-0','-ms','-os','-k4096',*(['-dLOC_AGENT_SCREEN','-i='+str(B)] if shell_source else []),*(['-dLOC_AGENT_TEST'] if shell_source and os.environ.get('DOS_SHELL_TEST') else []),'-fe=DOSAGENT.EXE',str(R/'management/agent.c')])

run(['wcl','-q','-bt=dos','-0','-ms','-os','-k12000','-fe=DOSINFO.EXE',str(R/'management/dosinfo.c')])

if shell_source:
 source=Path(shell_source)
 run(['wcl','-q','-bt=dos','-0','-ms','-os','-k2048','-dLOC_HANDOFF_MAIN','-fe=LOCHAND.EXE',str(source/'launch.c')])
 run(['nasm','-f','bin',str(source/'memwatch.asm'),'-o','MEMWATCH.COM'])
 run(['wcl','-q','-bt=dos','-0','-ms','-os','-k2048','-fe=MEMLEARN.EXE',str(source/'memlearn.c'),str(source/'learn.c')])
 run(['wcl','-q','-bt=dos','-0','-ml','-os','-k16384','-fe=LOCSOLO.EXE',*[str(source/(n+'.c')) for n in ('library','memory','memory_dos','learn','launch','locshell')]])
 for name in ('LOCSHELL.BAT','LCSMAIN.BAT','CLASSIC.BAT'):
  (B/name).write_bytes((R/'locshell/install'/name).read_text().replace('\n','\r\n').encode('ascii'))
 (B/'LOCSHELL.TXT').write_bytes((R/'locshell/USERGUIDE.txt').read_text().replace('\n','\r\n').encode('ascii'))

if shell_source:
 run(['wpp',str(S/'APPS/FTPSRV/FTPSRV.CPP'),'-fo=classic.o','-0','-ml','-s','-ei','-zp2','-dCFG_H="FTPSRV.CFG"','-i='+str(S/'TCPINC'),'-i='+str(S/'INCLUDE')])
 run(['wlink','system','dos','option','eliminate','option','stack=4096','name','FTPSRV.EXE','file',','.join([*network_objects,'classic.o'])])
