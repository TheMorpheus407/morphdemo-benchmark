/* demo16.c -- "ANNEAL", the sixteen kilobyte production.
 *
 * The piece is one continuous landscape that changes its mind three times.
 *
 *   0 - 17 s   EMERGENCE   a boiling noise terrain, ember light, no pulse
 *   17 - 38 s  ORDER       the noise collapses into a regular lattice of
 *                           pillars and a pulse starts to travel through it
 *   38 - 60 s  RESOLUTION  the lattice reorganises: everything that is not
 *                           part of the word sinks away, the five letters of
 *                           MORPH rise one after another and light up
 *
 * The picture is a single ray marched height field.  Because the terrain is
 * a function z = h(x, y) the march is cheap: the ray is stepped along z and
 * the height is looked up, which is exact and needs no sphere tracing.
 *
 * The music and the picture share one clock: the wall clock.  The sound device
 * is fed exactly as much as that clock asks for, so the beat of the score and
 * the beat of the landscape are the same beat and the piece lasts exactly one
 * minute.
 */
#include <stdio.h>

#include "platform.h"
#include "fmath.h"
#include "syms_common.h"

#define SYMS(X) SYMS_ALL(X)
#include "symtab.h"

#define RATE  48000
#define DUR   60.0f
#define BEAT  (60.0f / 90.0f)      /* 90 beats per minute */
/* The sound device is fed in small blocks on purpose.  Its period is 200 ms,
 * so a write only returns once the device has room again, and a large block
 * would keep the picture waiting for the sound card.  768 frames are 16 ms,
 * which is short enough for the picture to run at the speed of the display. */
#define MAXBLK 768                  /* the largest audio block   */

/* --------------------------------------------------------------- shaders - */
static const char VS[] =
    "#version 330\n"
    "void main(){vec2 p=vec2(gl_VertexID&1,gl_VertexID>>1);\n"
    "gl_Position=vec4(p*4.-1.,0.,1.);}\n";

