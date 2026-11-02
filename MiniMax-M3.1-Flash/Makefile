# MorphDemo -- build the three size constrained audiovisual demos.
#
#   make            build all three
#   make check      build and verify the exact file sizes
#
# The binaries are plain dynamically linked ELF executables.  They use only
# libX11, libGL, libasound and libc, all of which are already present in the
# supplied environment.

CC      ?= gcc
PYTHON  ?= python3
PATCHELF?= patchelf
SIZE    ?= size

SRC     := src
BUILD   := build

include tools/env.mk

CFLAGS  := -Os -no-pie -fno-plt -fno-asynchronous-unwind-tables -fno-stack-protector \
           -fomit-frame-pointer -fno-math-errno -ffunction-sections -fdata-sections \
           -fno-plt -Wall -Wextra -Wno-unused-parameter
LDLIBS  := $(LIBX11) $(LIBGL) $(LIBALSA) -L$(GLIBCDIR)
export NIX_DONT_SET_RPATH_x86_64_unknown_linux_gnu := 1
LDFLAGS := -Wl,--build-id=none -Wl,--hash-style=gnu -Wl,--allow-shlib-undefined \
           -Wl,--no-as-needed -Wl,--gc-sections -Wl,-rpath,$(RUNPATH)

ALL_CFLAGS := $(CFLAGS) -I$(SRC)

TARGETS := morphdemo-64k morphdemo-16k morphdemo-4k

# `all` is the default goal; the build directory rule below would otherwise
# become it, because it comes first in the file
.DEFAULT_GOAL := all

# the build directory is created by the rule below, not by a shell in the
# environment, so that a clean build always works
$(BUILD):
	mkdir -p $(BUILD)

.PHONY: all check clean
all: $(TARGETS)

tools/env.mk: tools/env.sh
	@sh tools/env.sh > $@

# ---------------------------------------------------------------- 64 KiB --
$(BUILD)/demo64.o: | $(BUILD)
$(BUILD)/demo64.o: $(SRC)/demo64.c $(SRC)/platform.c $(SRC)/platform.h $(SRC)/symtab.h \
                   $(SRC)/syms_common.h $(SRC)/fmath.h $(SRC)/glconst.h $(SRC)/x11min.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD)/plat64.o: | $(BUILD)
$(BUILD)/plat64.o: $(SRC)/platform.c $(SRC)/platform.h $(SRC)/symtab.h $(SRC)/syms_common.h \
                   $(SRC)/x11min.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD)/start64.o: | $(BUILD)
$(BUILD)/start64.o: $(SRC)/start.s
	$(CC) -c $< -o $@

morphdemo-64k: $(BUILD)/demo64.o $(BUILD)/plat64.o $(BUILD)/start64.o $(SRC)/compact.ld
	$(CC) -nostartfiles -no-pie -T $(SRC)/compact.ld $(BUILD)/demo64.o $(BUILD)/plat64.o \
	    $(BUILD)/start64.o -o $@.elf $(LDFLAGS) $(LDLIBS) -lc
	strip -s $@.elf
	$(SIZE) $@.elf || true
	$(PYTHON) tools/exact_size.py $@.elf $@ 65536

# ---------------------------------------------------------------- 16 KiB --
$(BUILD)/demo16.o: | $(BUILD)
$(BUILD)/demo16.o: $(SRC)/demo16.c $(SRC)/platform.c $(SRC)/platform.h $(SRC)/symtab.h \
                   $(SRC)/syms_common.h $(SRC)/fmath.h $(SRC)/glconst.h $(SRC)/x11min.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD)/plat16.o: | $(BUILD)
$(BUILD)/plat16.o: $(SRC)/platform.c $(SRC)/platform.h $(SRC)/symtab.h $(SRC)/syms_common.h \
                   $(SRC)/x11min.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD)/start16.o: | $(BUILD)
$(BUILD)/start16.o: $(SRC)/start.s
	$(CC) -c $< -o $@

