/* demo64.c -- "CONVERGENCE", the sixty four kilobyte production.
 *
 * Sixty five thousand five hundred and thirty six particles live in a pair of
 * floating point textures.  A simulation shader moves them, a billboard shader
 * draws them, and the whole thing is finished with a two pass bloom.
 *
 *   0 - 17 s    EMERGENCE    the swarm is a turbulent nebula, cold and loud
 *   17 - 37 s   ORDER        a rotationally ordered lattice crystallises out
 *                            of the chaos and starts to breathe with the beat
 *   37 - 60 s   RESOLUTION   the lattice folds into the word MORPH, one letter
 *                            after another, while the music resolves
 *
 * The particles are not decoration: every one of them belongs to a cell of
 * the word, and the cell decides where the particle finally lands.  The
 * letters rise out of the swarm, one after another, on the beat.
 */
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "fmath.h"
#include "syms_common.h"

#define SYMS(X) SYMS_ALL(X)
#include "symtab.h"

#define RATE   48000
#define DUR    60.0f
#define BEAT   (60.0f / 96.0f)        /* 96 beats per minute */
#define STEP   (BEAT * 0.25f)         /* sixteenth note        */
#define N      256                    /* the state texture edge */
#define NPART  (N * N)                /* 65536 particles       */
#define BLOOMX 480
#define BLOOMY 270
#define CELLS  174                    /* 29 columns x 6 rows   */
#define DLY    8192                    /* the delay of the room  */
#define CELL   0.30f                  /* the cell size         */
#define MAXBLK 768                   /* the largest audio block */

/* --------------------------------------------------------------- shaders - */
/* A full screen triangle; the fragment stages do all the work.
 * The triangle reaches past the screen, so the texture coordinates are
 * doubled: over the visible part of the picture they run from 0 to 1. */
static const char VS[] =
    "#version 330\n"
    "out vec2 vUv;\n"
    "void main(){vec2 p=vec2(gl_VertexID&1,gl_VertexID>>1);vUv=p*2.;\n"
    "gl_Position=vec4(p*4.-1.,0.,1.);}\n";

