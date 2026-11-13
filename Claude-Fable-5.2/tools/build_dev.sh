#!/bin/sh
# Dev build of a C demo (hosted, shaders loaded from files, frame/wav dump options):
#   tools/build_dev.sh <demo>        -> build/<demo>_dev
#   tools/build_dev.sh <demo> gen    -> build/<demo>_gen   (same, but with the minified embedded shaders)
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd); N=$1
X=""; SUF=dev
if [ "$2" = gen ]; then python3 "$ROOT/tools/glslmin.py" "$ROOT/src/$N/chunks.def" "$ROOT/src/$N/shaders_gen.h" --stats; X=-DUSE_GEN; SUF=gen; fi
gcc -O1 -g -std=gnu11 -DDEV $X -DDEMO_DIR="\"$ROOT/src/$N\"" -w -msse4.1 -ffast-math "$ROOT/src/$N/demo.c" -o "$ROOT/build/${N}_$SUF" -ldl -lm -lpthread -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2
