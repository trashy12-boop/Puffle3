import importlib.util,unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('capture',Path(__file__).resolve().parents[1]/'scripts/summarize-draw-capture.py')
capture=importlib.util.module_from_spec(spec);spec.loader.exec_module(capture)
class CaptureTests(unittest.TestCase):
 def test_scene_boundaries_exclude_loading_and_next_scene(self):
  rows=[dict(completed_us=t,draw_gap_us=g,render_cpu_us=1000) for t,g in [(110000,90000),(120000,10000),(150000,30000),(200000,50000),(300000,100000)]]
  result=capture.summarize(rows,'Benchmark scene gameplay guest_us 100000\nBenchmark scene end guest_us 200000','gameplay')
  self.assertEqual(result['draw_gap_samples'],3)
  self.assertEqual(result['median_draw_gap_ms'],30)
  self.assertEqual(result['p99_draw_gap_ms'],50)
  self.assertAlmostEqual(result['drawn_fps_over_complete_gaps'],100/3)
 def test_incomplete_or_ambiguous_scene_fails(self):
  for log in ['','Benchmark scene a guest_us 1','Benchmark scene a guest_us 1\nBenchmark scene a guest_us 2\nBenchmark scene end guest_us 3']:
   with self.assertRaises(ValueError):capture.summarize([],log,'a')
 def test_insufficient_samples_fail(self):
  with self.assertRaises(ValueError):capture.summarize([],'Benchmark scene a guest_us 1\nBenchmark scene end guest_us 2','a')
if __name__=='__main__':unittest.main()