/* the state of one particle: position and life */
static const char FS_SIM[] =
    "#version 330\n"
    "uniform sampler2D uPos,uVel;\n"
    "uniform float uDt,uT,uW1,uW2,uW3,uClear,uForm,uKick,uOrder,uGold;\n"
    "in vec2 vUv;\n"
    "const uint F[5]=uint[5](0x23bac631u,0x1d18c62eu,0x3d18fa92u,0x3d18fa10u,0x231fc631u);\n"
    "float letterBit(int lc,int lr){\n"
    "if(lc<0||lc>28||lr<0||lr>5)return 0.;\n"
    "int li=lc/6,cx=lc-li*6;if(cx>4)return 0.;\n"
    "return float((F[li]>>uint(29-lr*5-cx))&1u);}\n"
    "float hash13(vec3 p){p=fract(p*.1031);p+=dot(p,p.zyx+31.32);\n"
    "return fract((p.x+p.y)*p.z);}\n"
    "float vn(vec3 x){vec3 i=floor(x),f=fract(x);f=f*f*(3.-2.*f);\n"
    "float a=hash13(i),b=hash13(i+vec3(1,0,0)),c=hash13(i+vec3(0,1,0)),d=hash13(i+vec3(1,1,0));\n"
    "float e=hash13(i+vec3(0,0,1)),g=hash13(i+vec3(1,0,1)),h=hash13(i+vec3(0,1,1)),k=hash13(i+vec3(1,1,1));\n"
    "return mix(mix(mix(a,b,f.x),mix(c,d,f.x),f.y),mix(mix(e,g,f.x),mix(h,k,f.x),f.y),f.z);}\n"
    "vec3 curl(vec3 p){float e=.18;\n"
    "float a=vn(p+vec3(0,e,0))-vn(p-vec3(0,e,0));\n"
    "float b=vn(p+vec3(0,0,e))-vn(p-vec3(0,0,e));\n"
    "float c=vn(p+vec3(e,0,0))-vn(p-vec3(e,0,0));\n"
    "return vec3(a-b,c-a,b-c)/(2.*e);}\n"
    /* the cell of the word a particle belongs to */
    "int cellOf(int id){return id%174;}\n"
    "float litOf(int id){int c=cellOf(id);return letterBit(c%29,c/29);}\n"
    /* how far its letter has risen: one letter after another */
    "float formOf(int id,int li){\n"
    "return clamp((uForm-float(li)*0.145)/0.40,0.,1.);}\n"
    /* where the lattice wants the particle */
    "vec3 lattice(int id,float t,float seed){\n"
    "int i=id%16,j=(id/16)%16,k=(id/256)%16;\n"
    "vec3 g=(vec3(i,j,k)-7.5)*.44;\n"
    "g*=1.+.075*sin(t*2.2-length(g)*1.35);\n"
    "float a=t*.11,ca=cos(a),sa=sin(a);\n"
    "g=vec3(g.x*ca-g.z*sa,g.y,g.x*sa+g.z*ca);\n"
    "float b=t*.07,cb=cos(b),sb=sin(b);\n"
    "g=vec3(g.x,g.y*cb-g.z*sb,g.y*sb+g.z*cb);\n"
    "g+=(vec3(hash13(vec3(seed,7.,1.)),hash13(vec3(seed,7.,2.)),hash13(vec3(seed,7.,3.)))-.5)*.07;\n"
    "return g;}\n"
    /* where the word wants the particle */
    "vec3 word(int id,float t,float seed,float g){\n"
    "int c=cellOf(id),lc=c%29,lr=c/29;\n"
    "vec3 p=vec3((float(lc)-14.)*.30,(2.5-float(lr))*.30,0.);\n"
    "vec3 j=vec3(hash13(vec3(seed,1.,2.)),hash13(vec3(seed,3.,4.)),hash13(vec3(seed,5.,6.)))-.5;\n"
    "p+=j*vec3(.10,.10,.30);\n"
    "p.z+=sin(t*1.7+seed*40.)*.045;\n"
    "p.y-=(1.-g)*1.35;\n"
    "return p;}\n"
    "void main(){\n"
    "ivec2 tc=ivec2(gl_FragCoord.xy);\n"
    "vec4 P=texelFetch(uPos,tc,0),V=texelFetch(uVel,tc,0);\n"
    "vec3 p=P.xyz;float life=P.w;float seed=V.w;\n"
    "vec3 v=V.xyz;\n"
    "int id=tc.x+tc.y*256;\n"
    "int c=cellOf(id),lc=c%29,li=lc/6;\n"
    "float lit=letterBit(lc,c/29);\n"
    "float g=formOf(id,li);\n"
    "vec3 lat=lattice(id,uT,seed);\n"
    "vec3 wrd=word(id,uT,seed,g);\n"
    "vec3 f=curl(p*.40+vec3(0,0,uT*.09))*(2.7*uW1+.30);\n"
    "f+=(lat-p)*uW2*7.5;\n"
    "if(lit>.5){\n"
    "  f+=(wrd-p)*g*uW3*11.;\n"
    "}else{\n"
    "  vec2 d2=p.xy;float l2=length(d2)+.001;\n"
    "  f+=vec3(d2/l2,0.)*uClear*4.5;\n"
    "}\n"
    "f+=normalize(p+vec3(.001,.001,.001))*uKick*3.2;\n"
    "f-=v*(1.35+3.6*uW1+2.4*uW3);\n"
    "v+=f*uDt;\n"
    "p+=v*uDt;\n"
    "float alive=lit>.5?g:mix(1.,0.,uClear);\n"
    "life=mix(life,alive,min(1.,uDt*2.2));\n"
    "gl_FragData[0]=vec4(p,life);\n"
    "gl_FragData[1]=vec4(v,seed);}\n";

/* the state the swarm starts from: a wide, slowly turning cloud */
static const char FS_SIMV[] =
    "#version 330\n"
    "uniform float uSeed;\n"
    "in vec2 vUv;\n"
    "float hash13(vec3 p){p=fract(p*.1031);p+=dot(p,p.zyx+31.32);\n"
    "return fract((p.x+p.y)*p.z);}\n"
    "void main(){\n"
    "vec3 c=vec3(gl_FragCoord.xy,uSeed);\n"
    "float h=hash13(c);\n"
    "float a=h*6.2831853;\n"
    "float u=hash13(c+7.1)*2.-1.;\n"
    "float sq=sqrt(max(0.,1.-u*u));\n"
    "float r=.9+pow(hash13(c+13.7),.62)*5.4;\n"
    "vec3 p=vec3(sq*cos(a),u,sq*sin(a))*r;\n"
    "p.y*=.78;\n"
    "p*=1.+.20*sin(a*3.+r*2.1);\n"
    "vec3 v=vec3(-p.z,0.,p.x)*.05/(1.+r*.35);\n"
    "v+=(vec3(hash13(c+21.3),hash13(c+31.7),hash13(c+41.1))-.5)*.35;\n"
    "gl_FragData[0]=vec4(p,1.);\n"
    "gl_FragData[1]=vec4(v,hash13(c+3.3));}\n";

