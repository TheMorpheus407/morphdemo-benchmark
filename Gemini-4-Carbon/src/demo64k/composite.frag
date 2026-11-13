#version 430
layout(location = 0) uniform vec4 u[6];
layout(binding = 0) uniform sampler2D texScene;
layout(binding = 1) uniform sampler2D texBloom;
out vec4 O;

/*
 * Final HDR Composite:
 * Chromatic lens dispersion + Anamorphic HDR Bloom + ACES RRT/ODT Tone Mapping + Cinema Grain
 */
vec3 acesFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec2 res = u[0].xy;
    float T  = u[0].z;
    float flash = u[5].z;
    vec2 uv  = gl_FragCoord.xy / res;
    vec2 p   = uv - 0.5;
    float r2 = dot(p, p);

    // Subtle edge chromatic dispersion (amplified briefly on singularity & supernova impact)
    float ca = (0.0018 + 0.0045 * flash) * r2;
    vec3 hdr;
    hdr.r = texture(texScene, clamp(uv + p * ca, 0.0005, 0.9995)).r;
    hdr.g = texture(texScene, uv).g;
    hdr.b = texture(texScene, clamp(uv - p * ca, 0.0005, 0.9995)).b;

    vec3 blm = texture(texBloom, uv).rgb;
    hdr += blm * (0.68 + 0.35 * flash);

    // Supernova flash accent at Bar 20 drop (t ~ 37.5s)
    hdr += vec3(1.0, 0.92, 0.80) * (flash * flash * 0.28);

    // Subtle cinema vignette & ACES filmic grading
    hdr *= 1.15 - 0.48 * r2;
    vec3 ldr = pow(acesFilm(max(hdr, vec3(0.0))), vec3(0.4545));

    // Fine organic film grain (prevents 8-bit banding in dark volumetric gradients)
    float gn = fract(sin(dot(gl_FragCoord.xy + fract(T) * 137.0, vec2(12.9898, 78.233))) * 43758.5453);
    ldr += (gn - 0.5) * (1.5 / 255.0);

    float fade = clamp(min(T * 0.55, (60.0 - T) * 0.38), 0.0, 1.0);
    O = vec4(clamp(ldr * fade, 0.0, 1.0), 1.0);
}
