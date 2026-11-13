#version 430
layout(location = 0) uniform vec4 u[2];
out vec4 O;

float t, k, ar, nA, oA, cA;
vec2 r;

mat2 R(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

float B(vec3 p, vec3 b) {
    return length(max(abs(p) - b, 0.0));
}

void H(float d, float m) {
    if (d < r.x) r = vec2(d, m);
}

vec2 M(vec3 p) {
    r.x = 99.0;
    if (cA < 0.9) {
        vec3 q = p;
        q.y -= cA * 2.6;
        q.xz *= R(t * 0.35);
        for (int i = 0; i < 3; i++) {
            q = abs(q) - 0.18 - 0.2 * oA;
            q.xy *= R(0.6 + 0.2 * sin(t * 0.3 + i));
            q.yz *= R(0.45);
        }
        float w = dot(sin(p * 6.0 + t * 4.0), cos(p.yzx * 6.0));
        H(mix(length(q) - (1.0 - cA) * (0.14 + 0.2 * oA + 0.06 * k) + w * 0.04,
              max(length(p) - 1.9, abs(w) * 0.16 - 0.05), nA * 0.8) * 0.7, 1);
    }
    for (int i = 0; i < 2; i++) {
        vec3 q = p;
        q.y -= cA * (i > 0 ? 1.3 : -1.1);
        q.xy *= R((1.0 - cA) * (1 + i + t * 0.3));
        H(length(vec2(length(q.xz) - 2.6 - i * 0.7, q.y)) - 0.03 * oA - 0.01 * ar, 2);
    }
    if (cA > 0.04) {
        H(B(vec3(mod(clamp(p.x, -3.6, 3.6) + 0.6, 1.2) - 0.6, p.y, p.z + 3.8), vec3(0.2, 2.6 * cA, 0.2)), 2);
        float d = B(p, vec3(3, 0.7, 0.4));
        if (d < 0.3) {
            int i = clamp(int(p.x * 0.9 + 2.5), 0, 4),
                m = int[](18732593, 15255086, 32045649, 32045584, 18415153)[i], j = 0;
            float f = i - 2, s = max(0.0, 1.0 - cA * 1.5);
            s *= s;
            vec3 q = p;
            q.x -= f * 1.1;
            q = (q - sin(vec3(2, 3, 5) * f + t) * s) * 5.7;
            q.xz *= R(s);
            for (d = 1.9; j < 25; j++)
                if ((m >> j & 1) > 0)
                    d = min(d, B(q - vec3(2 - j % 5, j / 5 - 2, 0), vec3(0.55)) - 0.06);
            d *= 0.15;
        }
        H(d, 3);
    }
    H(p.y + 1.22 + 0.02 * sin(length(p.xz) * 5.0 - t * 4.0) * oA, 4);
    return r;
}

void main() {
    vec4 E = u[1] * 6e-5;
    t = u[0].z; k = E.x; ar = E.w;
    nA = 1.0 - smoothstep(6.0, 22.0, t);
    oA = smoothstep(14.0, 28.0, t);
    cA = smoothstep(40.0, 46.5, t);

    vec2 v = (gl_FragCoord.xy - 0.5 * u[0].xy) / u[0].y, h;
    v.x += sin(v.y * 140.0 + t * 45.0) * 0.03 * nA * (0.2 + E.y);

    float a = mix(t * 0.36, sin(t * 0.2) * 0.14, cA), D = 4.4 + 0.8 * cA, d = 0.0, m = 0.0;
    vec3 ro = vec3(sin(a) * D, 0.16, cos(a) * D),
         rd = normalize(vec3(v, -1.45)),
         C = vec3(0.2, 0.7, 1), A = mix(C, C.zyx, cA),
         col = A * pow(max(-rd.z, 0.0), 9.0) * 0.45 * cA + 0.015, gl = vec3(0);
    rd.xz *= R(-a);

    for (int i = 0; i < 64; i++) {
        h = M(ro + rd * d);
        gl += A * (0.007 + 0.006 * k) / (1.0 + h.x * h.x * 55.0);
        if (h.x < 0.002 || d > 16.0) { m = h.x < 0.002 ? h.y : 0.0; break; }
        d += h.x;
    }

    if (m > 0.5) {
        vec3 p = ro + rd * d, l = vec3(0.37, 0.74, 0.56);
        vec2 e = vec2(0.003, 0);
        vec3 n = normalize(h.x - vec3(M(p - e.xyy).x, M(p - e.yxy).x, M(p - e.yyx).x));
        float fr = 1.0 - max(dot(n, -rd), 0.0),
              gd = m > 3.5 ? smoothstep(0.95, 1.0, max(cos(p.x * 3.0), cos(p.z * 3.0))) / (1.0 + d * d * 0.08)
                           : max(cos(p.y * 7.0 - t * 2.0), 0.0) * (0.4 + ar);
        fr *= fr * fr;
        col = mix(col, (m == 3.0 ? A : vec3(0.05)) * (0.2 + 0.9 * max(dot(n, l), 0.0))
                     + pow(max(dot(reflect(-l, n), -rd), 0.0), 32.0) * 0.8
                     + A * (0.3 * fr + (m < 1.5 ? 0.3 * nA + k * max(sin(p.x * 9.0 + t * 3.0), 0.0)
                                      : m == 3.0 ? 0.4 + 0.3 * ar : 0.2 * gd)), exp(-0.04 * d * d));
    }

    O = vec4(pow(tanh((col + gl) * (1.2 - 0.3 * dot(v, v))), vec3(0.45)) * min(min(t * 0.5, (60.0 - t) * 0.3), 1.0), 1);
}
