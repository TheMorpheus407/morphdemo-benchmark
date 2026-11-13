#!/usr/bin/env python3
"""GLSL minifier + C embedder for the MorphDemo C demos (python3 stdlib only).

usage: glslmin.py chunks.def out.h [--keep-names] [--stats]

chunks.def lists the shader source chunks:   CH(c_identifier, "file.glsl")
All chunks of a demo are minified together: comments/whitespace are removed,
numbers are shortened and every user identifier is renamed consistently across
all chunks (most frequent -> shortest), so shared chunks, varyings and macros
keep matching.  Output is a C header with one string per chunk.
Limits (by design, keeps the tool small): identifiers after '.' are never
renamed (swizzles / fields); names of user struct fields are kept verbatim.
"""
import re, sys, os, collections

RESERVED = set("""
attribute const uniform varying buffer shared coherent volatile restrict readonly writeonly atomic_uint layout centroid flat smooth
noperspective patch sample break continue do for while switch case default if else subroutine in out inout float double int void bool
true false invariant precise discard return mat2 mat3 mat4 dmat2 dmat3 dmat4 mat2x2 mat2x3 mat2x4 mat3x2 mat3x3 mat3x4 mat4x2 mat4x3
mat4x4 vec2 vec3 vec4 ivec2 ivec3 ivec4 bvec2 bvec3 bvec4 dvec2 dvec3 dvec4 uint uvec2 uvec3 uvec4 lowp mediump highp precision
sampler1D sampler2D sampler3D samplerCube sampler2DShadow sampler2DArray isampler2D usampler2D isampler3D usampler3D image2D iimage2D
uimage2D image3D struct common partition active asm class union enum typedef template this resource goto inline noinline public static
extern external interface long short half fixed unsigned superp input output filter sizeof cast namespace using
radians degrees sin cos tan asin acos atan sinh cosh tanh asinh acosh atanh pow exp log exp2 log2 sqrt inversesqrt abs sign floor
trunc round roundEven ceil fract mod modf min max clamp mix step smoothstep isnan isinf floatBitsToInt floatBitsToUint intBitsToFloat
uintBitsToFloat fma frexp ldexp packUnorm2x16 packHalf2x16 unpackHalf2x16 length distance dot cross normalize faceforward reflect
refract matrixCompMult outerProduct transpose determinant inverse lessThan lessThanEqual greaterThan greaterThanEqual equal notEqual
any all not uaddCarry usubBorrow umulExtended imulExtended bitfieldExtract bitfieldInsert bitfieldReverse bitCount findLSB findMSB
textureSize textureQueryLod textureQueryLevels texture textureProj textureLod textureOffset texelFetch texelFetchOffset textureGrad
textureGather texture2D texture3D textureCube dFdx dFdy dFdxFine dFdyFine fwidth interpolateAtCentroid EmitVertex EndPrimitive
imageLoad imageStore imageSize atomicAdd memoryBarrier barrier noise1 main
gl_Position gl_PointSize gl_ClipDistance gl_VertexID gl_InstanceID gl_FragCoord gl_FrontFacing gl_PointCoord gl_FragDepth gl_FragColor
gl_FragData gl_TexCoord gl_Color gl_Vertex gl_MultiTexCoord0 gl_ModelViewProjectionMatrix gl_GlobalInvocationID gl_LocalInvocationID
gl_WorkGroupID gl_NumWorkGroups gl_WorkGroupSize gl_LocalInvocationIndex gl_PrimitiveID gl_Layer gl_SampleID gl_in gl_PerVertex
location binding std140 std430 local_size_x local_size_y local_size_z rgba32f rgba16f rg32f rg16f r32f r16f rgba8 r8 r32ui rgba32ui
r32i points lines triangles triangle_strip line_strip max_vertices origin_upper_left pixel_center_integer early_fragment_tests
index component offset xfb_buffer depth_any depth_greater depth_less depth_unchanged
defined define undef ifdef ifndef endif elif version extension line pragma error core compatibility enable require warn disable
""".split())

TOK = re.compile(r"""
  (?P<num>(?:0[xX][0-9a-fA-F]+[uU]?)|(?:(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?(?:lf|LF|[fFuU])?))
 |(?P<id>[A-Za-z_][A-Za-z0-9_]*)
 |(?P<op><<=|>>=|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\^\^|\+=|-=|\*=|/=|%=|&=|\|=|\^=|\#\#|[-+*/%<>=!&|^~?:;,.(){}\[\]\#])
 |(?P<ws>\s+)
""", re.X)

def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", " ", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)

def tokenize(text):
    out, pos = [], 0
    while pos < len(text):
        m = TOK.match(text, pos)
        if not m:
            raise SystemExit("glslmin: cannot tokenize near: %r" % text[pos:pos + 40])
        pos = m.end()
        if m.lastgroup != "ws":
            out.append((m.lastgroup, m.group()))
    return out

