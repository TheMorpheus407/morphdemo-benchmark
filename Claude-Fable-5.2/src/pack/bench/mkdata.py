#!/usr/bin/env python3
# Build synthetic, representative payloads for the packer bench (NOT shipped; harvested text stays in bench/data).
import subprocess, struct, glob, os, random, sys
sys.path.insert(0, os.path.dirname(__file__))
from minify import minify
D = os.path.join(os.path.dirname(__file__), 'data')
def sect(path, name):
    out = '/tmp/_sect.bin'
    subprocess.run(['objcopy', '-O', 'binary', '-j', name, path, out], check=True)
    return open(out, 'rb').read()
tiny = open('/tmp/tiny', 'rb').read()
elfbits = tiny[:0x318] + sect('/tmp/tiny', '.dynsym') + sect('/tmp/tiny', '.dynstr') + sect('/tmp/tiny', '.rela.dyn') + sect('/tmp/tiny', '.rela.plt') + sect('/tmp/tiny', '.dynamic')
names4k = """libX11.so.6 libGL.so.1 libEGL.so.1 libpulse-simple.so.0 XOpenDisplay XCreateWindow XInternAtom XChangeProperty XMapRaised XCreateBitmapFromData XCreatePixmapCursor XGetWindowAttributes XCheckWindowEvent XCreateImage XPutImage XDefaultGC XSetErrorHandler glXChooseVisual glXCreateContext glXMakeCurrent glXSwapBuffers eglGetPlatformDisplay eglInitialize eglBindAPI eglCreateContext eglMakeCurrent glGetString glCreateShaderProgramv glUseProgram glBindTexture glTexStorage2D glBindFramebuffer glFramebufferTexture2D glViewport glTexCoord4i glRecti glReadPixels glBlitFramebuffer glFinish pa_simple_new pa_simple_write pthread_create clock_gettime usleep""".split()
names_more = """XCloseDisplay XPending XNextEvent XLookupKeysym XStoreName XSetWMProtocols XSync XFlush XDestroyWindow XCreateColormap XFree XSetInputFocus XGrabKeyboard XShmCreateImage XShmAttach XShmPutImage XRRGetScreenResourcesCurrent XRRGetOutputPrimary XRRGetCrtcInfo glXQueryExtensionsString glXGetProcAddress glXDestroyContext eglQueryString eglChooseConfig eglCreatePbufferSurface eglTerminate eglGetError glGenTextures glTexImage2D glTexParameteri glGenFramebuffers glCheckFramebufferStatus glDrawBuffers glCreateShader glShaderSource glCompileShader glGetShaderiv glGetShaderInfoLog glCreateProgram glAttachShader glLinkProgram glGetProgramiv glGetProgramInfoLog glGetUniformLocation glUniform1f glUniform2f glUniform3f glUniform4f glUniform1i glUniform4fv glUniformMatrix4fv glActiveTexture glGenBuffers glBindBuffer glBufferData glBindBufferBase glGenVertexArrays glBindVertexArray glDrawArrays glDrawArraysInstanced glDispatchCompute glMemoryBarrier glEnable glDisable glBlendFunc glClear glClearColor glGenerateMipmap glTexStorage3D glBindImageTexture glDepthFunc glDepthMask glPointSize pa_simple_get_latency pa_simple_free pa_simple_drain pa_strerror snd_pcm_open snd_pcm_set_params snd_pcm_writei snd_pcm_recover pthread_join nanosleep memset memcpy sinf cosf powf expf logf floorf sqrtf fmodf tanhf atan2f""".split()
def cstr(lst): return b''.join(s.encode() + b'\0' for s in lst)
code_small = open(os.path.join(D, 'code_small.bin'), 'rb').read()
bb = glob.glob('/nix/store/*busybox-1.37.0/bin/busybox')[0]
bbtext = sect(bb, '.text'); bbro = sect(bb, '.rodata')
rnd = random.Random(1234)
def tables(n):
    out = bytearray()
    while len(out) < n:
        k = rnd.randrange(5)
        if k == 0: out += b''.join(struct.pack('<f', 440.0 * 2 ** ((i - 69) / 12)) for i in range(24, 24 + rnd.randrange(12, 48)))
        elif k == 1: out += b''.join(struct.pack('<f', round(rnd.uniform(-2, 2), 2)) for i in range(rnd.randrange(16, 64)))
        elif k == 2: out += bytes(rnd.choice([0, 0, 3, 5, 7, 10, 12, 15, 17, 19, 24]) + 36 * (i % 4 == 0) for i in range(rnd.randrange(32, 128)))
        elif k == 3: out += b''.join(struct.pack('<H', rnd.randrange(0, 4000)) for i in range(rnd.randrange(16, 64)))
        else: out += b''.join(struct.pack('<fff', round(rnd.uniform(0, 1), 3), round(rnd.uniform(0, 8), 1), i * 0.5) for i in range(rnd.randrange(8, 32)))
    return bytes(out[:n])
def mini(path):
    try: return minify(open(path, errors='replace').read()).encode()
    except Exception as e: return b''
intro = open(os.path.join(D, 'intro_min.glsl'), 'rb').read()
mx = sorted(glob.glob('/nix/store/z7h2vwdnaq3rwz706mzfv2vvd86xpai2-python3.13-materialx-1.39.4/libraries/*/genglsl/**/*.glsl', recursive=True), key=lambda p: -os.path.getsize(p))
bl = sorted(glob.glob('/nix/store/c2492x2ryabgzp4pxxlmmxx3x07kcmws-source/source/blender/**/*.glsl', recursive=True), key=lambda p: -os.path.getsize(p))
print(len(mx), 'materialx files', len(bl), 'blender files')
mxm = [m for m in (mini(p) for p in mx) if len(m) > 400 and m.count(b'#') < 6]
blm = [m for m in (mini(p) for p in bl) if len(m) > 400 and m.count(b'#') < 6]
print('minified bytes: mx', sum(map(len, mxm)), 'blender', sum(map(len, blm)))
def glsl(n, pool, first=b''):
    out = bytearray(first)
    for m in pool:
        if len(out) >= n: break
        out += m[: n - len(out)] + b'\0'
    return bytes(out)
def write(name, parts):
    data = b''.join(parts)
    open(os.path.join(D, name), 'wb').write(data)
    print(name, len(data), [len(p) for p in parts])
# 4k-class payloads
write('p4k_a.bin', [elfbits[:420], code_small[:900], cstr(names4k), glsl(6400, mxm[0:1], intro + b'\0')])
write('p4k_b.bin', [elfbits[:420], code_small[1500:2400], cstr(names4k[::-1]), glsl(6600, blm[40:70])])
write('p4k_c.bin', [elfbits[:420], bbtext[40000:40900], cstr(names4k), glsl(6400, mxm[3:14])])
# 16k-class
write('p16k_a.bin', [elfbits, code_small, bbtext[20000:29000], tables(3000), cstr(names4k + names_more[:60]), glsl(38000, mxm[1:] + blm[10:], intro + b'\0')])
write('p16k_b.bin', [elfbits, bbtext[60000:74000], tables(2500), cstr(names4k + names_more[30:]), glsl(36000, blm[20:] )])
# 64k-class
write('p64k_a.bin', [elfbits, code_small, bbtext[80000:150000], tables(20000), cstr(names4k + names_more), glsl(145000, mxm + blm, intro + b'\0')])
