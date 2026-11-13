#version 430
layout(location = 0) uniform vec4 u[6];
layout(binding = 0) uniform sampler2D texPrevSim;
out vec4 O;

/*
 * 512x512 GPU Wave & Vorticity Simulation:
 * R = current wave height h(t)
 * G = previous wave height h(t-1)
 * B = smoothed radial energy field
 * A = swirl / curl phase
 */
void main() {
    vec2 uv = gl_FragCoord.xy / 512.0;
    vec2 px = vec2(1.0 / 512.0);
    float T = u[0].z;
    float eKick = u[1].x;
    float eSnr  = u[1].y;
    float eArp  = u[1].z;

    vec4 c  = texture(texPrevSim, uv);
    float n = texture(texPrevSim, uv + vec2(0.0, px.y)).r;
    float s = texture(texPrevSim, uv - vec2(0.0, px.y)).r;
    float e = texture(texPrevSim, uv + vec2(px.x, 0.0)).r;
    float w = texture(texPrevSim, uv - vec2(px.x, 0.0)).r;

    // 2D damped wave equation
    float lap = (n + s + e + w) * 0.5 - c.r;
    float nextH = (c.r + (c.r - c.g) * 0.982 + lap * 0.42) * 0.992;

    // Inject concentric kick impulse at center & orbiting arpeggio emitters
    vec2 p = (uv - 0.5) * 10.0;
    float d0 = length(p);
    nextH += eKick * 0.22 * exp(-6.0 * d0 * d0) * cos(T * 18.0);

    for (int i = 0; i < 3; i++) {
        float a = T * (1.1 + 0.3 * float(i)) + float(i) * 2.094395;
        vec2 ep = vec2(cos(a), sin(a)) * (2.4 + 0.6 * sin(T * 0.5));
        float de = length(p - ep);
        nextH += (eArp * 0.14 + eSnr * 0.10) * exp(-10.0 * de * de);
    }

    nextH = clamp(nextH, -2.0, 2.0);
    float energy = mix(c.b, abs(nextH) + 0.35 * eArp * exp(-0.18 * d0 * d0), 0.08);
    float swirl  = sin(d0 * 3.0 - T * 2.5 + atan(p.y, p.x) * 3.0);

    O = vec4(nextH, c.r, energy, swirl);
}
