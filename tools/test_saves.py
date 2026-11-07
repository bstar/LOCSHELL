#!/usr/bin/env python3
"""Exercise save loss, concurrency, corruption, authentication and export recovery."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'management'))
from saves.vault import Vault, Conflict, digest, reconcile
from saves.backup import export, import_backup
from saves.server import Handler
from saves.replica import Replica, RecoveryRequired
from http.server import ThreadingHTTPServer
import json
import tempfile
import threading
import unittest
import urllib.request
import urllib.error

class SaveTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.v = Vault(self.root / 'vault')
        self.c = self.v.create('monkey2', digest(b'edition'), 'Main')['id']
        self.a, self.b, self.d = (digest(x) for x in (b'first', b'second', b'third'))
        for x in (b'first', b'second', b'third'):
            self.v.put_blob(digest(x), x)
    def tearDown(self):
        self.v.close()
        self.tmp.cleanup()
    def capture(self, files, parents=()):
        return self.v.capture(self.c, files, list(parents), 'card-1', '386')
    def test_dashboard_report_is_backup_metadata(self):
        from saves.dashboard import report, encode
        empty=report(self.v.root)
        self.assertEqual((empty['state'],empty['games'],empty['snapshots']),('EMPTY',0,0))
        ident=self.capture({'SAVEGAME.001':self.a})
        self.v.publish(self.c,ident,None,'dashboardoperation0001')
        result=report(self.v.root)
        self.assertEqual((result['mode'],result['state'],result['games'],result['snapshots']),('BACKUP','AVAILABLE',1,1))
        self.assertEqual(self.v.campaign(self.c)['head'],ident)
        data=encode(result)
        self.assertTrue(data.startswith(b'LOC_VAULT 1\r\n'))
        self.assertIn(b'Automatic sync is off',data)
        self.assertNotIn(b'SYNCED',data)
    def test_dashboard_rejects_damaged_published_metadata(self):
        from saves.dashboard import report
        ident=self.capture({'SAVEGAME.001':self.a})
        self.v.publish(self.c,ident,None,'dashboardoperation0002')
        self.v.db.execute('UPDATE snapshots SET document=? WHERE id=?',(b'{}',ident))
        with self.assertRaises(ValueError): report(self.v.root)

    def test_three_way(self):
        base = {'SAVEGAME.001': self.a}
        left = {**base, 'SAVEGAME.002': self.b}
        right = {**base, 'SAVEGAME.003': self.d}
        merged, conflicts = reconcile(base, left, right, independent=True)
        self.assertEqual(merged, {**left, **right})
        self.assertEqual(conflicts, [])
        self.assertEqual(reconcile(base, left, right)[1], ['saved-world'])
        self.assertEqual(reconcile(base, {'SAVEGAME.001': self.b}, {'SAVEGAME.001': self.d}, True)[1], ['SAVEGAME.001'])
    def test_absence_is_not_deletion(self):
        base = {'SAVEGAME.001': self.a}
        self.assertEqual(reconcile(base, {}, base), (base, []))
        self.assertEqual(reconcile(base, {}, base, allow_deletes=True), ({}, []))
    def test_cas_and_retry(self):
        first = self.capture({'SAVEGAME.001': self.a})
        second = self.capture({'SAVEGAME.001': self.b})
        self.v.publish(self.c, first, None, 'operation00000001')
        with self.assertRaises(Conflict):
            self.v.publish(self.c, second, None, 'operation00000002')
        self.assertEqual(len(self.v.history(self.c)), 2)
        self.assertEqual(self.v.publish(self.c, first, None, 'operation00000001'), first)
        with self.assertRaises(ValueError):
            self.v.publish(self.c, second, None, 'operation00000001')
        second = self.capture({'SAVEGAME.001': self.b}, [first])
        self.v.publish(self.c, second, first, 'operation00000002')
        self.assertEqual(self.v.publish(self.c, first, None, 'operation00000001'), first)
        self.assertEqual(self.v.campaign(self.c)['head'], second)
    def test_corruption_and_paths(self):
        with self.assertRaises(ValueError): self.capture({'../EVIL': self.a})
        with self.assertRaises(FileNotFoundError): self.capture({'SAVEGAME.001': digest(b'missing')})
        self.v.blob_path(self.a).write_bytes(b'damaged')
        with self.assertRaises(ValueError): self.v.get_blob(self.a)
        with self.assertRaises(ValueError): self.v.put_blob(self.a, b'first')
    def test_backup_roundtrip_and_damage(self):
        ident = self.capture({'SAVEGAME.001': self.a})
        self.v.publish(self.c, ident, None, 'operation00000001')
        export(self.v.root, self.root / 'export')
        import_backup(self.root / 'export', self.root / 'restored')
        restored = Vault(self.root / 'restored')
        self.assertEqual(restored.campaign(self.c)['head'], ident)
        self.assertEqual(restored.get_blob(self.a), b'first')
        restored.close()
        (self.root / 'export' / 'vault.sqlite').write_bytes(b'corrupt')
        with self.assertRaises(ValueError): import_backup(self.root / 'export', self.root / 'bad')
        self.assertFalse((self.root / 'bad').exists())
    def test_two_writers(self):
        candidates = [self.capture({'SAVEGAME.001': x}) for x in (self.a, self.b)]
        barrier = threading.Barrier(2)
        outcomes = []
        def publish(i):
            vault = Vault(self.v.root)
            try:
                barrier.wait()
                try: vault.publish(self.c, candidates[i], None, 'concurrent0000000' + str(i)); outcomes.append('won')
                except Conflict: outcomes.append('conflict')
            finally: vault.close()
        threads = [threading.Thread(target=publish, args=(i,)) for i in range(2)]
        for t in threads: t.start()
        for t in threads: t.join()
        self.assertEqual(sorted(outcomes), ['conflict', 'won'])
    def replica(self, label):
        root = self.root / label
        root.mkdir()
        return Replica(root, self.root / (label + '-state'), ['SAVEGAME.001', 'SAVEGAME.002'], 'monkey2', digest(b'edition'))
    def test_independent_machines_and_conflict_preservation(self):
        left, right = self.replica('left'), self.replica('right')
        (left.root / 'SAVEGAME.001').write_bytes(b'first')
        left.sync(self.v, self.c, '386', True)
        right.sync(self.v, self.c, '486', True)
        (left.root / 'SAVEGAME.001').write_bytes(b'second')
        (right.root / 'SAVEGAME.002').write_bytes(b'third')
        right.sync(self.v, self.c, '486', True)
        self.assertEqual(left.sync(self.v, self.c, '386', True)['state'], 'synced')
        self.assertEqual((left.root / 'SAVEGAME.002').read_bytes(), b'third')
        right.sync(self.v, self.c, '486', True)
        (left.root / 'SAVEGAME.001').write_bytes(b'left conflict')
        (right.root / 'SAVEGAME.001').write_bytes(b'right conflict')
        right.sync(self.v, self.c, '486', True)
        result = left.sync(self.v, self.c, '386', True)
        self.assertEqual(result['state'], 'conflict')
        self.assertEqual((left.root / 'SAVEGAME.001').read_bytes(), b'left conflict')
        snapshot = self.v.snapshot(result['local'])
        self.assertEqual(self.v.get_blob(snapshot['files']['SAVEGAME.001']), b'left conflict')
    def test_recovery_at_every_file_boundary(self):
        for boundary in range(2):
            r = self.replica('recovery' + str(boundary))
            (r.root / 'SAVEGAME.001').write_bytes(b'before')
            (r.root / 'ASSET.DAT').write_bytes(b'untouched')
            from saves.vault import durable
            durable(r.state / 'objects' / self.a, b'first')
            durable(r.state / 'objects' / self.b, b'second')
            with self.assertRaises(RecoveryRequired):
                r.install({'SAVEGAME.001': self.a, 'SAVEGAME.002': self.b}, fail_after=boundary)
            with self.assertRaises(RecoveryRequired): r.files()
            r.recover()
            self.assertEqual((r.root / 'SAVEGAME.001').read_bytes(), b'before')
            self.assertFalse((r.root / 'SAVEGAME.002').exists())
            self.assertEqual((r.root / 'ASSET.DAT').read_bytes(), b'untouched')
    def test_initial_divergence_is_preserved(self):
        a, b = self.replica('initial-a'), self.replica('initial-b')
        (a.root / 'SAVEGAME.001').write_bytes(b'first')
        (b.root / 'SAVEGAME.001').write_bytes(b'second')
        a.sync(self.v, self.c, '386')
        result = b.sync(self.v, self.c, '486')
        self.assertEqual(result['state'], 'conflict')
        self.assertEqual((b.root / 'SAVEGAME.001').read_bytes(), b'second')
    def test_conflict_decision_retry_and_fork(self):
        from saves.decisions import choose, fork
        a = self.capture({'SAVEGAME.001': self.a})
        b = self.capture({'SAVEGAME.001': self.b})
        self.v.publish(self.c, a, None, 'operation00000001')
        chosen = choose(self.v, self.c, b, a, 'decision00000001')
        self.assertEqual(choose(self.v, self.c, b, a, 'decision00000001'), chosen)
        with self.assertRaises(ValueError): choose(self.v, self.c, a, a, 'decision00000001')
        other = fork(self.v, self.c, a, 'Other progress')
        self.assertEqual(self.v.snapshot(other['head'])['files'], {'SAVEGAME.001': self.a})
        self.assertEqual(self.v.campaign(self.c)['head'], chosen)
    def test_edition_mismatch(self):
        r = self.replica('wrong-edition')
        c = self.v.create('monkey-ega', digest(b'other edition'), 'Main')['id']
        with self.assertRaises(ValueError): r.sync(self.v, c, '386')
        self.assertFalse(self.v.history(c))
    def test_cli_host_commands_do_not_connect_to_dos(self):
        import dosctl
        from unittest.mock import patch
        from contextlib import redirect_stdout
        import io
        with patch.object(sys, 'argv', ['dosctl', 'saves', '--vault', str(self.v.root), 'status']):
            with patch.object(dosctl, 'connect', side_effect=AssertionError('Unexpected DOS connection')):
                with redirect_stdout(io.StringIO()) as output:
                    self.assertEqual(dosctl.command_main(), 0)
                self.assertEqual(json.loads(output.getvalue())['campaigns'][0]['id'], self.c)
    def test_http(self):
        server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
        server.root, server.token = self.v.root, 'test-token'
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        try:
            url = 'http://127.0.0.1:%d/v1/campaigns' % server.server_port
            with self.assertRaises(urllib.error.HTTPError) as exc: urllib.request.urlopen(url)
            self.assertEqual(exc.exception.code, 401)
            exc.exception.close()
            request = urllib.request.Request(url, headers={'Authorization': 'Bearer test-token'})
            self.assertEqual(json.load(urllib.request.urlopen(request))[0]['id'], self.c)
        finally:
            server.shutdown(); thread.join(); server.server_close()

if __name__ == '__main__': unittest.main()