morphdemo-16k: $(BUILD)/demo16.o $(BUILD)/plat16.o $(BUILD)/start16.o $(SRC)/compact.ld
	$(CC) -nostartfiles -no-pie -T $(SRC)/compact.ld $(BUILD)/demo16.o $(BUILD)/plat16.o \
	    $(BUILD)/start16.o -o $@.elf $(LDFLAGS) $(LDLIBS) -lc
	strip -s $@.elf
	$(SIZE) $@.elf || true
	$(PYTHON) tools/exact_size.py $@.elf $@ 16384

# ----------------------------------------------------------------- 4 KiB --
# The four kilobyte binary is a self extracting wrapper: it carries the
# production in compressed form, unpacks it into an anonymous in memory file
# at run time and executes that file.  The specification allows this.
$(BUILD)/demo4.o: | $(BUILD)
$(BUILD)/demo4.o: $(SRC)/demo4.c $(SRC)/platform.h $(SRC)/symtab.h \
                  $(SRC)/syms_common.h $(SRC)/glconst.h $(SRC)/x11min.h
	$(CC) $(ALL_CFLAGS) -c $< -o $@

$(BUILD)/start.o: | $(BUILD)
$(BUILD)/start.o: $(SRC)/start.s
	$(CC) -c $< -o $@

# the production on its own, as a normal dynamically linked program
$(BUILD)/demo4_inner.elf: | $(BUILD)
$(BUILD)/demo4_inner.elf: $(BUILD)/demo4.o $(BUILD)/start.o $(SRC)/tiny.ld
	$(CC) -nostartfiles -no-pie -T $(SRC)/tiny.ld $(BUILD)/demo4.o $(BUILD)/start.o -o $@ \
	    -Wl,--dynamic-linker,/lib64/ld-linux-x86-64.so.2 $(LDFLAGS) $(LDLIBS) -lc
	strip -s $@

$(BUILD)/demo4_payload.bin: | $(BUILD)
$(BUILD)/demo4_payload.bin: $(BUILD)/demo4_inner.elf $(SRC)/lzpack.py tools/exact_size.py
	$(PYTHON) tools/exact_size.py s $< $(BUILD)/demo4_inner.bin
	$(PYTHON) $(SRC)/lzpack.py c $(BUILD)/demo4_inner.bin $@

$(BUILD)/stub.o: | $(BUILD)
$(BUILD)/stub.o: $(SRC)/stub.c $(BUILD)/demo4_payload.bin
	$(CC) -Os -no-pie -ffreestanding -fno-builtin -fno-stack-protector \
	      -fomit-frame-pointer -fno-asynchronous-unwind-tables \
	      -fno-tree-loop-distribute-patterns -I $(BUILD) -c $< -o $@

$(BUILD)/stub_start.o: | $(BUILD)
$(BUILD)/stub_start.o: $(SRC)/stub_start.s $(BUILD)/demo4_payload.bin
	$(CC) -c -I $(BUILD) $< -o $@

morphdemo-4k: $(BUILD)/stub.o $(BUILD)/stub_start.o $(SRC)/stub.ld
	$(CC) -nostdlib -no-pie -T $(SRC)/stub.ld $(BUILD)/stub.o $(BUILD)/stub_start.o -o $@.elf
	strip -s $@.elf
	$(SIZE) $@.elf || true
	$(PYTHON) tools/exact_size.py $@.elf $@ 4096

# ----------------------------------------------------------------- check --
check: all
	@for f in $(TARGETS); do \
	    s=$$(stat -c%s $$f); \
	    case $$f in \
	      morphdemo-64k) want=65536;; \
	      morphdemo-16k) want=16384;; \
	      morphdemo-4k)  want=4096;; \
	    esac; \
	    if [ $$s -eq $$want ]; then echo "OK   $$f $$s bytes"; \
	    else echo "FAIL $$f is $$s bytes, expected $$want"; exit 1; fi; \
	done

clean:
	$(PYTHON) tools/clean.py
