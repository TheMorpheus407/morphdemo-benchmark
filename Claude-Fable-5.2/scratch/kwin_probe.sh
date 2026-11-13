#!/bin/sh
. /home/candidate/workspace/scratch/env.sh
echo "INSIDE: DISPLAY=$DISPLAY WAYLAND_DISPLAY=$WAYLAND_DISPLAY"
$XDPYINFO | grep -E 'dimensions|resolution'
$XPROP -root | grep -iE '_NET_SUPPORTED|_NET_WORKAREA|_NET_DESKTOP_GEOMETRY|Xft|RESOURCE' | cut -c1-600
/home/candidate/workspace/scratch/t_glx 2>&1 | grep -E 'screen|RENDERER|fps'
