#!/bin/sh
# Dev helper: run a command inside a headless KWin Wayland session with XWayland
# (emulates the KDE desktop of the target machine: NVIDIA GLX through XWayland).
#   kwin_run.sh "<kwin size args>" command...     e.g.  kwin_run.sh "--width 1920 --height 1080 --scale 2" ./demo
KARGS="$1"; shift
export XDG_RUNTIME_DIR=/tmp/xdg; mkdir -p $XDG_RUNTIME_DIR; chmod 700 $XDG_RUNTIME_DIR
exec env -u DISPLAY -u WAYLAND_DISPLAY QT_QPA_PLATFORM= KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1 \
  kwin_wayland --virtual $KARGS --xwayland --socket wayland-test --no-lockscreen --no-global-shortcuts --exit-with-session "$@" 2>/tmp/xdg/kwin.err
