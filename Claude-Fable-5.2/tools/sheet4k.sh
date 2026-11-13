#!/bin/sh
# contact sheet for the 4k dev tool: tools/sheet4k.sh T0 T1 DT [cols] [scale] [extra dev4k flags]
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd); T0=$1; T1=$2; DT=$3; COLS=${4:-4}; SC=${5:-0.25}
D="$ROOT/build/cap/seq_4k"; rm -rf "$D"; mkdir -p "$D"
"$ROOT/build/dev4k" $6 --seq $T0 $T1 $DT "$D/f" 2>&1 | grep -E 'ERROR|error' || true
python3 "$ROOT/tools/ppm2png.py" --sheet "$ROOT/build/cap/k4_sheet_${T0}_${T1}.png" $COLS $SC $(ls "$D"/f_*.ppm | sort)
rm -rf "$D"; echo "$ROOT/build/cap/k4_sheet_${T0}_${T1}.png"
