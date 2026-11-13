/* MorphDemo 4k player (payload ELF, x86-64, GNU as / Intel syntax).
 *
 * Hand-made dynamic ELF: only dlsym is bound by ld.so, every other function is
 * looked up by name at start-up and its address pushed on the stack (the import
 * table), most of it reachable with 8-bit displacements from rbx.
 *
 *  video: X11 window (EWMH fullscreen, blank cursor); if the X server's GLX is
 *         NVIDIA's -> GLX context, blit of the 1920x1080 float target to the
 *         window + glXSwapBuffers; otherwise (Xvfb etc.) off-screen EGL context
 *         on the GPU + glReadPixels/XPutImage.
 *  audio: the whole soundtrack is rendered once by a fragment shader into the
 *         same float texture (4096x704 frames, dry + echo send), a small SSE loop
 *         adds a feedback echo network, a thread streams it with pa_simple.
 *  clock: wall clock anchored to the audio writer (playback position).
 */
        .intel_syntax noprefix

/* ------------------------------------------------------------ import list */
        .set    NIMP, 0
        .macro  IMP name
        .set    I_\name, NIMP
        .set    NIMP, NIMP+1
        .endm
#define IMPORTS \
        X(XOpenDisplay) X(glXChooseVisual) X(XCreateColormap) X(XCreateBitmapFromData) X(XCreatePixmapCursor) \
        X(XCreateWindow) X(XInternAtom) X(XChangeProperty) X(XMapRaised) X(glXGetClientString) \
        X(glXCreateContext) X(glXMakeCurrent) X(eglGetPlatformDisplay) X(eglInitialize) X(eglBindAPI) \
        X(eglCreateContext) X(eglMakeCurrent) X(XCreateGC) X(XCreateImage) X(glBindTexture) X(glTexStorage2D) \
        X(glFramebufferTexture2DEXT) X(pa_simple_new) X(pthread_create) X(pa_simple_write) \
        X(glViewport) X(glCreateShaderProgramv) X(glUseProgram) X(glReadPixels) X(glRecti) X(glUniform1f) \
        X(XCheckWindowEvent) X(XGetWindowAttributes) X(glBindFramebufferEXT) X(glBlitFramebuffer) \
        X(glXSwapBuffers) X(XPutImage) X(usleep)
#define X(n) IMP n;
        IMPORTS
#undef X
/* import table in bss; rbx = table + 128 so that the first 32 entries are reachable with 8-bit displacements */
#define F(n) qword ptr [rbx + 8*I_##n - 128]

        .set    BASE, 0x400000
        .set    W, 1920
        .set    H, 1080
        .set    SW, 4096
        .set    SH, 704
        .set    D1, 16875               /* echo taps in frames: dotted 8th (channels swapped) */
        .set    D2, 4409

/* ------------------------------------------------------------- ELF header */
        .section .all,"awx",@progbits
ehdr:   .byte   0x7f,'E','L','F',2,1,1,0
        .quad   0
        .word   2,62
        .long   1
        .quad   _start
        .quad   phdr-ehdr
        .quad   0
        .long   0
        .word   64,56,3,64,0,0
phdr:   .long   3,4                     /* PT_INTERP */
        .quad   interp-ehdr,interp,interp,28,28,1
        .long   1,7                     /* PT_LOAD rwx */
        .quad   0,ehdr,ehdr,_file_end-ehdr,_mem_end-ehdr,0x1000
        .long   2,6                     /* PT_DYNAMIC */
        .quad   dynamic-ehdr,dynamic,dynamic,dynamic_end-dynamic,dynamic_end-dynamic,8
interp: .asciz  "/lib64/ld-linux-x86-64.so.2"
dynamic:.quad   1,l_c-dynstr
        .quad   1,l_x-dynstr
        .quad   1,l_gl-dynstr
        .quad   1,l_egl-dynstr
        .quad   1,l_pa-dynstr
        .quad   5,dynstr                /* DT_STRTAB  */
        .quad   6,dynsym                /* DT_SYMTAB  */
        .quad   7,rela                  /* DT_RELA    */
        .quad   8,24                    /* DT_RELASZ  */
        .quad   9,24                    /* DT_RELAENT */
