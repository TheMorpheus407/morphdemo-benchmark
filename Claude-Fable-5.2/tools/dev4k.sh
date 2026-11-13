#!/bin/sh
# builds the 4k dev tool -> build/dev4k
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
gcc -O1 -g -std=gnu11 -DDEMO_DIR="\"$ROOT/src/demo4k\"" -w -msse4.1 "$ROOT/src/demo4k/dev4k.c" -o "$ROOT/build/dev4k" -ldl -lm -lpthread -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2
