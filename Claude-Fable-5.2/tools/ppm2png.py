#!/usr/bin/env python3
"""Dev helper: convert PPM/raw dumps to (optionally downscaled) PNG, or build contact sheets.
usage: ppm2png.py in.ppm out.png [scale]
       ppm2png.py --sheet out.png cols scale in1.ppm in2.ppm ...   (labels = file names)"""
import sys
from PIL import Image, ImageDraw
def load(p, scale):
    im = Image.open(p).convert('RGB')
    if scale != 1: im = im.resize((max(1,int(im.width*scale)), max(1,int(im.height*scale))), Image.LANCZOS)
    return im
if sys.argv[1] == '--sheet':
    out, cols, scale = sys.argv[2], int(sys.argv[3]), float(sys.argv[4]); files = sys.argv[5:]
    ims = [load(f, scale) for f in files]
    w, h = ims[0].size; rows = (len(ims)+cols-1)//cols
    sheet = Image.new('RGB', (cols*w, rows*h), (0,0,0)); d = ImageDraw.Draw(sheet)
    for i, im in enumerate(ims):
        x, y = (i%cols)*w, (i//cols)*h; sheet.paste(im, (x,y))
        import os, re
        lab = os.path.basename(files[i]); m = re.search(r'(\d+\.\d+)', lab); lab = m.group(1) if m else lab
        d.rectangle((x, y, x+8*len(lab)+6, y+13), fill=(0,0,0)); d.text((x+3, y+1), lab, fill=(255,255,0))
    sheet.save(out)
else:
    scale = float(sys.argv[3]) if len(sys.argv) > 3 else 1.0
    load(sys.argv[1], scale).save(sys.argv[2])
