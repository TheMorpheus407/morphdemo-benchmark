# MorphDemo — FROM NOISE TO ORDER

Three self-contained real-time audiovisual demos as native Linux executables
of exact sizes:

| executable       | size      | tier                                             |
|------------------|-----------|--------------------------------------------------|
| `morphdemo-64k`  | 65,536 B  | richest: 96 particles, fbm clouds, constellation, mandala, full score |
| `morphdemo-16k`  | 16,384 B  | medium: 64 particles, echo, hats                  |
| `morphdemo-4k`   | 4,096 B   | minimal, carried inside a self-decompressing stub |

Each executable is a complete standalone 60-second production: real-time
OpenGL rendering, runtime-synthesized music, three phases with smooth
transitions, the word **MORPH**, automatic termination, and clean exit on
Escape. No external assets, no network, no subprocesses.

## Build

On the reference machine (see `ENVIRONMENT.json`; tools live in
`/run/current-system/sw/bin` — put that directory on `PATH`):

```sh
make            # builds all three final executables
make verify     # checks exact file sizes (65536/16384/4096) and ELF EXEC
make tools      # builds the verification helpers (build/shot, build/sendkey)
make test4k     # 4k tier, 8 s test build, run through the stub (release path)
make dbg4k      # same, plus -DDBG (logs shader compile/link errors)
make clean
```

Only `gcc`, `ld`, `make`, `python3` and coreutils are needed. The Makefile
exports `LIBRARY_PATH=/run/current-system/sw/share/nix-ld/lib` itself.

### What the build does

* **64k / 16k** — `src/morphdemo.c` compiled `-DTIER=2` / `-DTIER=1` with
  `gcc -Os … -nostdlib -nostartfiles -no-pie`, linked through the gcc
  driver with `-T src/tiny.ld` (one RWX `PT_LOAD`, no `PT_GNU_STACK`) against
  `-lX11 -lGL -lXrandr -lpulse-simple`, stripped by `tools/sstrip.py`
  (drops section headers, truncates at the end of the last `PT_LOAD`) and
  padded with `truncate` to the exact size.
* **4k** — a chain:
  1. `src/demo4k.c` compiled `-DMINI -Oz -fno-plt`, linked **directly with
     `ld`** (not the gcc driver):
     `NIX_DONT_SET_RPATH_x86_64_unknown_linux_gnu=1 ld --dynamic-linker /lib64/ld-linux-x86-64.so.2 -T src/tiny4k.ld -L/run/current-system/sw/share/nix-ld/lib --no-as-needed -lX11 -lGL -lXrandr -lpulse-simple -l:libc.so.6 --hash-style=sysv -z norelro -z max-page-size=4096 --build-id=none --allow-shlib-undefined`.
     (`NIX_DONT_SET_RPATH_…` stops the nix `ld` wrapper from injecting an
     `-rpath`; `/lib64/ld-linux-x86-64.so.2` resolves the libraries by soname
     without any `RUNPATH` — verified in `ENVIRONMENT.json`.)
  2. `tools/sstrip.py` → `tools/lzpack.py` (LZSS + adaptive range coder,
     ~61 %) → `build/payload.bin`.
  3. `ld -r -b binary` embeds the compressed payload in `src/stub.c`'s
     `.data`; the stub is linked (`-T src/stub.ld`, one RWX `PT_LOAD`, no
     interpreter, no dynamic section) and stripped.
  4. The build **fails loudly** if the stripped stub exceeds 4096 bytes,
     otherwise it is copied to `morphdemo-4k` and padded with `truncate`.

## The production (identical timeline in all three tiers)

```
phase 1  EMERGENCE       t =  0..20   scattered drifting particles,
                                      inharmonic ring-modulated noise bed,
                                      55 Hz drone
phase 2  TRANSFORMATION  t = 20..40   particles lock onto a 9x5 lattice,
                                      kick (120 BPM) pulses them, arpeggio
phase 3  RESOLUTION      t = 40..60   the lattice slowly rotates, warm
                                      palette, MORPH title (t = 42..57),
                                      climax flash at t = 52, fade, exit
```

Total runtime 60 s (spec range 50–70 s), then the process exits with code 0
and prints `morphdemo: done`. Escape quits early, also with code 0.

## Implementation notes

