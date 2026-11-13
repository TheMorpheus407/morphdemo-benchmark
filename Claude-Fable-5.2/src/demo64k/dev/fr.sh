#!/bin/sh
# dev helper: render frames at given times and assemble a sheet.  fr.sh "t1 t2 ..." [cols] [scale] [name]
ROOT=/home/candidate/workspace; B=$ROOT/build; C=$B/cap/d64; mkdir -p $C
COLS=${2:-3}; SC=${3:-0.333}; NAME=${4:-fr}
ARGS=""; FILES=""
for t in $1; do f=$C/${NAME}_$(printf '%06.2f' $t).ppm; ARGS="$ARGS --frame $t $f"; FILES="$FILES $f"; done
$B/demo64k_dev $ARGS 2>&1 | grep -v -e '^$' -e '^GL:' -e 'demo_init'
python3 $ROOT/tools/ppm2png.py --sheet $C/$NAME.png $COLS $SC $FILES
python3 - $FILES <<'PY'
import sys
from PIL import Image
import numpy as np
for f in sys.argv[1:]:
    a=np.asarray(Image.open(f)).astype(float)/255.; l=a.mean(axis=2)
    print("%s mean %.3f p50 %.3f p99 %.3f max %.3f sat%% %.2f" % (f[-10:-4], l.mean(), np.percentile(l,50), np.percentile(l,99), l.max(), 100*(a.max(axis=2)>0.98).mean()))
PY
echo $C/$NAME.png
