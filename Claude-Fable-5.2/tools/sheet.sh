#!/bin/sh
# Dev helper: contact sheet of frames rendered by a demo's dev tool.
#   tools/sheet.sh <demo> <t0> <t1> <dt> [cols] [scale]   -> build/cap/<demo>_sheet_<t0>_<t1>.png
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd); N=$1; T0=$2; T1=$3; DT=$4; COLS=${5:-4}; SC=${6:-0.25}
D="$ROOT/build/cap/seq_$N"; rm -rf "$D"; mkdir -p "$D"
"$ROOT/build/${N}_dev" --seq $T0 $T1 $DT "$D/f" 2>&1 | grep -v -e '^$' -e '^GL:' -e 'demo_init' || true
python3 "$ROOT/tools/ppm2png.py" --sheet "$ROOT/build/cap/${N}_sheet_${T0}_${T1}.png" $COLS $SC $(ls "$D"/f_*.ppm | sort)
rm -rf "$D"
echo "$ROOT/build/cap/${N}_sheet_${T0}_${T1}.png"
