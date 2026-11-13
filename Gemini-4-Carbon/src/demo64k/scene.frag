#version 430
layout(location = 0) uniform vec4 u[6];
layout(binding = 0) uniform sampler2D texSim;
out vec4 O;

/*
 * ARCHITECTURES OF ENTROPY — Pass 2: 5-Act HDR Raymarcher + Exact Perspective gl_FragDepth
 * u[0] = (width, height, time, beat_pos)
 * u[1] = (kick, snare, arp, bass)
 * u[2] = (ro.x, ro.y, ro.z, focal_len)
 * u[3] = (ta.x, ta.y, ta.z, lead_env)
 * u[4] = (wSing, wMono, wAstro, wCitadel)
 * u[5] = (wMorph, 0, flash, pad_env)
 */

float T, beatPos, eKick, eSnr, eArp, eBass, eLead, ePad;
float wSing, wMono, wAstro, wCitadel, wMorph;

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float seg2(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    return length(pa - ba * clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0));
}

float bowlArch(vec2 p) {
    vec2 c = vec2(0.03, 0.27);
    float arc = p.x > c.x
        ? abs(length(p - c) - 0.29)
        : min(length(p - vec2(c.x, 0.56)), length(p - vec2(c.x, -0.02)));
    return min(arc, min(seg2(p, vec2(-0.35, 0.56), vec2(c.x, 0.56)),
                        seg2(p, vec2(-0.35, -0.02), vec2(c.x, -0.02))));
}

/* Chiseled Roman-futurist architectural glyph centerline distance */
float glyphCitadel(vec2 p, int idx) {
    if (idx == 0) { // M with architectural base serifs
        p.x = abs(p.x);
        float d = min(seg2(p, vec2(0.39, -0.56), vec2(0.39, 0.56)),
                      seg2(p, vec2(0.39, 0.56), vec2(0.0, -0.10)));
        return min(d, seg2(p, vec2(0.30, -0.56), vec2(0.47, -0.56)));
    }
    if (idx == 1) { // O: faceted octagonal-stadium ring
        return abs(length(vec2(p.x, abs(p.y) - min(abs(p.y), 0.21))) - 0.36);
    }
    if (idx == 2) { // R with base serifs
        float d = min(seg2(p, vec2(-0.35, -0.56), vec2(-0.35, 0.56)), bowlArch(p));
        d = min(d, seg2(p, vec2(0.02, -0.02), vec2(0.37, -0.56)));
        return min(d, min(seg2(p, vec2(-0.43, -0.56), vec2(-0.25, -0.56)),
                          seg2(p, vec2(0.29, -0.56), vec2(0.44, -0.56))));
    }
    if (idx == 3) { // P with base & top serifs
        float d = min(seg2(p, vec2(-0.35, -0.56), vec2(-0.35, 0.56)), bowlArch(p));
        return min(d, seg2(p, vec2(-0.43, -0.56), vec2(-0.24, -0.56)));
    }
    // H with architectural serifs
    vec2 sp = vec2(abs(p.x), abs(p.y));
    float d = min(seg2(vec2(abs(p.x), p.y), vec2(0.36, -0.56), vec2(0.36, 0.56)),
                  seg2(p, vec2(-0.36, 0.0), vec2(0.36, 0.0)));
    return min(d, seg2(sp, vec2(0.27, 0.56), vec2(0.44, 0.56)));
}

float gyroid(vec3 p) {
    return dot(sin(p), cos(p.yzx));
}

/*
 * Material IDs:
 * 1.0 = Fractured Singularity Core & Brutalist Tectonic Monoliths
 * 2.0 = Chrono-Mechanical Astrolabe Gear Rings & Celestial Citadel Colonnade
 * 3.0 = Chiseled Beveled Platinum/Alabaster MORPH Monument
 * 4.0 = Wave-Simulated Obsidian Rift / Stepped Mirror Plaza
 * 5.0 = Glowing Sapphire-Gold Inlaid Energy Channel inside MORPH & Plinth
 */
