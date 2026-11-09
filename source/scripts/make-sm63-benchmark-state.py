#!/usr/bin/env python3
"""Create a NEW empty-save SM63 benchmark state with the normal Mario bindings
and a slower (80 viewport pixels/second) right-stick cursor for scripted menus.
Does not modify an installed emulator or include game assets. Pass the new
folder to benchmark-rpcs3.py --state-template.
"""
import argparse,json,struct
from pathlib import Path

def make_state(output):
    output.mkdir(parents=True,exist_ok=False)
    (output/'profiles').mkdir();(output/'saves').mkdir()
    bindings=[[0,0,0] for _ in range(24)]
    for button,key in {0:38,1:90,2:88,3:67,4:38,5:40,6:37,7:39,8:257,10:256,14:80,15:16,16:38,17:40,18:37,19:39}.items():bindings[button][0]=key
    game=b'/dev_hdd0/ruffle/games/SuperMario63.swf';identity=14695981039346656037
    for c in game:identity=((identity^c)*1099511628211)&0xffffffffffffffff
    data=b'RPF2'+struct.pack('<H',len(game))+game+b''.join(struct.pack('<H',v) for row in bindings for v in row)+struct.pack('<HBBBB',80,18,0,2,0)
    checksum=2166136261
    for c in data:checksum=((checksum^c)*16777619)&0xffffffff
    (output/'profiles'/f'{identity:016x}.rpf').write_bytes(data+struct.pack('<I',checksum))
    (output/'state.json').write_text(json.dumps({'game':game.decode(),'mouse_speed':80,'acceleration':0,'mouse_stick':'right','renderer':'automatic RSX/fallback','starting_saves':'empty'},indent=2)+'\n')
    return output.resolve()
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);a=p.parse_args();print(make_state(a.output))