dynsym: .quad   0,0,0                   /* (doubles as DT_NULL terminator) */
dynamic_end:
        .long   s_dlsym-dynstr
        .byte   0x12,0
        .word   0
        .quad   0,0
rela:   .quad   dlsym,(1<<32)|6,0       /* R_X86_64_GLOB_DAT */
dlsym:  .quad   0
dynstr: .byte   0
l_c:    .asciz  "libc.so.6"
l_x:    .asciz  "libX11.so.6"
l_gl:   .asciz  "libGL.so.1"
l_egl:  .asciz  "libEGL.so.1"
l_pa:   .asciz  "libpulse-simple.so.0"
s_dlsym:.asciz  "dlsym"
names:
#define X(n) .asciz #n;
        IMPORTS
#undef X
        .byte   0
a_state:.asciz  "_NET_WM_STATE"
a_full: .asciz  "_NET_WM_STATE_FULLSCREEN"
visattr:.long   4,5,0                   /* GLX_RGBA, GLX_DOUBLEBUFFER */
sspec:  .long   5,48000,2               /* FLOAT32LE, 48 kHz, stereo */
battr:  .long   -1,38400,-1,-1,-1       /* tlength = 100 ms -> low, known latency */
c_gain: .float  0.33
c_time: .float  1.048576e-3             /* 2^20 ns -> s */
#include "shaders.inc"

/* ------------------------------------------------------------------ code */
        .globl  _start
_start:
        /* resolve imports: table[k] = dlsym(RTLD_DEFAULT, name k) */
        mov     ebp, offset names
        mov     ebx, offset imptab
1:      xor     edi, edi
        mov     rsi, rbp
        call    qword ptr [dlsym]
        mov     [rbx], rax
        add     rbx, 8
2:      inc     rbp
        cmp     byte ptr [rbp-1], 0
        jne     2b
        cmp     byte ptr [rbp], 0
        jne     1b
        mov     ebx, offset imptab+128
        mov     r14d, offset vars

        xor     edi, edi
        call    F(XOpenDisplay)
        mov     rbp, rax                /* rbp = Display* */
        mov     rdi, rax
        xor     esi, esi
        mov     edx, offset visattr
        call    F(glXChooseVisual)
        mov     r13, rax                /* r13 = XVisualInfo* */
        mov     rax, [rbp+0xe8]         /* screens[0] */
        mov     r15, [rax+0x10]         /* r15 = root window */

        mov     rdi, rbp
        mov     rsi, r15
        mov     rdx, [r13]              /* visual */
        xor     ecx, ecx
        call    F(XCreateColormap)
        mov     [r14+SWA+96], rax       /* swa.colormap */
        mov     byte ptr [r14+SWA+72], 1 /* swa.event_mask = KeyPressMask */

        mov     rdi, rbp                /* invisible cursor from an all-zero 1x1 bitmap */
        mov     rsi, r15
        lea     rdx, [r14+ZERO]
        push    1
        pop     rcx
        mov     r8d, ecx
        call    F(XCreateBitmapFromData)
        mov     rdi, rbp
        mov     rsi, rax
        mov     rdx, rax
        lea     rcx, [r14+ZERO]
        mov     r8, rcx
        xor     r9d, r9d
        push    r9
        push    r9
        call    F(XCreatePixmapCursor)
        mov     [r14+SWA+104], rax      /* swa.cursor */

        lea     rax, [r14+SWA]          /* XCreateWindow(d, root, 0,0, w,h, 0, depth, InputOutput, visual, mask, &swa) */
        push    rax
        push    0x6808                  /* CWBorderPixel|CWEventMask|CWColormap|CWCursor */
        push    qword ptr [r13]         /* visual */
        push    1
        mov     eax, [r13+20]           /* depth */
        push    rax
        push    0
        mov     rax, [rbp+0xe8]
        mov     r8d, [rax+0x18]         /* screen width  */
        mov     r9d, [rax+0x1c]         /* screen height */
        mov     rdi, rbp
        mov     rsi, r15
        xor     edx, edx
        xor     ecx, ecx
        call    F(XCreateWindow)
        mov     r12, rax                /* r12 = window */

        mov     rdi, rbp                /* _NET_WM_STATE = _NET_WM_STATE_FULLSCREEN */
        mov     esi, offset a_full
        xor     edx, edx
        call    F(XInternAtom)
        mov     [r14+ATOM], rax
        mov     rdi, rbp
        mov     esi, offset a_state
        xor     edx, edx
        call    F(XInternAtom)
        push    1
        lea     rcx, [r14+ATOM]
        push    rcx
        mov     rdi, rbp
        mov     rsi, r12
        mov     rdx, rax
        push    4
        pop     rcx
        push    32
        pop     r8
        xor     r9d, r9d
        call    F(XChangeProperty)
        mov     rdi, rbp
        mov     rsi, r12
        call    F(XMapRaised)

        /* GL context: NVIDIA GLX on the window, else off-screen EGL on the GPU */
        mov     rdi, rbp
        push    1
        pop     rsi
        call    F(glXGetClientString)
        cmp     byte ptr [rax], 'N'
        jne     egl
        mov     rdi, rbp
        mov     rsi, r13
        xor     edx, edx
        push    1
        pop     rcx
        call    F(glXCreateContext)
        mov     rdi, rbp
        mov     rsi, r12
        mov     rdx, rax
        call    F(glXMakeCurrent)
        xor     r13d, r13d              /* r13 = 0: GLX mode */
        jmp     glok