static const char FS[] =
    "#version 330\n"
    "uniform vec2 uRes;\n"
    "uniform vec3 uCam,uTgt;\n"
    "uniform float uT,uBeat,uOrder;\n"
    "uniform int uSteps;\n"
    "out vec4 outColor;\n"
    /* the sun: high and to the left, so the relief casts long shadows */
    "const vec3 SUN=vec3(-.4466,.6699,-.5955);\n"
    /* the five letters, each packed into thirty bits: six rows of five
     * columns, top row first, leftmost column in the highest bit */
    "const uint F[5]=uint[5](0x23bac631u,0x1d18c62eu,0x3d18fa92u,0x3d18fa10u,0x231fc631u);\n"
    "float hash21(vec2 p){p=fract(p*vec2(.1031,.1030));p+=dot(p,p.yx+33.33);\n"
    "return fract((p.x+p.y)*p.x);}\n"
    "float vnoise(vec2 p){vec2 i=floor(p),f=fract(p);f=f*f*(3.-2.*f);\n"
    "float a=hash21(i),b=hash21(i+vec2(1,0)),c=hash21(i+vec2(0,1)),d=hash21(i+vec2(1,1));\n"
    "return mix(mix(a,b,f.x),mix(c,d,f.x),f.y);}\n"
    /* two octaves are enough for the broad shapes, the ridge makes the edges */
    "float fbm2(vec2 p){return .62*vnoise(p)+.31*vnoise(p*2.13+7.7);}\n"
    /* one cell of the word */
    "float letterBit(int lc,int lr){\n"
    "if(lc<0||lc>28||lr<0||lr>5)return 0.;\n"
    "int li=lc/6,cx=lc-li*6;if(cx>4)return 0.;\n"
    "return float((F[li]>>uint(29-lr*5-cx))&1u);}\n"
    /* the grid of the world: one cell is half a unit, and the cells of the
     * word are cells of this very grid */
    "vec2 grid(vec2 p){return vec2((p.x+7.)*2.,(1.25-p.y)*2.);}\n"
    /* the height of the world at a point of the ground */
    "float terrain(vec2 p){\n"
    "float t=uT,h=0.;\n"
    /* the noise of the opening */
    "float w1=1.-smoothstep(11.,25.,t);\n"
    "if(w1>.002){\n"
    "float n=fbm2(p*.26+vec2(t*.05,-t*.035));\n"
    "float r=fbm2(p*.58+vec2(-t*.09,t*.04));\n"
    "r=1.-abs(r*2.-1.);\n"
    "h+=w1*(3.1*n-1.15+1.5*r*r*r);}\n"
    /* the lattice: one pillar per cell */
    "float w2=smoothstep(15.,25.,t)*(1.-smoothstep(38.,46.,t));\n"
    "if(w2>.002){vec2 g=grid(p),f=fract(g)-.5;\n"
    "float b=exp(-dot(f,f)*6.);\n"
    "float wv=.5+.5*sin(g.x*.26+g.y*.19-t*2.4);\n"
    "h+=w2*b*(.85+.95*wv);}\n"
    /* the word: the same cells, but only the ones that carry a letter.
     *
     * A letter cell is a solid tile, not a bump.  The distance to the edge of
     * the cell is taken over the three by three neighbourhood, so that two
     * cells side by side join into one continuous stroke instead of two
     * separate pillars. */
    "if(t>34. && abs(p.x)<8.6 && abs(p.y)<2.6){vec2 g=grid(p),cl=floor(g);\n"
    "float hh=0.;\n"
    "for(int dy=-1;dy<=1;dy++){for(int dx=-1;dx<=1;dx++){\n"
    "vec2 cc=cl+vec2(float(dx),float(dy));\n"
    "vec2 dd=abs(g-cc-.5);float ed=.5-max(dd.x,dd.y);\n"
    "if(ed>-.14&&letterBit(int(cc.x),int(cc.y))>.5){\n"
    "int li=int(cc.x)/6;\n"
    "float w=smoothstep(35.+float(li)*1.2,42.+float(li)*1.2,t);\n"
    "hh=max(hh,w*smoothstep(-.14,0.,ed));}}}\n"
    "h+=hh*1.0;}\n"
    "return h;}\n"
    "vec3 sky(vec3 rd){\n"
    "float h=clamp(rd.y*.5+.5,0.,1.);\n"
    "vec3 zen=mix(vec3(.020,.030,.070),vec3(.045,.090,.150),uOrder);\n"
    "vec3 hor=mix(vec3(.52,.16,.045),vec3(.42,.30,.18),uOrder);\n"
    "vec3 c=mix(hor,zen,pow(h,.5));\n"
    "float s=max(dot(rd,SUN),0.);\n"
    "c+=vec3(1.,.70,.35)*pow(s,120.)*3.4*(.5+.7*uOrder);\n"
    "c+=vec3(1.,.45,.18)*pow(s,6.)*.34*(1.-.5*uOrder);\n"
    "return c;}\n"
    "void main(){\n"
    "vec2 uv=(2.*gl_FragCoord.xy-uRes)/uRes.y;\n"
    "vec3 ro=uCam,ww=normalize(uTgt-ro);\n"
    "vec3 uu=normalize(cross(ww,vec3(0,1,0))),vv=cross(uu,ww);\n"
    "vec3 rd=normalize(uv.x*uu+uv.y*vv+1.85*ww);\n"
    /* march the height field: step down in z, look the height up */
    "float t=0.,hit=-1.,tp=0.;\n"
    "if(rd.z<-.001){float ts=(3.6-ro.z)/rd.z;if(ts>0.)t=ts;}\n"
    "for(int i=0;i<uSteps;i++){\n"
    "vec3 pos=ro+rd*t;\n"
    "if(pos.z<-.05)break;\n"
    "float d=pos.z-terrain(pos.xy);\n"
    "if(d<0.){float a=tp,b=t;\n"
    "for(int k=0;k<6;k++){float m=.5*(a+b);vec3 pm=ro+rd*m;\n"
    "if(pm.z-terrain(pm.xy)<0.)b=m;else a=m;}\n"
    "hit=b;break;}\n"
    "tp=t;t+=clamp(.6*d/max(.10,-rd.z),.020,.85);\n"
    "if(t>48.)break;}\n"
    "vec3 col;\n"
    "if(hit>0.){\n"
    "vec3 pos=ro+rd*hit;float e=.035;\n"
    "vec3 n=normalize(vec3(terrain(pos.xy-vec2(e,0))-terrain(pos.xy+vec2(e,0)),\n"
    "terrain(pos.xy-vec2(0,e))-terrain(pos.xy+vec2(0,e)),2.*e));\n"
    "float dif=max(dot(n,SUN),0.);\n"
    "float amb=.10+.15*max(n.y,0.);\n"
    "float spe=pow(max(dot(reflect(rd,n),SUN),0.),48.);\n"
    "vec3 base=mix(vec3(.46,.14,.045),vec3(.26,.27,.30),uOrder);\n"
    "col=base*(amb+dif*2.2);\n"
    "col+=vec3(1.,.85,.62)*spe*(.6+1.0*uOrder);\n"
    "col+=mix(vec3(1.,.34,.07),vec3(.45,.85,1.),uOrder)*pow(1.-max(dot(n,-rd),0.),3.)*.5;\n"
    /* the letters burn */
    "if(uT>34.){vec2 g=grid(pos.xy),cl=floor(g);\n"
    "if(letterBit(int(cl.x),int(cl.y))>.5){\n"
    "int li=int(cl.x)/6;\n"
    "float w=smoothstep(35.+float(li)*1.2,42.+float(li)*1.2,uT);\n"
    "col+=mix(vec3(1.15,.80,.35),vec3(.80,1.00,1.30),uOrder)*w*(.45+1.05*uBeat);}}\n"
    "col=mix(col,sky(rd),1.-exp(-hit*.016));}\n"
    "else col=sky(rd);\n"
    "col*=1.-smoothstep(56.5,60.,uT);\n"
    "outColor=vec4(col,1.);}\n";

