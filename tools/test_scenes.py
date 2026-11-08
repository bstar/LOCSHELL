"""Scene installation boundaries, exact bytes, rollback and volume preservation."""
from pathlib import Path
from types import SimpleNamespace as N
import ftplib
import hashlib
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'management'))
import scenes
import operations

class FTP:
    def __init__(self):
        self.files={'/LOCSHELL/ISLAND/VOLUME.CFG':b'21\r\n','/LOCSHELL/ISLAND/SCENE.EGA':b'previous'}
        self.dirs={'/LOCSHELL','/LOCSHELL/ISLAND'}
        self.commands=[]
        self.capabilities='211 locshell=22 layout=2 saverselect=1'
        self.fail=False
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def sendcmd(self,cmd):self.commands.append(cmd);return self.capabilities if cmd=='SITE AGENT' else '211 OK'
    def cwd(self,path):
        if path not in self.dirs:raise ftplib.error_perm('550 no directory')
    def mkd(self,path):self.dirs.add(path)
    def rename(self,old,new):
        if self.fail and new.endswith('/SCENE.EGA'):
            self.fail=False
            raise OSError('injected commit failure')
        if new in self.files:raise ftplib.error_perm('550 already exists')
        self.files[new]=self.files.pop(old)

class Scenes(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.pack=self.root/'pack';self.pack.mkdir()
        for name in scenes.REQUIRED:(self.pack/name).write_bytes(('sample-'+name).encode())
        self.ftp=FTP()
        def read(ftp,path):
            if path not in ftp.files:raise ftplib.error_perm('550 missing')
            return ftp.files[path]
        self.api=N(recipes=N(BASE=self.root/'state'),ftplib=ftplib,connect=lambda a:self.ftp,
                   read=read,put=lambda f,p,b:f.files.__setitem__(p,b))
        self.a=N(credentials=None,dry_run=False,select=False,preview=False)
    def tearDown(self):self.tmp.cleanup()
    def install(self):return scenes.install_pack(self.a,self.api,self.pack,'a'*40)
    def test_payload_and_preferences(self):
        result=self.install()
        self.assertEqual(result['state'],'installed')
        self.assertEqual(result['files'],17)
        self.assertFalse(result['runtime_verified'])
        self.assertEqual(self.ftp.files['/LOCSHELL/ISLAND/VOLUME.CFG'],b'21\r\n')
        for name in scenes.REQUIRED:self.assertEqual(self.ftp.files['/LOCSHELL/ISLAND/'+name],(self.pack/name).read_bytes())
        self.assertTrue(all(p.startswith('/LOCSHELL/') for p in self.ftp.files))
    def test_dry_run_does_not_create_remote_directory(self):
        self.ftp.dirs.remove('/LOCSHELL/ISLAND');before=dict(self.ftp.files)
        self.a.dry_run=True;self.a.select=True;self.a.preview=True
        self.install()
        self.assertEqual(self.ftp.files,before)
        self.assertNotIn('/LOCSHELL/ISLAND',self.ftp.dirs)
        self.assertNotIn('SITE SAVER SELECT island',self.ftp.commands)
    def test_pending_job_blocks_every_write(self):
        self.ftp.files['/LOCSHELL/NET/JOBS/LOCK/ID.TXT']=b'J123ABCD';before=dict(self.ftp.files)
        with self.assertRaisesRegex(RuntimeError,'pending'):self.install()
        self.assertEqual(before,self.ftp.files)
    def test_old_server_refused(self):
        self.ftp.capabilities='211 locshell=22';before=dict(self.ftp.files)
        with self.assertRaisesRegex(RuntimeError,'upgrade'):self.install()
        self.assertEqual(before,self.ftp.files)
    def test_restore_undoes_only_owned_payload(self):
        result=self.install()
        operations.restore(N(release=result['release'],credentials=None),self.api)
        self.assertEqual(self.ftp.files['/LOCSHELL/ISLAND/SCENE.EGA'],b'previous')
        self.assertEqual(self.ftp.files['/LOCSHELL/ISLAND/VOLUME.CFG'],b'21\r\n')
    def test_failed_commit_rolls_back_and_does_not_select(self):
        self.ftp.fail=True;self.a.select=True
        with self.assertRaises(operations.ReleaseFailure):self.install()
        self.assertEqual(self.ftp.files['/LOCSHELL/ISLAND/SCENE.EGA'],b'previous')
        self.assertNotIn('SITE SAVER SELECT island',self.ftp.commands)
    def test_activation_only_after_verified_install(self):
        self.a.select=True;self.a.preview=True;self.install()
        self.assertEqual(self.ftp.commands[-2:],['SITE SAVER SELECT island','SITE SAVER PREVIEW island'])
    def test_incomplete_pack_never_contacts_device(self):
        (self.pack/'MOON.BIN').unlink()
        with self.assertRaises(ValueError):self.install()
        self.assertFalse(self.ftp.commands)
    def test_pinned_pack_generation(self):
        pin=scenes.prepare_pack(self.root/'real-pack')
        self.assertEqual(len(pin),40)
        self.assertEqual({p.name for p in (self.root/'real-pack').iterdir()},scenes.REQUIRED)
        self.assertNotIn('VOLUME.CFG',scenes.REQUIRED)

if __name__=='__main__':unittest.main()
