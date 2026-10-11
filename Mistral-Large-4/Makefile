# MorphDemo — FROM NOISE TO ORDER
#
# Three self-contained real-time audiovisual demos as native Linux
# executables of exact sizes:
#
#   morphdemo-64k   65536 bytes   richest tier (96 particles, constellation,
#                                full score, 5-octave noise)
#   morphdemo-16k   16384 bytes   medium tier (64 particles, echo, hats)
#   morphdemo-4k     4096 bytes   minimal tier carried inside a tiny
#                                self-decompressing stub (memfd + execve)
#
# Build (on the reference machine, see ENVIRONMENT.json):
#
#   make            # builds all three final executables
#   make verify     # checks exact file sizes and ELF sanity
#   make tools      # builds the verification helpers in build/
#   make clean
#
# Only gcc, ld, make, python3 and coreutils are needed.  The demos link
# against the system runtime libraries (libX11, libGL, libpulse-simple),
# located through the system nix-ld library directory.

NIX_LD_LIB := /run/current-system/sw/share/nix-ld/lib
export LIBRARY_PATH := $(NIX_LD_LIB)

CC      := gcc
LD      := ld
PY      := python3

BASE    := $(CURDIR)
BUILD   := $(BASE)/build

# freestanding, size-optimized, non-PIE, single-RWX-segment link
CFLAGS  := -Os -std=gnu11 -ffreestanding -fno-stack-protector \
           -fno-asynchronous-unwind-tables -fno-ident -fomit-frame-pointer \
           -fno-pie -Wall
LDFLAGS := -nostdlib -nostartfiles -no-pie -Wl,--build-id=none \
           -Wl,-z,norelro -Wl,--hash-style=sysv -Wl,-z,max-page-size=4096 \
           -Wl,-rpath,$(NIX_LD_LIB)
LIBS    := -lX11 -lGL -lXrandr -lpulse-simple

DEMO_LDFLAGS := $(LDFLAGS) -T src/tiny.ld
STUB_LDFLAGS := $(LDFLAGS) -T src/stub.ld

.PHONY: all verify tools clean test4k dbg4k
all: morphdemo-64k morphdemo-16k morphdemo-4k

# ---- tier 2: 64 KiB (direct, padded to exact size) ----
$(BUILD)/demo2.elf: src/morphdemo.c src/minilib.h src/tiny.ld | $(BUILD)
	$(CC) $(CFLAGS) -DTIER=2 src/morphdemo.c -o $@ $(DEMO_LDFLAGS) $(LIBS)
$(BUILD)/demo2.stripped: $(BUILD)/demo2.elf
	$(PY) tools/sstrip.py $< $@
morphdemo-64k: $(BUILD)/demo2.stripped
	cp $< $@
	chmod +x $@
	truncate -s 65536 $@

# ---- tier 1: 16 KiB (direct, padded to exact size) ----
$(BUILD)/demo1.elf: src/morphdemo.c src/minilib.h src/tiny.ld | $(BUILD)
	$(CC) $(CFLAGS) -DTIER=1 src/morphdemo.c -o $@ $(DEMO_LDFLAGS) $(LIBS)
$(BUILD)/demo1.stripped: $(BUILD)/demo1.elf
	$(PY) tools/sstrip.py $< $@
morphdemo-16k: $(BUILD)/demo1.stripped
	cp $< $@
	chmod +x $@
	truncate -s 16384 $@

# ---- tier 0: 4 KiB (stub + LZSS-compressed demo4k, padded) ----
# demo4k resolves every library function at runtime with dlsym (see
# src/minilib.h, -DMINI), so it needs only one dynamic symbol (dlsym).
# It is compiled with -fno-plt (the dlsym call goes through one GLOB_DAT
# entry instead of a PLT) and linked directly with ld, not the gcc driver:
# /lib64 interpreter (see ENVIRONMENT.json — resolves the libraries by
# soname without any RPATH), three program headers (src/tiny4k.ld), no
# RUNPATH, sysv hash.  -Oz is used for this tier only (the 64k/16k tiers
# keep -Os).  The NIX_DONT_SET_RPATH_* variable stops the nix ld wrapper
# from injecting an -rpath for the -L directory.
$(BUILD)/demo4k.o: src/demo4k.c src/minilib.h | $(BUILD)
	$(CC) $(CFLAGS) -Oz -fno-plt -DMINI -c src/demo4k.c -o $@
