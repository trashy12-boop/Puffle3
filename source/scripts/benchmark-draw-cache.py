#!/usr/bin/env python3
"""Compare cache replacement bookkeeping on the host; this is not console FPS.
Usage: benchmark-draw-cache.py BASELINE_DRAW_CACHE_RS
Requires cargo/rustc on PATH. Builds in a temporary directory.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
source = Path(__file__).resolve().parent.parent
baseline = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='puffle-cache-') as tmp:
    root = Path(tmp)
    (root / 'src').mkdir()
    (root / 'Cargo.toml').write_text('[package]\nname="cache-comparison"\nversion="0.1.0"\nedition="2024"\n[dependencies]\ntiny-skia={path=' + json.dumps(str(source/'vendor/tiny-skia')) + ',default-features=false,features=["std"]}\n')
    code = '''#[path=BASELINE]mod before;
#[path=CURRENT]mod after;
macro_rules! run {($m:ident)=>{{
 let mut cache=$m::DrawCache::new();let p=tiny_skia::Pixmap::new(4,4).unwrap();let r=tiny_skia::IntRect::from_xywh(0,0,4,4).unwrap();
 let start=std::time::Instant::now();
 for i in 0..100_000 {let key=$m::Key{path:i%1024,clip:0,matrix:[0;6],colors:[0;8],bitmap_version:0,width:4,height:4};cache.insert(key,r,$m::DrawCache::snapshot(&p,r),&p);}
 (start.elapsed().as_secs_f64(),cache.bytes())
}};}
fn main(){for _ in 0..5 {println!("before {:?} after {:?}",run!(before),run!(after));}}
'''
    code=code.replace('BASELINE',json.dumps(str(baseline))).replace('CURRENT',json.dumps(str(source/'player/src/draw_cache.rs')))
    (root/'src/main.rs').write_text(code)
    subprocess.run(['cargo','run','--release','--manifest-path',str(root/'Cargo.toml')],check=True)