/* the background */
static const char FS_BG[] =
    "#version 330\n"
    "uniform vec2 uRes;uniform float uT,uOrder,uKick,uGold;\n"
    "in vec2 vUv;out vec4 outColor;\n"
    "float hash21(vec2 p){p=fract(p*vec2(.1031,.1030));p+=dot(p,p.yx+33.33);\n"
    "return fract((p.x+p.y)*p.x);}\n"
    "void main(){\n"
    "vec2 uv=gl_FragCoord.xy/uRes;\n"
    "vec3 c=mix(vec3(.014,.010,.034),vec3(.008,.026,.050),uv.y);\n"
    "c=mix(c,vec3(.020,.036,.070),uOrder*.7);\n"
    "c=mix(c,vec3(.045,.030,.055),uGold*.6);\n"
    /* a slow nebula */
    "float n=0.;\n"
    "for(int i=0;i<3;i++){float s=.006+float(i)*.004;\n"
    "n+=sin(uv.x*23.+uT*.07+float(i))*sin(uv.y*19.-uT*.05+float(i)*1.7);}\n"
    "c+=vec3(.07,.03,.12)*max(n,0.)*.40*(1.-uOrder*.75);\n"
    "c+=vec3(.10,.06,.02)*max(n,0.)*.30*uGold;\n"
    /* stars */
    "vec2 sp=uv*vec2(uRes.x/uRes.y,1.)*90.;\n"
    "vec2 si=floor(sp);float h=hash21(si);\n"
    "if(h>.975){vec2 fp=fract(sp)-.5;\n"
    "float d=length(fp);\n"
    "float tw=.6+.4*sin(uT*3.+h*60.);\n"
    "c+=vec3(.75,.82,1.)*exp(-d*d*90.)*.75*tw;}\n"
    "c+=vec3(.10,.14,.30)*uKick*.28;\n"
    "c+=vec3(.16,.10,.03)*uKick*.18*uGold;\n"
    "float vg=1.+.55*pow(length(uv-.5)*1.35,2.4);\n"
    "outColor=vec4(c/vg,1.);}\n";

/* the particles */
static const char VS_DRAW[] =
    "#version 330\n"
    "uniform sampler2D uPos;uniform mat4 uVP;uniform vec2 uRes;\n"
    "uniform float uT,uOrder,uBeat,uSize,uGold;\n"
    "out float vLife;out vec3 vCol;out vec2 vOff;out float vDim;\n"
    "void main(){\n"
    "int id=gl_VertexID/6;\n"
    "int k=gl_VertexID-id*6;\n"
    "ivec2 c=ivec2(id&255,id>>8);\n"
    "vec4 P=texelFetch(uPos,c,0);\n"
    "vec4 cp=uVP*vec4(P.xyz,1.);\n"
    "float sx=(k==1||k==4||k==5)?1.:-1.;\n"
    "float sy=(k==2||k==3||k==5)?1.:-1.;\n"
    "vOff=vec2(sx,sy);\n"
    "float px=uSize*(.55+.70*P.w);\n"
    "vec2 off=vOff*px*2./uRes;\n"
    "gl_Position=vec4(cp.xy+off*cp.w,cp.z,cp.w);\n"
    "vLife=P.w;\n"
    "float seed=float(id)*.618;\n"
    "vec3 cold=vec3(.26,.22,.95);\n"
    "vec3 mid=vec3(.30,.85,1.05);\n"
    "vec3 hot=vec3(1.20,.80,.42);\n"
    "vec3 col=mix(cold,mid,uOrder);\n"
    "col=mix(col,hot,uGold);\n"
    "col*=.72+.45*sin(seed*31.+uT*2.);\n"
    "vCol=col*(.78+.55*uBeat);\n"
    "vDim=clamp(1.35-length(P.xyz)*.055,.25,1.);}\n";

static const char FS_DRAW[] =
    "#version 330\n"
    "in float vLife;in vec3 vCol;in vec2 vOff;in float vDim;out vec4 outColor;\n"
    "void main(){\n"
    "float r2=dot(vOff,vOff);\n"
    "if(r2>1.)discard;\n"
    "float halo=exp(-r2*2.8),core=exp(-r2*9.0);\n"
    "outColor=vec4(vCol*(halo*.20+core*.80)*vLife*vDim,1.);}\n";

