#!/usr/bin/env python3
"""
Build helper for MorphDemo:
1. `minify-shaders <out.h> <VAR=path.glsl> ...`
   Token-level GLSL minifier (strips comments, shortens float literals, renames
   user identifiers by frequency while preserving GLSL built-ins & swizzles) and
   emits a C header.
2. `pack <inner_elf> <stub.S> <stub.ld> <out_exe> <exact_size>`
   Strips trailing ELF section headers from <inner_elf>, searches optimal raw LZMA1
   parameters, assembles the self-contained x86-64 unpacker stub, packs the payload,
   and pads deterministically to <exact_size> bytes.
"""
import collections
import lzma
import os
import re
import string
import struct
import subprocess
import sys
import tempfile

GLSL_RESERVED = {
    # Keywords & types
    "attribute", "const", "uniform", "varying", "buffer", "shared", "coherent",
    "volatile", "restrict", "readonly", "writeonly", "layout", "centroid", "flat",
    "smooth", "noperspective", "patch", "sample", "break", "continue", "do", "for",
    "while", "switch", "case", "default", "if", "else", "subroutine", "in", "out",
    "inout", "float", "double", "int", "void", "bool", "true", "false", "invariant",
    "precise", "discard", "return", "mat2", "mat3", "mat4", "vec2", "vec3", "vec4",
    "ivec2", "ivec3", "ivec4", "bvec2", "bvec3", "bvec4", "uint", "uvec2", "uvec3",
    "uvec4", "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler2DShadow",
    "image2D", "struct", "location", "binding", "main",
    # Built-in functions
    "radians", "degrees", "sin", "cos", "tan", "asin", "acos", "atan", "sinh",
    "cosh", "tanh", "asinh", "acosh", "atanh", "pow", "exp", "log", "exp2", "log2",
    "sqrt", "inversesqrt", "abs", "sign", "floor", "trunc", "round", "roundEven",
    "ceil", "fract", "mod", "modf", "min", "max", "clamp", "mix", "step",
    "smoothstep", "isnan", "isinf", "floatBitsToInt", "floatBitsToUint",
    "intBitsToFloat", "uintBitsToFloat", "length", "distance", "dot", "cross",
    "normalize", "faceforward", "reflect", "refract", "matrixCompMult",
    "outerProduct", "transpose", "determinant", "inverse", "lessThan",
    "lessThanEqual", "greaterThan", "greaterThanEqual", "equal", "notEqual",
    "any", "all", "not", "textureSize", "texture", "textureProj", "textureLod",
    "textureGrad", "texelFetch", "dFdx", "dFdy", "fwidth", "emitVertex",
    # Built-in variables
    "gl_Position", "gl_PointSize", "gl_VertexID", "gl_InstanceID",
    "gl_FragCoord", "gl_FrontFacing", "gl_PointCoord", "gl_FragDepth",
}

TOKEN_RE = re.compile(
    r"(#[^\n]*\n)"
    r"|(\d+\.\d*(?:[eE][+-]?\d+)?|\.\d+(?:[eE][+-]?\d+)?|\d+[eE][+-]?\d+|\b0[xX][0-9a-fA-F]+u?\b|\b\d+u?\b)"
    r"|(\b[a-zA-Z_][a-zA-Z0-9_]*\b)"
    r"|(==|!=|<=|>=|&&|\|\||<<|>>|\+=|-=|\*=|/=|\+\+|--|[+\-*/%=<>!&|^~?:.,;()\[\]{}])"
    r"|(\s+)"
)


def _name_gen():
    chars1 = string.ascii_letters + "_"
    chars2 = string.ascii_letters + string.digits + "_"
    for c in chars1:
        if c not in GLSL_RESERVED:
            yield c
    for c1 in chars1:
        for c2 in chars2:
            s = c1 + c2
            if s not in GLSL_RESERVED and not s.startswith("gl_"):
                yield s


def minify_Float(tok: str) -> str:
    if tok.startswith(("0x", "0X")) or tok.endswith("u") or ("." not in tok and "e" not in tok and "E" not in tok):
        return tok
    # Split mantissa and exponent
    m = re.match(r"^(\d*\.\d*|\d+)([eE][+-]?\d+)?$", tok)
    if not m:
        return tok
    mant, exp = m.group(1), m.group(2) or ""
    if "." in mant:
        mant = mant.rstrip("0")
        if mant.startswith("0.") and len(mant) > 2:
            mant = mant[1:]  # 0.5 -> .5
        elif mant == "0.":
            mant = "0."
    if exp:
        exp = exp.replace("E", "e").replace("+", "")
        if mant.endswith(".") and len(mant) > 1:
            mant = mant[:-1]  # 1.e-3 -> 1e-3
    return mant + exp


