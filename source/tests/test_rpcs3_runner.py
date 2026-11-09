"""Lifecycle tests for the benchmark tool. These use a fake process, not RPCS3."""
import hashlib,importlib.util,json,sys,tempfile,unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch
from PIL import Image
spec=importlib.util.spec_from_file_location('runner',Path(__file__).resolve().parents[1]/'scripts/benchmark-rpcs3.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)
class RunnerTests(unittest.TestCase):
 def run_case(self,root,fail=False,locked=False):
  config=root/'config';cache=root/'cache';data=config/'rpcs3/dev_hdd0/ruffle'
  (data/'games').mkdir(parents=True);(data/'profiles').mkdir();(data/'saves').mkdir()
  (data/'profiles/original').write_bytes(b'original profile');(data/'saves/original').write_bytes(b'original save')
  (data/'games/SuperMario63.swf').write_bytes(b'fixture')
  template=root/'state';(template/'profiles').mkdir(parents=True);(template/'saves').mkdir();(template/'profiles/fixture').write_bytes(b'benchmark profile')
  for name in ['rpcs3','xdotool']:(root/name).write_bytes(b'tool fixture')
  route=root/'route.json';route.write_text(json.dumps({'swf_name':'SuperMario63.swf','swf_sha256':hashlib.sha256(b'fixture').hexdigest(),'steps':[{'capture':'observed'}]}))
  lock=cache/'rpcs3/RPCS3.buf';lock.parent.mkdir(parents=True)
  if locked:lock.touch()
  clock=SimpleNamespace(t=0)
  def sleep(n):clock.t+=n
  class Process:
   pid=987654321;returncode=None
   def __init__(self,*a,**k):
    self.returncode=7 if fail else None
    (data/'ruffle-ps3-dev.log').write_text('main menu ready\n')
    (data/'saves/created-during-run').write_bytes(b'new save')
    lock.touch()
   def poll(self):return self.returncode
   def terminate(self):self.returncode=-15
   def wait(self,timeout=None):return self.returncode
   def kill(self):self.returncode=-9
  argv=['runner','--rpcs3',str(root/'rpcs3'),'--xdotool',str(root/'xdotool'),'--executable',str(root/'rpcs3'),'--swf',str(data/'games/SuperMario63.swf'),'--config-home',str(config),'--cache-home',str(cache),'--scenario',str(route),'--output',str(root/'output'),'--state-template',str(template)]
  with patch.object(sys,'argv',argv),patch.object(runner.subprocess,'Popen',Process),patch.object(runner.subprocess,'run',return_value=SimpleNamespace(stdout='123\n')),patch.object(runner.time,'monotonic',side_effect=lambda:clock.t),patch.object(runner.time,'sleep',side_effect=sleep),patch('PIL.ImageGrab.grab',return_value=Image.new('RGB',(1,1))):
   if fail or locked:
    with self.assertRaises(SystemExit):runner.main()
   else:runner.main()
  self.assertEqual((data/'profiles/original').read_bytes(),b'original profile')
  self.assertEqual((data/'saves/original').read_bytes(),b'original save')
  self.assertFalse((data/'profiles/fixture').exists());self.assertFalse((data/'saves/created-during-run').exists())
  if locked:
   self.assertTrue(lock.exists());self.assertFalse((root/'output').exists())
  else:
   self.assertFalse(lock.exists());self.assertFalse((data/'benchmark-enable').exists())
   self.assertEqual((root/'output/final-state/saves/created-during-run').read_bytes(),b'new save')
   status=json.loads((root/'output/run.json').read_text())['status']
   self.assertEqual(status,'blocked' if fail else 'route-completed-review-required')
 def test_success_restores_state_and_preserves_result(self):
  with tempfile.TemporaryDirectory() as d:self.run_case(Path(d))
 def test_failed_emulator_restores_state_and_saves_report(self):
  with tempfile.TemporaryDirectory() as d:self.run_case(Path(d),fail=True)
 def test_existing_lock_stops_before_modifying_state(self):
  with tempfile.TemporaryDirectory() as d:self.run_case(Path(d),locked=True)
if __name__=='__main__':unittest.main()