egl:    mov     edi, 0x31DD             /* EGL_PLATFORM_SURFACELESS_MESA */
        xor     esi, esi
        xor     edx, edx
        call    F(eglGetPlatformDisplay)
        mov     r15, rax
        mov     rdi, rax
        xor     esi, esi
        xor     edx, edx
        call    F(eglInitialize)
        mov     edi, 0x30A2             /* EGL_OPENGL_API */
        call    F(eglBindAPI)
        mov     rdi, r15
        xor     esi, esi
        xor     edx, edx
        xor     ecx, ecx
        call    F(eglCreateContext)
        mov     rdi, r15
        xor     esi, esi
        xor     edx, edx
        mov     rcx, rax
        call    F(eglMakeCurrent)
        mov     rdi, rbp                /* GC + XImage wrapping the read-back buffer */
        mov     rsi, r12
        xor     edx, edx
        xor     ecx, ecx
        call    F(XCreateGC)
        mov     [r14+GCX], rax
        xor     eax, eax
        push    rax
        push    32
        push    H
        push    W
        mov     rdi, rbp
        mov     rsi, [r13]              /* visual */
        mov     edx, [r13+20]           /* depth  */
        push    2
        pop     rcx                     /* ZPixmap */
        xor     r8d, r8d
        mov     r9d, offset imgbuf
        call    F(XCreateImage)
        mov     r13, rax                /* r13 = XImage*: EGL mode */
glok:
        /* one RGBA32F 4096x1080 texture as render target for sound and picture */
        mov     edi, 0x0DE1
        push    1
        pop     rsi
        call    F(glBindTexture)
        mov     edi, 0x0DE1
        push    1
        pop     rsi
        mov     edx, 0x8814
        mov     ecx, SW
        mov     r8d, H
        call    F(glTexStorage2D)
        mov     edi, 0x8D40
        push    1
        pop     rsi
        call    F(glBindFramebufferEXT)
        mov     edi, 0x8D40
        mov     esi, 0x8CE0
        mov     edx, 0x0DE1
        push    1
        pop     rcx
        xor     r8d, r8d
        call    F(glFramebufferTexture2DEXT)

        /* soundtrack: render, read back, echo network, fold down to stereo */
        mov     edx, SW
        mov     ecx, SH
        mov     esi, offset s_snd
        call    program
        call    rect
        xor     edi, edi
        xor     esi, esi
        mov     edx, SW
        mov     ecx, SH
        mov     r8d, 0x1908             /* GL_RGBA */
        mov     r9d, 0x1406             /* GL_FLOAT */
        push    rax
        push    offset snd4
        call    F(glReadPixels)
        mov     esi, offset snd4
        mov     edi, offset sndbuf
        mov     ecx, SW*SH
        movss   xmm3, [c_gain]
        shufps  xmm3, xmm3, 0
