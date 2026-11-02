#!/bin/sh
# env.sh -- locate the runtime libraries of the supplied environment.
#
# The demos link against the X11, GL and ALSA libraries that are already
# installed on the machine.  No development packages are needed, so the shared
# objects are located by their soname and referenced by path; the resulting
# binaries only carry the soname in DT_NEEDED.
#
# NixOS keeps a farm of the system libraries in /run/current-system/sw/share/
# nix-ld/lib.  Pointing the run path at that single directory keeps the
# binaries short and lets the loader resolve every soname.
#
# Writes make variables to stdout.

findlib() {
    for d in /run/current-system/sw/share/nix-ld/lib /run/current-system/sw/lib \
             /run/current-system/sw/lib64 /usr/lib /usr/lib64 /lib /lib64; do
        if [ -e "$d/$1" ]; then
            echo "$d/$1"
            return 0
        fi
    done
    find /nix/store -maxdepth 3 -name "$1" -print -quit 2>/dev/null
}

X11=$(findlib libX11.so.6)
GL=$(findlib libGL.so.1)
ALSA=$(findlib libasound.so.2)
LIBC=$(findlib libc.so.6)

if [ -z "$X11" ] || [ -z "$GL" ] || [ -z "$ALSA" ]; then
    echo "env.sh: required libraries not found" >&2
    exit 1
fi

# one common directory is all it takes when the farm is available
FARM=/run/current-system/sw/share/nix-ld/lib
if [ -e "$FARM/libX11.so.6" ] && [ -e "$FARM/libGL.so.1" ] && [ -e "$FARM/libasound.so.2" ]; then
    RUNPATH="$FARM"
    X11="$FARM/libX11.so.6"
    GL="$FARM/libGL.so.1"
    ALSA="$FARM/libasound.so.2"
    LIBC="$FARM/libc.so.6"
else
    RUNPATH="$(dirname "$X11"):$(dirname "$GL"):$(dirname "$ALSA")"
fi

echo "LIBX11 := $X11"
echo "LIBGL := $GL"
echo "LIBALSA := $ALSA"
echo "GLIBCDIR := $(dirname "$LIBC")"
echo "RUNPATH := $RUNPATH"
