/* Platform layer shared by the C demos.
 *
 *  video : X11 window (EWMH fullscreen, blank cursor, Escape / WM close)
 *          mode 1  GLX context on the window, blit + glXSwapBuffers
 *                  (used when the X server's GLX is served by the NVIDIA driver,
 *                   e.g. XWayland on the KDE desktop)
 *          mode 2  off-screen EGL context on the GPU (surfaceless / device
 *                  platform), glReadPixels + XPutImage
 *                  (used when the X server only offers software GLX, e.g. Xvfb)
 *  audio : PulseAudio simple API from a writer thread; the visual clock follows
 *          the real playback position (written - latency), so picture and sound
 *          stay locked even if the sink starts late.
 */
#ifndef PLAT_C
#define PLAT_C

#define AU_RATE 48000
#define AU_CHUNK 1024
#define AU_TLEN_FRAMES 4800 /* 100 ms target buffer */
#define AU_START_FRAMES 8192
/* the picture is shown this much ahead of the measured audio position: compensates
 * presentation / capture latency so that sound never leads the image perceptibly */
#define PLAT_VIDEO_LEAD 0.045
#define PLAT_MAXW 3840
#define PLAT_MAXH 2160

static Display *xd;
static Window xw;
static Visual *xvis;
static int xdepth, win_w, win_h, plat_mode, plat_soft;
static Atom wm_delete;
static GC xgc;
static XImage *ximg;
static int ximg_w, ximg_h;
static GLuint pres_fbo, pres_tex;
static u8 pres_buf[PLAT_MAXW * PLAT_MAXH * 4];

static double plat_clock(void) {
  long ts[2];
  clock_gettime(1, ts);
  return (double)ts[0] + (double)ts[1] * 1e-9;
}

static int plat_xerr(Display *d, void *e) { (void)d; (void)e; return 0; }

static int plat_has(const char *s, const char *sub) {
  if (!s) return 0;
  for (; *s; s++) {
    const char *a = s, *b = sub;
    while (*b && *a == *b) a++, b++;
    if (!*b) return 1;
  }
  return 0;
}

/* 0 = software rasteriser, 1 = hardware, 2 = NVIDIA */
static int plat_renderer_class(void) {
  const char *r = (const char *)glGetString(GL_RENDERER);
  if (!r || plat_has(r, "llvmpipe") || plat_has(r, "softpipe") || plat_has(r, "oftware")) return 0;
  return plat_has(r, "NVIDIA") ? 2 : 1;
}

static void *egl_dpy;
static int plat_egl_try(void *dpy) {
  void *cfg = 0, *surf = 0, *ctx;
  if (!dpy || !eglInitialize(dpy, 0, 0)) return 0;
  eglBindAPI(0x30A2 /* EGL_OPENGL_API */);
  ctx = eglCreateContext(dpy, 0, 0, 0);
  if (!ctx) { /* no EGL_KHR_no_config_context: fall back to a tiny pbuffer */
    static const int ca[] = {0x3033, 1, 0x3040, 8, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3038};
    static const int pa[] = {0x3057, 16, 0x3056, 16, 0x3038};
    int n = 0;
    if (!eglChooseConfig(dpy, ca, &cfg, 1, &n) || n < 1) return 0;
    surf = eglCreatePbufferSurface(dpy, cfg, pa);
    ctx = eglCreateContext(dpy, cfg, 0, 0);
    if (!ctx) return 0;
  }
  if (!eglMakeCurrent(dpy, surf, surf, ctx)) return 0;
  egl_dpy = dpy;
  return plat_renderer_class();
}

static int plat_egl_init(void) {
  int c, i, n = 0;
  void *devs[8];
  unsigned (*qd)(int, void **, int *);
  if (!eglGetPlatformDisplay) return 0;
  c = plat_egl_try(eglGetPlatformDisplay(0x31DD /* surfaceless */, 0, 0));
  if (c) return c;
  if (egl_dpy) eglMakeCurrent(egl_dpy, 0, 0, 0);
  qd = (unsigned (*)(int, void **, int *))eglGetProcAddress("eglQueryDevicesEXT");
  if (qd && qd(8, devs, &n))
    for (i = 0; i < n; i++) {
      c = plat_egl_try(eglGetPlatformDisplay(0x313F /* device */, devs[i], 0));
      if (c) return c;
      if (egl_dpy) eglMakeCurrent(egl_dpy, 0, 0, 0);
    }
  return 0;
}

