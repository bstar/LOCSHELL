#!/usr/bin/env python3
import sys,unittest,tempfile,os,json,subprocess
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'management'))
import recipes
class Tests(unittest.TestCase):
 def test_parse_inventory(self):
  rows,errors,end=recipes.parse_scan('D\tD:\\GAMES\nF\tD:\\GAMES\\X.EXE\t42\t12345678\nERROR\tE:\\\tread\nEND\t1\t1\n')
  self.assertEqual(rows[1]['size'],42);self.assertEqual(rows[1]['crc32'],'12345678');self.assertEqual(end['errors'],1);self.assertEqual(len(errors),1)
 def test_incomplete_scan(self):self.assertIsNone(recipes.parse_scan('F\tD:\\X\t4')[2])
 def test_custom_not_required(self):
  p=subprocess.run([sys.executable,str(Path(recipes.__file__).with_name('dosctl.py')),'--json','recipes'],env={**os.environ,'DOSCTL_CUSTOM_RECIPES':'/nonexistent'},capture_output=True,text=True,check=True)
  result=json.loads(p.stdout);self.assertEqual(len(result['recipes']),6);self.assertTrue(all(x['source']=='builtin' for x in result['recipes']))
if __name__=='__main__':unittest.main()