def minify_glsl(src: str, keep_names: set[str] | None = None) -> str:
    keep = set(GLSL_RESERVED)
    if keep_names:
        keep |= keep_names

    # Strip comments
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.DOTALL)
    src = re.sub(r"//[^\n]*", "", src)

    raw_tokens = []
    prev_non_ws = ""
    counts = collections.Counter()

    for m in TOKEN_RE.finditer(src + "\n"):
        directive, num, ident, punct, ws = m.groups()
        if ws:
            continue
        if directive:
            raw_tokens.append(("DIR", directive.strip() + "\n"))
            prev_non_ws = "\n"
        elif num:
            raw_tokens.append(("NUM", minify_Float(num)))
            prev_non_ws = num
        elif ident:
            if prev_non_ws == "." or ident in keep or ident.startswith("gl_"):
                raw_tokens.append(("RAW_ID", ident))
            else:
                raw_tokens.append(("ID", ident))
                counts[ident] += 1
            prev_non_ws = ident
        elif punct:
            raw_tokens.append(("PUNCT", punct))
            prev_non_ws = punct

    gen = _name_gen()
    remap = {}
    for ident, _ in counts.most_common():
        remap[ident] = next(gen)

    out = []
    prev_kind = ""
    prev_text = ""
    for kind, text in raw_tokens:
        if kind == "ID":
            text = remap[text]
            kind = "RAW_ID"
        if kind == "DIR":
            if out and not out[-1].endswith("\n"):
                out.append("\n")
            out.append(text)
        else:
            need_space = False
            if prev_kind in ("RAW_ID", "NUM") and kind in ("RAW_ID", "NUM"):
                need_space = True
            elif prev_text in ("+", "-") and text in (prev_text, prev_text * 2):
                need_space = True
            elif prev_kind == "NUM" and text.startswith("."):
                need_space = True
            if need_space:
                out.append(" ")
            out.append(text)
        prev_kind = kind
        prev_text = text

    return "".join(out).strip() + "\n"


def c_escape(s: str) -> str:
    out = []
    for ch in s:
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n\"\n\"")
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def cmd_minify_shaders(args: list[str]) -> None:
    out_header = args[0]
    pairs = args[1:]
    os.makedirs(os.path.dirname(out_header) or ".", exist_ok=True)
    chunks = ["/* Auto-generated minified GLSL shaders */\n#pragma once\n"]
    for pair in pairs:
        # Optional syntax: VAR[:keep1,keep2]=path.glsl for multi-stage varyings
        lhs, path = pair.split("=", 1)
        if ":" in lhs:
            var_name, keep_csv = lhs.split(":", 1)
            keep_set = set(keep_csv.split(","))
        else:
            var_name, keep_set = lhs, set()
        raw = open(path, "r", encoding="utf-8").read()
        mini = minify_glsl(raw, keep_set)
        chunks.append(f"static const char {var_name}[] =\n{c_escape(mini)};\n")
        print(f"  [glsl] {path}: {len(raw)}B -> {len(mini)}B ({var_name})")
    open(out_header, "w", encoding="utf-8").write("\n".join(chunks))


