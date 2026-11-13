#!/bin/sh
# Dev helper: start a private Xvfb + PulseAudio null sink (so that several people can
# run/capture windowed demos concurrently without sharing the screen or the audio sink).
#   eval "$(tools/private_env.sh 96)"     -> exports DISPLAY=:96 and PULSE_SERVER=unix:/tmp/pulse-96
#   tools/private_env.sh 96 stop
N=$1
PA=/nix/store/igqqqab47468vk06ingdxbanq2w23hjd-pulseaudio-17.0/.bin-unwrapped/pulseaudio
if [ "$2" = stop ]; then
  [ -f /tmp/penv-$N.pids ] && kill $(cat /tmp/penv-$N.pids) 2>/dev/null; rm -f /tmp/penv-$N.pids; exit 0
fi
if [ ! -S /tmp/.X11-unix/X$N ]; then
  Xvfb :$N -screen 0 1920x1080x24 -nolisten tcp -ac >/dev/null 2>&1 &
  echo $! >> /tmp/penv-$N.pids
fi
if [ ! -S /tmp/pulse-$N ]; then
  cat > /tmp/pulse-$N.pa <<EOT
.fail
load-module module-native-protocol-unix socket=/tmp/pulse-$N auth-anonymous=1
load-module module-null-sink sink_name=candidate rate=48000 channels=2
set-default-sink candidate
set-default-source candidate.monitor
EOT
  env -u PULSE_SERVER XDG_RUNTIME_DIR=/tmp/pulse-rt-$N DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/pulse-rt-$N/nobus sh -c "mkdir -p /tmp/pulse-rt-$N; exec $PA --daemonize=no --exit-idle-time=-1 --disable-shm -n -F /tmp/pulse-$N.pa" >/dev/null 2>&1 &
  echo $! >> /tmp/penv-$N.pids
fi
i=0; while [ $i -lt 150 ] && { [ ! -S /tmp/.X11-unix/X$N ] || [ ! -S /tmp/pulse-$N ]; }; do sleep 0.1; i=$((i+1)); done
echo "export DISPLAY=:$N PULSE_SERVER=unix:/tmp/pulse-$N"