/* the bloom: one pass for the box down, two for the blur */
static const char FS_BLUR[] =
    "#version 330\n"
    "uniform sampler2D uTex;uniform vec2 uTexel;uniform float uMode,uThresh;\n"
    "in vec2 vUv;out vec4 outColor;\n"
    "void main(){\n"
    "vec3 s;\n"
    "if(uMode<.5){\n"
    "s=(texture(uTex,vUv+vec2(-.5,-.5)*uTexel).rgb\n"
    "+texture(uTex,vUv+vec2(.5,-.5)*uTexel).rgb\n"
    "+texture(uTex,vUv+vec2(-.5,.5)*uTexel).rgb\n"
    "+texture(uTex,vUv+vec2(.5,.5)*uTexel).rgb)*.25;}\n"
    "else if(uMode<1.5){\n"
    "s=texture(uTex,vUv).rgb*.227;\n"
    "s+=(texture(uTex,vUv+vec2(1.,0.)*uTexel).rgb+texture(uTex,vUv-vec2(1.,0.)*uTexel).rgb)*.194;\n"
    "s+=(texture(uTex,vUv+vec2(2.,0.)*uTexel).rgb+texture(uTex,vUv-vec2(2.,0.)*uTexel).rgb)*.121;\n"
    "s+=(texture(uTex,vUv+vec2(3.,0.)*uTexel).rgb+texture(uTex,vUv-vec2(3.,0.)*uTexel).rgb)*.054;\n"
    "s+=(texture(uTex,vUv+vec2(4.,0.)*uTexel).rgb+texture(uTex,vUv-vec2(4.,0.)*uTexel).rgb)*.016;}\n"
    "else{\n"
    "s=texture(uTex,vUv).rgb*.227;\n"
    "s+=(texture(uTex,vUv+vec2(0.,1.)*uTexel).rgb+texture(uTex,vUv-vec2(0.,1.)*uTexel).rgb)*.194;\n"
    "s+=(texture(uTex,vUv+vec2(0.,2.)*uTexel).rgb+texture(uTex,vUv-vec2(0.,2.)*uTexel).rgb)*.121;\n"
    "s+=(texture(uTex,vUv+vec2(0.,3.)*uTexel).rgb+texture(uTex,vUv-vec2(0.,3.)*uTexel).rgb)*.054;\n"
    "s+=(texture(uTex,vUv+vec2(0.,4.)*uTexel).rgb+texture(uTex,vUv-vec2(0.,4.)*uTexel).rgb)*.016;}\n"
    "if(uMode<.5)s=max(s-vec3(uThresh),vec3(0.));\n"
    "outColor=vec4(s,1.);}\n";

static const char FS_COMP[] =
    "#version 330\n"
    "uniform sampler2D uScene,uBloom;uniform float uAmt,uFade;\n"
    "in vec2 vUv;out vec4 outColor;\n"
    "void main(){\n"
    "vec3 c=texture(uScene,vUv).rgb+texture(uBloom,vUv).rgb*uAmt;\n"
    "outColor=vec4(c*uFade,1.);}\n";

/* --------------------------------------------------------------- music --- */
static unsigned int rs = 0x9e3779b9u;

static float rnd(void) {
    rs = rs * 1664525u + 1013904223u;
    return (float)((rs >> 8) & 0xffffffu) * (1.f / 8388608.f) - 1.f;
}

/* the progression, one chord per bar: Am F C G */
static const float ROOT[4] = { 55.00f, 43.65f, 65.41f, 49.00f };
static const float CHORD[4][3] = {
    { 220.00f, 261.63f, 329.63f },     /* A C E */
    { 174.61f, 220.00f, 261.63f },     /* F A C */
    { 261.63f, 329.63f, 392.00f },     /* C E G */
    { 196.00f, 246.94f, 293.66f }      /* G B D */
};
static const int ARP[16] = { 0, 1, 2, 1, 0, 2, 1, 2, 0, 1, 2, 2, 1, 0, 2, 1 };

