#!/usr/bin/env python3
import importlib.util,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch,Mock
from types import SimpleNamespace
s=importlib.util.spec_from_file_location('dosctl',Path(__file__).resolve().parents[1]/'management/dosctl.py');m=importlib.util.module_from_spec(s);sys.modules[s.name]=m;s.loader.exec_module(m)
class FTP:
 def __init__(self,quit=True,fail=False):self.files={};self.commands=[];self.quit=quit;self.fail=fail
 def __enter__(self):return self
 def __exit__(self,*a):pass
 def sendcmd(self,s):
  self.commands.append(s)
  if s=='SITE AGENT':return '211 layout=2'
  if s=='SITE HELP':return '211 HELP QUIT' if self.quit else '211 HELP'
  base='/LOCSHELL/NET/JOBS/LOCK';self.files[base+'/STATE.TXT']=b'completed';self.files[base+'/RESULT.TXT']=b'program_exit_code\n0';self.files[base+'/OUTPUT.TXT']=b'ok';return '211 OK'
 def mkd(self,p):pass
 def storbinary(self,s,b):
  if self.fail:raise OSError('disk full')
  self.files[s[5:]]=b.read()
 def retrbinary(self,s,write):write(self.files[s[5:]])
 def rename(self,a,b):
  for k in list(self.files):
   if k==a or k.startswith(a+'/'):self.files[b+k[len(a):]]=self.files.pop(k)
class Tests(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.saved=m.recipes.BASE;m.recipes.BASE=Path(self.tmp.name)
 def tearDown(self):
  m.recipes.BASE=self.saved;self.tmp.cleanup()

 def invoke(self,f):
  with patch.object(m,'connect',return_value=f),patch.object(m.time,'sleep'),patch.object(sys,'argv',['dosctl','exec','C:\\DOS\\MEM.EXE']):return m.main()
 def test_success(self):
  f=FTP();self.assertEqual(self.invoke(f),0);self.assertEqual(f.commands.count('SITE QUIT'),1);self.assertFalse(any('/LOCK/' in k for k in f.files))
 def test_unsupported(self):
  f=FTP(quit=False)
  with self.assertRaises(RuntimeError):self.invoke(f)
  self.assertFalse(f.files)
 def test_resume_does_not_resubmit(self):
  f=FTP();base='/LOCSHELL/NET/JOBS/LOCK';job='J123ABCD'
  f.files.update({base+'/ID.TXT':job.encode(),base+'/STATE.TXT':b'completed',base+'/RESULT.TXT':b'program_exit_code\n0',base+'/OUTPUT.TXT':b'recovered'})
  a=SimpleNamespace(credentials=None,cmd='exec',label='recovery test');m.track(a,job,'waiting')
  with patch.object(m,'connect',return_value=f),patch.object(sys,'argv',['dosctl','resume']):m.main()
  self.assertFalse((m.state_dir(a)/'pending.json').exists());self.assertNotIn('SITE QUIT',f.commands)
  self.assertIn('/LOCSHELL/NET/JOBS/'+job+'/OUTPUT.TXT',f.files)
 def test_connection_slot_retry(self):
  import json,ftplib
  with tempfile.TemporaryDirectory() as directory:
   p=Path(directory)/'access.json';p.write_text(json.dumps({'host':'example.invalid','username':'test','password':'test'}))
   first=Mock();first.connect.side_effect=ftplib.error_temp('421 try later');second=Mock()
   with patch.object(m.ftplib,'FTP',side_effect=[first,second]),patch.object(m.time,'sleep') as delay:
    self.assertIs(m.connect(SimpleNamespace(credentials=p)),second);first.close.assert_called_once();delay.assert_called_once();second.sock.settimeout.assert_called_once_with(30);self.assertEqual(second.timeout,30)
 def test_bad_credentials_are_not_retried(self):
  import json,ftplib
  p=Path(self.tmp.name)/'access.json';p.write_text(json.dumps({'host':'example.invalid','username':'test','password':'test'}))
  ftp=Mock();ftp.login.side_effect=ftplib.error_perm('530 login failed')
  with patch.object(m.ftplib,'FTP',return_value=ftp) as factory,patch.object(m.time,'sleep') as delay:
   with self.assertRaises(ftplib.error_perm):m.connect(SimpleNamespace(credentials=p))
   factory.assert_called_once();ftp.close.assert_called_once();delay.assert_not_called()
 def test_incomplete(self):
  f=FTP(fail=True)
  with self.assertRaises(OSError):self.invoke(f)
  self.assertNotIn('SITE QUIT',f.commands);self.assertFalse(any(k.endswith('/READY') for k in f.files))
unittest.main()
