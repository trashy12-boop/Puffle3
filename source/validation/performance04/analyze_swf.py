#!/usr/bin/env python3
"""Read supplied SWFs without distributing them. Counts nested sprite tags."""
import sys,zlib,struct,hashlib,json,collections
NAMES={0:'End',1:'ShowFrame',2:'DefineShape',6:'DefineBits',9:'SetBackgroundColor',10:'DefineFont',11:'DefineText',12:'DoAction',14:'DefineSound',15:'StartSound',18:'SoundStreamHead',19:'SoundStreamBlock',20:'DefineBitsLossless',21:'DefineBitsJPEG2',22:'DefineShape2',26:'PlaceObject2',32:'DefineShape3',33:'DefineText2',34:'DefineButton2',35:'DefineBitsJPEG3',36:'DefineBitsLossless2',37:'DefineEditText',39:'DefineSprite',43:'FrameLabel',46:'DefineMorphShape',48:'DefineFont2',59:'DoInitAction',60:'DefineVideoStream',61:'VideoFrame',69:'FileAttributes',70:'PlaceObject3',75:'DefineFont3',82:'DoABC',83:'DefineShape4',84:'DefineMorphShape2',87:'DefineBinaryData',90:'DefineBitsJPEG4'}
def analyze(path):
 raw=open(path,'rb').read();data=raw[8:] if raw[:3]==b'FWS' else zlib.decompress(raw[8:]);n=data[0]>>3;bits=5+4*n;pos=(bits+7)//8
 def field(start,width):
  v=int.from_bytes(data[:pos],'big')>>(pos*8-start-width)&((1<<width)-1)
  return v-(1<<width) if v>>(width-1) else v
 rect=[field(5+i*n,n)/20 for i in range(4)];fps=struct.unpack_from('<H',data,pos)[0]/256;frames=struct.unpack_from('<H',data,pos+2)[0];counts=collections.Counter();action_bytes=0
 def tags(buf):
  nonlocal action_bytes
  p=0
  while p+2<=len(buf):
   h=struct.unpack_from('<H',buf,p)[0];p+=2;code=h>>6;length=h&63
   if length==63:length=struct.unpack_from('<I',buf,p)[0];p+=4
   assert p+length<=len(buf);payload=buf[p:p+length];p+=length;counts[code]+=1
   if code in (12,59,82):action_bytes+=length
   if code==39:tags(payload[4:])
   if code==0:break
 tags(data[pos+4:]);return {'file':path,'sha256':hashlib.sha256(raw).hexdigest(),'file_bytes':len(raw),'uncompressed_bytes':len(data)+8,'version':raw[3],'stage_rect':rect,'declared_fps':fps,'root_frame_count':frames,'action_tag_bytes':action_bytes,'tags':{NAMES.get(k,str(k)):v for k,v in sorted(counts.items())}}
print(json.dumps([analyze(p) for p in sys.argv[1:]],indent=2))
