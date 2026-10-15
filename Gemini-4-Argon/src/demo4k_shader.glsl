#version 430
layout(location=0) uniform vec4 u[2];
out vec4 fragColor;

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

float h1(float n) { return fract(sin(n) * 43758.5); }

float noise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    vec4 v = dot(i, vec3(1, 57, 113)) + vec4(0, 57, 113, 170);
    v = mix(fract(sin(v) * 43758.5), fract(sin(v + 1.0) * 43758.5), f.x);
    v.xy = mix(v.xz, v.yw, f.y);
    return mix(v.x, v.y, f.z);
}

float fbm(vec3 p) {
    return 0.5 * noise3(p) + 0.25 * noise3(p * 2.03 + 7.7) + 0.125 * noise3(p * 4.1 + 13.1);
}

float seg(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}

float sdPR(vec2 q, float r) {
    float d = min(
        seg(q, vec2(-0.28, -0.5), vec2(-0.28, 0.5)),
        max(abs(length(max(abs(q - vec2(-0.04, 0.24)) - vec2(0.14, 0.04), 0.0)) - 0.22), -q.x - 0.28)
    );
    return r > 0.0 ? min(d, seg(q, vec2(-0.02, 0.02), vec2(0.28, -0.5))) : d;
}

float sdMorph2D(vec2 p) {
    vec2 mq = vec2(abs(p.x + 2.15), p.y);
    float d = min(seg(mq, vec2(0.32, -0.5), vec2(0.32, 0.5)), seg(mq, vec2(0.32, 0.5), vec2(0.0, -0.08)));
    d = min(d, abs(length(max(abs(p + vec2(1.08, 0.0)) - vec2(0.10, 0.28), 0.0)) - 0.22));
    d = min(d, min(sdPR(p, 1.0), sdPR(p - vec2(1.08, 0.0), 0.0)));
    vec2 hq = vec2(abs(p.x - 2.15), p.y);
    d = min(d, min(seg(hq, vec2(0.28, -0.5), vec2(0.28, 0.5)), seg(hq, vec2(0.0, 0.0), vec2(0.28, 0.0))));
    return d - 0.075;
}

float g_glow, g_ring_glow;
int g_mat;

float map(vec3 p) {
    float t = u[0].x, kick = u[0].y;
    float p12 = smoothstep(15.0, 21.0, t), p23 = smoothstep(38.0, 43.0, t);

    vec3 q1 = p;
    q1.xz *= rot(t * 0.4);
    q1.xy *= rot(t * 0.27);
    float turb = (fbm(q1 * 1.9 + t * 1.3) - 0.45) * (1.75 - 1.45 * p12 + 0.35 * kick);
    float d_foam = length(q1) - 1.25 + turb;
    for (int i = 0; i < 4; i++) {
        float fi = float(i);
        vec3 bp = q1 - 1.75 * sin(t * vec3(1.2, 1.0, 0.8) + fi * vec3(1.7, 2.1, 2.9));
        d_foam = min(d_foam, length(bp) - 0.38 + 0.35 * turb);
    }

    vec3 q2 = p;
    q2.z += t * 2.8;
    q2.xy *= rot(q2.z * 0.08 + t * 0.15);
    vec3 f = abs(mod(q2 + 2.5, 5.0) - 2.5);
    if (f.x < f.y) f.xy = f.yx;
    if (f.x < f.z) f.xz = f.zx;
    if (f.y < f.z) f.yz = f.zy;
    f = abs(f - vec3(1.55 + 0.1 * kick, 0.82, 0.42));
    float strut = min(length(f.xy), min(length(f.yz), length(f.xz))) - 0.075;
    float nodes = length(f) - 0.16 - 0.04 * kick;
    float d_lattice = max(min(strut, nodes), 1.25 - length(p.xy));

    vec3 q3 = p;
    q3.xz *= rot(sin(t * 0.35) * 0.16);
    q3.yz *= rot(cos(t * 0.25) * 0.06);
    vec2 w = vec2(sdMorph2D(q3.xy * 1.18) / 1.18, abs(q3.z) - 0.20);
    float d_text = min(max(w.x, w.y), 0.0) + length(max(w, 0.0)) - 0.03;

    float d_rings = 1e5;
    for (int i = 0; i < 3; i++) {
        float fi = float(i);
        vec3 rp = p;
        rp.xz *= rot(t * (0.35 - 0.12 * fi) + fi * 0.8);
        rp.xy *= rot(t * (0.28 + 0.1 * fi) * (fi == 1.0 ? -1.0 : 1.0) + fi);
        d_rings = min(d_rings, length(vec2(length(rp.xy) - 2.75 - fi * 0.42 - 0.06 * kick, rp.z)) - 0.038);
    }
    float floor_d = p.y + 1.75 + 0.03 * sin(length(p.xz) * 4.5 - t * 6.0) * p23;
    float d_mono = min(d_text, min(d_rings, floor_d));
    float d = mix(mix(min(d_foam, 2.2), d_lattice, p12), min(d_mono, 2.5), p23);

    float nd = min(nodes, d_foam);
    g_glow += 0.006 / (0.025 + nd * nd * 14.0);
    g_ring_glow += 0.008 / (0.015 + d_rings * d_rings * 18.0);

    g_mat = (p23 > 0.4 && d_text <= d_mono + 0.001) ? 2 :
            ((p23 > 0.4 && floor_d <= d_mono + 0.001) ? 3 :
            ((p12 > 0.3 && nodes <= strut + 0.005) ? 4 : 1));
    return d * 0.78;
}

