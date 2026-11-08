"""Contained import preserves state, refuses replay, and updates named bootstrap files."""
import ftplib
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace as N
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'management'))
import installation_import as imp

class FTP:
    def __init__(self):
        self.files={'/OLDAPP/LIBRARY.TXT':b'Existing game\r\nD:\\PLAY.EXE\r\n',
                    '/OLDAPP/OLD.CFG':b'VIEW=7\r\nLOCK=1\r\n',
                    '/OLDAPP/LEARN.DAT':b'original-memory-records',
                    '/OLDNET/MTCP.CFG':b'FILE C:\\OLDNET\\USERS.TXT\r\n',
                    '/OLDNET/USERS.TXT':b'private-example-settings',
                    '/OLDISLE/SCENE.EGA':b'original-image',
                    '/OLDISLE/VOLUME.CFG':b'21\r\n',
                    '/AUTOEXEC.BAT':b'CALL C:\\OLDAPP\\LOCSHELL.BAT\r\n'}
        self.dirs={'/','/OLDAPP','/OLDNET','/OLDISLE'}
    def __enter__(self):return self
    def __exit__(self,*args):pass
    def cwd(self,path):
        if path not in self.dirs:raise ftplib.error_perm('550 missing directory')
    def sendcmd(self,cmd):return '211 Disk C has 100000000 free bytes' if cmd=='SITE DISKFREE C' else '211 layout=2'
    def mkd(self,path):
        if (path.rsplit('/',1)[0] or '/') not in self.dirs:raise ftplib.error_perm('550 parent missing')
        self.dirs.add(path)
    def nlst(self,path):
        prefix=path+'/'
        return sorted({p[len(prefix):].split('/')[0] for p in [*self.files,*self.dirs] if p.startswith(prefix)})
    def rename(self,old,new):
        if old in self.dirs:
            if new.rsplit('/',1)[0] not in self.dirs:raise ftplib.error_perm('550 parent missing')
            for name in list(self.files):
                if name.startswith(old+'/'):self.files[new+name[len(old):]]=self.files.pop(name)
            self.dirs={new+name[len(old):] if name==old or name.startswith(old+'/') else name for name in self.dirs}
            return
        if new in self.files:raise ftplib.error_perm('550 already exists')
        self.files[new]=self.files.pop(old)