/* --------------------------------------------------------------- music --- */
/* A drone under the whole piece, a noise bed for the opening, a pulse from
 * the moment the lattice forms and a pentatonic line that resolves into the
 * chord of the last phase. */
static unsigned int rs = 0x2f6e2b1u;

static float rnd(void) {
    rs = rs * 1664525u + 1013904223u;
    return (float)((rs >> 8) & 0xffffffu) * (1.f / 8388608.f) - 1.f;
}

/* the scale of the piece: a minor pentatonic and its extensions */
static const int SCALE[10] = { 0, 3, 5, 7, 10, 12, 15, 17, 19, 22 };
/* the melody, one entry per eighth note */
static const int PATT[16] = { 0, 2, 3, 4, 3, 2, 1, 2, 0, 3, 5, 6, 5, 3, 2, 0 };

static void synth(float *out, int n, float t0) {
    static float pd1 = 0.f, pd2 = 0.f, pk = 0.f, pbk = 0.f, pbm = 0.f;
    static float ppad[3] = { 0.f, 0.f, 0.f };
    static float lp = 0.f, lpn = 0.f;
    static float bellenv = 0.f, belldec = 0.f, bellhz = 0.f, bellmod = 0.f;
    static float kickenv = 0.f;
    static int lastbeat = -1, laststep = -1;
    const float dt = 1.f / RATE;

    for (int i = 0; i < n; i++) {
        float t = t0 + i * dt;

        /* the beat and the eighth note grid */
        int beat = (int)(t / BEAT);
        if (beat != lastbeat) {
            lastbeat = beat;
            kickenv = 1.f;
            pk = 0.f;
        }
        int step = (int)(t / (BEAT * 0.5f));
        if (step != laststep) {
            laststep = step;
            if (t > 19.f && t < 47.f) {
                int deg = PATT[step & 15];
                float semi = (float)SCALE[deg];
                bellhz = 220.f * (1.f + 0.06f * fsin(t * 0.7f));
                bellhz *= 1.f + semi * 0.0594631f;    /* two^(semi/12) */
                bellenv = 0.32f;
                belldec = 1.6f + 0.9f * (float)(deg % 3);
                bellmod = 1.f;
            } else if (t >= 47.f) {
                /* the line thins out and holds the chord */
                if ((step & 3) == 0) {
                    int deg = PATT[(step >> 2) & 15];
                    bellhz = 220.f * (1.f + 0.06f * fsin(t * 0.7f));
                    bellhz *= 1.f + (float)SCALE[deg] * 0.0594631f;
                    bellenv = 0.26f;
                    belldec = 0.9f;
                    bellmod = 0.f;
                }
            }
        }

        /* the drone, always there */
        pd1 += 55.f * dt;
        pd2 += 55.37f * dt;
        float saw = 2.f * (pd1 - (float)(int)pd1) - 1.f;
        float saw2 = 2.f * (pd2 - (float)(int)pd2) - 1.f;
        float drone = saw * 0.6f + saw2 * 0.4f;
        lp += (drone - lp) * 0.0022f;             /* low pass */
        float sw = 0.09f + 0.05f * fsin(t * 0.23f);

        /* the noise of the opening */
        float nz = rnd();
        lpn += (nz - lpn) * (0.02f + 0.05f * fclamp(1.f - t / 16.f, 0.f, 1.f));
        float noise = lpn * 0.42f * fclamp(1.f - (t - 4.f) / 14.f, 0.f, 1.f);

        /* the pulse that orders the lattice */
        float kick = 0.f;
        if (t > 15.f) {
            kickenv *= 0.9996f;
            float kf = 42.f + 60.f * kickenv * kickenv;
            pk += kf * dt;
            kick = fsin(pk * 6.2831853f) * kickenv * kickenv * 0.62f;
        }

        /* the line that resolves */
        float bell = 0.f;
        if (bellenv > 0.0005f) {
            bellenv *= 0.9992f - 0.00055f * belldec;
            pbk += bellhz * dt;
            float car = fsin(pbk * 6.2831853f);
            if (bellmod > 0.f) {
                pbm += bellhz * 2.76f * dt;
                car += 0.4f * fsin(pbm * 6.2831853f) * bellmod;
            }
            bell = car * bellenv * 0.16f;
        }

        /* the chord of the last phase */
        float pad = 0.f;
        if (t > 36.f) {
            float a = fclamp((t - 36.f) / 8.f, 0.f, 1.f);
            ppad[0] += 220.f * dt;
            ppad[1] += 329.63f * dt;
            ppad[2] += 440.f * dt;
            pad = (fsin(ppad[0] * 6.2831853f) + fsin(ppad[1] * 6.2831853f) * 0.7f +
                   fsin(ppad[2] * 6.2831853f) * 0.5f) * 0.055f * a;
        }

        float s = lp * sw + noise + kick + bell + pad;
        s *= 0.9f;
        if (t < 2.0f) s *= t * t * 0.25f;
        if (t > 56.f) s *= fclamp((60.f - t) * 0.5f, 0.f, 1.f);
        out[2 * i] = s;
        out[2 * i + 1] = s * 0.96f;
    }
}