def short_num(t):
    if t[:2] in ("0x", "0X") or not re.search(r"[.eE]", t):
        return t
    m = re.match(r"^(\d*)\.?(\d*)((?:[eE][-+]?\d+)?)(lf|LF|[fF]?)$", t)
    if not m:
        return t
    ip, fp, ex, suf = m.groups()
    ip = ip.lstrip("0"); fp = fp.rstrip("0")
    if not ex and not fp and len(ip) > 3 and ip.endswith("000"):
        z = len(ip) - len(ip.rstrip("0")); cand = ip.rstrip("0") + "e" + str(z)
        if len(cand) < len(ip) + 1:
            return cand + suf
    if not ip and not fp:
        return "0." + ex + suf
    return ip + "." + fp + ex + suf

def names():
    first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    for c in first:
        yield c
    for c in first:
        for d in first + "0123456789":
            yield c + d
    for c in first:
        for d in first:
            for e in first + "0123456789":
                yield c + d + e

def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    keep = "--keep-names" in sys.argv; stats = "--stats" in sys.argv
    defs, out = args[0], args[1]
    base = os.path.dirname(os.path.abspath(defs))
    chunks = re.findall(r'CH\(\s*(\w+)\s*,\s*"([^"]+)"\s*\)', open(defs).read())
    # per chunk: list of lines, each line = (is_directive, tokens)
    parsed, freq, fields = [], collections.Counter(), set()
    for cid, fn in chunks:
        src = strip_comments(open(os.path.join(base, fn)).read()).replace("\\\n", " ")
        lines, cur = [], []
        for ln in src.split("\n"):
            if ln.lstrip().startswith("#"):
                if cur: lines.append((False, cur, False)); cur = []
                lines.append((True, tokenize(ln.strip()), bool(re.match(r'\s*#\s*define\s+\w+\(', ln))))
            else:
                cur += tokenize(ln)
        if cur: lines.append((False, cur, False))
        parsed.append((cid, fn, lines))
        flat = [t for _, toks, _ in lines for t in toks]
        # struct fields are not renamed
        for i, (k, v) in enumerate(flat):
            if k == "id" and v == "struct":
                j = i
                while j < len(flat) and flat[j][1] != "{": j += 1
                depth = 0
                while j < len(flat):
                    if flat[j][1] == "{": depth += 1
                    elif flat[j][1] == "}":
                        depth -= 1
                        if depth == 0: break
                    elif flat[j][0] == "id" and flat[j + 1][1] in (";", ",", "["): fields.add(flat[j][1])
                    j += 1
    def renamable(toks, i):
        k, v = toks[i]
        if k != "id" or v in RESERVED or v in fields or v.startswith("gl_"): return False
        if i > 0 and toks[i - 1][1] == ".": return False
        return True
    for _, _, lines in parsed:
        for isdir, toks, _ in lines:
            if isdir and len(toks) > 1 and toks[1][1] in ("version", "extension", "pragma"): continue
            for i in range(len(toks)):
                if renamable(toks, i): freq[toks[i][1]] += 1
    mapping = {}
    if not keep:
        gen = names()
        used = RESERVED | fields
        for ident, _ in sorted(freq.items(), key=lambda kv: (-kv[1], kv[0])):
            n = next(gen)
            while n in used: n = next(gen)
            mapping[ident] = n
    def join(toks, skip=False):
        s, prev = "", None
        for i, (k, v) in enumerate(toks):
            if k == "id" and not skip and renamable(toks, i): v = mapping.get(v, v)
            elif k == "num" and not skip: v = short_num(v)
            if prev is not None:
                pk, pv = prev
                need = pk in ("id", "num") and k in ("id", "num")
                if (pv[-1] in "+-" and v[0] == pv[-1]) or (pv[-1] == "/" and v[0] in "/*"): need = True
                if pk == "num" and v[0] == ".": need = True
                if need: s += " "
            s += v; prev = (k, v)
        return s
    def emit(toks, isdir, funclike=False):
        if not isdir: return join(toks)
        word = toks[1][1] if len(toks) > 1 else ""
        if word in ("version", "extension", "pragma"):
            return "#" + word + " " + " ".join(v for _, v in toks[2:]).replace(" : ", ":").replace(" :", ":")
        if word == "define":
            name = join(toks[2:3])
            if funclike:
                depth, j = 0, 3
                while j < len(toks):
                    if toks[j][1] == "(": depth += 1
                    elif toks[j][1] == ")":
                        depth -= 1
                        if depth == 0: break
                    j += 1
                return "#define " + name + join(toks[3:j + 1]) + join(toks[j + 1:])
            body = join(toks[3:])
            return "#define " + name + (" " + body if body else "")
        rest = join(toks[2:])
        return "#" + word + (" " + rest if rest else "")
    res, total_in, total_out = [], 0, 0
    for cid, fn, lines in parsed:
        o = ""
        for isdir, toks, fl in lines:
            if isdir:
                if o and not o.endswith("\n"): o += "\n"
                o += emit(toks, True, fl) + "\n"
            else:
                o += emit(toks, False)
        res.append((cid, o))
        total_in += os.path.getsize(os.path.join(base, fn)); total_out += len(o)
    with open(out, "w") as f:
        f.write("/* generated by tools/glslmin.py - do not edit */\n")
        for cid, o in res:
            esc = o.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
            f.write('static const char %s[] = "%s";\n' % (cid, esc))
    if stats:
        sys.stderr.write("glslmin: %d chunks, %d -> %d bytes, %d identifiers renamed\n" % (len(res), total_in, total_out, len(mapping)))

if __name__ == "__main__":
    main()
