import unittest,tempfile,json,sys
from pathlib import Path
from types import SimpleNamespace as N
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'management'))
import flows
class Tests(unittest.TestCase):
 def setUp(self):
  self.t=tempfile.TemporaryDirectory();self.base=Path(self.t.name);self.api=N(recipes=N(BASE=self.base),state_dir=lambda a:self.base/'scope');self.a=N(flow='T123ABCD',credentials=None,timeout=1,flow_command='run');self.p=flows.path_for(self.api,self.a.flow);self.p.parent.mkdir(parents=True)
 def tearDown(self):self.t.cleanup()
 def test_cancel_does_not_submit(self):
  flows.save(self.p,{'id':self.a.flow,'state':'ready','cancel_requested':True})
  with patch.object(flows,'invoke') as invoke:
   result=flows.step(self.a,self.api);self.assertEqual(result['state'],'cancelled');invoke.assert_not_called()
 def test_uncertain_not_replayed(self):
  flows.save(self.p,{'id':self.a.flow,'state':'uncertain','cancel_requested':False})
  with patch.object(flows,'invoke') as invoke:
   self.assertEqual(flows.step(self.a,self.api)['state'],'uncertain');invoke.assert_not_called()
 def test_cancel_survives_checkpoint(self):
  flows.save(self.p,{'id':self.a.flow,'state':'waiting','cancel_requested':True});old={'id':self.a.flow,'state':'ready','cancel_requested':False};flows.checkpoint(self.p,old);self.assertTrue(json.loads(self.p.read_text())['cancel_requested'])
 def test_scope_mismatch(self):
  flows.save(self.p,{'id':self.a.flow,'state':'ready','cancel_requested':False,'scope':'other'})
  with self.assertRaises(ValueError):flows.step(self.a,self.api)
unittest.main()
