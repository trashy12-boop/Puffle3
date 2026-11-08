#!/usr/bin/env python3
"""Restricted single-thread PS3 backend for Rust 1.99.1. No filesystem/network/threads."""
from pathlib import Path
import sys, shutil
root=Path(sys.argv[1])/'library/std/src/sys'
assets=Path(__file__).parent/'std'
def branch(rel,code):
 p=root/rel;s=p.read_text()
 if '// ruffle-ps3 backend' not in s:
  p.write_text(s.replace('cfg_select! {','cfg_select! {\n    // ruffle-ps3 backend\n    '+code,1))
branch('alloc/mod.rs','target_os = "ps3" => { mod ps3; use ps3 as imp; }\n')
branch('io/error/mod.rs','target_os = "ps3" => { mod generic; pub use generic::*; }\n')
branch('random/mod.rs','target_os = "ps3" => { mod ps3; pub use ps3::fill_bytes; }\n')
branch('time/mod.rs','target_os = "ps3" => { mod ps3; use ps3 as imp; }\n')
branch('stdio/mod.rs','target_os = "ps3" => { mod ps3; pub use ps3::*; }\n')
p=root/'thread_local/mod.rs';s=p.read_text()
if 'target_os = "ps3"' not in s:p.write_text(s.replace('target_os = "vexos",','target_os = "vexos",\n        target_os = "ps3",'))
p=root.parent.parent/'build.rs';s=p.read_text()
if 'target_os == "ps3"' not in s:p.write_text(s.replace('if target_os == "linux"','if target_os == "ps3" || target_os == "linux"'))
for kind in ['alloc','random','time','stdio']:shutil.copyfile(assets/(kind+'.rs'),root/kind/'ps3.rs')
