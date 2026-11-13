#version 430
layout(location = 0) uniform vec4 u[3];
out vec4 O;

/*
 * ABYSSAL MORPHOGENESIS — Pass 1: HDR Organic Raymarcher + Volumetric Plankton
 * u[0] = (width, height, time, frame)
 * u[1] = (sub_env, drop_env, shimmer_env, pad_env)
 */

float T, eSub, eDrop, eShim, ePad;
float phDrift, phCell, phHelix, phSanct;

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float seg2(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}

float bowl2(vec2 p) {
    vec2 c = vec2(0.02, 0.265);
    float arc = p.x > c.x
        ? abs(length(p - c) - 0.285)
        : min(length(p - vec2(c.x, 0.55)), length(p - vec2(c.x, -0.02)));
    return min(arc, min(seg2(p, vec2(-0.34, 0.55), vec2(c.x, 0.55)),
                        seg2(p, vec2(-0.34, -0.02), vec2(c.x, -0.02))));
}

float glyphMorph(vec2 p, int idx) {
    if (idx == 0) { // M
        p.x = abs(p.x);
        return smin(seg2(p, vec2(0.38, -0.55), vec2(0.38, 0.55)),
                    seg2(p, vec2(0.38, 0.55), vec2(0.0, -0.08)), 0.035);
    }
    if (idx == 1) { // O
        return abs(length(vec2(p.x, abs(p.y) - min(abs(p.y), 0.20))) - 0.35);
    }
    if (idx == 2) { // R
        float d = smin(seg2(p, vec2(-0.34, -0.55), vec2(-0.34, 0.55)), bowl2(p), 0.03);
        return smin(d, seg2(p, vec2(0.0, -0.02), vec2(0.35, -0.55)), 0.03);
    }
    if (idx == 3) { // P
        return smin(seg2(p, vec2(-0.34, -0.55), vec2(-0.34, 0.55)), bowl2(p), 0.03);
    }
    // H
    float st = seg2(vec2(abs(p.x), p.y), vec2(0.35, -0.55), vec2(0.35, 0.55));
    return smin(st, seg2(p, vec2(-0.35, 0.0), vec2(0.35, 0.0)), 0.035);
}

float gyroid(vec3 p) {
    return dot(sin(p), cos(p.yzx));
}

/* Returns (distance, material_id):
 * 1 = scattered proto-spores (Phase I) -> dividing radiolarian organism (Phase II)
 * 2 = coral double-helix (Phase III) & sea-lotus sanctuary crown (Phase IV)
 * 3 = living-glass MORPH calligram (Phase IV)
 * 4 = abyssal caustic seabed / mirror basin
 */