$(BUILD)/demo4k.elf: $(BUILD)/demo4k.o src/tiny4k.ld
	NIX_DONT_SET_RPATH_x86_64_unknown_linux_gnu=1 $(LD) -o $@ \
	    $(BUILD)/demo4k.o \
	    --dynamic-linker /lib64/ld-linux-x86-64.so.2 -T src/tiny4k.ld \
	    -L$(NIX_LD_LIB) --no-as-needed -lX11 -lGL -lXrandr -lpulse-simple \
	    -l:libc.so.6 --hash-style=sysv -z norelro -z max-page-size=4096 \
	    --build-id=none --allow-shlib-undefined
$(BUILD)/demo4k.stripped: $(BUILD)/demo4k.elf
	$(PY) tools/sstrip.py $< $@
$(BUILD)/payload.bin: $(BUILD)/demo4k.stripped
	$(PY) tools/lzpack.py $< $@
$(BUILD)/payload.o: $(BUILD)/payload.bin
	cd $(BUILD) && $(LD) -r -b binary -o payload.o payload.bin
$(BUILD)/stub.elf: src/stub.c src/stub.ld $(BUILD)/payload.o
	$(CC) $(CFLAGS) -Oz src/stub.c $(BUILD)/payload.o -o $@ $(STUB_LDFLAGS)
$(BUILD)/stub.stripped: $(BUILD)/stub.elf
	$(PY) tools/sstrip.py $< $@
morphdemo-4k: $(BUILD)/stub.stripped
	@sz=$$(stat -c%s $(BUILD)/stub.stripped); \
	if [ $$sz -gt 4096 ]; then \
	    echo "FAIL: stub.stripped is $$sz bytes (> 4096) — payload too big"; \
	    exit 1; \
	fi
	cp $(BUILD)/stub.stripped $@
	chmod +x $@
	truncate -s 4096 $@

# ---- 4k test variants: same chain with -DDUR=8, run through the stub
# (validates memfd+decompress+execveat end to end).  Each variant gets
# its own build/<name>/ directory so the release chain is untouched.
#   make test4k   release code path (glCreateShaderProgramv)
#   make dbg4k    adds -DDBG: logs shader compile/link errors
$(BUILD)/rel8:
	mkdir -p $(BUILD)/rel8
$(BUILD)/d4krel8.o: src/demo4k.c src/minilib.h | $(BUILD)
	$(CC) $(CFLAGS) -Oz -fno-plt -DMINI -DDUR=8 -c src/demo4k.c -o $@
$(BUILD)/d4krel8.elf: $(BUILD)/d4krel8.o src/tiny4k.ld
	NIX_DONT_SET_RPATH_x86_64_unknown_linux_gnu=1 $(LD) -o $@ \
	    $(BUILD)/d4krel8.o \
	    --dynamic-linker /lib64/ld-linux-x86-64.so.2 -T src/tiny4k.ld \
	    -L$(NIX_LD_LIB) --no-as-needed -lX11 -lGL -lXrandr -lpulse-simple \
	    -l:libc.so.6 --hash-style=sysv -z norelro -z max-page-size=4096 \
	    --build-id=none --allow-shlib-undefined
$(BUILD)/d4krel8.stripped: $(BUILD)/d4krel8.elf
	$(PY) tools/sstrip.py $< $@
$(BUILD)/rel8/payload.bin: $(BUILD)/d4krel8.stripped | $(BUILD)/rel8
	$(PY) tools/lzpack.py $< $@
$(BUILD)/rel8/payload.o: $(BUILD)/rel8/payload.bin
	cd $(BUILD)/rel8 && $(LD) -r -b binary -o payload.o payload.bin
