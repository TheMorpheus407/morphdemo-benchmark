#!/bin/sh
# Final (unpacked) build of a C demo: freestanding flat dynamic ELF.
#   tools/build_payload.sh <demo>    -> build/<demo>.payload
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd); N=$1; B="$ROOT/build"
python3 "$ROOT/tools/glslmin.py" "$ROOT/src/$N/chunks.def" "$ROOT/src/$N/shaders_gen.h" --stats
CF="-Os -std=gnu11 -fno-pic -no-pie -ffreestanding -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fomit-frame-pointer -ffast-math -fno-ident -fno-tree-loop-distribute-patterns -fno-align-functions -fno-align-jumps -fno-align-loops -fmerge-all-constants -msse4.1 -Wall -Wno-unused-function -Wno-unused-variable -Wno-misleading-indentation"
gcc $CF -c "$ROOT/src/$N/demo.c" -o "$B/$N.o"
gcc -c "$ROOT/src/common/hdr.S" -o "$B/hdr.o"
ld -z noexecstack --no-warn-rwx-segments -T "$ROOT/src/common/link.ld" -Map "$B/$N.map" -o "$B/$N.elf" "$B/hdr.o" "$B/$N.o"
objcopy -O binary "$B/$N.elf" "$B/$N.payload"
chmod +x "$B/$N.payload"
echo "payload: $(stat -c %s "$B/$N.payload") bytes   (xz: $(xz -9e --format=raw -c "$B/$N.payload" | wc -c))"
