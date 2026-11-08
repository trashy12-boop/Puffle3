#!/usr/bin/env python3
"""Original CC0 test movie: hold mapped Z to move a red vector square.
No proprietary SWF compiler or game assets required.
"""
import struct
from pathlib import Path
class Bits:
 def __init__(self):self.s=''
 def u(self,v,n):self.s+=f'{v&((1<<n)-1):0{n}b}';return self
 def out(self):return int(self.s.ljust((len(self.s)+7)//8*8,'0'),2).to_bytes((len(self.s)+7)//8,'big')
def rect(x0,x1,y0,y1):
 n=max(abs(v).bit_length()+1 for v in (x0,x1,y0,y1));b=Bits().u(n,5)
 for v in (x0,x1,y0,y1):b.u(v,n)
 return b.out()
def matrix(x,y):
 n=max(abs(x).bit_length()+1,abs(y).bit_length()+1);return Bits().u(0,1).u(0,1).u(n,5).u(x,n).u(y,n).out()
def tag(n,b=b''):
 return struct.pack('<H',(n<<6)|min(len(b),63))+(struct.pack('<I',len(b)) if len(b)>=63 else b'')+b
def push(v):
 b=b'\x00'+v.encode()+b'\x00' if isinstance(v,str) else b'\x07'+struct.pack('<i',v)
 return b'\x96'+struct.pack('<H',len(b))+b
def action(n,b):return bytes([n])+struct.pack('<H',len(b))+b
b=Bits().u(1,4).u(0,4).u(0,1).u(5,5).u(1,5).u(0,1).u(0,1).u(1,1)
for x,y in [(800,0),(0,800),(-800,0),(0,-800)]:b.u(1,1).u(1,1).u(9,4).u(1,1).u(x,11).u(y,11)
b.u(0,6)
shape=struct.pack('<H',1)+rect(0,800,0,800)+bytes([1,0,255,80,64,255,0])+b.out()
init=push('PS3_INTERACTIVE_INIT')+b'\x26\x00'
# if Key.isDown(90), move /box._x by 3, and trace the interaction.
move=push('/box')+push(0)+push('/box')+push(0)+b'\x22'+push(3)+b'\x0a\x23'+push('PS3_Z_INPUT')+b'\x26'
query=push(90)+push(1)+push('Key')+b'\x1c'+push('isDown')+b'\x52\x12'+action(0x9d,struct.pack('<h',len(move)))
loop=query+move+b'\x00'
body=rect(0,6400,0,4800)+struct.pack('<HH',30*256,3)
body+=tag(9,bytes([16,32,64]))+tag(32,shape)+tag(26,bytes([0x26])+struct.pack('<HH',1,1)+matrix(800,1600)+b'box\0')+tag(12,init)+tag(1)+tag(12,loop)+tag(1)+tag(12,action(0x81,struct.pack('<H',1))+b'\x06\x00')+tag(1)+tag(0)
Path(__file__).with_name('fixtures').mkdir(exist_ok=True)
Path(__file__).with_name('fixtures').joinpath('interactive.swf').write_bytes(b'FWS'+bytes([8])+struct.pack('<I',len(body)+8)+body)

# Derive a separate one-second PCM tone movie from the same legal fixture.
import math
rate=22050
raw=b''.join(struct.pack('<h',int(6000*math.sin(2*math.pi*440*i/rate))) for i in range(rate))
sound=struct.pack('<HB I',2,0x3a,rate)+raw
# Insert DefineSound and StartSound before the first ShowFrame tag.
prefix=rect(0,6400,0,4800)+struct.pack('<HH',30*256,1)
audio_body=prefix+tag(14,sound)+tag(15,struct.pack('<HB',2,0))+tag(12,push('PS3_AUDIO_INIT')+b'\x26\x07\x00')+tag(1)+tag(0)
Path(__file__).with_name('fixtures').joinpath('tone.swf').write_bytes(b'FWS'+bytes([8])+struct.pack('<I',len(audio_body)+8)+audio_body)
# SharedObject read/write fixture: the second launch must print 7.
get=push('so')+push('ps3test')+push(1)+push('SharedObject')+b'\x1c'+push('getLocal')+b'\x52\x1d'
read=push('PS3_SAVE_READ:')+push('so')+b'\x1c'+push('data')+b'\x4e'+push('value')+b'\x4e\x47\x26'
write=push('so')+b'\x1c'+push('data')+b'\x4e'+push('value')+push(7)+b'\x4f'
flush=push(0)+push('so')+b'\x1c'+push('flush')+b'\x52\x17\x07\x00'
save_body=prefix+tag(12,get+read+write+flush)+tag(1)+tag(0)
Path(__file__).with_name('fixtures').joinpath('save.swf').write_bytes(b'FWS'+bytes([8])+struct.pack('<I',len(save_body)+8)+save_body)
# Compressed mouse fixture follows the Flash virtual mouse and traces clicks.
import zlib
function_body=push('PS3_MOUSE_DOWN')+b'\x26\x00'
function=action(0x9b,b'onMouseDown\0'+struct.pack('<HH',0,len(function_body))+function_body)
mouse_move=push('/box')+push(0)+push('_xmouse')+b'\x1c\x23'+push('/box')+push(1)+push('_ymouse')+b'\x1c\x23\x00'
mouse_body=rect(0,6400,0,4800)+struct.pack('<HH',30*256,3)+tag(9,bytes([16,32,64]))+tag(32,shape)+tag(26,bytes([0x26])+struct.pack('<HH',1,1)+matrix(800,1600)+b'box\0')+tag(12,function+b'\x00')+tag(1)+tag(12,mouse_move)+tag(1)+tag(12,action(0x81,struct.pack('<H',1))+b'\x06\x00')+tag(1)+tag(0)
Path(__file__).with_name('fixtures').joinpath('mouse.swf').write_bytes(b'CWS'+bytes([8])+struct.pack('<I',len(mouse_body)+8)+zlib.compress(mouse_body))