/* --------------------------------------------------------------- camera -- */
static void camera(float t, float *pos, float *tgt) {
    static const float K[5][6] = {
        {  0.0f, 3.10f, 14.5f,  0.0f, 0.55f, -2.5f },
        {  6.0f, 2.70f, 11.0f,  0.0f, 0.55f, -1.0f },
        { -5.5f, 2.35f,  8.6f,  0.0f, 0.60f,  0.0f },
        {  1.2f, 2.60f, 10.8f,  0.0f, 0.10f, -0.3f },
        {  0.0f, 4.15f,  9.1f,  0.0f,-0.25f,  0.0f }
    };
    static const float KT[5] = { 0.f, 17.f, 33.f, 47.f, 60.f };
    int i = 0;
    while (i < 3 && t > KT[i + 1]) i++;
    float u = fsmooth(KT[i], KT[i + 1], t);
    for (int j = 0; j < 3; j++) {
        pos[j] = fmix(K[i][j], K[i + 1][j], u);
        tgt[j] = fmix(K[i][3 + j], K[i + 1][3 + j], u);
    }
    pos[0] += 0.26f * fsin(t * 0.21f);          /* never completely still */
    pos[1] += 0.09f * fsin(t * 0.17f + 1.3f);
}

/* ----------------------------------------------------------------- main -- */
int main(void) {
    GLuint prog;
    int uRes, uCam, uTgt, uT, uBeat, uOrder, uSteps;
    float buf[MAXBLK * 2];
    float pos[3], tgt[3];
    float t = 0.f;
    long frames = 0;              /* frames the sound device has taken */
    int steps = 120;              /* the length of the march */
    double wall0, lastframe;

    if (!plat_init(1920, 1080, "MorphDemo 16k - ANNEAL")) return 1;
    prog = plat_program(VS, FS);
    if (!prog) return 1;
    plat_use(prog);
    uRes = plat_uni("uRes");
    uCam = plat_uni("uCam");
    uTgt = plat_uni("uTgt");
    uT = plat_uni("uT");
    uBeat = plat_uni("uBeat");
    uOrder = plat_uni("uOrder");
    uSteps = plat_uni("uSteps");

    SYM(glViewport, 0, 0, g_width, g_height);
    SYM(glDisable, GL_DEPTH_TEST);
    SYM(glClearColor, 0.f, 0.f, 0.f, 1.f);

    plat_audio_open(RATE);
    wall0 = plat_now();
    lastframe = wall0;

    for (;;) {
        int need, w;
        double wall;
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

        /* keep the frame time below a twentieth of a second */
        {
            double ft = plat_now() - lastframe;
            lastframe = plat_now();
            if (ft > 0.050 && steps > 48) steps -= 8;
            else if (ft < 0.022 && steps < 120) steps += 4;
        }

        camera(t, pos, tgt);
        SYM(glClear, GL_COLOR_BUFFER_BIT);
        SYM(glUniform2f, uRes, (float)g_width, (float)g_height);
        SYM(glUniform3f, uCam, pos[0], pos[1], pos[2]);
        SYM(glUniform3f, uTgt, tgt[0], tgt[1], tgt[2]);
        SYM(glUniform1f, uT, t);
        SYM(glUniform1f, uBeat, 1.f - ffract(t / BEAT));
        SYM(glUniform1f, uOrder, fsmooth(9.f, 46.f, t));
        SYM(glUniform1i, uSteps, steps);
        SYM(glDrawArrays, GL_TRIANGLES, 0, 3);
        plat_swap();
    }

    plat_audio_close();
    plat_done();
    return 0;

}
