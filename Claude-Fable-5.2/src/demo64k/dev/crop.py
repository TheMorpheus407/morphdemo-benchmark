#!/usr/bin/env python3
# dev helper: crop.py in.ppm out.png x y w h [scale]
import sys
from PIL import Image
im=Image.open(sys.argv[1]); x,y,w,h=map(int,sys.argv[3:7]); sc=float(sys.argv[7]) if len(sys.argv)>7 else 1
im=im.crop((x,y,x+w,y+h))
if sc!=1: im=im.resize((int(w*sc),int(h*sc)), Image.LANCZOS)
im.save(sys.argv[2])