/* Creates the window and a GL context.  Returns 0 on failure. */
static int plat_init(void) {
  static const int va[] = {4 /*RGBA*/, 5 /*DOUBLEBUFFER*/, 8, 8, 9, 8, 10, 8, 0};
  static const char zero[8];
  XVisualInfo *vi = 0;
  XSetWindowAttributes swa;
  XColor col;
  Window root;
  Atom st, fs;
  Pixmap pm;
  void *glx = 0;
  const char *cv = 0;
  int scr, i;

  if (!XOpenDisplay) return 0;
  xd = XOpenDisplay(0);
  if (!xd) xd = XOpenDisplay(":0");
  if (!xd) return 0;
  XSetErrorHandler(plat_xerr);
  scr = XDefaultScreen(xd);
  root = XRootWindow(xd, scr);
  win_w = XDisplayWidth(xd, scr);
  win_h = XDisplayHeight(xd, scr);

  if (glXChooseVisual) {
    vi = glXChooseVisual(xd, scr, va);
    cv = glXGetClientString(xd, 1 /* GLX_VENDOR */);
  }
  xvis = vi ? vi->visual : XDefaultVisual(xd, scr);
  xdepth = vi ? vi->depth : XDefaultDepth(xd, scr);

  for (i = 0; i < (int)sizeof swa; i++) ((u8 *)&swa)[i] = 0;
  for (i = 0; i < (int)sizeof col; i++) ((u8 *)&col)[i] = 0;
  swa.colormap = XCreateColormap(xd, root, xvis, 0);
  swa.event_mask = KeyPressMask | StructureNotifyMask;
  xw = XCreateWindow(xd, root, 0, 0, win_w, win_h, 0, xdepth, 1, xvis, CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &swa);
  XStoreName(xd, xw, "MORPH");
  st = XInternAtom(xd, "_NET_WM_STATE", 0);
  fs = XInternAtom(xd, "_NET_WM_STATE_FULLSCREEN", 0);
  XChangeProperty(xd, xw, st, 4 /* XA_ATOM */, 32, 0, (const u8 *)&fs, 1);
  wm_delete = XInternAtom(xd, "WM_DELETE_WINDOW", 0);
  XSetWMProtocols(xd, xw, &wm_delete, 1);
  pm = XCreateBitmapFromData(xd, xw, zero, 1, 1);
  XDefineCursor(xd, xw, XCreatePixmapCursor(xd, pm, pm, &col, &col, 0, 0));
  XMapRaised(xd, xw);
  XSync(xd, 0);
  XSetInputFocus(xd, xw, 2 /* RevertToParent */, 0); /* only matters without a window manager */
  xgc = XCreateGC(xd, xw, 0, 0);

  /* 1. NVIDIA GLX on the window */
  if (vi && cv && cv[0] == 'N' && cv[1] == 'V') {
    glx = glXCreateContext(xd, vi, 0, 1);
    if (glx && glXMakeCurrent(xd, xw, glx)) {
      if (plat_renderer_class()) { plat_mode = 1; return 1; }
      glXMakeCurrent(xd, 0, 0);
    }
    glx = 0;
  }
  /* 2. off-screen GPU context + XPutImage */
  if (plat_egl_init()) { plat_mode = 2; return 1; }
  if (egl_dpy) eglMakeCurrent(egl_dpy, 0, 0, 0);
  /* 3. whatever GLX the server has (possibly software) */
  if (vi) {
    glx = glXCreateContext(xd, vi, 0, 1);
    if (glx && glXMakeCurrent(xd, xw, glx)) {
      plat_mode = 1;
      plat_soft = !plat_renderer_class();
      return 1;
    }
  }
  return 0;
}

/* Returns 0 when the user asked to quit (Escape or window close). */
static int plat_poll(void) {
  XEvent e;
  while (XPending(xd)) {
    XNextEvent(xd, &e);
    if (e.type == KeyPress && XLookupKeysym(&e.xkey, 0) == XK_Escape) return 0;
    if (e.type == ClientMessage && (Atom)e.xclient.data.l[0] == wm_delete) return 0;
    if (e.type == ConfigureNotify) { win_w = e.xconfigure.width; win_h = e.xconfigure.height; }
  }
  return 1;
}

