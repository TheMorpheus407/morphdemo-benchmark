#!/bin/sh
# Builds the (unpacked) 4k payload: build/demo4k.payload
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd); S="$ROOT/src/demo4k"; B="$ROOT/build"
python3 "$S/mk4k.py"
gcc -c -x assembler-with-cpp -I"$S" "$S/player.S" -o "$B/demo4k.o"
ld --no-warn-rwx-segments -z noexecstack -T "$S/link.ld" -o "$B/demo4k.elf" "$B/demo4k.o"
objcopy -O binary -j .all "$B/demo4k.elf" "$B/demo4k.payload"
chmod +x "$B/demo4k.payload"
echo "payload: $(stat -c %s "$B/demo4k.payload") bytes   (xz: $(xz -9e --format=raw -c "$B/demo4k.payload" 2>/dev/null | wc -c))"
