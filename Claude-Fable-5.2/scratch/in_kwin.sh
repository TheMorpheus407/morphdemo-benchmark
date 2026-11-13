#!/bin/sh
. /home/candidate/workspace/scratch/env.sh
cd /home/candidate/workspace/build
./t1 & PID=$!
sleep 2.5
echo "--- xwininfo"; $XWININFO -root -tree 2>/dev/null | grep -E 'MORPH' | head -5
WID=$($XDOTOOL search --name MORPH 2>/dev/null | head -1); echo "wid=$WID"
$XWININFO -id $WID 2>/dev/null | grep -E 'Absolute|Width|Height|Depth|Map State'
echo "--- active window:"; $XPROP -root _NET_ACTIVE_WINDOW 2>/dev/null
$XPROP -id $WID _NET_WM_STATE WM_STATE 2>/dev/null
echo "--- sending Escape at $(date +%s.%N)"; $XDOTOOL key Escape
T0=$(date +%s.%N); wait $PID; RC=$?; T1=$(date +%s.%N)
echo "exit code $RC after $(echo "$T1 - $T0" | bc -l 2>/dev/null || python3 -c "print($T1-$T0)") s"
