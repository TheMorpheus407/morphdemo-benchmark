#!/bin/sh
# verify.sh -- check the three productions.
#
#   sh tools/verify.sh [display]
#
# For every binary it checks the exact file size, runs the production from
# beginning to end and measures how long it takes, and then runs it a second
# time and sends an escape key in the middle to see whether it stops early.
#
# The display defaults to $DISPLAY.

set -u

DISP=${1:-${DISPLAY:-:0}}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT" || exit 1

XCAP=$ROOT/build/wincap
XSEND=$ROOT/build/xsend

if [ ! -x "$XCAP" ]; then
    mkdir -p build
    cc -O2 -o "$XCAP" tools/wincap.c \
        -L/run/current-system/sw/share/nix-ld/lib -lX11 \
        -Wl,-rpath,/run/current-system/sw/share/nix-ld/lib || exit 1
fi
if [ ! -x "$XSEND" ]; then
    cc -O1 -o "$XSEND" tools/xsend.c -ldl \
        -L/run/current-system/sw/share/nix-ld/lib -lX11 \
        -Wl,-rpath,/run/current-system/sw/share/nix-ld/lib || exit 1
fi

fail=0

check_size() {
    want=$1
    got=$(stat -c%s "$2")
    if [ "$got" -eq "$want" ]; then
        echo "  size      $2 = $got bytes        OK"
    else
        echo "  size      $2 = $got bytes, expected $want   FAIL"
        fail=1
    fi
}

run_full() {
    bin=$1
    start=$(date +%s.%N)
    DISPLAY=$DISP "./$bin" > "/tmp/$bin.log" 2>&1
    rc=$?
    end=$(date +%s.%N)
    dur=$(awk "BEGIN{printf \"%.1f\", $end-$start}")
    if [ "$rc" -ne 0 ]; then
        echo "  run       $bin exited with $rc                FAIL"
        cat "/tmp/$bin.log"
        fail=1
    elif awk "BEGIN{exit !($dur>=50 && $dur<=70)}"; then
        echo "  run       $bin lasted ${dur}s (50..70s)     OK"
    else
        echo "  run       $bin lasted ${dur}s, out of range   FAIL"
        fail=1
    fi
    if grep -q "sound device" "/tmp/$bin.log"; then
        echo "  audio     $bin: no sound device              FAIL"
        fail=1
    else
        echo "  audio     $bin: the device was opened        OK"
    fi
}

run_escape() {
    bin=$1
    DISPLAY=$DISP "./$bin" > "/tmp/$bin.esc.log" 2>&1 &
    pid=$!
    sleep 8
    if ! kill -0 $pid 2>/dev/null; then
        echo "  escape    $bin stopped on its own too early    FAIL"
        fail=1
        return
    fi
    DISPLAY=$DISP "$XSEND" "$DISP" > /dev/null 2>&1
    sleep 3
    if kill -0 $pid 2>/dev/null; then
        echo "  escape    $bin ignored the escape key        FAIL"
        kill $pid
        fail=1
    else
        wait $pid 2>/dev/null
        echo "  escape    $bin stopped on escape             OK"
    fi
}

for spec in "morphdemo-4k 4096" "morphdemo-16k 16384" "morphdemo-64k 65536"; do
    set -- $spec
    echo "=== $1 ==="
    if [ ! -x "$1" ]; then
        echo "  missing   $1                                FAIL"
        fail=1
        continue
    fi
    check_size "$2" "$1"
    run_full "$1"
    run_escape "$1"
done

if [ "$fail" -eq 0 ]; then
    echo "all checks passed"
else
    echo "there were failures"
fi
exit $fail
