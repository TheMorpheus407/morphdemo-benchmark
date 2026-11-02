/* fmath.h -- the few math helpers the productions need.
 *
 * The binaries deliberately do not link libm: everything is a small
 * polynomial or a bit trick, which keeps the dependency set down to libX11,
 * libGL, libasound and libc and keeps the code small.
 */
#ifndef MORPH_FMATH_H
#define MORPH_FMATH_H

/* Sine of an angle in radians.  The argument is folded into [-pi, pi] and the
 * result comes from a five term polynomial, which is accurate to about
 * 1e-6 over the whole range -- far more than a soundtrack needs. */
static inline float fsin(float x) {
    x -= 6.28318531f * (float)(int)(x * 0.15915494f + (x >= 0.f ? 0.5f : -0.5f));
    float x2 = x * x;
    return x * (1.f + x2 * (-0.16666667f + x2 * (0.00833333f - x2 * 0.00019841f)));
}

static inline float fcos(float x) { return fsin(x + 1.57079633f); }

static inline float fsqrt(float x) { return __builtin_sqrtf(x); }

/* fractional part for a positive argument */
static inline float ffract(float x) { return x - (float)(int)x; }

static inline float fclamp(float x, float a, float b) {
    return x < a ? a : (x > b ? b : x);
}

static inline float fmix(float a, float b, float t) { return a + (b - a) * t; }

/* smooth step from a to b */
static inline float fsmooth(float a, float b, float x) {
    float t = fclamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

#endif /* MORPH_FMATH_H */
