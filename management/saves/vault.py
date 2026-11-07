"""Durable immutable save storage and conservative three-way reconciliation."""
import hashlib
import json
import os
from pathlib import Path
import re
import secrets
import sqlite3
import tempfile
import time

HEX = re.compile(r'[0-9a-f]{64}\Z')
NAME = re.compile(r'[A-Z0-9_!#$%&\-{}()@^`]{1,8}(?:\.[A-Z0-9_!#$%&\-{}()@^`]{1,3})?\Z')
GAMES = {'u6', 'savage', 'martian', 'monkey-ega', 'monkey2'}

class Conflict(Exception):
    def __init__(self, head):
        self.head = head
        super().__init__('Campaign changed; reconcile against its current head')

def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode()

def digest(data):
    return hashlib.sha256(data).hexdigest()

def durable(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, name = tempfile.mkstemp(dir=path.parent)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(data)
            f.flush()
            os.fsync(f.fileno())
        os.replace(name, path)
        fd = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
    finally:
        if os.path.exists(name):
            os.unlink(name)

def validate_files(files):
    if not isinstance(files, dict) or len(files) > 512:
        raise ValueError('Invalid file manifest')
    for path, sha in files.items():
        if not isinstance(path, str) or any(not NAME.fullmatch(p) for p in path.split('/')) or path.count('/') > 2:
            raise ValueError('Expected relative uppercase DOS 8.3 path')
        if not isinstance(sha, str) or not HEX.fullmatch(sha):
            raise ValueError('Invalid content hash')

def reconcile(base, local, remote, independent=False, allow_deletes=False):
    """Return (merged manifest, conflicts). Missing local files are not deletions."""
    for files in (base, local, remote):
        validate_files(files)
    if not allow_deletes:
        local = {**base, **local}
    if local == remote or remote == base:
        return dict(local), []
    if local == base:
        return dict(remote), []
    if not independent:
        return None, ['saved-world']
    result, conflicts = {}, []
    for name in sorted(base.keys() | local.keys() | remote.keys()):
        b, l, r = base.get(name), local.get(name), remote.get(name)
        if l == r or r == b:
            chosen = l
        elif l == b:
            chosen = r
        else:
            conflicts.append(name)
            continue
        if chosen is not None:
            result[name] = chosen
    return (None if conflicts else result), conflicts

class Vault:
    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.db = sqlite3.connect(self.root / 'vault.sqlite', timeout=30, isolation_level=None)
        self.db.row_factory = sqlite3.Row
        self.db.execute('PRAGMA journal_mode=WAL')
        self.db.execute('PRAGMA synchronous=FULL')
        self.db.executescript('''
        CREATE TABLE IF NOT EXISTS campaigns(id TEXT PRIMARY KEY, game TEXT NOT NULL,
          compatibility TEXT NOT NULL, name TEXT NOT NULL, head TEXT);
        CREATE TABLE IF NOT EXISTS snapshots(id TEXT PRIMARY KEY, campaign TEXT NOT NULL,
          document BLOB NOT NULL);
        CREATE TABLE IF NOT EXISTS publications(operation TEXT PRIMARY KEY, campaign TEXT NOT NULL,
          snapshot TEXT NOT NULL, expected TEXT);
        ''')

    def close(self):
        self.db.close()

    def blob_path(self, sha):
        if not isinstance(sha, str) or not HEX.fullmatch(sha):
            raise ValueError('Invalid hash')
        return self.root / 'objects' / sha[:2] / sha[2:]

    def put_blob(self, sha, data):
        if digest(data) != sha:
            raise ValueError('Content hash mismatch')
        path = self.blob_path(sha)
        if path.exists():
            self.get_blob(sha)
        else:
            durable(path, data)
        return sha

    def get_blob(self, sha):
        data = self.blob_path(sha).read_bytes()
        if digest(data) != sha:
            raise ValueError('Damaged vault object')
        return data

    def campaigns(self):
        return [dict(r) for r in self.db.execute('SELECT * FROM campaigns ORDER BY game,name,id')]

    def campaign(self, ident):
        row = self.db.execute('SELECT * FROM campaigns WHERE id=?', (ident,)).fetchone()
        if row is None:
            raise KeyError('Unknown campaign')
        return dict(row)

    def create(self, game, compatibility, name):
        if game not in GAMES or not isinstance(compatibility, str) or not HEX.fullmatch(compatibility):
            raise ValueError('Unknown game or invalid compatibility fingerprint')
        if not isinstance(name, str) or not name.strip() or len(name) > 80:
            raise ValueError('Invalid campaign name')
        ident = secrets.token_hex(16)
        self.db.execute('INSERT INTO campaigns VALUES(?,?,?,?,NULL)', (ident, game, compatibility, name))
        return self.campaign(ident)

    def snapshot(self, ident):
        row = self.db.execute('SELECT document FROM snapshots WHERE id=?', (ident,)).fetchone()
        if row is None:
            raise KeyError('Unknown snapshot')
        document = bytes(row[0])
        if digest(document) != ident:
            raise ValueError('Damaged snapshot metadata')
        return json.loads(document)

    def capture(self, campaign, files, parents, replica, machine):
        self.campaign(campaign)
        validate_files(files)
        if not isinstance(parents, list) or len(parents) > 8 or len(set(parents)) != len(parents):
            raise ValueError('Invalid parents')
        for parent in parents:
            if self.snapshot(parent)['campaign'] != campaign:
                raise ValueError('Parent belongs to another campaign')
        for value in (replica, machine):
            if not isinstance(value, str) or not value or len(value) > 128:
                raise ValueError('Invalid source identity')
        for sha in set(files.values()):
            self.get_blob(sha)
        document = encoded(dict(campaign=campaign, files=files, parents=parents,
                                replica=replica, machine=machine, received_ns=time.time_ns()))
        ident = digest(document)
        self.db.execute('INSERT INTO snapshots VALUES(?,?,?)', (ident, campaign, document))
        return ident

    def publish(self, campaign, snapshot, expected, operation):
        if not isinstance(operation, str) or not re.fullmatch('[a-zA-Z0-9-]{16,80}', operation):
            raise ValueError('Invalid operation ID')
        self.db.execute('BEGIN IMMEDIATE')
        try:
            prior = self.db.execute('SELECT * FROM publications WHERE operation=?', (operation,)).fetchone()
            if prior:
                if (prior['campaign'], prior['snapshot'], prior['expected']) != (campaign, snapshot, expected):
                    raise ValueError('Operation ID reused with different arguments')
                self.db.execute('COMMIT')
                return snapshot
            head = self.campaign(campaign)['head']
            if head != expected:
                raise Conflict(head)
            doc = self.snapshot(snapshot)
            if doc['campaign'] != campaign or (head and head not in doc['parents']):
                raise ValueError('Publication must descend from the current campaign head')
            for sha in set(doc['files'].values()):
                self.get_blob(sha)
            self.db.execute('UPDATE campaigns SET head=? WHERE id=?', (snapshot, campaign))
            self.db.execute('INSERT INTO publications VALUES(?,?,?,?)', (operation, campaign, snapshot, expected))
            self.db.execute('COMMIT')
            return snapshot
        except BaseException:
            if self.db.in_transaction:
                self.db.execute('ROLLBACK')
            raise

    def history(self, campaign):
        self.campaign(campaign)
        return [dict(id=r['id'], **json.loads(r['document'])) for r in self.db.execute(
            'SELECT id,document FROM snapshots WHERE campaign=? ORDER BY rowid DESC', (campaign,))]
