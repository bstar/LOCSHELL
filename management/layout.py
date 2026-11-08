"""LOCSHELL's private host-side container; never stored in the public checkout."""
import os
from pathlib import Path
HOME = Path(os.environ.get('LOCSHELL_HOME', Path.home() / '.local/share/locshell'))
ACCESS = HOME / 'access.json'
STATE = HOME / 'state'
VAULT = HOME / 'vault'
TEMP = HOME / 'tmp'

def temporary_directory(prefix):
    import tempfile
    HOME.mkdir(parents=True,exist_ok=True,mode=0o700)
    TEMP.mkdir(exist_ok=True,mode=0o700)
    return tempfile.TemporaryDirectory(prefix=prefix,dir=TEMP)
