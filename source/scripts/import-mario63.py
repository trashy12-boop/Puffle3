#!/usr/bin/env python3
"""Validate an independently obtained Mario SWF and copy it to a staged HDD root.
Example: import-mario63.py game.swf /path/to/RPCS3/dev_hdd0
For a PS3, stage into a local directory, then copy ruffle/games/SuperMario63.swf
from there to /dev_hdd0 with a console file manager or FTP.
"""
import argparse,hashlib,pathlib,struct,zlib
p=argparse.ArgumentParser(description=__doc__);p.add_argument('swf',type=pathlib.Path);p.add_argument('hdd_root',type=pathlib.Path);a=p.parse_args();raw=a.swf.read_bytes()
if not 8<=len(raw)<=16*1024*1024:p.error('File size is outside the player limit')
if raw[:3]not in(b'FWS',b'CWS'):p.error('Expected FWS or CWS')
declared=int.from_bytes(raw[4:8],'little')
if not 8<=declared<=32*1024*1024:p.error('Invalid declared SWF length')
if raw[:3]==b'CWS':
 d=zlib.decompressobj();body=d.decompress(raw[8:],declared-8+1)
 if len(body)!=declared-8 or not d.eof:p.error('Corrupt or oversized compressed SWF')
else:body=raw[8:]
if len(body)!=declared-8 or not body:p.error('Length mismatch')
n=body[0]>>3;pos=(5+n*4+7)//8
if not n or len(body)<pos+4:p.error('Truncated stage header')
fps=struct.unpack_from('<H',body,pos)[0]/256;bits=int.from_bytes(body[:pos],'big');rect=[]
for i in range(4):
 v=(bits>>(pos*8-5-(i+1)*n))&((1<<n)-1);rect.append((v-(1<<n)if v&(1<<(n-1))else v)/20)
if fps!=32 or rect!=[0.,450.,0.,300.]:p.error('Not the expected 450x300, 32 FPS game profile')
def scan(buf,depth=0):
 if depth>64:raise ValueError('Excessive sprite depth')
 off=0
 while off+2<=len(buf):
  h=struct.unpack_from('<H',buf,off)[0];off+=2;code,size=h>>6,h&63
  if size==63:
   if off+4>len(buf):raise ValueError('Truncated tag')
   size=struct.unpack_from('<I',buf,off)[0];off+=4
  if off+size>len(buf):raise ValueError('Truncated tag payload')
  data=buf[off:off+size];off+=size
  if code==82 or(code==69 and size>=4 and int.from_bytes(data[:4],'little')&8):raise ValueError('AVM2 movie does not match the profile')
  if code==39:
   if size<4:raise ValueError('Truncated sprite')
   scan(data[4:],depth+1)
  if code==0:return
try:scan(body[pos+4:])
except (ValueError,struct.error)as e:p.error(str(e))
out=a.hdd_root/'ruffle/games/SuperMario63.swf';out.parent.mkdir(parents=True,exist_ok=True)
if out.exists()and out.read_bytes()!=raw:p.error('A different game file exists at the destination; back it up before importing')
out.write_bytes(raw);print(f'{out}\nSHA256 {hashlib.sha256(raw).hexdigest()}\nSWF {raw[3]}, {fps:g} FPS; file copied unchanged')
