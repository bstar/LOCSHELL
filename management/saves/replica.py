"""Filesystem reference client with crash-recoverable installation.

This implementation is also the protocol oracle for the native DOS client.
Profiles must be independently validated before enrollment of real saves.
"""
import json
from pathlib import Path
import secrets
from .vault import durable, digest, encoded, validate_files, reconcile, Conflict

class RecoveryRequired(Exception):
    pass

class Replica:
    def __init__(self, game_root, state_root, owned, game, compatibility):
        self.root, self.state = Path(game_root), Path(state_root)
        self.owned = tuple(owned)
        validate_files({p: '0' * 64 for p in self.owned})
        if len(set(p.casefold() for p in self.owned)) != len(self.owned):
            raise ValueError('Duplicate save paths')
        if self.root.resolve() == self.state.resolve() or self.root.resolve() in self.state.resolve().parents:
            raise ValueError('State must be outside the game installation')
        self.state.mkdir(parents=True, exist_ok=True)
        self.meta_path = self.state / 'REPLICA.JSON'
        if not self.meta_path.exists():
            self.save_meta({'id': secrets.token_hex(16), 'base': None, 'pending': [], 'campaign': None, 'game': game, 'compatibility': compatibility})
        self.meta = json.loads(self.meta_path.read_bytes())
        if (self.meta.get('game'), self.meta.get('compatibility')) != (game, compatibility):
            raise ValueError('Installation identity or edition changed')
        self.journal = self.state / 'RESTORE.JSON'

    def save_meta(self, value):
        durable(self.meta_path, encoded(value))

    def target(self, name):
        if name not in self.owned:
            raise ValueError('File is not owned by this game profile')
        path = self.root / name
        if not path.resolve().is_relative_to(self.root.resolve()):
            raise ValueError('Save path escapes installation')
        return path

    def files(self):
        if self.journal.exists():
            raise RecoveryRequired('Finish or roll back the interrupted restore')
        files = {}
        for name in self.owned:
            p = self.target(name)
            if p.is_file():
                data = p.read_bytes()
                sha = digest(data)
                durable(self.state / 'objects' / sha, data)
                files[name] = sha
        return files

    def local_blob(self, sha):
        data = (self.state / 'objects' / sha).read_bytes()
        if digest(data) != sha:
            raise ValueError('Damaged local recovery object')
        return data

    def snapshot(self):
        files = self.files()
        record = {'files': files, 'base': self.meta['base'], 'campaign': self.meta['campaign']}
        ident = digest(encoded(record))
        durable(self.state / 'snapshots' / ident, encoded(record))
        if ident not in self.meta['pending']:
            self.meta['pending'].append(ident)
            self.save_meta(self.meta)
        return files

    def install(self, files, fail_after=None):
        validate_files(files)
        if set(files) - set(self.owned):
            raise ValueError('Incoming snapshot contains unowned files')
        if self.journal.exists():
            raise RecoveryRequired('An installation is already interrupted')
        before = self.snapshot()
        for sha in set(files.values()):
            self.local_blob(sha)
        durable(self.journal, encoded({'before': before, 'after': files}))
        self._apply(files, fail_after)
        if self._read_targets() != files:
            raise ValueError('Installation verification failed')
        self._clear_journal()

    def _read_targets(self):
        return {n: digest(self.target(n).read_bytes()) for n in self.owned if self.target(n).is_file()}

    def _apply(self, files, fail_after=None):
        for index, name in enumerate(self.owned):
            path = self.target(name)
            if name in files:
                durable(path, self.local_blob(files[name]))
            elif path.exists():
                path.unlink()
            if fail_after is not None and index == fail_after:
                raise RecoveryRequired('Simulated power interruption')

    def _clear_journal(self):
        self.journal.unlink()
        import os
        fd = os.open(self.state, os.O_DIRECTORY | os.O_RDONLY)
        try: os.fsync(fd)
        finally: os.close(fd)

    def recover(self, finish=False):
        record = json.loads(self.journal.read_bytes())
        files = record['after' if finish else 'before']
        validate_files(files)
        if set(files) - set(self.owned):
            raise ValueError('Invalid recovery journal')
        self._apply(files)
        if self._read_targets() != files:
            raise ValueError('Recovery verification failed')
        self._clear_journal()

    def sync(self, vault, campaign, machine, independent=False):
        """Preserve local progress before comparing it with the hub.

        Caller must hold the installation's foreground/session lock. This
        reference transport is in-process; native transport uses the HTTP API.
        """
        if self.meta['campaign'] not in (None, campaign):
            raise ValueError('Switch campaigns explicitly before syncing')
        if (self.state / 'SESSION.JSON').exists():
            raise RecoveryRequired('Game session is still open')
        identity = vault.campaign(campaign)
        if (identity['game'], identity['compatibility']) != (self.meta['game'], self.meta['compatibility']):
            raise ValueError('Campaign edition does not match installation')
        local = self.snapshot()
        base_id = self.meta['base']
        base = vault.snapshot(base_id)['files'] if base_id else {}
        head = vault.campaign(campaign)['head']
        remote = vault.snapshot(head)['files'] if head else {}
        # Save the local branch BEFORE conflict handling, including every
        # queued offline checkpoint. Initial divergent installs remain siblings.
        self.meta['campaign'] = campaign
        self.save_meta(self.meta)
        parent = base_id
        for pending in self.meta['pending']:
            record = json.loads((self.state / 'snapshots' / pending).read_bytes())
            for sha in set(record['files'].values()):
                vault.put_blob(sha, self.local_blob(sha))
            parent = vault.capture(campaign, record['files'], [parent] if parent else [], self.meta['id'], machine)
        merged, conflicts = reconcile(base, local, remote, independent)
        if conflicts:
            return {'state': 'conflict', 'files': conflicts, 'local': parent, 'remote': head}
        for sha in set(merged.values()):
            durable(self.state / 'objects' / sha, vault.get_blob(sha))
        if merged == remote and head:
            chosen = head
        else:
            parents = list(dict.fromkeys(p for p in (head, parent) if p))
            chosen = vault.capture(campaign, merged, parents, self.meta['id'], machine)
            # Publish before local replacement: stale writers cannot install
            # a result computed against a head that another PC has advanced.
            vault.publish(campaign, chosen, head, secrets.token_hex(16))
        if merged != local:
            self.install(merged)
        self.meta.update(base=chosen, pending=[])
        self.save_meta(self.meta)
        return {'state': 'synced', 'head': chosen}
