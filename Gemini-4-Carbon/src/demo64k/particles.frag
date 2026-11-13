#version 430
layout(location = 0) in vec4 vCol;
layout(location = 1) in vec2 vUv;
out vec4 O;

void main() {
    float r2 = dot(vUv, vUv);
    if (r2 > 1.0) discard;
    // Crisp diamond core + soft optical falloff
    float core = exp(-4.8 * r2) + 0.35 * max(1.0 - (abs(vUv.x) + abs(vUv.y)) * 1.15, 0.0);
    O = vec4(vCol.rgb * core, 1.0);
}