1:      movsd   xmm0, [rsi-D1*16+8]     /* wet pair one dotted 8th ago ... */
        shufps  xmm0, xmm0, 0xE1        /* ... with the channels swapped */
        movsd   xmm1, [rsi-D2*16+8]
        addps   xmm0, xmm1
        mulps   xmm0, xmm3
        movsd   xmm1, [rsi+8]
        addps   xmm0, xmm1
        movlps  [rsi+8], xmm0           /* feedback */
        movsd   xmm1, [rsi]
        addps   xmm0, xmm1              /* + dry */
        movlps  [rdi], xmm0
        add     rsi, 16
        add     rdi, 8
        dec     ecx
        jnz     1b

        /* picture program */
        mov     edx, W
        mov     ecx, H
        mov     esi, offset s_img
        call    program

        /* audio: stream the buffer from a thread */
        call    clock
        add     rax, 0x7fffffff         /* fallback: start after ~2.1 s even without sound */
        mov     [r14+T0], rax
        xor     edi, edi
        mov     esi, offset names       /* any string as the client name */
        push    1
        pop     rdx
        xor     ecx, ecx
        mov     r8, rsi
        mov     r9d, offset sspec
        push    rcx
        push    rcx
        push    offset battr
        push    rcx
        call    F(pa_simple_new)
        test    rax, rax
        jz      frame
        mov     rcx, rax                /* thread argument = pa handle */
        lea     rdi, [r14+ZERO]
        xor     esi, esi
        mov     edx, offset audio
        call    F(pthread_create)

frame:  mov     rdi, rbp
        mov     rsi, r12
        push    -1
        pop     rdx
        lea     rcx, [r14+EVT]
        call    F(XCheckWindowEvent)
        test    eax, eax
        jz      1f
        cmp     byte ptr [r14+EVT], 2   /* KeyPress */
        jne     frame
        cmp     byte ptr [r14+EVT+84], 9 /* Escape */
        jne     frame
quit:   mov     eax, 231
        xor     edi, edi
        syscall
1:      call    clock
        sub     rax, [r14+T0]
        jns     2f
        xor     eax, eax
2:      shr     rax, 20
        cmp     eax, 57220              /* 60 s */
        jae     quit
        cvtsi2ss xmm0, eax
        mulss   xmm0, [c_time]
        xor     edi, edi
        call    F(glUniform1f)
        call    rect
        test    r13, r13
        jnz     3f
        /* GLX: scale to the window (flipped: the shader renders top-down) and swap */
        mov     rdi, rbp
        mov     rsi, r12
        lea     rdx, [r14+WATTR]
        call    F(XGetWindowAttributes)
        mov     edi, 0x8CA9             /* GL_DRAW_FRAMEBUFFER */
        xor     esi, esi
        call    F(glBindFramebufferEXT)
        push    0x2601                  /* GL_LINEAR */
        push    0x4000                  /* GL_COLOR_BUFFER_BIT */
        push    0                       /* dstY1 */
        mov     eax, [r14+WATTR+8]
        push    rax                     /* dstX1 = window width */
        xor     edi, edi
        xor     esi, esi
        mov     edx, W
        mov     ecx, H
        xor     r8d, r8d
        mov     r9d, [r14+WATTR+12]     /* dstY0 = window height */
        call    F(glBlitFramebuffer)
        add     rsp, 32
        mov     rdi, rbp
        mov     rsi, r12
        call    F(glXSwapBuffers)
        mov     edi, 0x8CA9
        push    1
        pop     rsi
        call    F(glBindFramebufferEXT)
        jmp     frame