void main() {
    float t = u[0].x, kick = u[0].y, snare = u[0].z, lead = u[1].x;
    float p12 = smoothstep(15.0, 21.0, t), p23 = smoothstep(38.0, 43.0, t);

    vec2 res = vec2(1920.0, 1080.0);
    vec2 uv = (gl_FragCoord.xy - 0.5 * res) / res.y;
    uv.x += (h1(floor(uv.y * 85.0) + floor(t * 24.0) * 17.3) - 0.5) * 0.07 * (1.0 - p12) * (0.35 + 0.65 * kick);

    vec3 ro = vec3(
        0.38 * sin(t * 0.32),
        0.15 + 0.22 * cos(t * 0.24) * (1.0 - 0.5 * p23),
        mix(-4.0, mix(-4.5, -5.1, p23), p12) + 0.3 * sin(t * 0.35)
    );
    vec3 ta = vec3(0.2 * cos(t * 0.25) * (1.0 - p23), -0.08 * p23, 0.0);
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(vec3(0, 1, 0), ww));
    vec3 rd = normalize(uv.x * uu + uv.y * cross(ww, uu) + (1.35 + 0.15 * p23) * ww);

    vec3 bg = vec3(0.015, 0.028, 0.068) + 0.18 * pow(max(0.0, 1.0 - length(uv * vec2(0.7, 1.1))), 2.5) * mix(vec3(0.05, 0.3, 0.6), vec3(0.4, 0.25, 0.08), p23);

    g_glow = g_ring_glow = 0.0;
    float dist = 0.0, d;
    vec3 p = ro;
    for (int i = 0; i < 76; i++) {
        p = ro + rd * dist;
        d = map(p);
        if (d < 0.002 || dist > 24.0) break;
        dist += d;
    }

    vec3 col = bg;
    if (d < 0.002) {
        int mat = g_mat;
        vec2 e = vec2(0.002, -0.002);
        vec3 n = normalize(e.xyy * map(p + e.xyy) + e.yyx * map(p + e.yyx) + e.yxy * map(p + e.yxy) + e.xxx * map(p + e.xxx));
        vec3 l1 = normalize(vec3(0.8, 1.4, -1.0));
        float diff = max(dot(n, l1), 0.0) * 0.85 + max(dot(n, vec3(-0.7, -0.4, 0.5)), 0.0) * 0.3 + 0.16;
        float spec = pow(max(dot(reflect(-l1, n), -rd), 0.0), 40.0);
        float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);

        vec3 base = mix(vec3(0.08, 0.45, 0.82), mix(vec3(0.08, 0.22, 0.38), vec3(0.15, 0.65, 0.9), p23), p12);
        if (mat == 4) base = vec3(1.0, 0.55, 0.12) * (1.2 + 1.5 * kick + 1.2 * lead);
        if (mat == 2) {
            base = mix(vec3(0.95, 0.72, 0.22), vec3(0.15, 0.85, 1.0), 0.35 + 0.35 * n.y) + smoothstep(0.12, 0.20, abs(p.z)) * vec3(0.3, 0.9, 1.0) * (0.8 + 0.6 * kick);
            spec *= 2.2;
        }
        if (mat == 3) {
            vec2 gp = abs(fract(p.xz * 1.2) - 0.5);
            base = vec3(0.02, 0.04, 0.09)
                 + smoothstep(0.035, 0.008, min(gp.x, gp.y)) * vec3(0.12, 0.55, 0.95) * (0.5 + 0.7 * kick)
                 + smoothstep(0.06, 0.0, abs(fract(length(p.xz) * 0.5 - t * 0.5) - 0.5)) * vec3(0.45, 0.3, 0.1);
        }
        col = mix(bg, base * diff + spec * vec3(1.0, 0.92, 0.8) + fres * mix(vec3(0.1, 0.75, 1.0), vec3(1.0, 0.6, 0.2), p12) * 0.65, exp(-0.011 * dist * dist));
    }

    col += g_glow * mix(vec3(0.15, 0.65, 1.0), mix(vec3(1.0, 0.5, 0.15), vec3(0.2, 0.8, 1.0), p23), p12) * (0.22 + 0.35 * kick + 0.25 * lead);
    col += g_ring_glow * vec3(0.35, 0.85, 1.0) * (0.35 + 0.45 * p23 + 0.3 * snare);
    col += (exp(-abs(t - 18.0) * 2.2) + exp(-abs(t - 40.5) * 2.0)) * smoothstep(0.10, 0.0, abs(length(uv) - fract(t * 0.8) * 1.1)) * vec3(0.7, 0.9, 1.0);

    float ghost = smoothstep(8.8, 9.4, t) * (1.0 - smoothstep(11.0, 11.8, t)) * step(0.2, sin(t * 32.0));
    if (ghost > 0.01) col += ghost * smoothstep(0.025, 0.0, abs(sdMorph2D(uv * 3.8))) * vec3(0.2, 0.6, 0.7);

    col += (1.0 - smoothstep(0.0, 12.0, t)) * (h1(dot(gl_FragCoord.xy, vec2(12.9898, 78.233)) + floor(t * 60.0)) - 0.5) * vec3(0.18, 0.32, 0.48);

    float fade = smoothstep(0.0, 2.0, t) * (1.0 - smoothstep(57.5, 60.0, t));
    col *= (1.0 - 0.25 * dot(uv, uv)) * fade;
    col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
    fragColor = vec4(pow(clamp(col, 0.0, 1.0), vec3(0.4545)), 1.0);
}
