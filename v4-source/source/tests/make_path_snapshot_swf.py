#!/usr/bin/env python3
"""Original AVM1 regression fixture; contains no game assets."""
from pathlib import Path
import struct

def push(v):
    b=b'\0'+v.encode()+b'\0' if isinstance(v,str) else b'\x07'+struct.pack('<i',v)
    return b'\x96'+struct.pack('<H',len(b))+b

def get(name): return push(name)+b'\x1c'
def member(obj,name): return obj+push(name)+b'\x4e'
def create(obj,name,depth): return push(depth)+push(name)+push(2)+obj+push('createEmptyMovieClip')+b'\x52\x17'
def assign(name,value): return push(name)+value+b'\x1d'
def rename(obj,name): return obj+push('_name')+push(name)+b'\x4f'
def trace(label,value): return push(label)+value+push(1)+push('String')+b'\x3d\x47\x26'
def tag(n,b=b''):return struct.pack('<H',(n<<6)|min(len(b),63))+(struct.pack('<I',len(b)) if len(b)>=63 else b'')+b

root=get('_root');parent=member(root,'parent');child=member(parent,'child');new=member(parent,'new')
a=create(root,'parent',1)+create(parent,'child',1)
a+=assign('early',child)+trace('initial=',get('early'))
a+=rename(child,'new')+assign('late',new)
a+=trace('renamed_early=',get('early'))+trace('renamed_late=',get('late'))
a+=push(0)+new+push('removeMovieClip')+b'\x52\x17'
a+=create(parent,'child',1)+create(parent,'new',2)
a+=trace('replaced_early=',get('early'))+trace('replaced_late=',get('late'))
a+=rename(parent,'renamedParent')
a+=trace('parent_rename=',member(member(root,'renamedParent'),'new'))
a+=b'\x07\x00'
# RECT: nbits=1; all coordinates 0, followed by padding.
body=b'\x08\x00'+struct.pack('<HH',32*256,1)+tag(12,a)+tag(1)+tag(0)
folder=Path(__file__).with_name('fixtures');folder.mkdir(exist_ok=True)
(folder/'path-snapshots.swf').write_bytes(b'FWS\x08'+struct.pack('<I',len(body)+8)+body)
(folder/'path-snapshots-output.txt').write_text('''initial=_level0.parent.child
renamed_early=_level0.parent.new
renamed_late=_level0.parent.new
replaced_early=_level0.parent.child
replaced_late=_level0.parent.new
parent_rename=_level0.renamedParent.new
''')
