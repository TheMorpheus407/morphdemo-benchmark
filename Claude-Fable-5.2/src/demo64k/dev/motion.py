#!/usr/bin/env python3
# dev helper: brightness and frame-to-frame change along the timeline (finds dead or jumpy stretches).
import subprocess, sys, os, numpy as np
from PIL import Image
B='/home/candidate/workspace/build'; C=B+'/cap/d64/mo'; os.makedirs(C,exist_ok=True)
step=float(sys.argv[1]) if len(sys.argv)>1 else 0.5
t0=float(sys.argv[2]) if len(sys.argv)>2 else 0.25
t1=float(sys.argv[3]) if len(sys.argv)>3 else 58.0
ts=np.arange(t0,t1,step)
args=[B+'/demo64k_dev']
for t in ts:
    args+=['--frame','%.4f'%t,'%s/a_%07.3f.ppm'%(C,t),'--frame','%.4f'%(t+1/60.),'%s/b_%07.3f.ppm'%(C,t)]
subprocess.run(args,stderr=subprocess.DEVNULL)
print("   t    mean   p99   diff(1/60s)")
for t in ts:
    a=np.asarray(Image.open('%s/a_%07.3f.ppm'%(C,t)).resize((480,270))).astype(float)/255
    b=np.asarray(Image.open('%s/b_%07.3f.ppm'%(C,t)).resize((480,270))).astype(float)/255
    d=np.abs(a-b).mean(); l=a.mean(axis=2)
    bar='#'*int(min(d*600,60))
    print("%6.2f %.3f %.3f %.4f %s"%(t,l.mean(),np.percentile(l,99),d,bar))
    os.remove('%s/a_%07.3f.ppm'%(C,t)); os.remove('%s/b_%07.3f.ppm'%(C,t))