def sstrip_elf64(data: bytes) -> bytes:
    b = bytearray(data)
    assert b[:4] == b"\x7fELF" and b[4] == 2, "Expected 64-bit ELF"
    e_phoff = struct.unpack_from("<Q", b, 32)[0]
    e_phentsize, e_phnum = struct.unpack_from("<HH", b, 54)
    # Drop trailing zero-sized metadata PHDRs (PT_NOTE, PT_GNU_PROPERTY, PT_GNU_STACK)
    while e_phnum > 1:
        off = e_phoff + (e_phnum - 1) * e_phentsize
        p_type = struct.unpack_from("<I", b, off)[0]
        p_filesz, p_memsz = struct.unpack_from("<QQ", b, off + 32)
        if p_filesz == 0 and p_memsz == 0 and p_type in (4, 0x6474E551, 0x6474E553):
            b[off : off + e_phentsize] = b"\x00" * e_phentsize
            e_phnum -= 1
        else:
            break
    struct.pack_into("<H", b, 56, e_phnum)
    # Update PT_PHDR filesz/memsz if first entry is PT_PHDR (type 6)
    if struct.unpack_from("<I", b, e_phoff)[0] == 6:
        ph_bytes = e_phnum * e_phentsize
        struct.pack_into("<QQ", b, e_phoff + 32, ph_bytes, ph_bytes)
    max_end = e_phoff + e_phentsize * e_phnum
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_offset = struct.unpack_from("<Q", b, off + 8)[0]
        p_filesz = struct.unpack_from("<Q", b, off + 32)[0]
        if p_offset + p_filesz > max_end:
            max_end = p_offset + p_filesz
    b = b[:max_end]
    struct.pack_into("<Q", b, 40, 0)         # e_shoff = 0
    struct.pack_into("<HHH", b, 58, 0, 0, 0) # e_shentsize, e_shnum, e_shstrndx = 0
    # Zero out any dead unreferenced /nix/store/... RPATH strings left in .dynstr after patchelf --remove-rpath
    cleaned = re.sub(rb"/nix/store/[^\x00]+", lambda m: b"\x00" * len(m.group(0)), bytes(b))
    return cleaned


def best_lzma1(payload: bytes) -> bytes:
    best_stream = None
    dict_sz = max(8192, 1 << (len(payload) - 1).bit_length())
    for mf in (lzma.MF_BT4, lzma.MF_HC4, lzma.MF_BT3, lzma.MF_BT2):
        for nice_len in (8, 10, 12, 14, 16, 20, 24, 32, 48, 64, 96, 128, 273):
            for depth in (0, 32, 128):
                comp = lzma.compress(
                    payload,
                    format=lzma.FORMAT_RAW,
                    filters=[{
                        "id": lzma.FILTER_LZMA1,
                        "lc": 0,
                        "lp": 0,
                        "pb": 0,
                        "nice_len": nice_len,
                        "mf": mf,
                        "depth": depth,
                        "dict_size": dict_sz,
                    }],
                )
                if best_stream is None or len(comp) < len(best_stream):
                    best_stream = comp
    assert best_stream is not None
    return best_stream


def cmd_pack(args: list[str]) -> None:
    inner_elf, stub_s, stub_ld, out_exe, exact_str = args
    exact_size = int(exact_str)

    raw_inner = open(inner_elf, "rb").read()
    stripped_inner = sstrip_elf64(raw_inner)

    comp = best_lzma1(stripped_inner)
    hdr = struct.pack("<I", len(stripped_inner))

    with tempfile.TemporaryDirectory() as td:
        stub_o = os.path.join(td, "stub.o")
        stub_elf = os.path.join(td, "stub.elf")
        stub_bin_path = os.path.join(td, "stub.bin")
        subprocess.run(
            ["gcc", "-c", f"-Wa,--defsym,FILE_SIZE={exact_size}", stub_s, "-o", stub_o],
            check=True,
        )
        subprocess.run(
            ["ld", "-T", stub_ld, "--build-id=none", stub_o, "-o", stub_elf],
            check=True,
        )
        subprocess.run(
            ["objcopy", "-O", "binary", stub_elf, stub_bin_path],
            check=True,
        )
        stub_bin = open(stub_bin_path, "rb").read()

    used = len(stub_bin) + len(hdr) + len(comp)
    headroom = exact_size - used
    print(
        f"  [pack] {out_exe}: inner={len(stripped_inner)}B -> lzma={len(comp)}B "
        f"+ stub={len(stub_bin)}B + hdr=4B = {used}B / {exact_size}B (headroom: {headroom}B)"
    )
    if used > exact_size:
        raise SystemExit(f"ERROR: {out_exe} exceeds budget by {used - exact_size} bytes!")

    final_bytes = stub_bin + hdr + comp + (b"\x00" * headroom)
    assert len(final_bytes) == exact_size
    open(out_exe, "wb").write(final_bytes)
    os.chmod(out_exe, 0o755)


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("Usage: pack.py <minify-shaders|pack> ...")
    mode = sys.argv[1]
    if mode == "minify-shaders":
        cmd_minify_shaders(sys.argv[2:])
    elif mode == "pack":
        cmd_pack(sys.argv[2:])
    else:
        raise SystemExit(f"Unknown mode: {mode}")


if __name__ == "__main__":
    main()
