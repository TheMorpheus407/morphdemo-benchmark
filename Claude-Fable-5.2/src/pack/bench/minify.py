#!/usr/bin/env python3
# Quick-and-dirty GLSL "minifier" used only to synthesize representative test data for the packer bench.
import re, sys, collections
KEYWORDS = set("""attribute const uniform varying buffer shared coherent volatile restrict readonly writeonly layout centroid flat smooth noperspective patch sample break continue do for while switch case default if else subroutine in out inout float double int void bool true false invariant precise discard return mat2 mat3 mat4 dmat2 dmat3 dmat4 mat2x2 mat2x3 mat2x4 mat3x2 mat3x3 mat3x4 mat4x2 mat4x3 mat4x4 vec2 vec3 vec4 ivec2 ivec3 ivec4 bvec2 bvec3 bvec4 dvec2 dvec3 dvec4 uint uvec2 uvec3 uvec4 lowp mediump highp precision sampler1D sampler2D sampler3D samplerCube sampler2DShadow sampler2DArray isampler2D usampler2D image2D struct
radians degrees sin cos tan asin acos atan sinh cosh tanh pow exp log exp2 log2 sqrt inversesqrt abs sign floor trunc round ceil fract mod modf min max clamp mix step smoothstep isnan isinf length distance dot cross normalize faceforward reflect refract matrixCompMult outerProduct transpose determinant inverse lessThan lessThanEqual greaterThan greaterThanEqual equal notEqual any all not texture textureLod texelFetch textureSize texture2D dFdx dFdy fwidth floatBitsToInt floatBitsToUint intBitsToFloat uintBitsToFloat fma
gl_FragCoord gl_FragColor gl_Position gl_VertexID gl_InstanceID gl_FragDepth gl_PointSize gl_PointCoord gl_TexCoord gl_Color main location binding std140 std430 local_size_x local_size_y local_size_z gl_GlobalInvocationID imageLoad imageStore define version if ifdef ifndef endif else elif undef extension core compatibility""".split())
SWIZ = re.compile(r'^[xyzw]{1,4}$|^[rgba]{1,4}$|^[stpq]{1,4}$')
TOK = re.compile(r'''(?P<pp>^[ \t]*\#[^\n]*$)|(?P<num>(?:0[xX][0-9a-fA-F]+[uU]?)|(?:(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuU]?))|(?P<id>[A-Za-z_]\w*)|(?P<op>\+\+|--|<<=|>>=|<<|>>|<=|>=|==|!=|&&|\|\||\^\^|[-+*/%&|^]=|[-+*/%<>=!&|^~?:;,.(){}\[\]])|(?P<ws>\s+)''', re.M)
def strip_comments(s):
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'//[^\n]*', '', s)
    return s
def shortnames():
    letters = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
    for c in letters: yield c
    for a in letters:
        for b in letters + '0123456789': yield a + b
def fixnum(n):
    if n.lower().startswith('0x'): return n
    n = n.rstrip('fF')
    m = re.match(r'^(\d*)\.(\d*)((?:[eE][+-]?\d+)?)$', n)
    if m:
        a, b, e = m.groups(); a = a.lstrip('0'); b = b.rstrip('0')
        if not a and not b: return '0.' + e
        return a + '.' + b + e
    return n
def minify(src):
    src = strip_comments(src)
    toks = []
    for m in TOK.finditer(src):
        k = m.lastgroup
        if k == 'ws': continue
        toks.append((k, m.group(k)))
    freq = collections.Counter()
    for i, (k, t) in enumerate(toks):
        if k == 'id' and t not in KEYWORDS and not t.startswith('gl_') and not (i > 0 and toks[i-1][1] == '.' and SWIZ.match(t)):
            freq[t] += 1
    names = {}
    gen = shortnames()
    for t, _ in freq.most_common():
        n = next(gen)
        while n in KEYWORDS: n = next(gen)
        names[t] = n
    out = []; prev_alnum = False
    for i, (k, t) in enumerate(toks):
        if k == 'pp':
            t = re.sub(r'\s+', ' ', t.strip())
            if out and not out[-1].endswith('\n'): out.append('\n')
            out.append(t + '\n'); prev_alnum = False; continue
        if k == 'id':
            if t in names and not (i > 0 and toks[i-1][1] == '.' and SWIZ.match(t)): t = names[t]
        elif k == 'num': t = fixnum(t)
        alnum = k in ('id', 'num')
        if alnum and prev_alnum: out.append(' ')
        out.append(t); prev_alnum = alnum
    return ''.join(out)
if __name__ == '__main__':
    data = ''.join(open(f, errors='replace').read() + '\n' for f in sys.argv[1:])
    sys.stdout.write(minify(data))
