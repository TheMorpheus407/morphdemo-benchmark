#!/usr/bin/env python3
"""Build step for the 4k demo: minify common/img/snd GLSL and emit shaders.inc for player.S.
usage: mk4k.py [--raw]    (--raw: no minification, for debugging)"""
import os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
tmp = os.path.join(here, "shaders_min.h")
args = [sys.executable, os.path.join(here, "..", "..", "tools", "glslmin.py"), os.path.join(here, "chunks.def"), tmp, "--stats"]
if "--raw" in sys.argv: args.append("--keep-names")
subprocess.check_call(args)
out = []
os.makedirs(os.path.join(here, "min"), exist_ok=True)
for name, body in re.findall(r'static const char (\w+)\[\] = "(.*)";', open(tmp).read()):
    out.append('%s: .asciz "%s"' % (name, body))
    raw = body.encode().decode("unicode_escape")
    # also keep the minified text as files so that the dev tool can run exactly what ships (dev4k --min)
    open(os.path.join(here, "min", {"s_common": "common.glsl", "s_img": "img.glsl", "s_snd": "snd.glsl"}[name]), "w").write(raw)
    sys.stderr.write("  %-9s %5d bytes\n" % (name, len(raw)))
open(os.path.join(here, "shaders.inc"), "w").write("\n".join(out) + "\n")
os.remove(tmp)
