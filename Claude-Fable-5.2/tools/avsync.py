#!/usr/bin/env python3
"""Dev helper: audio/video offset of an avrec capture of a demo with a 4-on-the-floor kick.
usage: avsync.py PREFIX BPM T0 T1 [FIRSTKICK]
 1. precise, modulo one beat: low-band audio envelope and frame brightness are folded over the
    beat period between T0 and T1 (seconds after launch); steepest rise of each = kick / flash.
 2. coarse, absolute (only if FIRSTKICK, the song time of the very first kick, is given): first
    low-band onset in the audio and first brightness jump in the video around that time; used to
    pick the right beat for the precise value."""
import sys, csv, numpy as np
pre, bpm, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
first = float(sys.argv[5]) if len(sys.argv) > 5 else None
info = dict(l.split()[:2] for l in open(pre + '.info') if len(l.split()) >= 2)
ainfo = dict(l.split()[:2] for l in open(pre + '.audio.info'))
launch = float(info['launch_time']); a0 = float(ainfo['first_sample_time'])
a = np.frombuffer(open(pre + '.audio.raw', 'rb').read(), dtype=np.int16).reshape(-1, 2).mean(axis=1)
k = 480
lp = np.convolve(np.convolve(a, np.ones(k) / k, mode='same'), np.ones(k) / k, mode='same')   # ~ <100 Hz, zero phase
env = np.abs(lp)
ta = a0 - launch + np.arange(len(env)) / 48000.0
rows = list(csv.DictReader(open(pre + '.frames.csv')))
tv = np.array([float(r['t']) for r in rows]); lum = np.array([float(r['r']) + float(r['g']) + float(r['b']) for r in rows])
beat = 60.0 / bpm; nb = 200
def fold(t, x):
    m = (t > t0) & (t < t1); ph = ((t[m] % beat) / beat * nb).astype(int) % nb
    s = np.bincount(ph, weights=x[m], minlength=nb); c = np.bincount(ph, minlength=nb)
    f = s / np.maximum(c, 1)
    return np.convolve(np.concatenate([f, f, f]), np.ones(5) / 5, mode='same')[nb:2 * nb]
fa, fv = fold(ta, env), fold(tv, lum)
rise = lambda f: int(np.argmax(np.roll(f, -1) - np.roll(f, 1))) / nb * beat
pa, pv = rise(fa), rise(fv)
d = (pv - pa + beat / 2) % beat - beat / 2
print("precise (mod beat): video - audio = %+.1f ms   [kick phase %.1f ms, flash phase %.1f ms; depth a %.2f v %.3f]" % (d * 1e3, pa * 1e3, pv * 1e3, (fa.max() - fa.min()) / (fa.max() + 1e-9), (fv.max() - fv.min()) / (fv.max() + 1e-9)))
if first is not None:
    pk = np.percentile(env[(ta > t0) & (ta < t1)], 99)
    w = (ta > first - 3)
    ia = np.nonzero(w & (env > 0.35 * pk))[0][0]
    wv = np.nonzero(tv > first - 3)[0]; dl = np.diff(lum)
    thr = 0.5 * np.max(dl[(tv[1:] > t0) & (tv[1:] < t1)])
    iv = [i for i in wv[:-1] if dl[i] > thr][0]
    coarse = tv[iv + 1] - ta[ia]
    n = round((coarse - d) / beat)
    print("coarse (absolute): first kick heard %.3f s, first flash seen %.3f s after launch -> %+.0f ms" % (ta[ia], tv[iv + 1], coarse * 1e3))
    print("=> video - audio = %+.1f ms  (positive: picture late); audio origin ~%.2f s after launch" % ((d + n * beat) * 1e3, ta[ia] - first))