* **Freestanding engine.** No libc: custom `_start`, raw syscalls, own
  `sinf`/smoothstep. The display is an override-redirect 1920x1080 window
  centered on the **primary** monitor (`XRRGetMonitors`; on this machine
  point 0,0 is off-screen and an off-screen override-redirect window is
  never composited — GLX swaps would stall to ~1 Hz).
* **Audio.** PulseAudio simple API, 48 kHz stereo. A table-driven voice bank
  (drone, kick, arpeggio, bell) plus the phase-1 noise bed is synthesized
  per sample and pushed at real-time rate with a 60 ms lookahead, so the
  score is locked to the picture (kick starts exactly at the phase-2
  lattice lock, the bell matches the t=52 flash). Without an audio server
  in the environment the demos run the full 60 s without sound.
* **4k tier specifics.** One `glRects` fullscreen draw; 277 `vec4` sprite
  slots are fed through a single uniform array `u_p[277]` — slot 276
  carries the time, so one `glGetUniformLocation` query serves both.
  `glCreateShaderProgramv` compiles and links the fragment shader in one
  call. Every library function is resolved at runtime with `dlsym` from one
  string table (`-DMINI`, see `src/minilib.h`), leaving `dlsym` as the only
  dynamic symbol (one `GLOB_DAT` relocation, no PLT thanks to `-fno-plt`).
  Escape is the hardcoded keycode 9. The MORPH title is 83 tight glow-dot
  sprites from an embedded 22-byte 5x7 bitmap.
* **Self-decompression (4k).** `src/stub.c` is a tiny static ELF that
  carries the compressed payload ELF in its `.data`. At startup it does
  `memfd_create` → `ftruncate` → `mmap` → LZSS/range-decode →
  `execveat(AT_EMPTY_PATH)` — all by code inside the executable itself, as
  permitted by the spec (no external unpacker, no `/proc` path needed;
  `vm.memfd_noexec = 0` on this machine).

## Verification (all performed on the reference machine)

* `make verify`: exact sizes 65536 / 16384 / 4096 bytes, all ELF `EXEC`.
* Full runs: 64k 60.19 s, 16k 60.0 s, 4k 60.10 s — all rc=0,
  `morphdemo: done`, window destroyed after exit (post-exit screenshot
  fails as expected).
* Escape at t=8 s: all three exit rc=0 with `morphdemo: done`.
* Screenshots across all phases: scatter → lattice → rotation/title are
  clearly distinguishable; fade to black at the end
  (luminance 0.146 → 0.074 → 0.013 → 0.000 at t = 58 → 60).
* MORPH title: cell-level font reconstruction 175/175 (64k and 4k).
* Audio captured from each demo's own stream node (`pw-record --target
  <object.serial>`): 55 Hz drone, noise bed fading by t=23, kick grid with
  exact 0.500 s spacing from t=20, arpeggio from t=26, bell at t=52, fade
  out; 60.9 s of audio for a 60 s picture.
* `strace -f -e trace=network`: only `AF_UNIX` sockets (X11 + PulseAudio),
  no network access.
* Dependencies (`readelf -d`): only the system runtime libraries listed in
  `ENVIRONMENT.json` (libX11, libGL, libXrandr, libpulse-simple; the 4k
  payload additionally libc.so.6 via the loader).
* Bare environment (`env -i DISPLAY=:0 XAUTHORITY=…`): all three run the
  full 60 s with rc=0 and visible output; audio is skipped gracefully.

## Files

```
morphdemo-64k / morphdemo-16k / morphdemo-4k   final executables (exact sizes)
Makefile                                        build (see above)
src/morphdemo.c                                 64k/16k engine (TIER=1/2)
src/demo4k.c                                    4k payload demo (-DMINI)
src/stub.c                                      4k self-decompressing loader
src/minilib.h                                   freestanding X11/GLX/Pulse
                                                declarations + MINI dlsym resolver
src/tiny.ld / tiny4k.ld / stub.ld               linker scripts (single-segment)
tools/lzpack.py                                 LZSS + range-coder compressor
tools/sstrip.py                                 section-header stripper
tools/shot.c / sendkey.c                        screenshot / Escape helpers
tools/wavshim.c / xqmon.c                       LD_PRELOAD debug shims
build/                                          intermediates + verification
                                                scripts (verify_full.sh,
                                                fine_test.py, title_check.py,
                                                analyze.py) and captured
                                                screenshots/audio
SPEC.md, ENVIRONMENT.json, prompt.txt           task inputs
```
