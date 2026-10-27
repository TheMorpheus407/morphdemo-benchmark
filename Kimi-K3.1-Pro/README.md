# MorphDemo — From Noise to Order

Three original, standalone native Linux audiovisual productions. Each runs a 60-second synthesized score and a continuously animated, synchronized visual sequence, then exits automatically. No interaction or configuration is required.

| Executable | Exact size | Production |
| --- | ---: | --- |
| `morphdemo-64k` | 65,536 bytes | **Crystal Assembly** — raymarched fragments align into crystalline machinery and a luminous gyroid core; 120 BPM. |
| `morphdemo-16k` | 16,384 bytes | **Polar Bloom** — turbulent filaments become flowing ribbons, orbiting motes, and an ordered floral emblem; 96 BPM. |
| `morphdemo-4k` | 4,096 bytes | **Interference** — sparse green noise condenses into ordered interference waves and a final hexagonal ring; 128 BPM. |

All three have distinct emergence, transformation, and resolution states. `MORPH` is drawn with embedded procedural bitmap lettering, not a font file.

## Build

On the supplied machine, run exactly:

```sh
make clean
make -j3 all
```

The build uses the available GCC, GNU binutils, GNU Make, Python 3, and `patchelf`. It needs no downloads and no additional SDKs or development headers. The Makefile uses the SDL2, OpenGL, C, and math libraries already present in the supplied environment.

To check the required sizes:

```sh
stat -c '%n: %s bytes' morphdemo-64k morphdemo-16k morphdemo-4k
```

## Run

```sh
./morphdemo-64k
./morphdemo-16k
./morphdemo-4k
```

Each program opens a 1920×1080 fullscreen OpenGL window and a 48 kHz stereo audio device. Press **Escape** or close the window to terminate early. The production itself lasts 60 seconds; normal process runtime includes brief driver and shader initialization.

## Implementation

- `src/main.c`, `src/audio.c`, and `src/platform.h` share the larger demos' SDL2/OpenGL engine and sample-accurate musical clock.
- `src/shaders/crystal.glsl`, `bloom.glsl`, and `interference.glsl` generate every visual at runtime. `tools/embed.py` minifies and embeds them in the executables at build time.
- The scores synthesize oscillators, seeded noise, percussion, bass, melody, chords, envelopes, stereo panning, and feedback delay directly into audio buffers. Visual beat pulses and phase changes use the same timeline as the sound.
- `src/tiny.c` is the 4k production's compact engine and synthesizer.
- `tools/lzr.py` implements the adaptive range-coded LZ compressor. `src/packstub.c` is its native x86-64 decoder and syscall-only launcher. The 16k and 4k executables decompress their own embedded production into an anonymous memfd and execute it with `execveat`; no external unpacker, shell, interpreter, renderer, or second demo is invoked.
- `tools/sstrip.py` removes non-runtime ELF section metadata, and `tools/pad.py` pads only the final native executables to their exact required sizes.
- `tools/x11_capture.py` and `tools/x11_escape.py` are optional development-time X11 verification helpers. They are not used by any production.

The programs do not read production assets, access the network, or require files from the source tree at runtime. Each delivered executable contains all of its own production code and data.
