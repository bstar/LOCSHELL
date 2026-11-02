from pathlib import Path
import subprocess
R=Path(__file__).resolve().parents[1]
subprocess.run(['wcl','-q','-bt=dos','-0','-ms','-os','-s','-k2048','-fe=RXMON.EXE',str(R/'management/resident.c')],cwd=R/'build',check=True)