vec2 mapScene(vec3 p) {
    vec2 res = vec2(50.0, 0.0);

    // 1. Chaotic Proto-Spores (Phase I) -> Coalescing Radiolarian Mitosis (Phase II)
    if (phSanct < 0.96) {
        vec3 q = p;
        q.y -= phSanct * 3.8;
        q.xz *= rot(T * 0.26);

        float dCells = 99.0;
        // 8 vesicles: widely scattered in chaotic Lissajous cloud in Phase I, fusing into symmetric 4-lobe mitosis in Phase II
        for (int i = 0; i < 8; i++) {
            float fi = float(i);
            vec3 chaosPos = vec3(
                sin(fi * 2.39 + T * (0.7 + 0.08 * fi)) * 2.15,
                cos(fi * 1.87 - T * (0.6 + 0.07 * fi)) * 1.15,
                sin(fi * 3.11 + T * 0.53) * 2.15
            );
            float a = floor(fi * 0.5) * 1.570796 + T * 0.42;
            float mitosisSep = (0.88 + 0.14 * sin(T * 1.7)) * (1.0 - 0.55 * phHelix);
            vec3 orderPos = vec3(cos(a), sin(a * 2.0) * 0.42, sin(a)) * mitosisSep;

            vec3 c = mix(chaosPos, orderPos, phCell);
            float r = mix(0.16 + 0.05 * sin(T * 3.5 + fi), 0.46 + 0.06 * eSub, phCell) * (1.0 - phSanct);
            dCells = smin(dCells, length(q - c) - r, mix(0.18, 0.38, phCell));
        }

        float turb = (gyroid(q * 3.8 + T * 1.9) + 0.5 * gyroid(q * 7.5 - T * 2.4)) * (0.14 * phDrift + 0.025);
        float shell = abs(dCells) - 0.055;
        float pores = abs(gyroid(q * 6.0)) - (0.22 + 0.14 * phCell);
        float radiolarian = max(shell, -pores * phCell * 0.85);
        float dOrg = mix(dCells + turb, smin(dCells * 0.62 + 0.05, radiolarian, 0.08) + turb * 0.25, phCell);
        dOrg = mix(dOrg, dOrg + 2.2, phSanct);
        if (dOrg < res.x) res = vec2(dOrg * 0.62, 1.0);
    }

    // 2. Coral Double-Helix (Phase III) -> Sea-Lotus Sanctuary Framing Arches (Phase IV)
    if (phHelix > 0.01) {
        vec3 q = p;
        // Outer helix pillars widen gracefully to x = +/- 4.2 in Phase IV so center is 100% clear
        float rad = mix(1.48, 4.35, phSanct);
        float twist = mix(1.22, 0.52, phSanct);
        float ang = q.y * twist - T * 0.55 * (1.0 - 0.7 * phSanct);
        vec2 cs = vec2(cos(ang), sin(ang)) * rad;
        float s1 = length(q.xz - cs) - (0.14 + 0.03 * sin(q.y * 5.0 + T * 3.0) + 0.025 * eSub);
        float s2 = length(q.xz + cs) - (0.14 + 0.03 * cos(q.y * 5.0 - T * 2.5) + 0.025 * eSub);
        float dHel = min(s1, s2);

        // Cross-rungs during Phase III DNA/coral spiral, dissolving cleanly as Sanctuary opens
        if (phSanct < 0.32) {
            float ry = mod(q.y + 0.34, 0.68) - 0.34;
            vec3 rq = vec3(q.x, ry, q.z);
            rq.xz *= rot(-ang);
            float rib = length(vec2(max(abs(rq.x) - rad, 0.0), length(vec2(rq.y, rq.z)))) - 0.046 * (1.0 - phSanct * 3.0);
            dHel = smin(dHel, rib, 0.12);
        }

        // Symmetrical backdrop sea-lotus ribs behind MORPH (exact Euclidean loop, zero sector discontinuity)
        if (phSanct > 0.05) {
            vec3 bp = p - vec3(0.0, -1.35, -2.55);
            if (length(bp) < 4.2) {
                for (int k = -3; k <= 3; k++) {
                    float snap = float(k) * 0.38;
                    vec2 fanDir = vec2(sin(snap), cos(snap));
                    float along = clamp(dot(bp.xy, fanDir), 0.0, 3.05 * phSanct);
                    float zCurve = -0.18 * along * along + 0.48 * along;
                    float dRib = length(vec3(bp.xy - fanDir * along, bp.z - zCurve)) - (0.068 - 0.011 * along);
                    dHel = smin(dHel, dRib, 0.14);
                }
            }
        }

        dHel = mix(2.5, dHel, phHelix);
        if (dHel < res.x) res = vec2(dHel * 0.72, 2.0);
    }

    // 3. Living-Glass MORPH Calligram (43s - 60s) — neighbor-checked for 100% artifact-free Euclidean SDF
    if (phSanct > 0.02) {
        vec3 q = p - vec3(0.0, 0.10, 0.0);
        float bound = length(max(abs(q) - vec3(3.35, 0.95, 0.60), 0.0));
        float dTxt = bound;
        if (bound < 0.45) {
            int centerIdx = clamp(int(floor((q.x + 2.875) / 1.15)), 0, 4);
            int i0 = max(centerIdx - 1, 0);
            int i1 = min(centerIdx + 1, 4);
            float unfold = max(0.0, 1.0 - phSanct * 1.35);
            unfold = unfold * unfold;
            float tubeR = (0.088 + 0.008 * ePad + 0.004 * sin(q.y * 12.0 - T * 4.0)) * smoothstep(0.02, 0.32, phSanct);

            dTxt = 9.0;
            for (int idx = i0; idx <= i1; idx++) {
                float fi = float(idx) - 2.0;
                vec3 lp = q;
                lp.x -= fi * 1.15;
                lp.y -= sin(T * 1.5 + fi * 1.1) * (0.03 + 0.55 * unfold);
                lp.z -= cos(T * 1.2 + fi * 1.6) * 0.42 * unfold;
                lp.xz *= rot(unfold * 1.1 * (mod(float(idx), 2.0) * 2.0 - 1.0));

                float d2 = glyphMorph(lp.xy * 1.05, idx) / 1.05;
                dTxt = min(dTxt, length(vec2(d2, lp.z)) - tubeR);
            }
        }
        if (dTxt < res.x) res = vec2(dTxt * 0.82, 3.0);
    }

    // 4. Abyssal Caustic Seabed / Mirror Basin
    float rip = sin(length(p.xz) * 4.2 - T * 2.8) * exp(-0.08 * dot(p.xz, p.xz));
    float dFloor = p.y + 1.48 + (0.05 * gyroid(p * 1.8 + vec3(0.0, T * 0.4, 0.0)) * (1.0 - 0.78 * phSanct)) + 0.024 * rip * (phCell + phSanct);
    if (dFloor < res.x) res = vec2(dFloor * 0.85, 4.0);

    return res;
}

