import unittest,tempfile,json,hashlib,sys,ftplib,copy
from pathlib import Path
from types import SimpleNamespace as N
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'management'))
import operations
class FTP:
 def __init__(self):self.files={'/APP/A.EXE':b'old-a','/APP/B.EXE':b'old-b'};self.fail=False
 def __enter__(self):return self
 def __exit__(self,*a):pass
 def rename(self,a,b):
  if self.fail and b=='/APP/B.EXE' and a.endswith('.NEW'):self.fail=False;raise OSError('injected install failure')
  if b in self.files:raise ftplib.error_perm('550 exists')
  self.files[b]=self.files.pop(a)
class Tests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.base=Path(self.tmp.name);self.f=FTP()
  def read(f,p):
   if p not in f.files:raise ftplib.error_perm('550 missing')
   return f.files[p]
  self.api=N(recipes=N(BASE=self.base),ftplib=ftplib,connect=lambda a:self.f,read=read,put=lambda f,p,b:f.files.__setitem__(p,b))
  files=[]
  for x in ['A','B']:
   p=self.base/(x+'.EXE');p.write_bytes(('new-'+x).encode());files.append({'source':str(p),'destination':'/APP/'+x+'.EXE','sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
  self.manifest=self.base/'manifest.json';self.manifest.write_text(json.dumps({'files':files,'preserve':['/APP/*.PRM']}));self.a=N(manifest=self.manifest,credentials=None,dry_run=False)
 def tearDown(self):self.tmp.cleanup()
 def test_dryrun(self):
  before=copy.deepcopy(self.f.files);self.a.dry_run=True;r=operations.deploy(self.a,self.api);self.assertEqual(r['state'],'dry-run');self.assertEqual(before,self.f.files)
 def test_commit_restore(self):
  r=operations.deploy(self.a,self.api);self.assertEqual(r['state'],'installed');self.assertEqual(self.f.files['/APP/A.EXE'],b'new-A');operations.restore(N(release=r['id'],credentials=None),self.api);self.assertEqual(self.f.files['/APP/A.EXE'],b'old-a');self.assertEqual(self.f.files['/APP/B.EXE'],b'old-b')
 def test_failure_rolls_back_all(self):
  self.f.fail=True
  with self.assertRaises(operations.ReleaseFailure):operations.deploy(self.a,self.api)
  self.assertEqual(self.f.files['/APP/A.EXE'],b'old-a');self.assertEqual(self.f.files['/APP/B.EXE'],b'old-b')
 def test_preserve_configuration(self):
  data=json.loads(self.manifest.read_text());p=self.base/'USER.PRM';p.write_bytes(b'replacement-settings');self.f.files['/APP/USER.PRM']=b'keep-settings'
  data['files'].append({'source':str(p),'destination':'/APP/USER.PRM','sha256':hashlib.sha256(p.read_bytes()).hexdigest()});self.manifest.write_text(json.dumps(data));operations.deploy(self.a,self.api);self.assertEqual(self.f.files['/APP/USER.PRM'],b'keep-settings')
 def test_devices_refused(self):
  data=json.loads(self.manifest.read_text());data['files'][0]['destination']='/APP/NUL';self.manifest.write_text(json.dumps(data));before=copy.deepcopy(self.f.files)
  with self.assertRaises(ValueError):operations.deploy(self.a,self.api)
  self.assertEqual(before,self.f.files)
 def test_external_change_refused(self):
  r=operations.deploy(self.a,self.api);self.f.files['/APP/B.EXE']=b'outside-change';before=copy.deepcopy(self.f.files)
  with self.assertRaises(RuntimeError):operations.restore(N(release=r['id'],credentials=None),self.api)
  self.assertEqual(before,self.f.files)
unittest.main()
