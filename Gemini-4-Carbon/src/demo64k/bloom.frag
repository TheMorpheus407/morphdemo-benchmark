#version 430
layout(location = 0) uniform vec4 u[6];
layout(binding = 0) uniform sampler2D texIn;
out vec4 O;

/*
 * Dual-pass HDR Separable + Anamorphic Cinema Streak Bloom (960x540)
 * u[5].xy = blur direction: (1, 0) = threshold + horizontal + anamorphic flare, (0, 1) = vertical
 */
void main() {
    vec2 bRes = max(floor(u[0].xy * 0.5), vec2(1.0));
    vec2 uv   = gl_FragCoord.xy / bRes;
    vec2 dir  = u[5].xy / bRes;
    bool isH  = (u[5].x > 0.5);

    const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 acc = vec3(0.0);

    for (int i = -4; i <= 4; i++) {
        vec2 suv = clamp(uv + dir * (float(i) * 2.2), 0.001, 0.999);
        vec3 s = texture(texIn, suv).rgb;
        if (isH) {
            float lum = max(max(s.r, s.g), s.b);
            s = s * smoothstep(0.55, 1.35, lum);
        }
        acc += s * w[abs(i)];
    }

    // Add wide horizontal cinema anamorphic blue-gold streak on pass 1
    if (isH) {
        vec3 streak = vec3(0.0);
        for (int i = -6; i <= 6; i++) {
            if (i == 0) continue;
            vec2 suv = clamp(uv + vec2(float(i) * 8.5 / bRes.x, 0.0), 0.001, 0.999);
            vec3 s = texture(texIn, suv).rgb;
            float lum = max(max(s.r, s.g), s.b);
            streak += s * smoothstep(1.05, 2.2, lum) * exp(-0.22 * float(abs(i)));
        }
        acc += streak * vec3(0.55, 0.78, 1.25) * 0.085;
    }

    O = vec4(acc, 1.0);
}
