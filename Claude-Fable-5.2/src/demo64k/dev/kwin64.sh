#!/bin/sh
# dev helper: run a command in a private headless KWin+XWayland session (GLX path test) with its own runtime dir
KARGS="$1"; shift
export XDG_RUNTIME_DIR=/tmp/xdg64; mkdir -p $XDG_RUNTIME_DIR; chmod 700 $XDG_RUNTIME_DIR
exec env -u DISPLAY -u WAYLAND_DISPLAY QT_QPA_PLATFORM= kwin_wayland --virtual $KARGS --xwayland --socket wayland-d64 --no-lockscreen --no-global-shortcuts --exit-with-session "$@" 2>/tmp/xdg64/kwin.err