static void synth(float *out, int n, float t0) {
    static float pb, pp[3], pa, paenv, pahz;
    static float pmel, pmelenv, pmeldec, pmelhz;
    static float pkick, phat, plp;
    static float dbuf[DLY];
    static int dpos, laststep = -1, lastbeat = -1;
    const float dt = 1.f / RATE;
    int i;

    for (i = 0; i < n; i++) {
        float t = t0 + i * dt;
        int bar = (int)(t / (BEAT * 4.f));
        int beat = (int)(t / BEAT);
        int step = (int)(t / STEP);
        int c = bar & 3;

        if (beat != lastbeat) {
            lastbeat = beat;
            pkick = 1.f;
            pahz = ROOT[c];                      /* the bass changes note */
        }
        if (step != laststep) {
            laststep = step;
            int arp = ARP[step & 15];
            float hz = CHORD[c][arp & 2] * ((arp & 2) ? 2.f : 1.f);
            if (arp & 1) hz *= 1.5f;            /* a fifth on top */
            pahz = hz;
            paenv = 1.f;
            phat = (step & 1) ? 1.f : 0.f;
            if (t > 21.f && t < 47.f) {
                pmelenv = 0.20f;
                pmeldec = 3.4f;
                pmelhz = CHORD[c][ARP[(step + 5) & 15]] * ((step & 8) ? 4.f : 2.f);
            } else if (t >= 47.f && (step & 3) == 0) {
                pmelenv = 0.17f;
                pmeldec = 1.0f;
                pmelhz = CHORD[c][0] * 4.f;
            }
        }

        /* the bass: the root of the chord, with a little pitch drop */
        pkick *= 0.9992f;
        float bf = ROOT[c] * (1.f + 0.9f * pkick * pkick);
        pb += bf * dt;
        float bass = (fsin(pb * 6.2831853f) * 0.72f +
                      fsin(pb * 6.2831853f * 2.f) * 0.18f) * 0.26f;

        /* the chord: a slow swell that carries the last phase */
        float padg = fsmooth(30.f, 44.f, t) * 0.15f;
        float pad = 0.f;
        for (int k = 0; k < 3; k++) {
            pp[k] += CHORD[c][k] * (1.f + 0.0035f * k) * dt;
            pad += fsin(pp[k] * 6.2831853f);
        }
        pad *= padg;

        /* the arpeggio: a short pluck on every sixteenth */
        pa += pahz * dt;
        paenv *= 0.9935f;
        float arp = (2.f * (pa - (float)(int)pa) - 1.f) * paenv * 0.085f;
        arp *= fsmooth(12.f, 20.f, t) * (1.f - fsmooth(44.f, 52.f, t));

        /* the melody */
        float mel = 0.f;
        if (pmelenv > 0.0002f) {
            pmelenv *= 0.99955f - 0.0004f * pmeldec;
            pmel += pmelhz * dt;
            mel = fsin(pmel * 6.2831853f) * pmelenv * 0.55f;
        }

        /* the hats */
        float hat = 0.f;
        if (phat > 0.001f) {
            float nz = rnd();
            phat *= 0.82f;
            plp += (nz - plp) * 0.5f;
            hat = (nz - plp) * phat * 0.22f * fsmooth(5.f, 13.f, t) *
                  (1.f - fsmooth(50.f, 56.f, t));
        }

        /* the air of the opening */
        float air = rnd() * 0.06f * fsmooth(0.f, 3.f, t) * (1.f - fsmooth(4.f, 18.f, t));

        /* a delay on the bright parts, which gives the piece its space */
        float d = dbuf[dpos];
        float s = bass + pad + air + hat + (arp + mel + d * 0.45f);
        dbuf[dpos] = s * 0.35f + d * 0.38f;
        dpos = (dpos + 1) & (DLY - 1);

        s *= 0.92f;
        if (t < 1.5f) s *= t * t * 0.44f;
        if (t > 56.f) s *= fclamp((60.f - t) * 0.5f, 0.f, 1.f);
        out[2 * i] = s;
        out[2 * i + 1] = s * 0.93f;
    }
}

/* ------------------------------------------------------------- matrices -- */
static void mul(float *o, const float *a, const float *b) {
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) {
            float s = 0.f;
            for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
            o[c * 4 + r] = s;
        }
}

static void persp(float *m, float fovy, float asp, float zn, float zf) {
    float h = fovy * 0.5f;
    float f = fcos(h) / fsin(h);        /* one over the tangent of half the angle */
    memset(m, 0, sizeof(float) * 16);
    m[0] = f / asp;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1.f;
    m[14] = 2.f * zf * zn / (zn - zf);
}

static void lookat(float *m, float ex, float ey, float ez,
                   float cx, float cy, float cz, float ux, float uy, float uz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float rl = 1.f / fsqrt(fx * fx + fy * fy + fz * fz);
    fx *= rl; fy *= rl; fz *= rl;
    float sx = fy * uz - fz * uy, sy = fz * ux - fx * uz, sz = fx * uy - fy * ux;
    rl = 1.f / fsqrt(sx * sx + sy * sy + sz * sz);
    sx *= rl; sy *= rl; sz *= rl;
    float vx = sy * fz - sz * fy, vy = sz * fx - sx * fz, vz = sx * fy - sy * fx;
    m[0] = sx;  m[4] = sy;  m[8] = sz;   m[12] = -(sx * ex + sy * ey + sz * ez);
    m[1] = vx;  m[5] = vy;  m[9] = vz;   m[13] = -(vx * ex + vy * ey + vz * ez);
    m[2] = -fx; m[6] = -fy; m[10] = -fz; m[14] = (fx * ex + fy * ey + fz * ez);
    m[3] = 0.f;  m[7] = 0.f;  m[11] = 0.f;  m[15] = 1.f;
}

