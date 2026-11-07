"""Portable verified vault exports. Import creates a new destination only."""
import json
import sqlite3
from pathlib import Path
from .vault import Vault, digest, durable, encoded

def verify(root):
    if not (Path(root) / 'vault.sqlite').is_file():
        raise FileNotFoundError('Vault does not exist')
    vault = Vault(root)
    try:
        if vault.db.execute('PRAGMA integrity_check').fetchone()[0] != 'ok':
            raise ValueError('Damaged vault database')
        for campaign in vault.campaigns():
            if campaign['head'] and vault.snapshot(campaign['head'])['campaign'] != campaign['id']:
                raise ValueError('Invalid campaign head')
            for snapshot in vault.history(campaign['id']):
                vault.snapshot(snapshot['id'])
                for sha in set(snapshot['files'].values()):
                    vault.get_blob(sha)
    finally:
        vault.close()

def export(root, destination):
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    source = Vault(root)
    try:
        target = sqlite3.connect(destination / 'vault.sqlite')
        try:
            source.db.backup(target)
        finally:
            target.close()
        copy = Vault(destination)
        try:
            for campaign in copy.campaigns():
                for snapshot in copy.history(campaign['id']):
                    for sha in set(snapshot['files'].values()):
                        copy.put_blob(sha, source.get_blob(sha))
        finally:
            copy.close()
        verify(destination)
        checksums = {str(p.relative_to(destination)): digest(p.read_bytes()) for p in destination.rglob('*')
                     if p.is_file() and not p.name.endswith(('-wal', '-shm'))}
        durable(destination / 'EXPORT.JSON', encoded({'version': 1, 'files': checksums}))
    finally:
        source.close()

def import_backup(source, destination):
    source = Path(source)
    manifest = json.loads((source / 'EXPORT.JSON').read_bytes())
    if manifest.get('version') != 1:
        raise ValueError('Unsupported backup format')
    files = manifest['files']
    if 'vault.sqlite' not in files:
        raise ValueError('Missing vault database')
    for name, sha in files.items():
        p = Path(name)
        if p.is_absolute() or '..' in p.parts or not (name == 'vault.sqlite' or (len(p.parts) == 3 and p.parts[0] == 'objects')):
            raise ValueError('Invalid export path')
        if digest((source / p).read_bytes()) != sha:
            raise ValueError('Backup checksum mismatch')
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=False)
    for name in files:
        durable(destination / name, (source / name).read_bytes())
    verify(destination)
