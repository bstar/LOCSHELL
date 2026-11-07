"""Read-only backup status for LOC SHELL. Never implies local saves are synced."""
import datetime
import hashlib
import json
from pathlib import Path
import sqlite3

def report(root):
    database=Path(root)/'vault.sqlite'
    if not database.is_file():
        raise FileNotFoundError('Vault does not exist')
    connection=sqlite3.connect(database.resolve().as_uri()+'?mode=ro',uri=True)
    try:
        connection.execute('BEGIN')
        rows=connection.execute('SELECT game,head FROM campaigns').fetchall()
        snapshots=connection.execute('SELECT COUNT(*) FROM snapshots').fetchone()[0]
        last=0
        for game,head in rows:
            if head:
                row=connection.execute('SELECT document FROM snapshots WHERE id=?',(head,)).fetchone()
                data=bytes(row[0]) if row else None
                if data is None or hashlib.sha256(data).hexdigest()!=head:
                    raise ValueError('Damaged published snapshot metadata')
                last=max(last,json.loads(data)['received_ns'])
        now=datetime.datetime.now(datetime.timezone.utc)
        return dict(mode='BACKUP',state='AVAILABLE' if any(head for game,head in rows) else 'EMPTY',
                    games=len({game for game,head in rows if head}),snapshots=snapshots,
                    last=datetime.datetime.fromtimestamp(last/1e9,datetime.timezone.utc).strftime('%Y-%m-%d %H:%MZ') if last else '',
                    reported=now.strftime('%Y-%m-%d %H:%MZ'),
                    detail='Backup snapshots only. Automatic sync is off; local changes are unchecked.')
    finally:
        connection.close()

def encode(status):
    keys=('mode','state','games','snapshots','last','reported','detail')
    lines=['LOC_VAULT 1']
    for key in keys:
        value=str(status[key])
        if '\r' in value or '\n' in value:
            raise ValueError('Invalid dashboard report value')
        lines.append(key.upper()+'='+value)
    return ('\r\n'.join(lines)+'\r\n').encode('ascii')