3:      /* EGL: read back as BGRA and put it into the window */
        xor     edi, edi
        xor     esi, esi
        mov     edx, W
        mov     ecx, H
        mov     r8d, 0x80E1             /* GL_BGRA */
        mov     r9d, 0x1401             /* GL_UNSIGNED_BYTE */
        push    rax
        push    offset imgbuf
        call    F(glReadPixels)
        push    H
        push    W
        xor     eax, eax
        push    rax
        push    rax
        mov     rdi, rbp
        mov     rsi, r12
        mov     rdx, [r14+GCX]
        mov     rcx, r13
        xor     r8d, r8d
        xor     r9d, r9d
        call    F(XPutImage)
        add     rsp, 48
        mov     edi, 6000               /* no vsync on this path: pace the loop */
        call    F(usleep)
        jmp     frame

/* glViewport(0,0,edx,ecx); p = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 2, {s_common, rsi}); glUseProgram(p) */
program:
        push    rax                     /* alignment */
        push    rsi
        push    offset s_common
        xor     edi, edi
        xor     esi, esi
        call    F(glViewport)
        mov     edi, 0x8B30
        push    2
        pop     rsi
        mov     rdx, rsp
        call    F(glCreateShaderProgramv)
        mov     edi, eax
        call    F(glUseProgram)
        add     rsp, 24
        ret

/* glRecti(-1,-1,1,1) */
rect:   push    rax
        push    -1
        pop     rdi
        mov     esi, edi
        push    1
        pop     rdx
        mov     ecx, edx
        call    F(glRecti)
        pop     rax
        ret

/* rax = CLOCK_MONOTONIC in ns */
clock:  push    rax
        push    rax
        mov     eax, 228
        push    1
        pop     rdi
        mov     rsi, rsp
        syscall
        pop     rax
        pop     rcx
        imul    rax, rax, 1000000000
        add     rax, rcx
        ret

/* audio thread: arg = pa handle */
audio:  mov     ebx, offset imptab+128
        mov     rbp, rdi
        mov     r12d, offset sndbuf
        mov     r13, -105000000         /* audio time written, ns; starts negative = output latency (measured) */
        xor     r15d, r15d
        push    rax
1:      mov     rdi, rbp
        mov     rsi, r12
        mov     edx, 7680               /* 20 ms */
        xor     ecx, ecx
        call    F(pa_simple_write)
        add     r12, 7680
        add     r13, 20000000
        cmp     r13, 100000000          /* the first 100 ms are accepted without playing */
        jl      1b
        call    clock
        sub     rax, r13                /* latest (= most drained) estimate of the stream origin */
        cmp     rax, r15
        jl      1b
        mov     r15, rax
        mov     [T0abs], rax
        jmp     1b
_file_end:

/* ------------------------------------------------------------------- bss */
        .section .bss,"aw",@nobits
        .balign 16
vars:
        .set    SWA, 0                  /* XSetWindowAttributes (112 bytes) */
        .set    ZERO, 112               /* 16 zero bytes: bitmap data / XColor / pthread_t */
        .set    ATOM, 128
        .set    GCX, 136
        .set    T0, 144
        .set    EVT, 152                /* XEvent (192 bytes) */
        .set    WATTR, 344              /* XWindowAttributes (136 bytes) */
        .skip   512
        .set    T0abs, vars+T0
imptab: .skip   8*NIMP
        .skip   D1*16                   /* silence before the track (echo taps read back into it) */
snd4:   .skip   SW*SH*16                /* dry L/R + send L/R per frame */
sndbuf: .skip   SW*SH*8                 /* final interleaved stereo */
imgbuf: .skip   W*H*4
_mem_end:
