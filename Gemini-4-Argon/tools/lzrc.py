#!/usr/bin/env python3
"""
Bit-exact Range-Coded LZ77 (LZRC) compressor and GLSL minifier for MorphDemo.
Matches the C decoder `unpack()` bit-for-bit.
"""
import re
import struct
import sys

GLSL_RESERVED = {
    "version", "layout", "location", "uniform", "out", "in", "vec2", "vec3", "vec4",
    "float", "int", "bool", "mat2", "mat3", "mat4", "void", "main", "for", "if", "else",
    "break", "continue", "return", "true", "false", "sin", "cos", "tan", "asin", "acos",
    "atan", "pow", "exp", "log", "exp2", "log2", "sqrt", "inversesqrt", "abs", "sign",
    "floor", "ceil", "fract", "mod", "min", "max", "clamp", "mix", "step", "smoothstep",
    "length", "distance", "dot", "cross", "normalize", "reflect", "refract",
    "gl_FragCoord", "gl_Position", "gl_VertexID", "gl_PointSize", "gl_PointCoord",
    "sampler2D", "texture", "texelFetch", "dFdx", "dFdy", "fwidth"
}


def minify_glsl(src: str, rename_idents: bool = True) -> str:
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.DOTALL)
    lines = []
    for line in src.splitlines():
        line = re.sub(r"//.*$", "", line).strip()
        if not line:
            continue
        lines.append(line)

    out_lines = []
    cur = []
    for line in lines:
        if line.startswith("#"):
            if cur:
                out_lines.append("".join(cur))
                cur = []
            out_lines.append(line)
        else:
            cur.append(line + " ")
    if cur:
        out_lines.append("".join(cur))

    minified = []
    for seg in out_lines:
        if seg.startswith("#"):
            minified.append(seg)
        else:
            s = re.sub(r"\s+", " ", seg).strip()
            s = re.sub(r"\s*([,;{}()=+\-*/<>?:|&!~\[\]])\s*", r"\1", s)
            minified.append(s)
    res = "\n".join(minified)

    if rename_idents:
        tokens = re.findall(r"\b[a-zA-Z_][a-zA-Z0-9_]*\b", res)
        counts = {}
        for t in tokens:
            if t not in GLSL_RESERVED and not re.match(r"^[xyzwrgba]{1,4}$", t) and len(t) > 1:
                counts[t] = counts.get(t, 0) + 1
        candidates = sorted(counts.keys(), key=lambda k: -counts[k] * (len(k) - 1))
        alphabet = [chr(c) for c in range(ord("a"), ord("z") + 1)] + [
            chr(c) for c in range(ord("A"), ord("Z") + 1)
        ]
        used = {t for t in tokens if len(t) == 1}
        avail = [c for c in alphabet if c not in used]
        mapping = {}
        for ident in candidates:
            if avail:
                mapping[ident] = avail.pop(0)
        res = re.sub(
            r"\b[a-zA-Z_][a-zA-Z0-9_]*\b",
            lambda m: mapping.get(m.group(0), m.group(0)),
            res,
        )

    res = re.sub(r"\b0\.(\d+)", r".\1", res)
    res = re.sub(r"\b(\d+)\.0\b", r"\1.", res)
    return res


class RangeEncoder:
    def __init__(self):
        self.low = 0
        self.range = 0xFFFFFFFF
        self.cache = -1
        self.cache_size = 0
        self.probs = [1024] * 16384
        self.out = bytearray()

    def shift_low(self):
        low_hi = (self.low >> 32) & 0xFF
        if low_hi != 0 or (self.low & 0xFFFFFFFF) < 0xFF000000:
            if self.cache >= 0:
                self.out.append((self.cache + low_hi) & 0xFF)
            for _ in range(self.cache_size):
                self.out.append((0xFF + low_hi) & 0xFF)
            self.cache_size = 0
            self.cache = (self.low >> 24) & 0xFF
        else:
            self.cache_size += 1
        self.low = (self.low << 8) & 0xFFFFFFFF

    def bit(self, ctx: int, b: int):
        p = self.probs[ctx]
        bound = (self.range >> 11) * p
        if b == 0:
            self.range = bound
            self.probs[ctx] = p + ((2048 - p) >> 5)
        else:
            self.low += bound
            self.range -= bound
            self.probs[ctx] = p - (p >> 5)
        while self.range < (1 << 24):
            self.range <<= 8
            self.shift_low()

    def tree(self, base: int, limit: int, val: int):
        m = 1
        bits = limit.bit_length() - 1
        for i in range(bits - 1, -1, -1):
            b = (val >> i) & 1
            self.bit(base + m, b)
            m = (m << 1) | b

    def gamma(self, base: int, val: int):
        assert val >= 1
        bits = val.bit_length() - 1
        for i in range(bits):
            self.bit(base + i, 1)
        self.bit(base + bits, 0)
        for i in range(bits - 1, -1, -1):
            self.bit(base + 32 + i, (val >> i) & 1)

    def finish(self) -> bytes:
        for _ in range(5):
            self.shift_low()
        return bytes(self.out)


def find_match(data: bytes, pos: int, rep0: int):
    n = len(data)
    best_len = 0
    best_dist = 0
    max_dist = min(pos, 16384)
    if pos >= rep0:
        l = 0
        while pos + l < n and l < 255 and data[pos + l] == data[pos - rep0 + l]:
            l += 1
        if l >= 2:
            best_len = l
            best_dist = rep0

    for d in range(1, max_dist + 1):
        if pos + best_len < n and data[pos + best_len] != data[pos - d + best_len]:
            continue
        l = 0
        while pos + l < n and l < 255 and data[pos + l] == data[pos - d + l]:
            l += 1
        if l > best_len and (l >= 3 or (l == 2 and d < 48)):
            best_len = l
            best_dist = d
    return best_len, best_dist


def compress(data: bytes) -> bytes:
    rc = RangeEncoder()
    n = len(data)
    pos = 0
    rep0 = 1
    prev = 0
    while pos < n:
        best_len, best_dist = find_match(data, pos, rep0)
        if best_len >= 2 and pos + 1 < n:
            next_len, next_dist = find_match(data, pos + 1, rep0)
            if next_len > best_len + 1 + (0 if best_dist == rep0 else 1):
                best_len = 0

        state = ((pos & 3) << 4) + (prev >> 4)
        if best_len < 2:
            rc.bit(state, 0)
            b = data[pos]
            rc.tree(256 + ((prev >> 3) << 8), 256, b)
            prev = b
            pos += 1
        else:
            rc.bit(state, 1)
            rc.gamma(8448, best_len - 1)
            if best_dist == rep0:
                rc.bit(8440 + (pos & 3), 1)
            else:
                rc.bit(8440 + (pos & 3), 0)
                d0 = best_dist - 1
                rc.gamma(8600, (d0 >> 4) + 1)
                rc.tree(8700, 16, d0 & 15)
                rep0 = best_dist
            prev = data[pos + best_len - 1]
            pos += best_len
    return struct.pack("<I", n) + rc.finish()


def format_c_array(name: str, blob: bytes) -> str:
    hex_bytes = ", ".join(f"0x{b:02x}" for b in blob)
    return f"static const uint8_t {name}[{len(blob)}] = {{ {hex_bytes} }};\n"
