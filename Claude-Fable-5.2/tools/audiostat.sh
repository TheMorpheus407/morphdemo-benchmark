#!/bin/sh
# Dev helper: loudness / peak report and spectrogram for a WAV.
#   tools/audiostat.sh in.wav [out.png] [t0 dur]
W=$1; P=${2:-${W%.wav}_spec.png}
SS=""; [ -n "$3" ] && SS="-ss $3 -t $4"
ffmpeg -hide_banner -nostats $SS -i "$W" -af ebur128=peak=true -f null - 2>&1 | grep -E 'I:|LRA:|Peak:' | tail -3
ffmpeg -hide_banner -nostats $SS -i "$W" -af astats=measure_perchannel=none -f null - 2>&1 | grep -E 'RMS level|Peak level|DC offset|Crest' | sed 's/.*] //'
ffmpeg -hide_banner -loglevel error -y $SS -i "$W" -lavfi "showspectrumpic=s=1600x600:legend=1:scale=log:fscale=log:drange=90" "$P" && echo "$P"
