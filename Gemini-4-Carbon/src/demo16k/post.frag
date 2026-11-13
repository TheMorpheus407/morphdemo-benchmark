#version 430
layout(location = 0) uniform vec4 u[3];
layout(binding = 0) uniform sampler2D texScene;
layout(binding = 1) uniform sampler2D texHist;
out vec4 O;

/*
 * ABYSSAL MORPHOGENESIS — Pass 2 & 3:
 * u[2].x == 0.0 -> Temporal Bioluminescent Feedback + Pre-Bloom into Ping-Pong History FBO
 * u[2].x == 1.0 -> Wide Golden-Angle Optical Bloom + Underwater Chromatic Dispersion + AgX-style Filmic Grade
 */

void main() {
    vec2 res = u[0].xy;
    float T  = u[0].z;
    float passId = u[2].x;
    vec2 uv = gl_FragCoord.xy / res;

    if (passId < 0.5) {
        vec4 cur = texture(texScene, uv);
        // Gentle upward buoyant drift in temporal feedback during Emergence, crisp in Resolution
        float phSanct = smoothstep(42.5, 48.5, T);
        vec2 flow = vec2(sin(uv.y * 10.0 + T * 1.4), -0.65) * (0.0018 * (1.0 - phSanct));
        vec3 prev = texture(texHist, clamp(uv + flow, 0.001, 0.999)).rgb;

        // Soft 8-tap halo around bright bioluminescent highlights
        vec3 halo = vec3(0.0);
        vec2 px = 2.5 / res;
        for (int i = 0; i < 8; i++) {
            float a = float(i) * 0.785398;
            vec3 s = texture(texScene, uv + vec2(cos(a), sin(a)) * px).rgb;
            halo += max(s - vec3(0.32), vec3(0.0));
        }
        halo *= 0.125;

        float fb = mix(0.66, 0.18, phSanct);
        vec3 outHdr = mix(cur.rgb + halo * 0.35, prev, fb);
        O = vec4(outHdr, 1.0);
        return;
    }

    // Final Pass: Subtle Radial Underwater Chromatic Dispersion + 12-tap Golden-Angle Bloom
    vec2 centered = uv - 0.5;
    float r2 = dot(centered, centered);
    float disp = 0.0032 * r2 * (1.0 + 0.6 * u[1].z);

    vec3 col;
    col.r = texture(texHist, uv + centered * disp).r;
    col.g = texture(texHist, uv).g;
    col.b = texture(texHist, uv - centered * disp).b;

    // Wide dreamy optical bloom
    vec3 bloom = vec3(0.0);
    vec2 aspect = vec2(res.y / res.x, 1.0);
    for (int i = 1; i <= 12; i++) {
        float fi = float(i);
        float ang = fi * 2.3999632;
        float rad = fi * 0.0034;
        vec3 s = texture(texHist, clamp(uv + vec2(cos(ang), sin(ang)) * aspect * rad, 0.001, 0.999)).rgb;
        bloom += max(s - vec3(0.24), vec3(0.0)) * (1.3 - fi * 0.06);
    }
    col += bloom * 0.065;

    // Vignette & organic filmic tonemapping
    col *= 1.18 - 0.52 * r2;
    vec3 x = max(col, vec3(0.0));
    col = (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);
    col = pow(clamp(col, 0.0, 1.0), vec3(0.4545));

    // Smooth fade-in and fade-out at 60s
    float fade = clamp(min(T * 0.55, (60.0 - T) * 0.4), 0.0, 1.0);
    O = vec4(col * fade, 1.0);
}
