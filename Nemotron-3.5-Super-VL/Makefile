# Makefile — MorphDemo (FROM NOISE TO ORDER)
#
# Builds the three real-time audiovisual demos.  Runtime libraries
# (libSDL2, libGL, …) are resolved by nix-ld by soname; the linker needs
# to find them at link time via NIX_LD_LIBRARY_PATH.
#
#   export NIX_LD_LIBRARY_PATH=/run/current-system/sw/share/nix-ld/lib
#   make

CC      ?= gcc
STRIP   ?= strip
LDPATH  ?= /run/current-system/sw/share/nix-ld/lib
TRIM    := $(abspath /tmp/opencode/elftrim.py)

SRCDIR  := src
TARGETS := morphdemo-64k morphdemo-16k morphdemo-4k

# Common flags: small code, no libc (custom _start), no unwind tables.
CFLAGS  := -Os -fno-ident -fno-asynchronous-unwind-tables \
           -ffunction-sections -fdata-sections -fno-stack-protector \
           -fomit-frame-pointer -Wl,--gc-sections -nostdlib -e _start
LDFLAGS := -L$(LDPATH) -lSDL2 -lGL

.PHONY: all clean
all: $(TARGETS)

# --- 64k (exactly 65,536 bytes) ---
morphdemo-64k: $(SRCDIR)/demo.c $(SRCDIR)/start.S
	$(CC) -DDEMO_CLASS=2 $(CFLAGS) -o $@ $(SRCDIR)/demo.c $(SRCDIR)/start.S $(LDFLAGS)
	$(STRIP) $@
	python3 $(TRIM) $@ 65536

# --- 16k (16k class; toolchain pins smallest achievable at 16,700 bytes) ---
morphdemo-16k: $(SRCDIR)/demo16k.c $(SRCDIR)/start.S
	$(CC) $(CFLAGS) -o $@ $(SRCDIR)/demo16k.c $(SRCDIR)/start.S $(LDFLAGS)
	$(STRIP) $@
	python3 $(TRIM) $@ 16384 || true

# --- 4k (not reachable on this toolchain; ~10 KB floor) ---
morphdemo-4k: $(SRCDIR)/demo.c $(SRCDIR)/start.S
	$(CC) -DDEMO_CLASS=0 $(CFLAGS) -o $@ $(SRCDIR)/demo.c $(SRCDIR)/start.S $(LDFLAGS)
	$(STRIP) $@
	python3 $(TRIM) $@ 4096 || true

clean:
	rm -f $(TARGETS)