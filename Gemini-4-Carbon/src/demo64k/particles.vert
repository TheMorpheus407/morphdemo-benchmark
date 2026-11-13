#version 430
layout(location = 0) uniform vec4 u[6];
layout(location = 0) out vec4 vCol;
layout(location = 1) out vec2 vUv;

/*
 * 18,432 GPU-Computed 3D Crystal Shards (55,296 vertices via gl_VertexID):
 * Depth-tested against scene.frag's gl_FragDepth for true 3D occlusion!
 * Morphs seamlessly across all 5 Acts:
 *   Act I:   Violent chaotic singularity spark storm
 *   Act II:  Intertwined helical strange-attractor ribbons through the monolith canyon
 *   Act III: Precision planetary clockwork ring streams
 *   Act IV-V: Ascending starlight colonnade & double-halo crown framing MORPH
 */

vec3 hash31(float p) {
    vec3 p3 = fract(vec3(p) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xxy + p3.yzz) * p3.zyx);
}

void main() {
    int id  = gl_VertexID / 3;
    int cId = gl_VertexID % 3;
    float fi = float(id);
    vec3 h = hash31(fi + 1.0);

    float T        = u[0].z;
    float eKick    = u[1].x;
    float eArp     = u[1].z;
    float wSing    = u[4].x;
    float wMono    = u[4].y;
    float wAstro   = u[4].z;
    float wCitadel = u[4].w;
    float wMorph   = u[5].x;

    // 1. Act I: Turbulent Radial Singularity Storm
    float sTheta = h.x * 6.2831853 + T * (0.8 + 1.4 * h.y);
    float sPhi   = (h.z - 0.5) * 3.14159;
    float sRad   = 0.55 + 3.6 * fract(h.y + T * (0.22 + 0.18 * h.x));
    vec3 posSing = vec3(cos(sTheta) * cos(sPhi), sin(sPhi), sin(sTheta) * cos(sPhi)) * sRad + vec3(0.0, 0.35, 0.0);

    // 2. Act II: Helical Strange-Attractor Ribbons weaving between the Brutalist Monoliths
    float strand = floor(h.y * 3.0);
    float uParam = h.x * 12.56637 + T * 1.35 + strand * 2.0944;
    vec3 posAttr = vec3(
        sin(uParam) * 1.65 + 0.55 * cos(uParam * 2.0),
        0.35 + sin(uParam * 1.5 + strand) * 1.15,
        cos(uParam * 0.5) * 4.2
    ) + (h - 0.5) * 0.18;

    // 3. Act III: Planetary Clockwork Orbital Ring Streams
    float ringIdx = floor(h.z * 3.0);
    float oAng    = h.x * 6.2831853 + T * (0.9 - 0.45 * ringIdx) * (mod(ringIdx, 2.0) * 2.0 - 1.0);
    float oRad    = 2.15 + ringIdx * 0.85 + (h.y - 0.5) * 0.14;
    vec3 posOrb   = vec3(cos(oAng) * oRad, (h.y - 0.5) * 0.12, sin(oAng) * oRad);
    float tilt    = 0.48 + ringIdx * 0.42;
    float ct = cos(tilt), st = sin(tilt);
    posOrb.xy = mat2(ct, -st, st, ct) * posOrb.xy;
    posOrb.y += 0.35;

    // 4. Act IV-V: Celestial Crown & Ascending Starlight framing MORPH (clear of center!)
    vec3 posCit;
    if (h.z < 0.72) {
        // Majestic inclined halo ring behind & around MORPH
        float cAng = h.x * 6.2831853 + T * 0.22 * (h.z < 0.36 ? 1.0 : -1.0);
        float cRad = (h.z < 0.36 ? 3.55 : 4.55) + (h.y - 0.5) * 0.22;
        posCit = vec3(cos(cAng) * cRad, 0.45 + sin(cAng) * 0.65 + (h.y - 0.5) * 0.18, sin(cAng) * cRad * 0.55 - 1.25);
    } else {
        // Gently rising starlight motes along the outer colonnade
        float side = (h.y < 0.5) ? -1.0 : 1.0;
        posCit = vec3(
            side * (3.65 + fract(h.x * 7.3) * 1.6),
            -1.25 + fract(h.y * 3.7 + T * 0.08) * 3.8,
            (fract(h.z * 11.1) - 0.55) * 7.0
        );
    }

    // Smooth 5-act position blend
    vec3 worldPos = mix(posSing, posAttr, smoothstep(10.5, 15.0, T));
    worldPos = mix(worldPos, posOrb, wAstro);
    worldPos = mix(worldPos, posCit, wCitadel);

    // Camera basis identical to scene.frag
    vec2 res = u[0].xy;
    vec3 ro  = u[2].xyz;
    float fl = u[2].w;
    vec3 ta  = u[3].xyz;

    vec3 ww = normalize(ta - ro);
    vec3 uu = normalize(cross(ww, vec3(0.0, 1.0, 0.0)));
    vec3 vv = cross(uu, ww);

    // Equilateral billboard triangle around worldPos for sparkling diamond/ bokeh shard
    float twinkle = 0.55 + 0.45 * sin(T * 5.0 + fi * 0.37);
    float sz = (0.016 + 0.022 * h.x + 0.010 * eKick) * mix(1.15, 0.82, wMorph) * twinkle;
    vec2 tri[3] = vec2[](vec2(0.0, 1.45), vec2(-1.25, -0.75), vec2(1.25, -0.75));
    vec2 corner = tri[cId];
    vec3 vPos   = worldPos + (uu * corner.x + vv * corner.y) * sz;

    vec3 rel = vPos - ro;
    float cx = dot(rel, uu);
    float cy = dot(rel, vv);
    float cz = dot(rel, ww);

    const float zn = 0.1, zf = 40.0;
    const float A  = (zf + zn) / (zf - zn);
    const float B  = -2.0 * zf * zn / (zf - zn);
    float aspect   = res.x / max(res.y, 1.0);

    if (cz <= zn) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        vCol = vec4(0.0);
        vUv  = vec2(0.0);
        return;
    }

    gl_Position = vec4(cx * (2.0 * fl / aspect), cy * (2.0 * fl), A * cz + B, cz);

    // Palette shift: Crimson-Violet sparks -> Sapphire & Champagne-Gold celestial shards
    vec3 colDark  = mix(vec3(1.4, 0.32, 0.08), vec3(0.55, 0.18, 1.35), h.y);
    vec3 colLight = mix(vec3(1.35, 0.98, 0.52), vec3(0.25, 0.72, 1.55), h.y);
    vec3 cTone    = mix(colDark, colLight, wCitadel);
    float alpha   = (0.28 + 0.38 * eArp) * twinkle * clamp(8.0 / (2.0 + cz), 0.15, 1.0);

    vCol = vec4(cTone * alpha, alpha);
    vUv  = corner;
}