class Import(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.ftp=FTP();self.package=self.root/'package';tree=self.package/'LOCSHELL';tree.mkdir(parents=True)
        files={}
        for name in ['LOCSHELL.EXE','LOCHAND.EXE','LOCSHELL.BAT','LCSMAIN.BAT','LIBRARY.TXT']:
            data=('new-'+name).encode();(tree/name).write_bytes(data);files[name]=hashlib.sha256(data).hexdigest()
        (self.package/'package.json').write_text(json.dumps({'files':files}))
        def read(f,p):
            if p not in f.files:raise ftplib.error_perm('550 missing file')
            return f.files[p]
        self.api=N(recipes=N(BASE=self.root/'state'),ftplib=ftplib,connect=lambda a:self.ftp,
                   read=read,put=lambda f,p,b:f.files.__setitem__(p,b))
        self.a=N(cmd='import-installation',shell='/OLDAPP',network='/OLDNET',scene='/OLDISLE',preferences='OLD.CFG',
                 package=self.package,startup=['/AUTOEXEC.BAT'],dry_run=False,credentials=None)
    def tearDown(self):self.temp.cleanup()
    def test_complete_import(self):
        before=dict(self.ftp.files);r=imp.handle(self.a,self.api)
        self.assertEqual(r['state'],'ready-for-local-start')
        self.assertFalse(r['runtime_verified'])
        for name in ['LIBRARY.TXT','LEARN.DAT']:self.assertEqual(self.ftp.files['/LOCSHELL/'+name],before['/OLDAPP/'+name])
        self.assertEqual(self.ftp.files['/LOCSHELL/LOCSHELL.CFG'],before['/OLDAPP/OLD.CFG'])
        self.assertEqual(self.ftp.files['/LOCSHELL/ISLAND/SCENE.EGA'],b'original-image')
        self.assertEqual(self.ftp.files['/LOCSHELL/ISLAND/VOLUME.CFG'],b'21\r\n')
        self.assertIn(b'C:\\LOCSHELL\\NET\\USERS.TXT',self.ftp.files['/LOCSHELL/NET/MTCP.CFG'])
        self.assertEqual(self.ftp.files['/AUTOEXEC.BAT'],b'CALL C:\\LOCSHELL\\LOCSHELL.BAT\r\n')
        self.assertTrue(all(p in before or p.startswith('/LOCSHELL/') for p in self.ftp.files))
        for name,data in before.items():
            if name!='/AUTOEXEC.BAT':self.assertEqual(self.ftp.files[name],data)
        with self.assertRaisesRegex(RuntimeError,'already exists'):imp.handle(self.a,self.api)
    def test_dry_run_has_no_remote_writes(self):
        before=dict(self.ftp.files);dirs=set(self.ftp.dirs);self.a.dry_run=True
        r=imp.handle(self.a,self.api)
        self.assertEqual(r['state'],'dry-run');self.assertEqual(before,self.ftp.files);self.assertEqual(dirs,self.ftp.dirs)
    def test_overlapping_sources_are_rejected_before_connection(self):
        self.a.network='/OLDAPP/NET';before=dict(self.ftp.files)
        with self.assertRaisesRegex(ValueError,'non-overlapping'):imp.handle(self.a,self.api)
        self.assertEqual(before,self.ftp.files)
    def test_script_root_paths_and_bare_aliases(self):
        source=b'C:\\OLDNET\\TOOL.EXE\r\nCD \\OLDNET\r\nD:\\OLDNET\\KEEP.EXE\r\nOLDUTIL.EXE -help\r\n'
        result=imp.rewrite(source,[('/OLDNET','/LOCSHELL/NET')],[('OLDUTIL.EXE','LOCSOLO.EXE')])
        self.assertEqual(result,b'C:\\LOCSHELL\\NET\\TOOL.EXE\r\nCD \\LOCSHELL\\NET\r\nD:\\OLDNET\\KEEP.EXE\r\nLOCSOLO.EXE -help\r\n')
    def test_pending_request_blocks_import(self):
        self.ftp.files['/OLDNET/AGENT/LOCAL.REQ']=b'pending';before=dict(self.ftp.files)
        with self.assertRaisesRegex(RuntimeError,'outstanding'):imp.handle(self.a,self.api)
        self.assertEqual(before,self.ftp.files)
    def test_package_corruption_blocks_every_write(self):
        (self.package/'LOCSHELL/LOCSHELL.EXE').write_bytes(b'broken');before=dict(self.ftp.files)
        with self.assertRaisesRegex(ValueError,'hash'):imp.handle(self.a,self.api)
        self.assertEqual(before,self.ftp.files)
    def test_catalogue_paths_require_review(self):
        self.ftp.files['/OLDAPP/LIBRARY.TXT']=b'Utility\nC:\\OLDAPP\\HELP.EXE\n';before=dict(self.ftp.files)
        with self.assertRaisesRegex(RuntimeError,'merge'):imp.handle(self.a,self.api)
        self.assertEqual(before,self.ftp.files)
    def test_empty_job_directory_refused(self):
        self.ftp.dirs.add('/OLDNET/JOBS/LOCK')
        with self.assertRaisesRegex(RuntimeError,'incomplete'):imp.handle(self.a,self.api)
    def test_archive_keeps_entire_previous_tree(self):
        self.ftp.files['/OLDISLE/HISTORY.BAK']=b'older-image'
        result=imp.handle(self.a,self.api)
        archived=imp.archive_import(N(import_id=result['id']),self.api)
        self.assertEqual(archived['state'],'archived')
        self.assertEqual(self.ftp.files['/LOCSHELL/BACKUP/'+result['id']+'/TREE2/HISTORY.BAK'],b'older-image')
        self.assertNotIn('/OLDAPP',self.ftp.dirs)
    def test_path_prefix_boundaries(self):
        old=b'A C:\\OLDNET2\\FILE\r\nB C:\\OLDNET\\FILE\r\n'
        self.assertEqual(imp.rewrite(old,[('/OLDNET','/LOCSHELL/NET')]),b'A C:\\OLDNET2\\FILE\r\nB C:\\LOCSHELL\\NET\\FILE\r\n')

if __name__=='__main__':unittest.main()
