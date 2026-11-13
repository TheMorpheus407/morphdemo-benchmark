# MorphDemo — *From Noise to Order*

Three standalone, procedural real-time x86-64 Linux audiovisual productions built to exact executable byte budgets:

| Executable | Exact Size | Title | Audio Engine | Visual Architecture |
| :--- | ---: | :--- | :--- | :--- |
| `morphdemo-4k` | `4,096` B | *Signal Crystallization* | 120 BPM Q14 fixed-point synth (`48 kHz` stereo `S16`) | Single-pass GLSL 430 raymarcher: electric scanline plasma $\to$ obsidian KIFS ferrofluid & gimbal rings $\to$ golden crystal colonnade & faceted `MORPH` blocks |
| `morphdemo-16k` | `16,384` B | *Abyssal Morphogenesis* | 108 BPM D-Aeolian $\to$ F-Lydian aquatic synth (`48 kHz` stereo `F32`) | 3-pass `RGBA16F` HDR pipeline: pelagic spore cloud $\to$ radiolarian mitosis $\to$ coral double-helix $\to$ living-glass `MORPH` sanctuary with temporal feedback & chromatic dispersion |
| `morphdemo-64k` | `65,536` B | *Architectures of Entropy* | 128 BPM C#m $\to$ Db Major electronic-orchestral synth + 4-channel Hadamard FDN hall reverb (`48 kHz` stereo `F32`) | 5-pass hybrid engine: `512x512` GPU wave simulation + soft-shadowed volumetric raymarcher writing `gl_FragDepth` + `18,432` depth-occluded 3D polygonal shards + anamorphic HDR bloom + ACES composite culminating in a beveled Roman-serif `MORPH` monument |

## Build Commands

Rebuild all three executables cleanly from source using the system toolchain (`gcc`, `ld`, `objcopy`, `strip`, `patchelf`, `python3`, `make`):

```bash
make clean && make
```

Verify exact byte sizes:

```bash
stat -c "%n: %s bytes" morphdemo-4k morphdemo-16k morphdemo-64k
```

## Running

Each executable runs for `60.0` seconds at `1920x1080` fullscreen with real-time synthesized stereo audio and exits cleanly at completion or immediately when pressing `Escape`:

```bash
./morphdemo-4k
./morphdemo-16k
./morphdemo-64k
```

## Implementation Overview

- **Self-Decompression Stub (`src/common/stub.S`, `src/common/stub.ld`)**: Custom `872`-byte static x86-64 ELF64 unpacker implementing a raw LZMA1 (`lc=0, lp=0, pb=0`) range decoder in assembly, writing the decompressed inner ELF into an anonymous in-memory file via `memfd_create` and executing it directly via `execveat(..., AT_EMPTY_PATH)` with zero external processes or temporary files.
- **Shader Minifier & Packer (`tools/pack.py`)**: Strips comments, compacts float literals, renames GLSL identifiers by token frequency (preserving cross-stage varyings), strips non-load ELF section metadata, searches optimal raw LZMA1 match-finder parameters, and pads deterministically to the exact target byte count.
- **Zero External Assets (`src/common/platform.h`, `src/demo4k/`, `src/demo16k/`, `src/demo64k/`)**: All geometry, distance fields, simulations, typography, lighting, post-processing, and musical scores are generated procedurally in real time at startup and per-frame/per-sample using system `libSDL2` and OpenGL 4.3+.
