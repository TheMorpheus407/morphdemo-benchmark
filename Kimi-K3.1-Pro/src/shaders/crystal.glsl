#version 330 core
uniform vec2 R;
uniform float T;
uniform float B;
uniform float E;
out vec4 O;
#define PI 3.14159265359
#define TAU 6.28318530718

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float hash31(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(mix(hash31(i), hash31(i + vec3(1,0,0)), f.x),
            mix(hash31(i + vec3(0,1,0)), hash31(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash31(i + vec3(0,0,1)), hash31(i + vec3(1,0,1)), f.x),
            mix(hash31(i + vec3(0,1,1)), hash31(i + vec3(1,1,1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float f = 0.0;
    f += 0.5000 * noise3(p); p = p * 2.03 + 7.1;
    f += 0.2500 * noise3(p); p = p * 2.01 + 3.7;
    f += 0.1250 * noise3(p); p = p * 2.04 + 1.9;
    f += 0.0625 * noise3(p);
    return f;
}

float box(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float octa(vec3 p, float s) {
    p = abs(p);
    return (p.x + p.y + p.z - s) * 0.57735027;
}

float torus(vec3 p, float r, float h) {
    return length(vec2(length(p.xy) - r, p.z)) - h;
}

vec2 nearest(vec2 a, vec2 b) {
    return a.x < b.x ? a : b;
}

vec2 scene(vec3 p) {
    float order = smoothstep(5.0, 26.0, T);
    float resolve = smoothstep(33.0, 51.0, T);
    float chaos = 1.0 - order;
    vec3 q = p;
    q += chaos * 0.48 * sin(p.zxy * 3.1 + vec3(T * 1.7, -T * 1.3, T * 1.1));
    q.xz *= rot(0.11 * T + chaos * 0.35 * sin(T * 1.9));
    q.xy *= rot(-0.075 * T + 0.18 * sin(T * 0.17));

    float core = octa(q, 1.18 + 0.07 * E);
    vec3 g = q * (3.3 + resolve * 1.4);
    float gyroid = abs(dot(sin(g), cos(g.zxy))) / (3.3 + resolve * 1.4) - 0.035;
    gyroid = max(gyroid, length(q) - 1.12);
    core = mix(core, gyroid, resolve * 0.88);
    vec2 result = vec2(core, 2.0);

    vec3 z = q;
    float sector = TAU / 12.0;
    float angle = atan(z.y, z.x) + chaos * 0.16 * sin(T * 2.3 + z.z * 4.0);
    angle = mod(angle + sector * 0.5, sector) - sector * 0.5;
    z.xy = vec2(cos(angle), sin(angle)) * length(z.xy);
    z.x -= 2.12 - 0.24 * order + 0.08 * sin(B * PI);
    z.yz *= rot(chaos * (1.3 * sin(T * 1.4 + z.x) + z.x) + resolve * 0.35);
    float blade = box(z, vec3(0.72, 0.045 + 0.045 * order, 0.15 + 0.08 * order));
    result = nearest(result, vec2(blade, 1.0));
    float tip = octa(z - vec3(0.78, 0, 0), 0.22 + 0.08 * E);
    result = nearest(result, vec2(tip, 4.0));

    vec3 r1 = q;
    r1.xy *= rot(0.22 * T * chaos);
    r1.yz *= rot(0.75 + chaos * sin(T * 0.9));
    float ring = torus(r1, 1.52 + 0.14 * order, 0.018 + 0.018 * order);
    result = nearest(result, vec2(ring, 3.0));
    vec3 r2 = q;
    r2.xz *= rot(-0.9 + chaos * cos(T * 1.1));
    ring = torus(r2, 1.82 + 0.24 * resolve, 0.014 + 0.012 * order);
    result = nearest(result, vec2(ring, 3.0));

    float plane = p.y + 2.65;
    return nearest(result, vec2(plane, 0.0));
}

vec3 normal_at(vec3 p) {
    vec2 e = vec2(0.002, 0);
    return normalize(vec3(
        scene(p + e.xyy).x - scene(p - e.xyy).x,
        scene(p + e.yxy).x - scene(p - e.yxy).x,
        scene(p + e.yyx).x - scene(p - e.yyx).x));
}

float glyph(vec2 p, int code) {
    if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 0.0;
    ivec2 i = ivec2(floor(p * vec2(5,6)));
    int bit = (code >> (i.y * 5 + 4 - i.x)) & 1;
    vec2 f = fract(p * vec2(5,6)) - 0.5;
    float cell = 1.0 - smoothstep(0.34, 0.48, max(abs(f.x), abs(f.y)));
    return float(bit) * cell;
}

float word(vec2 p) {
    if (p.x < 0.0 || p.x > 6.18 || p.y < 0.0 || p.y > 1.0) return 0.0;
    int i = int(p.x / 1.28);
    int code = 599442993;
    if (i == 1) code = 488162862;
    if (i == 2) code = 1025462865;
    if (i == 3) code = 1025458704;
    if (i == 4) code = 589284913;
    return glyph(vec2(mod(p.x, 1.28) / 1.05, p.y), code);
}

vec3 background(vec3 rd, vec2 uv, float order) {
    float cloud = fbm(rd * 3.2 + vec3(0, 0, T * 0.025));
    float wisps = fbm(rd * 6.0 + cloud * 2.0 - vec3(T * 0.015));
    vec3 col = vec3(0.004, 0.007, 0.018);
    col += vec3(0.025, 0.045, 0.11) * cloud * cloud * 1.8;
    col += vec3(0.08, 0.015, 0.10) * pow(wisps, 3.0) * (1.2 - order * 0.5);
    vec2 sky = vec2(atan(rd.x, rd.z), asin(clamp(rd.y, -1.0, 1.0))) * 95.0;
    vec2 cell = floor(sky);
    float rnd = hash21(cell);
    vec2 d = fract(sky) - 0.5;
    float star = smoothstep(0.10, 0.0, length(d)) * step(0.955, rnd);
    star *= 0.4 + 0.6 * sin(rnd * 40.0 + T * (1.0 + rnd * 2.0)) * 0.5 + 0.3;
    col += star * mix(vec3(0.3,0.65,1.0), vec3(1.0,0.75,0.4), rnd);
    float horizon = exp(-12.0 * abs(rd.y + 0.10));
    col += horizon * vec3(0.012, 0.045, 0.065) * order;
    return col;
}

void main() {
    vec2 uv = (2.0 * gl_FragCoord.xy - R.xy) / R.y;
    float order = smoothstep(5.0, 29.0, T);
    float resolve = smoothstep(36.0, 53.0, T);
    float camera_angle = T * 0.085;
    vec3 ro = vec3(1.6 * sin(camera_angle), 0.75 + 0.35 * sin(T * 0.13), 7.7 * cos(camera_angle));
    ro *= 1.0 + 0.22 * resolve;
    vec3 ww = normalize(-ro);
    vec3 uu = normalize(cross(vec3(0,1,0), ww));
    vec3 vv = cross(ww, uu);
    vec3 rd = normalize(uu * uv.x + vv * uv.y + ww * 2.0);
    vec3 col = background(rd, uv, order);

    float travel = 0.0;
    float glow = 0.0;
    float material = -1.0;
    vec3 pos = ro;
    for (int i = 0; i < 88; ++i) {
        pos = ro + rd * travel;
        vec2 d = scene(pos);
        glow += min(0.008, 0.00045 / (0.015 + d.x * d.x));
        if (d.x < 0.0018 || travel > 22.0) {
            material = d.y;
            break;
        }
        travel += max(0.003, d.x * 0.62);
    }

    if (material >= 0.0 && travel < 22.0) {
        vec3 n = normal_at(pos);
        vec3 light = normalize(vec3(2.5, 3.5, 4.0));
        float diffuse = max(0.0, dot(n, light));
        float rim = pow(1.0 - max(0.0, dot(n, -rd)), 3.0);
        float specular = pow(max(0.0, dot(reflect(-light, n), -rd)), 42.0);
        vec3 base;
        if (material < 0.5) {
            float gx = pow(abs(sin(pos.x * PI * 0.5)), 42.0);
            float gz = pow(abs(sin(pos.z * PI * 0.5)), 42.0);
            float rings = pow(abs(sin(length(pos.xz) * PI)), 54.0);
            float grid = max(max(gx, gz) * 0.4, rings * 0.65);
            float fade = exp(-0.075 * length(pos.xz));
            base = vec3(0.006,0.012,0.022) + grid * fade * mix(vec3(0.02,0.16,0.20), vec3(0.16,0.10,0.035), resolve);
            col = mix(col, base * (0.5 + diffuse * 0.5), 0.82);
        } else {
            if (material < 1.5) base = mix(vec3(0.025,0.22,0.31), vec3(0.03,0.55,0.62), order);
            else if (material < 2.5) base = mix(vec3(0.30,0.045,0.22), vec3(0.72,0.36,0.055), resolve);
            else if (material < 3.5) base = vec3(0.18,0.065,0.40) + resolve * vec3(0.25,0.18,0.02);
            else base = vec3(0.45,0.70,0.82) + resolve * vec3(0.4,0.1,-0.2);
            vec3 object = base * (0.18 + diffuse * 1.1);
            object += rim * mix(vec3(0.05,0.42,0.65), vec3(0.8,0.32,0.08), resolve) * (0.45 + E * 0.25);
            object += specular * vec3(1.0,0.85,0.65) * 0.8;
            col = mix(col, object, exp(-0.022 * travel));
        }
    }
    col += glow * mix(vec3(0.015,0.12,0.20), vec3(0.24,0.075,0.018), resolve) * (0.45 + order);
    col += (0.004 + 0.010 * E) * vec3(0.06,0.22,0.36) / (0.18 + dot(uv,uv));

    float intro = smoothstep(1.0, 2.2, T) * (1.0 - smoothstep(4.8, 6.5, T));
    float finale = smoothstep(48.0, 51.0, T) * (1.0 - smoothstep(58.2, 59.7, T));
    float title_alpha = max(intro, finale);
    vec2 title_uv = uv - vec2(0.0, mix(-0.62, 0.0, finale));
    float title_scale = mix(0.135, 0.155, finale);
    vec2 tp = title_uv / vec2(title_scale, title_scale * 0.82) + vec2(3.09, 0.5);
    float shadow = word(tp + vec2(0.07,-0.09));
    float title = word(tp);
    col *= 1.0 - shadow * title_alpha * 0.72;
    vec3 title_color = mix(vec3(0.35,0.85,1.0), vec3(1.0,0.72,0.28), finale);
    col += title * title_alpha * title_color * (1.15 + 0.25 * E);
    float underline = exp(-380.0 * abs(title_uv.y + 0.16)) * smoothstep(0.75,0.25,abs(uv.x));
    col += underline * title_alpha * title_color * 0.35;

    float grain = hash21(gl_FragCoord.xy + fract(T) * 911.0) - 0.5;
    col += grain * mix(0.12, 0.018, order);
    col *= 1.0 - 0.32 * dot(uv * vec2(0.52,0.72), uv * vec2(0.52,0.72));
    col = 1.0 - exp(-col * 1.45);
    col = pow(max(col, 0.0), vec3(0.4545));
    col *= smoothstep(0.0, 1.1, T) * (1.0 - smoothstep(59.0, 60.0, T));
    O = vec4(col, 1.0);
}