/* Shows the rw x rh colour buffer of `fbo`, scaled to the window (aspect kept). */
static void plat_present(GLuint fbo, int rw, int rh) {
  int dw = win_w > PLAT_MAXW ? PLAT_MAXW : win_w, dh = win_h > PLAT_MAXH ? PLAT_MAXH : win_h;
  int fw, fh, x0, y0;
  if (dw < 1 || dh < 1) return;
  fw = dw; fh = (int)((long)dw * rh / rw);
  if (fh > dh) { fh = dh; fw = (int)((long)dh * rw / rh); }
  x0 = (dw - fw) / 2; y0 = (dh - fh) / 2;
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
  if (plat_mode == 1) {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    if (x0 | y0) { glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); }
    glBlitFramebuffer(0, 0, rw, rh, x0, y0, x0 + fw, y0 + fh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glXSwapBuffers(xd, xw);
  } else {
    static double next;
    double now;
    if (dw != ximg_w || dh != ximg_h) {
      ximg_w = dw; ximg_h = dh;
      glGenTextures(1, &pres_tex);
      glBindTexture(GL_TEXTURE_2D, pres_tex);
      glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, dw, dh);
      glGenFramebuffers(1, &pres_fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pres_fbo);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pres_tex, 0);
      ximg = XCreateImage(xd, xvis, xdepth, 2 /* ZPixmap */, 0, (char *)pres_buf, dw, dh, 32, 0);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pres_fbo);
    if (x0 | y0) { glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); }
    glBlitFramebuffer(0, 0, rw, rh, x0, y0 + fh, x0 + fw, y0, GL_COLOR_BUFFER_BIT, GL_LINEAR); /* flipped for X */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, pres_fbo);
    glReadPixels(0, 0, dw, dh, GL_BGRA, GL_UNSIGNED_BYTE, pres_buf);
    if (ximg) XPutImage(xd, xw, xgc, ximg, 0, 0, 0, 0, dw, dh);
    XSync(xd, 0);
    /* no vsync on this path: pace to ~120 Hz so a 30/60 Hz grabber always finds a fresh frame */
    now = plat_clock();
    if (next < now - 0.05) next = now;
    next += 1.0 / 120.0;
    if (next > now) usleep((unsigned)((next - now) * 1e6));
  }
}

/* ------------------------------------------------------------------ audio */
typedef void (*au_render_fn)(float *out, int frame0, int nframes);
static void *au_pa;
static au_render_fn au_render;
static float *au_buf;
static int au_total;
static volatile int au_pos, au_started, au_dead;
static volatile double au_off;
static double plat_t0, plat_tfall;
static int plat_fall;

static void *au_thread(void *arg) {
  int err = 0, pos = 0, n;
  (void)arg;
  while (pos < au_total) {
    n = au_total - pos < AU_CHUNK ? au_total - pos : AU_CHUNK;
    if (au_render) au_render(au_buf + 2 * pos, pos, n);
    if (au_pa && !au_dead) {
      if (pa_simple_write(au_pa, au_buf + 2 * pos, (unsigned long)n * 8, &err) < 0) au_dead = 1;
      else {
        u64 lat = pa_simple_get_latency(au_pa, &err);
        if (lat < 10000000ull) {
          double off = (double)(pos + n) / AU_RATE - (double)lat * 1e-6 - plat_clock();
          if (!au_started) {
            if (pos + n >= AU_START_FRAMES) { au_off = off; au_started = 1; }
          } else {
            double d = off - au_off;
            au_off = (d > 0.25 || d < -0.25) ? off : au_off + d * 0.1;
          }
        }
      }
    }
    pos += n;
    au_pos = pos;
  }
  return 0;
}

/* buf: 2*total floats (interleaved stereo).  render may be NULL if buf is already filled. */
static void plat_audio_start(au_render_fn render, float *buf, int total) {
  static const pa_sample_spec ss = {5 /* FLOAT32LE */, AU_RATE, 2};
  static const pa_buffer_attr ba = {(u32)-1, AU_TLEN_FRAMES * 8, (u32)-1, (u32)-1, (u32)-1};
  long th[8];
  int err = 0;
  au_render = render; au_buf = buf; au_total = total;
  if (pa_simple_new) au_pa = pa_simple_new(0, "MORPH", 1 /* PLAYBACK */, 0, "MORPH", &ss, 0, &ba, &err);
  plat_t0 = plat_clock();
  if (pthread_create(th, 0, au_thread, 0)) au_dead = 1, au_pa = 0;
}

/* Seconds of production time; follows the audio playback position. */
static double plat_time(void) {
  double now = plat_clock();
  if (au_started && !plat_fall) return now + au_off + PLAT_VIDEO_LEAD;
  if (!au_pa || au_dead || now - plat_t0 > 3.0) { /* silent fallback: wall clock */
    if (!plat_fall) { plat_fall = 1; plat_tfall = now; }
    return now - plat_tfall;
  }
  return 0;
}

static void plat_shutdown(void) {
  if (xd) { XDestroyWindow(xd, xw); XCloseDisplay(xd); }
}

#endif
