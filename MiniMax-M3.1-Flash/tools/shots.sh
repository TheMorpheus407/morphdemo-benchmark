#!/bin/sh
# shots.sh -- capture the demo window at a list of moments.
#
#   sh tools/shots.sh <binary> <prefix> <second> [second ...]
#
# The binary is started once and the window is grabbed at every requested
# moment of its run, so one run yields a strip of the whole production.

set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

BIN=$1
PREFIX=$2
shift 2

XCAP=$ROOT/build/wincap
if [ ! -x "$XCAP" ]; then
    mkdir -p build
    cc -O2 -o "$XCAP" tools/wincap.c \
        -L/run/current-system/sw/share/nix-ld/lib -lX11 \
        -Wl,-rpath,/run/current-system/sw/share/nix-ld/lib || exit 1
fi

mkdir -p work/frames

DISPLAY=${DISPLAY:-:0} "./$BIN" > "/tmp/$(basename "$BIN").shots.log" 2>&1 &
pid=$!

start=$(date +%s.%N)
for t in "$@"; do
    now=$(date +%s.%N)
    wait_t=$(awk "BEGIN{printf \"%.2f\", $t-($now-$start)}")
    if [ "$(awk "BEGIN{print ($wait_t>0)?1:0}")" = "1" ]; then
        sleep "$wait_t"
    fi
    if ! kill -0 $pid 2>/dev/null; then
        echo "shots: the production ended before t=$t"
        break
    fi
    DISPLAY=${DISPLAY:-:0} "$XCAP" "work/frames/$PREFIX""_$t.ppm" 2>/dev/null
    ffmpeg -y -loglevel error -i "work/frames/$PREFIX""_$t.ppm" \
        -vf scale=760:428 "work/frames/$PREFIX""_$t.png" 2>/dev/null
    echo "shots: captured t=$t"
done

wait $pid
echo "shots: $BIN finished"
