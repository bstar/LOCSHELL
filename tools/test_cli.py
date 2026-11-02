#!/usr/bin/env python3
import importlib.util,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
s=importlib.util.spec_from_file_location('dosctl',Path(__file__).resolve().parents[1]/'management/dosctl.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class FTP:
 def __init__(self,quit=True,fail=False):self.files={};self.commands=[];self.quit=quit;self.fail=fail
 def __enter__(self):return self
 def __exit__(self,*a):pass
 def sendcmd(self,s):
  self.commands.append(s)
  if s=='SITE HELP':return '211 HELP QUIT' if self.quit else '211 HELP'
  base='/NET/JOBS/LOCK';self.files[base+'/STATE.TXT']=b'completed';self.files[base+'/RESULT.TXT']=b'program_exit_code\n0';self.files[base+'/OUTPUT.TXT']=b'ok';return '211 OK'
 def mkd(self,p):pass
 def storbinary(self,s,b):
  if self.fail:raise OSError('disk full')
  self.files[s[5:]]=b.read()
 def retrbinary(self,s,write):write(self.files[s[5:]])
 def rename(self,a,b):
  for k in list(self.files):
   if k==a or k.startswith(a+'/'):self.files[b+k[len(a):]]=self.files.pop(k)
class Tests(unittest.TestCase):
 def invoke(self,f):
  with patch.object(m,'connect',return_value=f),patch.object(m.time,'sleep'),patch.object(sys,'argv',['dosctl','exec','C:\\DOS\\MEM.EXE']):return m.main()
 def test_success(self):
  f=FTP();self.assertEqual(self.invoke(f),0);self.assertEqual(f.commands.count('SITE QUIT'),1);self.assertFalse(any('/LOCK/' in k for k in f.files))
 def test_unsupported(self):
  f=FTP(quit=False)
  with self.assertRaises(RuntimeError):self.invoke(f)
  self.assertFalse(f.files)
 def test_incomplete(self):
  f=FTP(fail=True)
  with self.assertRaises(OSError):self.invoke(f)
  self.assertNotIn('SITE QUIT',f.commands);self.assertFalse(any(k.endswith('/READY') for k in f.files))
unittest.main()