$(BUILD)/stubrel8.elf: src/stub.c src/stub.ld $(BUILD)/rel8/payload.o
	$(CC) $(CFLAGS) -Oz src/stub.c $(BUILD)/rel8/payload.o -o $@ $(STUB_LDFLAGS)
$(BUILD)/stubrel8.stripped: $(BUILD)/stubrel8.elf
	$(PY) tools/sstrip.py $< $@
	chmod +x $@
test4k: $(BUILD)/stubrel8.stripped

$(BUILD)/dbg:
	mkdir -p $(BUILD)/dbg
$(BUILD)/d4kdbg.o: src/demo4k.c src/minilib.h | $(BUILD)
	$(CC) $(CFLAGS) -Oz -fno-plt -DMINI -DDBG -DDUR=8 -c src/demo4k.c -o $@
$(BUILD)/d4kdbg.elf: $(BUILD)/d4kdbg.o src/tiny4k.ld
	NIX_DONT_SET_RPATH_x86_64_unknown_linux_gnu=1 $(LD) -o $@ \
	    $(BUILD)/d4kdbg.o \
	    --dynamic-linker /lib64/ld-linux-x86-64.so.2 -T src/tiny4k.ld \
	    -L$(NIX_LD_LIB) --no-as-needed -lX11 -lGL -lXrandr -lpulse-simple \
	    -l:libc.so.6 --hash-style=sysv -z norelro -z max-page-size=4096 \
	    --build-id=none --allow-shlib-undefined
$(BUILD)/d4kdbg.stripped: $(BUILD)/d4kdbg.elf
	$(PY) tools/sstrip.py $< $@
$(BUILD)/dbg/payload.bin: $(BUILD)/d4kdbg.stripped | $(BUILD)/dbg
	$(PY) tools/lzpack.py $< $@
$(BUILD)/dbg/payload.o: $(BUILD)/dbg/payload.bin
	cd $(BUILD)/dbg && $(LD) -r -b binary -o payload.o payload.bin
$(BUILD)/stubdbg.elf: src/stub.c src/stub.ld $(BUILD)/dbg/payload.o
	$(CC) $(CFLAGS) -Oz src/stub.c $(BUILD)/dbg/payload.o -o $@ $(STUB_LDFLAGS)
$(BUILD)/stubdbg.stripped: $(BUILD)/stubdbg.elf
	$(PY) tools/sstrip.py $< $@
	chmod +x $@
dbg4k: $(BUILD)/stubdbg.stripped

$(BUILD):
	mkdir -p $(BUILD)

# ---- verification helpers ----
# (they use the real X11 headers; locate the dev outputs in the nix store)
X11_INC  := $(firstword $(wildcard /nix/store/*-libx11-*-dev/include))
XORG_INC := $(firstword $(wildcard /nix/store/*-xorgproto-*/include))
TOOLS_CFLAGS := -O2 -Wall -I$(X11_INC) -I$(XORG_INC)
tools: $(BUILD)/shot $(BUILD)/sendkey
$(BUILD)/shot: tools/shot.c | $(BUILD)
	$(CC) $(TOOLS_CFLAGS) tools/shot.c -o $@ -lX11
$(BUILD)/sendkey: tools/sendkey.c | $(BUILD)
	$(CC) $(TOOLS_CFLAGS) tools/sendkey.c -o $@ -lX11

# ---- verification ----
verify: all
	@for f in morphdemo-64k:65536 morphdemo-16k:16384 morphdemo-4k:4096; do \
		bin=$${f%%:*}; want=$${f##*:}; \
		got=$$(stat -c%s $$bin); \
		if [ "$$got" = "$$want" ]; then \
			echo "OK   $$bin  $$got bytes"; \
		else \
			echo "FAIL $$bin  got $$got, want $$want"; exit 1; \
		fi; \
	done
	@for b in morphdemo-64k morphdemo-16k morphdemo-4k; do \
		readelf -h $$b | grep -q 'Type:.*EXEC' && echo "OK   $$b is ELF EXEC" || { echo "FAIL $$b not EXEC"; exit 1; }; \
	done

clean:
	rm -rf $(BUILD) morphdemo-64k morphdemo-16k morphdemo-4k
