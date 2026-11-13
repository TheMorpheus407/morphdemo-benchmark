#!/usr/bin/env python3
# dev helper: per-section band levels (dBFS RMS) of a wav.  mix.py file.wav [seconds per row]
import sys, wave, numpy as np
w=wave.open(sys.argv[1]); n=w.getnframes(); a=np.frombuffer(w.readframes(n),dtype=np.int16).reshape(-1,2).astype(np.float64)/32768.
step=float(sys.argv[2]) if len(sys.argv)>2 else 4.0
sr=48000; bands=[(20,80),(80,300),(300,2000),(2000,8000),(8000,20000)]
print("  t      rms   peak |  sub   low   mid    hi   air | corr")
for s in np.arange(0,len(a)/sr,step):
    x=a[int(s*sr):int((s+step)*sr)]
    if len(x)<1024: break
    m=x.mean(axis=1); rms=np.sqrt((x**2).mean()); pk=np.abs(x).max()
    S=np.abs(np.fft.rfft(m*np.hanning(len(m))))**2; f=np.fft.rfftfreq(len(m),1/sr)
    tot=S.sum()+1e-20
    bl=[10*np.log10(S[(f>=lo)&(f<hi)].sum()/tot+1e-12)+20*np.log10(rms+1e-9) for lo,hi in bands]
    c=np.corrcoef(x[:,0],x[:,1])[0,1] if x[:,0].std()>0 and x[:,1].std()>0 else 1
    print("%5.1f %6.1f %6.1f | %5.1f %5.1f %5.1f %5.1f %5.1f | %.2f" % (s,20*np.log10(rms+1e-9),20*np.log10(pk+1e-9),*bl,c))