vec3 calcNormal(vec3 p) {
    vec2 e = vec2(0.0022, -0.0022);
    return normalize(
        e.xyy * mapScene(p + e.xyy).x +
        e.yyx * mapScene(p + e.yyx).x +
        e.yxy * mapScene(p + e.yxy).x +
        e.xxx * mapScene(p + e.xxx).x
    );
}

float caustics(vec2 uv, float tm) {
    vec2 p = uv * 2.6;
    float acc = 0.0;
    for (int i = 0; i < 3; i++) {
        p = p * rot(0.65) * 1.32 + vec2(sin(p.y * 1.7 + tm), cos(p.x * 1.7 - tm * 0.8)) * 0.45;
        acc += 1.0 / (1.0 + 18.0 * abs(sin(p.x) * cos(p.y)));
    }
    return acc * 0.34;
}

void main() {
    T = u[0].z;
    eSub  = u[1].x;
    eDrop = u[1].y;
    eShim = u[1].z;
    ePad  = u[1].w;

    phDrift = 1.0 - smoothstep(11.0, 19.0, T);
    phCell  = smoothstep(12.5, 20.5, T);
    phHelix = smoothstep(27.5, 35.0, T);
    phSanct = smoothstep(42.0, 48.0, T);

    vec2 uv = (gl_FragCoord.xy - 0.5 * u[0].xy) / u[0].y;
    uv += vec2(sin(uv.y * 11.0 + T * 2.4), cos(uv.x * 9.0 - T * 1.9)) * (0.014 * phDrift);

    // Phase-continuous camera orbit: 0.2792527 * 45.0 == 4 * pi (12.5663706), arriving smoothly front-center!
    float orbitAng = mix(T * 0.2792527, 12.5663706 + sin((T - 45.0) * 0.24) * 0.14, phSanct);
    float camDist  = mix(mix(4.6, 3.7, phCell), mix(4.5, 5.15, phSanct), phHelix);
    float camY     = mix(0.28 * sin(T * 0.31), 0.15 + 0.05 * sin(T * 0.25), phSanct);

    vec3 ro = vec3(sin(orbitAng) * camDist, camY, cos(orbitAng) * camDist);
    vec3 ta = vec3(0.0, mix(0.0, 0.08, phSanct), 0.0);
    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
    vec3 vv = cross(uu, ww);
    vec3 rd = normalize(uv.x * uu + uv.y * vv + 1.48 * ww);

    vec3 deepAbyss = vec3(0.004, 0.016, 0.038);
    vec3 aquaGlow  = vec3(0.04, 0.88, 0.68);
    vec3 magGlow   = vec3(0.92, 0.18, 0.58);
    vec3 pearlGlow = vec3(1.00, 0.84, 0.58);

    float up = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 col = mix(deepAbyss, vec3(0.012, 0.065, 0.12), pow(up, 2.2));
    float shaft = caustics(rd.xz / (abs(rd.y) + 0.35), T * 0.55);
    col += aquaGlow * shaft * smoothstep(-0.1, 0.85, rd.y) * (0.09 + 0.08 * phSanct + 0.06 * eShim);

    float d = 0.0, mat = 0.0;
    vec3 vol = vec3(0.0);
    for (int i = 0; i < 84; i++) {
        vec3 p = ro + rd * d;
        vec2 h = mapScene(p);

        // Volumetric bioluminescent plankton field (dense & turbulent in Phase I)
        vec3 sp = p * 1.75 + vec3(sin(T * 0.45), -T * 0.38, cos(T * 0.38));
        sp += 0.42 * sin(sp.yzx * 1.9 + T * 1.1) * (0.35 + phDrift);
        vec3 cellId = floor(sp);
        vec3 h3 = fract(sin(cellId * vec3(12.9898, 78.233, 37.719)) * 43758.5453);
        vec3 fsp = fract(sp) - 0.5 - 0.28 * sin(T * 1.5 + h3 * 6.283);
        float dSpore = length(fsp);
        float sporeTwinkle = 0.5 + 0.5 * sin(T * 4.8 + h3.x * 25.0);
        float sporeGain = (0.0026 * phDrift + 0.0005 * (1.0 - phDrift)) * (0.7 + 1.5 * eDrop * sporeTwinkle);
        vec3 sporeCol = mix(aquaGlow, magGlow, h3.y);
        vol += sporeCol * sporeGain / (0.005 + dSpore * dSpore * 26.0) * exp(-0.10 * d);

        vec3 auraTone = (h.y > 2.5 && h.y < 3.5)
            ? mix(aquaGlow, pearlGlow, 0.55 + 0.35 * sin(p.x * 0.9 + T))
            : mix(aquaGlow, magGlow, 0.4 + 0.4 * sin(p.y * 1.5 - T * 1.6));
        float auraStrength = (h.y > 2.5 && h.y < 3.5)
            ? (0.0052 + 0.004 * ePad)
            : (0.0022 + 0.0028 * eDrop);
        vol += auraTone * auraStrength / (1.0 + h.x * h.x * 75.0) * exp(-0.07 * d);

        if (h.x < 0.0016 || d > 18.0) {
            if (h.x < 0.0016) mat = h.y;
            break;
        }
        d += h.x;
    }

    if (mat > 0.5) {
        vec3 p = ro + rd * d;
        vec3 n = calcNormal(p);
        vec3 v = -rd;
        vec3 l1 = normalize(vec3(0.35, 0.88, 0.42));
        vec3 l2 = normalize(vec3(-0.65, 0.25, -0.55));

        float ndl1 = max(dot(n, l1), 0.0);
        float ndl2 = max(dot(n, l2), 0.0);
        float fres = pow(1.0 - max(dot(n, v), 0.0), 3.6);

        float ao = clamp(0.5 + 0.5 * mapScene(p + n * 0.18).x / 0.18, 0.15, 1.0);
        float sss = clamp(mapScene(p - n * 0.22 + l1 * 0.15).x / 0.22 + 0.65, 0.0, 1.2);

        float cPat = caustics(p.xz * 0.75 + n.xz * 0.2, T * 0.9);
        vec3 surf = vec3(0.0);

        if (mat < 1.5) {
            vec3 base = mix(vec3(0.02, 0.16, 0.22), vec3(0.14, 0.05, 0.22), 0.5 + 0.5 * sin(p.y * 4.0 + T));
            surf = base * (0.25 + 0.85 * ndl1) * ao;
            surf += mix(aquaGlow, magGlow, 0.5 + 0.5 * cos(length(p) * 6.0 - T * 3.0)) * (0.40 * sss + 0.55 * fres);
            surf += aquaGlow * cPat * 0.32 * ndl1;
        } else if (mat < 2.5) {
            float synWave = pow(0.5 + 0.5 * sin(p.y * 4.5 - T * 5.2), 6.0);
            vec3 base = vec3(0.025, 0.09, 0.14);
            surf = base * (0.3 + 0.8 * ndl1 + 0.3 * ndl2) * ao;
            surf += mix(aquaGlow, magGlow, 0.5 + 0.5 * sin(p.y * 1.2 + p.x * 0.4)) * (0.28 * sss + (0.45 + 0.9 * eDrop) * synWave);
            surf += pearlGlow * fres * (0.38 + 0.42 * phSanct);
        } else if (mat < 3.5) {
            vec3 envRef = mix(deepAbyss, pearlGlow, pow(max(reflect(-v, n).y * 0.5 + 0.5, 0.0), 2.5));
            float vein = pow(max(dot(n, v), 0.0), 2.2) * (0.78 + 0.22 * sin(p.x * 7.0 - T * 3.5));
            surf = vec3(0.05, 0.24, 0.28) * (0.35 + 0.75 * ndl1) * ao;
            surf += mix(aquaGlow, pearlGlow, 0.62) * vein * (0.98 + 0.55 * ePad);
            surf += envRef * (0.35 + 0.95 * fres);
            surf += vec3(1.0, 0.95, 0.86) * pow(max(dot(reflect(-l1, n), v), 0.0), 42.0) * 1.35;
        } else {
            vec3 floorBase = vec3(0.008, 0.028, 0.052) * ao;
            float ringGlow = (exp(-12.0 * abs(length(p.xz) - 2.9)) + 0.6 * exp(-14.0 * abs(length(p.xz) - 1.8))) * phSanct;
            surf = floorBase * (0.4 + 0.7 * ndl1);
            surf += aquaGlow * cPat * (0.22 + 0.18 * eShim) / (1.0 + 0.09 * dot(p.xz, p.xz));
            surf += mix(aquaGlow, pearlGlow, 0.55) * ringGlow * (0.55 + 0.45 * eSub);
            surf += mix(aquaGlow, magGlow, 0.35) * fres * 0.22 * phSanct;
        }

        surf += vec3(0.65, 0.95, 0.90) * pow(max(dot(reflect(-l1, n), v), 0.0), 28.0) * 0.55;
        col = mix(col, surf, exp(-0.032 * d * d));
    }

    col += vol;
    O = vec4(max(col, vec3(0.0)), clamp(d / 18.0, 0.0, 1.0));
}
