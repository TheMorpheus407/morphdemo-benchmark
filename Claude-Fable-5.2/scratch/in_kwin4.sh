#!/bin/sh
cd /home/candidate/workspace/build
../tools/avrec 14 30 cap/k4kwin win @big shots "8,12" -- ./demo4k.payload &
sleep 13; . /home/candidate/workspace/scratch/env.sh; $XDOTOOL key Escape; wait
