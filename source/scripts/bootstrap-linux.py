#!/usr/bin/env python3
"""Fetch the verified Linux x86_64 toolchain. No root or system install needed.
Run from the repository root: python3 scripts/bootstrap-linux.py /absolute/tools
Then source /absolute/tools/env.sh; scripts/get-ruffle.sh; scripts/build-player.sh --release; make package
NVIDIA Cg remains subject to its own license; downloaded for compilation only.
"""
import argparse,concurrent.futures,hashlib,json,os,pathlib,subprocess,sys,tarfile,urllib.request
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory',type=pathlib.Path,help='Toolchain installation directory')
args=parser.parse_args()
out=args.directory.resolve();out.mkdir(parents=True,exist_ok=True)
manifest=json.loads((pathlib.Path(__file__).parent/'toolchain-lock.json').read_text())
def fetch(a):
 p=out/a['file']
 if not p.exists():urllib.request.urlretrieve(a['url'],p)
 digest=hashlib.sha256(p.read_bytes()).hexdigest()
 if digest!=a['sha256']:raise RuntimeError(f'Checksum mismatch: {p}')
 print('Verified',p.name,flush=True);return p
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:archives=list(pool.map(fetch,manifest))
for item,archive in zip(manifest,archives):
 target=out/item['extract'];target.mkdir(exist_ok=True)
 with tarfile.open(archive) as tar:tar.extractall(target,filter='data')
 if item['extract']=='rust-dist':
  installer=target/item['file'].removesuffix('.tar.xz')/'install.sh'
  subprocess.run([str(installer),f'--prefix={out}/rust','--disable-ldconfig'],check=True)
ps3dev=out/'ps3dev-dist/ps3dev'
env=f'''export PS3DEV='{ps3dev}'
export PSL1GHT="$PS3DEV"
export PATH='{out}/rust/bin':"$PS3DEV/bin":"$PS3DEV/ppu/bin":"$PATH"
export CARGO_HOME='{out}/cargo-home'
export LD_LIBRARY_PATH='{out}/cg-dist/usr/lib64':"${{LD_LIBRARY_PATH:-}}"
'''
(out/'env.sh').write_text(env);print('Source',out/'env.sh')