vec2 mapScene(vec3 p) {
    vec2 res = vec2(60.0, 0.0);

    // 1. Act I-III Central Singularity -> Clockwork Core (lifts & dissolves when Citadel opens)
    if (wCitadel < 0.96) {
        vec3 q = p - vec3(0.0, 0.35 + wCitadel * 4.5, 0.0);
        q.xz *= rot(T * 0.45);
        q.xy *= rot(T * 0.31);

        // Fractured polyhedral core in Act I -> precision icosa-octahedron core in Act III
        vec3 aq = abs(q);
        float oct = (aq.x + aq.y + aq.z - (1.28 - 0.25 * wAstro + 0.12 * eKick)) * 0.57735;
        float box = sdBox(q, vec3(0.76 + 0.08 * eBass));
        float core = mix(oct, max(oct, box), 0.55 + 0.4 * wAstro);

        // Violent tectonic fissures in Act I, stabilizing into crisp geometric channels in Act III
        float fiss = gyroid(q * 4.6 + T * 2.2 * wSing) * (0.26 * wSing + 0.06 * wMono);
        float groove = abs(gyroid(q * 7.5)) - 0.12;
        core = max(core + fiss, -groove * 0.08) * (1.0 - wCitadel) + wCitadel * 2.5;
        if (core < res.x) res = vec2(core * 0.72, 1.0);
    }

    // 2. Act II Brutalist Tectonic Monolith Canyon (11s - 28s)
    if (wMono > 0.01) {
        vec3 mp = p;
        float cellZ = clamp(round(mp.z / 2.6), -3.0, 3.0);
        mp.z -= cellZ * 2.6;
        mp.x = abs(mp.x) - (2.85 + 0.35 * sin(cellZ * 1.7));
        // Heave upward out of the rift with staggered heights
        float hMono = (2.4 + 0.7 * cos(cellZ * 2.3)) * wMono;
        mp.y -= hMono - 1.4;
        mp.xy *= rot(-0.14 * (1.0 - wAstro));
        float dM = sdBox(mp, vec3(0.46, hMono, 0.68)) - 0.03;
        // Chiseled horizontal architectural grooves
        dM = max(dM, -(abs(mod(p.y + 0.2, 0.4) - 0.2) - 0.025));
        if (dM < res.x) res = vec2(dM * 0.85, 1.0);
    }

    // 3. Act III-V Chrono-Mechanical Astrolabe Rings -> Celestial Citadel Crown
    if (wAstro > 0.01) {
        // Beat-quantized mechanical ratchet angle at 128 BPM during Act III, serene majestic rotation in Act IV-V
        float bFloor = floor(beatPos);
        float bFract = smoothstep(0.0, 0.32, fract(beatPos));
        float ratchet = (bFloor + bFract) * 0.261799; // 15 deg per beat!
        float baseAng = mix(ratchet, T * 0.16, wCitadel);

        for (int i = 0; i < 3; i++) {
            float fi = float(i);
            // In Act IV-V, translate astrolabe to z = -2.45 BEHIND MORPH and stand rings upright as a cathedral solar halo!
            vec3 rq = p - vec3(0.0, mix(0.35, 0.25, wCitadel), mix(0.0, -2.45, wCitadel));
            float dir = mod(fi, 2.0) * 2.0 - 1.0;
            float tiltA = mix((0.52 + fi * 0.46) + baseAng * dir, T * 0.14 * dir, wCitadel);
            float tiltB = mix(baseAng * (0.7 - 0.35 * fi), 1.570796 + (fi - 1.0) * 0.16, wCitadel);
            rq.yz *= rot(tiltB);
            rq.xz *= rot(tiltA);

            float R = mix(1.95 + fi * 0.82, 2.35 + fi * 0.82, wCitadel);
            float ringR = length(rq.xz) - R;
            float ang = atan(rq.z, rq.x);
            float teeth = clamp(cos(ang * (24.0 + fi * 12.0)), -0.5, 0.5) * 0.038;
            vec2 cs = abs(vec2(ringR - teeth, rq.y)) - vec2(0.088, 0.065 + 0.015 * eArp);
            float dRing = (length(max(cs, 0.0)) + min(max(cs.x, cs.y), 0.0));
            dRing = mix(3.0, dRing, wAstro);
            if (dRing < res.x) res = vec2(dRing * 0.82, 2.0);
        }
    }

    // 4. Act IV-V Alabaster & Platinum Citadel Colonnade + Stepped Monument Dais
    if (wCitadel > 0.02) {
        // Grand symmetrical colonnade framing the plaza (x = +/- 4.6, z from -6.0 to +2.0)
        vec3 cp = p;
        float cz = clamp(round(cp.z / 2.2), -3.0, 1.0);
        cp.z -= cz * 2.2;
        cp.x = abs(cp.x) - 4.55;
        float colH = 3.2 * wCitadel;
        float flute = cos(atan(cp.z, cp.x) * 12.0) * 0.018;
        float dCol = length(cp.xz) - (0.34 + flute);
        dCol = max(dCol, abs(p.y - (colH - 1.38)) - colH);
        // Capital & base plinth on each column
        float cap = min(sdBox(vec3(cp.x, p.y + 1.28, cp.z), vec3(0.50, 0.14, 0.50)),
                        sdBox(vec3(cp.x, p.y - (2.0 * colH - 1.48), cp.z), vec3(0.54, 0.16, 0.54)));
        float dArch = min(dCol, cap);

        // Stepped central monument dais beneath MORPH
        float dais1 = sdBox(p - vec3(0.0, -1.26, 0.0), vec3(3.65 * wCitadel, 0.12, 1.25 * wCitadel)) - 0.02;
        float dais2 = sdBox(p - vec3(0.0, -1.08, 0.0), vec3(3.25 * wMorph,   0.09, 0.85 * wMorph)) - 0.02;
        dArch = min(dArch, min(dais1, dais2));
        if (dArch < res.x) res = vec2(dArch * 0.88, 2.0);

        // Glowing golden-sapphire trim strip along dais edge
        if (wMorph > 0.05) {
            float trim = sdBox(p - vec3(0.0, -1.08, 0.0), vec3(3.28 * wMorph, 0.022, 0.88 * wMorph));
            if (trim < res.x) res = vec2(trim, 5.0);
        }
    }

    // 5. Act V Monumental Chiseled Beveled MORPH Sculpture (45s - 60s)
    if (wMorph > 0.01) {
        vec3 q = p - vec3(0.0, 0.02, 0.0);
        float bound = sdBox(q, vec3(3.45, 1.05, 0.65));
        if (bound < 0.5) {
            int centerIdx = clamp(int(floor((q.x + 2.95) / 1.18)), 0, 4);
            int i0 = max(centerIdx - 1, 0);
            int i1 = min(centerIdx + 1, 4);
            float rise = 1.0 - wMorph;
            rise = rise * rise;

            float bestBody = 9.0;
            float bestInlay = 9.0;
            for (int idx = i0; idx <= i1; idx++) {
                float fi = float(idx) - 2.0;
                vec3 lp = q;
                lp.x -= fi * 1.18;
                // Majestic architectural ascent from the dais + precision 90-deg lock-in
                lp.y += rise * (1.45 + 0.25 * abs(fi));
                lp.xz *= rot(rise * 1.570796 * (mod(float(idx), 2.0) * 2.0 - 1.0));

                float d2 = glyphCitadel(lp.xy * 1.04, idx) / 1.04;
                float halfW = 0.092 * smoothstep(0.01, 0.22, wMorph);
                float halfZ = 0.145 * smoothstep(0.01, 0.22, wMorph);

                // Precision 45-degree chamfered octagonal prism cross-section
                float a = d2 - halfW;
                float b = abs(lp.z) - halfZ;
                float cham = (a + b + 0.055) * 0.70710678;
                float dPrism = max(max(a, b), cham);
                bestBody = min(bestBody, dPrism);

                // Inlaid glowing sapphire-gold energy vein recessed on front & back faces
                float dVein = max(d2 - 0.024, abs(abs(lp.z) - halfZ) - 0.018);
                bestInlay = min(bestInlay, dVein);
            }
            if (bestBody < res.x)  res = vec2(bestBody * 0.85, 3.0);
            if (bestInlay < res.x) res = vec2(bestInlay * 0.85, 5.0);
        } else if (bound < res.x) {
            res = vec2(bound, 3.0);
        }
    }

    // 6. Wave-Simulated Rift / Mirror Plaza Floor (sampling GPU wave FBO texSim!)
    vec2 simUv = clamp(p.xz * 0.065 + 0.5, 0.01, 0.99);
    float waveH = textureLod(texSim, simUv, 0.0).r;
    float roughTerrain = gyroid(p * 1.6 + vec3(0.0, T * 0.3, 0.0)) * 0.14 * (1.0 - wCitadel);
    float dFloor = p.y + 1.38 + roughTerrain - waveH * (0.045 + 0.035 * wCitadel);
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

float calcSoftShadow(vec3 ro, vec3 rd) {
    float res = 1.0;
    float t = 0.06;
    for (int i = 0; i < 18; i++) {
        float h = mapScene(ro + rd * t).x;
        res = min(res, 8.0 * h / t);
        t += clamp(h, 0.04, 0.35);
        if (res < 0.02 || t > 8.0) break;
    }
    return clamp(res, 0.08, 1.0);
}

float calcAO(vec3 p, vec3 n) {
    float occ = 0.0, sca = 1.0;
    for (int i = 1; i <= 4; i++) {
        float h = 0.055 * float(i);
        float d = mapScene(p + h * n).x;
        occ += (h - d) * sca;
        sca *= 0.65;
    }
    return clamp(1.0 - 2.2 * occ, 0.12, 1.0);
}

vec3 skyEnv(vec3 rd) {
    // Transitions from dark volcanic/violet void (Acts I-III) to rich royal-sapphire & golden celestial sky (Acts IV-V)
    vec3 darkSky  = mix(vec3(0.012, 0.006, 0.022), vec3(0.045, 0.018, 0.045), clamp(rd.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 lightSky = mix(vec3(0.025, 0.065, 0.17), vec3(0.08, 0.22, 0.48), pow(clamp(rd.y * 0.5 + 0.5, 0.0, 1.0), 1.3));
    vec3 sunDir   = normalize(vec3(0.48, 0.52, -0.71));
    float sunGlow = pow(max(dot(rd, sunDir), 0.0), 12.0);
    lightSky += vec3(1.35, 0.92, 0.45) * sunGlow * 0.85;
    darkSky  += vec3(0.85, 0.22, 0.08) * pow(max(dot(rd, normalize(vec3(0.0, 0.2, -1.0))), 0.0), 7.0) * 0.35;
    return mix(darkSky, lightSky, wCitadel);
}

void main() {
    T        = u[0].z;
    beatPos  = u[0].w;
    eKick    = u[1].x;
    eSnr     = u[1].y;
    eArp     = u[1].z;
    eBass    = u[1].w;
    eLead    = u[3].w;
    wSing    = u[4].x;
    wMono    = u[4].y;
    wAstro   = u[4].z;
    wCitadel = u[4].w;
    wMorph   = u[5].x;
    ePad     = u[5].w;

    vec2 uv = (gl_FragCoord.xy - 0.5 * u[0].xy) / u[0].y;
    vec3 ro = u[2].xyz;
    float fl = u[2].w;
    vec3 ta = u[3].xyz;

    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
    vec3 vv = cross(uu, ww);
    vec3 rd = normalize(uv.x * uu + uv.y * vv + fl * ww);

    vec3 col = skyEnv(rd);

    // Palette accents: Crimson-Copper (Entropy) -> Royal Sapphire & Champagne Gold (Order)
    vec3 accWarm = mix(vec3(1.35, 0.28, 0.06), vec3(1.35, 0.96, 0.48), wCitadel);
    vec3 accCool = mix(vec3(0.45, 0.12, 1.15), vec3(0.18, 0.62, 1.45), wCitadel);

    float d = 0.0, mat = 0.0;
    vec3 vol = vec3(0.0);
    for (int i = 0; i < 92; i++) {
        vec3 p = ro + rd * d;
        vec2 h = mapScene(p);

        // Volumetric singularity fissures & celestial god-rays
        float gVol = abs(gyroid(p * 1.4 - vec3(0.0, T * 0.5, 0.0)));
        float beam = exp(-3.5 * gVol) * exp(-0.08 * d);
        vol += mix(accWarm, accCool, 0.5 + 0.5 * sin(p.y + T)) * beam * (0.0042 * wSing + 0.0018 * (1.0 - wSing));

        // Crisp surface energy glow around core, astrolabe, and MORPH inlays
        vec3 gTone = (h.y > 4.5) ? mix(accCool, accWarm, 0.65) : mix(accWarm, accCool, wAstro * 0.55);
        float gGain = (h.y > 4.5) ? (0.0065 + 0.005 * eLead) : (0.0028 + 0.003 * eKick);
        vol += gTone * gGain / (1.0 + h.x * h.x * 85.0) * exp(-0.06 * d);

        if (h.x < 0.0016 || d > 28.0) {
            if (h.x < 0.0016) mat = h.y;
            break;
        }
        d += h.x;
    }

    if (mat > 0.5) {
        vec3 p = ro + rd * d;
        vec3 n = calcNormal(p);
        vec3 v = -rd;
        vec3 lDir = normalize(mix(vec3(0.35, 0.78, 0.52), vec3(0.28, 0.68, 0.67), wCitadel));
        vec3 fillDir = normalize(vec3(-0.6, 0.35, -0.7));

        float ndl  = max(dot(n, lDir), 0.0);
        float fill = max(dot(n, fillDir), 0.0);
        float ao   = calcAO(p, n);
        float sh   = (ndl > 0.01) ? calcSoftShadow(p + n * 0.012, lDir) : 0.08;
        float fres = pow(1.0 - max(dot(n, v), 0.0), 4.0);
        vec3 refl  = skyEnv(reflect(-v, n));

        vec3 surf = vec3(0.0);
        if (mat < 1.5) {
            // Obsidian / Basalt Monoliths & Core with glowing magma/energy veins
            float vein = smoothstep(0.92, 0.99, sin(p.y * 9.0 + gyroid(p * 3.0) * 2.5 - T * 2.5));
            vec3 base = vec3(0.035, 0.032, 0.042);
            surf = base * (0.22 + 0.95 * ndl * sh + 0.25 * fill) * ao;
            surf += refl * (0.15 + 0.75 * fres);
            surf += accWarm * vein * (0.85 + 1.2 * eBass);
        } else if (mat < 2.5) {
            // Brushed Bronze/Steel Astrolabe -> Sunlit Alabaster Marble & Platinum Citadel
            vec3 alb = mix(vec3(0.11, 0.095, 0.085), vec3(0.68, 0.66, 0.62), wCitadel);
            float gradTick = smoothstep(0.88, 0.98, cos(atan(p.z, p.x) * 48.0)) * (1.0 - wCitadel * 0.6);
            surf = alb * (0.24 + 0.88 * ndl * sh + 0.28 * fill) * ao;
            surf += refl * mix(0.45, 0.32, wCitadel) * (0.25 + 0.75 * fres);
            surf += mix(accWarm, accCool, 0.5) * gradTick * (0.35 + 0.65 * eArp);
        } else if (mat < 3.5) {
            // Monumental Beveled MORPH Sculpture: Brushed Gold-Tinted Titanium Bevels + Iridescent Rim
            vec3 irid = 0.5 + 0.5 * cos(vec3(0.0, 2.1, 4.2) + dot(n, v) * 4.5 + p.x * 0.4);
            vec3 body = mix(vec3(0.26, 0.30, 0.40), vec3(0.95, 0.78, 0.46), abs(n.x) * 0.55 + 0.25);
            body = mix(body, irid, 0.18);
            surf = body * (0.32 + 0.95 * ndl * sh + 0.35 * fill) * ao;
            surf += refl * (0.45 + 0.95 * fres);
            surf += vec3(1.55, 1.28, 0.85) * pow(max(dot(reflect(-lDir, n), v), 0.0), 42.0) * 1.65 * sh;
        } else if (mat < 4.5) {
            // Wave-Simulated Obsidian Rift -> Mirror Marble Plaza
            vec2 simUv = clamp(p.xz * 0.065 + 0.5, 0.01, 0.99);
            vec4 sim   = textureLod(texSim, simUv, 0.0);
            float grid = max(smoothstep(0.965, 0.995, cos(p.x * 2.85)),
                             smoothstep(0.965, 0.995, cos(p.z * 2.85))) / (1.0 + 0.04 * d * d);
            vec3 flrAlb = mix(vec3(0.018, 0.018, 0.025), vec3(0.09, 0.12, 0.19), wCitadel);
            surf = flrAlb * (0.25 + 0.75 * ndl * sh) * ao;
            surf += refl * (0.25 + 0.75 * fres) * (0.45 + 0.55 * wCitadel);
            surf += mix(accWarm, accCool, 0.6) * (grid * 0.35 + abs(sim.r) * 0.85 + sim.b * 0.32);
        } else {
            // Material 5: Blazing Champagne-Gold & Sapphire Inlaid Energy Vein inside MORPH & Dais
            float pulse = 0.85 + 0.15 * sin(p.x * 5.0 - T * 4.5);
            surf = mix(vec3(2.1, 1.55, 0.62), vec3(0.45, 1.15, 2.2), 0.28 + 0.22 * sin(p.x * 1.4 + T * 1.5))
                 * (1.35 + 0.75 * eLead + 0.35 * ePad) * pulse;
        }

        surf += vec3(1.0, 0.92, 0.82) * pow(max(dot(reflect(-lDir, n), v), 0.0), 32.0) * 0.48 * sh;
        col = mix(col, surf, exp(-0.0038 * d * d));
    }

    col += vol;
    O = vec4(max(col, vec3(0.0)), 1.0);

    // Exact IEEE-754 perspective depth matching particles.vert (zn = 0.1, zf = 40.0)
    const float zn = 0.1, zf = 40.0;
    const float A = (zf + zn) / (zf - zn);
    const float B = -2.0 * zf * zn / (zf - zn);
    if (mat > 0.5) {
        float cz = max(d * dot(rd, ww), zn);
        gl_FragDepth = clamp(0.5 * (A + B / cz) + 0.5, 0.0, 1.0);
    } else {
        gl_FragDepth = 1.0;
    }
}
