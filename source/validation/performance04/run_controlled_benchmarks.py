import subprocess
from pathlib import Path
w=Path.cwd();out=w/'measurements/controlled';out.mkdir(exist_ok=True)
replays=w/'source/RufflePS3/validation/performance04/replays'
for name,file,frames in [('mix','The_right_mix.swf',600),('bill','bulletbill.swf',1200),('mario','Super Mario 63.swf',2100)]:
 for mode,exe,cache in [('baseline','benchmark-scalar-clock','scalar'),('optimized','benchmark-simd-clock','cache'),('verify','benchmark-simd-clock','verify')]:
  prefix=out/f'{name}-{mode}'
  with open(str(prefix)+'.log','w') as f:
   subprocess.run([str(w/'tooling'/exe),str(w/'games'/file),str(frames),cache,str(prefix),str(replays/(name+'.txt'))],stdout=f,stderr=subprocess.STDOUT,check=True,timeout=180)
  print(name,mode,'complete',flush=True)
 # Check same state under scalar and vector math as well as cache.
 samples=list(out.glob(name+'-baseline-*.rgba'))
 bad=[p.name for p in samples if p.read_bytes()!=Path(str(p).replace('-baseline-','-optimized-')).read_bytes()]
 print(name,'compared',len(samples),'mismatches',bad,flush=True)
 if bad:raise RuntimeError('rendering regression or nondeterministic replay')