/* --------------------------------------------------------------- gl aids - */
static GLuint mktex(int w, int h, GLenum ifmt, GLenum fmt, GLenum type, GLint filt) {
    GLuint t = 0;
    SYM(glGenTextures, 1, &t);
    SYM(glBindTexture, GL_TEXTURE_2D, t);
    SYM(glTexImage2D, GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, type, NULL);
    SYM(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    SYM(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
    SYM(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    SYM(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

static GLuint mkfbo(GLuint tex) {
    GLuint f = 0;
    SYM(glGenFramebuffers, 1, &f);
    SYM(glBindFramebuffer, GL_FRAMEBUFFER, f);
    SYM(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (SYM(glCheckFramebufferStatus, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "morphdemo: incomplete framebuffer\n");
        return 0;
    }
    return f;
}

/* the state of the swarm needs two attachments: the place and the motion */
static GLuint mkfbo2(GLuint a, GLuint b) {
    GLuint f = 0;
    static const GLenum bufs[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    SYM(glGenFramebuffers, 1, &f);
    SYM(glBindFramebuffer, GL_FRAMEBUFFER, f);
    SYM(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, a, 0);
    SYM(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, b, 0);
    SYM(glDrawBuffers, 2, bufs);
    if (SYM(glCheckFramebufferStatus, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "morphdemo: incomplete state framebuffer\n");
        return 0;
    }
    return f;
}

/* ----------------------------------------------------------------- main -- */
int main(void) {
    GLuint pSim, pSimV, pBG, pDraw, pBlur, pComp;
    GLuint texPos[2], texVel[2], fboState[2], texScene, texBloomA, texBloomB;
    GLuint fboScene, fboBloomA, fboBloomB;
    GLuint vao = 0;
    int cur = 0, i;
    int uDt, uT, uW1, uW2, uW3, uClear, uForm, uKick, uOrder, uGold;
    int bRes, bT, bOrder, bKick, bGold;
    int dPos, dVP, dT, dOrder, dBeat, dSize, dRes, dGold;
    int blTex, blTexel, blMode, blThresh;
    int cScene, cBloom, cAmt, cFade;
    float buf[MAXBLK * 2];
    float vp[16], proj[16], view[16];
    float t = 0.f, kick = 0.f;
    long frames = 0;              /* frames the sound device has taken */
    double wall0, lastwall;

    if (!plat_init(1920, 1080, "MorphDemo 64k - CONVERGENCE")) return 1;

    pSim = plat_program(VS, FS_SIM);
    pSimV = plat_program(VS, FS_SIMV);
    pBG = plat_program(VS, FS_BG);
    pDraw = plat_program(VS_DRAW, FS_DRAW);
    pBlur = plat_program(VS, FS_BLUR);
    pComp = plat_program(VS, FS_COMP);
    if (!pSim || !pSimV || !pBG || !pDraw || !pBlur || !pComp) return 1;

    /* the state textures: position and life, velocity and seed */
    for (i = 0; i < 2; i++) {
        texPos[i] = mktex(N, N, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST);
        texVel[i] = mktex(N, N, GL_RGBA32F, GL_RGBA, GL_FLOAT, GL_NEAREST);
        fboState[i] = mkfbo2(texPos[i], texVel[i]);
        if (!fboState[i]) {
            /* the driver does not want float targets: fall back to half floats */
            texPos[i] = mktex(N, N, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, GL_NEAREST);
            texVel[i] = mktex(N, N, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, GL_NEAREST);
            fboState[i] = mkfbo2(texPos[i], texVel[i]);
            if (!fboState[i]) return 1;
        }
    }
    texScene = mktex(g_width, g_height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
    texBloomA = mktex(BLOOMX, BLOOMY, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
    texBloomB = mktex(BLOOMX, BLOOMY, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
    fboScene = mkfbo(texScene);
    fboBloomA = mkfbo(texBloomA);
    fboBloomB = mkfbo(texBloomB);
    if (!fboScene || !fboBloomA || !fboBloomB) return 1;

    SYM(glGenVertexArrays, 1, &vao);
    SYM(glBindVertexArray, vao);

    /* the initial state of the swarm */
    plat_use(pSimV);
    SYM(glBindFramebuffer, GL_FRAMEBUFFER, fboState[0]);
    SYM(glViewport, 0, 0, N, N);
    SYM(glUniform1f, plat_uni("uSeed"), 3.7f);
    SYM(glDrawArrays, GL_TRIANGLES, 0, 3);

    plat_use(pSim);
    uDt = plat_uni("uDt"); uT = plat_uni("uT");
    uW1 = plat_uni("uW1"); uW2 = plat_uni("uW2"); uW3 = plat_uni("uW3");
    uClear = plat_uni("uClear"); uForm = plat_uni("uForm");
    uKick = plat_uni("uKick"); uOrder = plat_uni("uOrder"); uGold = plat_uni("uGold");
    plat_use(pBG);
    bRes = plat_uni("uRes"); bT = plat_uni("uT");
    bOrder = plat_uni("uOrder"); bKick = plat_uni("uKick"); bGold = plat_uni("uGold");
    plat_use(pDraw);
    dPos = plat_uni("uPos"); dVP = plat_uni("uVP"); dT = plat_uni("uT");
    dOrder = plat_uni("uOrder"); dBeat = plat_uni("uBeat"); dSize = plat_uni("uSize");
    dRes = plat_uni("uRes"); dGold = plat_uni("uGold");
    plat_use(pBlur);
    blTex = plat_uni("uTex"); blTexel = plat_uni("uTexel");
    blMode = plat_uni("uMode"); blThresh = plat_uni("uThresh");
    plat_use(pComp);
    cScene = plat_uni("uScene"); cBloom = plat_uni("uBloom");
    cAmt = plat_uni("uAmt"); cFade = plat_uni("uFade");

    SYM(glDisable, GL_DEPTH_TEST);
    SYM(glDisable, GL_CULL_FACE);
    SYM(glClearColor, 0.f, 0.f, 0.f, 1.f);

    plat_audio_open(RATE);
    wall0 = plat_now();
    lastwall = wall0;

    for (;;) {
        int need, w;
        double wall;
        float w1, w2, w3, clear, form, order, gold, beat, ex, ey, ez, size;
        if (plat_poll()) break;

        /* The wall clock is the clock of the production: the sound device is
         * fed exactly as much as it asks for, so the music always plays at
         * its own speed and the picture follows it.  When the machine is
         * slow, frames are dropped but the music never stutters.
         *
         * The device is fed in a loop and not with a single block: one block
         * per picture would cap the sound at "pictures per second times block
         * size", and the picture is only drawn thirty times a second. */
        wall = plat_now() - wall0;
        need = (int)((wall * RATE) - (double)frames);
        if (need < 0) need = 0;
        while (need > 0) {
            int n = need > MAXBLK ? MAXBLK : need;
            synth(buf, n, (float)frames / RATE);
            w = plat_audio_write(buf, n);
            frames += w;
            need -= w;
            if (w <= 0) break;        /* the device is not taking any more */
        }
        t = (float)frames / RATE;
        if (t >= DUR) break;
        if (wall > 67.0) break;

        /* the shape of the production */
        w1 = 1.f - fsmooth(11.f, 20.f, t);
        w2 = fsmooth(15.f, 24.f, t) * (1.f - fsmooth(37.f, 43.f, t));
        w3 = fsmooth(37.f, 44.f, t);
        clear = fsmooth(35.f, 41.f, t);
        form = fsmooth(37.f, 48.f, t);
        order = fsmooth(8.f, 40.f, t);
        gold = fsmooth(36.f, 46.f, t);
        beat = 1.f - ffract(t / BEAT);
        kick *= 0.88f;
        if (ffract(t / BEAT) < 0.06f) kick = 1.f;

        /* the camera: it drifts around the swarm and then settles in front
         * of the word */
        {
            float p1 = fsmooth(14.f, 32.f, t);
            float p2 = fsmooth(35.f, 48.f, t);
            float a = fsin(t * 0.13f), b = fcos(t * 0.11f);
            float ax = fsin(t * 0.16f), ay = fcos(t * 0.14f);
            ex = fmix(a * 2.8f, ax * 2.0f, p1);
            ey = fmix(b * 1.3f, ay * 0.8f, p1);
            ez = fmix(8.6f, 7.2f, p1);
            ex = fmix(ex, 0.f, p2);
            ey = fmix(ey, 0.f, p2);
            ez = fmix(ez, 5.6f, p2);
            ex += 0.22f * fsin(t * 0.23f) * (1.f - p2);
            ey += 0.10f * fsin(t * 0.19f + 1.f) * (1.f - p2);
        }
        persp(proj, 1.05f, (float)g_width / (float)g_height, 0.1f, 60.f);
        lookat(view, ex, ey, ez, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f);
        mul(vp, proj, view);

        /* --- the simulation ------------------------------------------- */
        {
            double now = plat_now();
            float dt = (float)(now - lastwall);
            lastwall = now;
            if (dt > 0.05f) dt = 0.05f;
            if (dt < 0.001f) dt = 0.001f;
            plat_use(pSim);
            SYM(glBindFramebuffer, GL_FRAMEBUFFER, fboState[1 - cur]);
            SYM(glViewport, 0, 0, N, N);
            SYM(glActiveTexture, GL_TEXTURE0);
            SYM(glBindTexture, GL_TEXTURE_2D, texPos[cur]);
            SYM(glActiveTexture, GL_TEXTURE1);
            SYM(glBindTexture, GL_TEXTURE_2D, texVel[cur]);
            SYM(glUniform1i, plat_uni("uPos"), 0);
            SYM(glUniform1i, plat_uni("uVel"), 1);
            SYM(glUniform1f, uDt, dt);
            SYM(glUniform1f, uT, t);
            SYM(glUniform1f, uW1, w1);
            SYM(glUniform1f, uW2, w2);
            SYM(glUniform1f, uW3, w3);
            SYM(glUniform1f, uClear, clear);
            SYM(glUniform1f, uForm, form);
            SYM(glUniform1f, uKick, kick);
            SYM(glUniform1f, uOrder, order);
            SYM(glUniform1f, uGold, gold);
            SYM(glDrawArrays, GL_TRIANGLES, 0, 3);
            cur = 1 - cur;
        }

        /* --- the picture ---------------------------------------------- */
        SYM(glBindFramebuffer, GL_FRAMEBUFFER, fboScene);
        SYM(glViewport, 0, 0, g_width, g_height);
        SYM(glDisable, GL_BLEND);
        plat_use(pBG);
        SYM(glUniform2f, bRes, (float)g_width, (float)g_height);
        SYM(glUniform1f, bT, t);
        SYM(glUniform1f, bOrder, order);
        SYM(glUniform1f, bKick, kick);
        SYM(glUniform1f, bGold, gold);
        SYM(glDrawArrays, GL_TRIANGLES, 0, 3);

        SYM(glEnable, GL_BLEND);
        SYM(glBlendFunc, GL_ONE, GL_ONE);
        plat_use(pDraw);
        SYM(glActiveTexture, GL_TEXTURE0);
        SYM(glBindTexture, GL_TEXTURE_2D, texPos[cur]);
        SYM(glUniform1i, dPos, 0);
        SYM(glUniform1f, dT, t);
        SYM(glUniform1f, dOrder, order);
        SYM(glUniform1f, dBeat, beat);
        SYM(glUniform1f, dGold, gold);
        size = fmix(4.6f, 3.1f, fsmooth(30.f, 46.f, t));
        SYM(glUniform1f, dSize, size);
        SYM(glUniform2f, dRes, (float)g_width, (float)g_height);
        SYM(glUniformMatrix4fv, dVP, 1, GL_FALSE, vp);
        SYM(glDrawArrays, GL_TRIANGLES, 0, NPART * 6);
        SYM(glDisable, GL_BLEND);

        /* --- the bloom ------------------------------------------------ */
        plat_use(pBlur);
        SYM(glUniform1i, blTex, 0);
        SYM(glUniform1f, blThresh, 0.42f);
        /* down */
        SYM(glBindFramebuffer, GL_FRAMEBUFFER, fboBloomA);
        SYM(glViewport, 0, 0, BLOOMX, BLOOMY);
        SYM(glActiveTexture, GL_TEXTURE0);
        SYM(glBindTexture, GL_TEXTURE_2D, texScene);
        SYM(glUniform2f, blTexel, 1.f / (float)g_width, 1.f / (float)g_height);
        SYM(glUniform1f, blMode, 0.f);
        SYM(glDrawArrays, GL_TRIANGLES, 0, 3);
        /* horizontal */
        SYM(glBindFramebuffer, GL_FRAMEBUFFER, fboBloomB);
        SYM(glBindTexture, GL_TEXTURE_2D, texBloomA);
        SYM(glUniform2f, blTexel, 1.f / (float)BLOOMX, 0.f);
        SYM(glUniform1f, blMode, 1.f);
        SYM(glDrawArrays, GL_TRIANGLES, 0, 3);
        /* vertical */
        SYM(glBindFramebuffer, GL_FRAMEBUFFER, fboBloomA);
        SYM(glBindTexture, GL_TEXTURE_2D, texBloomB);
        SYM(glUniform2f, blTexel, 0.f, 1.f / (float)BLOOMY);
        SYM(glUniform1f, blMode, 2.f);
        SYM(glDrawArrays, GL_TRIANGLES, 0, 3);

        /* --- the final image ------------------------------------------ */
        SYM(glBindFramebuffer, GL_FRAMEBUFFER, 0);
        SYM(glViewport, 0, 0, g_width, g_height);
        plat_use(pComp);
        SYM(glActiveTexture, GL_TEXTURE0);
        SYM(glBindTexture, GL_TEXTURE_2D, texScene);
        SYM(glUniform1i, cScene, 0);
        SYM(glActiveTexture, GL_TEXTURE1);
        SYM(glBindTexture, GL_TEXTURE_2D, texBloomA);
        SYM(glUniform1i, cBloom, 1);
        SYM(glUniform1f, cAmt, 1.15f);
        SYM(glUniform1f, cFade, 1.f - fsmooth(56.5f, 60.f, t));
        SYM(glDrawArrays, GL_TRIANGLES, 0, 3);
        plat_swap();
    }

    plat_audio_close();
    plat_done();
    return 0;
}
