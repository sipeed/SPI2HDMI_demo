#!/usr/bin/env python3
"""Compare every frame decoded by the actual C codec with a local source video."""
import argparse
import ctypes
import importlib.util
from pathlib import Path
import re
import subprocess
import tempfile
import numpy as np

base=Path(__file__).resolve().parents[1]
repo=base.parents[2]
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('video',type=Path)
ap.add_argument('--start',type=float,default=18)
ap.add_argument('--reference',type=Path,help='Write packed source frames for tests/run.sh')
a=ap.parse_args()
text=(base/'clip_160x90.h').read_text()
defs={k:int(v) for k,v in re.findall(r'#define (\w+) (\d+)',text)}
blob=bytes(map(int,re.findall(r'\d+',text.split('clip_data[] = {')[1].split('}')[0])))
fps=defs['CLIP_FPS'];count=defs['CLIP_FRAMES'];size=defs['CLIP_W']*defs['CLIP_H']//8
assert size==1800 and defs['CLIP_CODEC_BITRUN1']==1
raw=subprocess.check_output(['ffmpeg','-v','error','-i',str(a.video),'-vf','scale=160:90','-r',str(fps),'-frames:v',str(int(a.start*fps)+count),'-f','rawvideo','-pix_fmt','gray','-'])
frames=np.frombuffer(raw,np.uint8).reshape(-1,14400)[int(a.start*fps):]
reference=[np.packbits(frame>=128).tobytes() for frame in frames]
assert len(reference)==count
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text('#include "'+str(base/'bitrun.h')+'"\nint decode(const unsigned char*s,unsigned n,unsigned char*d){return bitrun_decode(s,n,d,1800);}\n')
 subprocess.run(['cc','-shared','-fPIC','-O2',str(d/'test.c'),'-o',str(d/'test.so')],check=True)
 lib=ctypes.CDLL(str(d/'test.so'));lib.decode.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_void_p]
 fb=ctypes.create_string_buffer(1800);pos=0
 for expected in reference:
  n=int.from_bytes(blob[pos:pos+2],'little');pos+=2;packet=blob[pos:pos+n];pos+=n
  assert len(packet)==n and lib.decode(packet,n,fb)==1 and fb.raw==expected
  for limit in (0,1,n//2,n-1):
   scratch=ctypes.create_string_buffer(1800);assert lib.decode(packet,limit,scratch)==0
 assert pos==len(blob)==defs['CLIP_BYTES']
 spec=importlib.util.spec_from_file_location('encoder',repo/'tools/gen_arcade_clip.py')
 encoder=importlib.util.module_from_spec(spec);spec.loader.exec_module(encoder)
 prev=bytes(1800)
 rng=np.random.default_rng(42)
 patterns=[bytes(1800),bytes([255])*1800,bytes([0x55])*1800,bytes([0xaa])*1800]
 patterns += [rng.integers(0,256,size=1800,dtype=np.uint8).tobytes() for _ in range(40)]
 for frame in patterns:
  packet=encoder.encode(frame,prev);fb=ctypes.create_string_buffer(prev,1800)
  assert lib.decode(packet,len(packet),fb)==1 and fb.raw==frame;prev=frame
if a.reference:a.reference.write_bytes(b''.join(reference))
print(f'PASS {count} C-decoded frames vs source; 44 edge/random frames; truncated-packet rejection')
