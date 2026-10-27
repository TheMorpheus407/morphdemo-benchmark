CC := gcc
LD := ld
PYTHON := python3
LIBDIR := /run/current-system/sw/share/nix-ld/lib
CLEAN_LD = NIX_DONT_SET_RPATH_x86_64_unknown_linux_gnu=1 $(LD)
COMMON := -ffunction-sections -fdata-sections -fno-ident -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-unwind-tables -fno-pie -fcf-protection=none -fomit-frame-pointer
FULL_CFLAGS := -std=c11 -O2 -Wall -Wextra $(COMMON)
PACK_CFLAGS := -std=c11 -Os $(COMMON)
DYNAMIC_FLAGS := -z noseparate-code --no-eh-frame-hdr --gc-sections --build-id=none --as-needed -rpath-link=$(LIBDIR) --dynamic-linker=/lib64/ld-linux-x86-64.so.2

.PHONY: all payloads clean
.DELETE_ON_ERROR:
all: morphdemo-64k morphdemo-16k morphdemo-4k
payloads: build/64k/payload build/16k/payload build/4k/payload

build/64k/shader.h: src/shaders/crystal.glsl tools/embed.py
	@mkdir -p $(@D)
	$(PYTHON) tools/embed.py --minify $< $@

build/16k/shader.h: src/shaders/bloom.glsl tools/embed.py
	@mkdir -p $(@D)
	$(PYTHON) tools/embed.py --minify $< $@

build/4k/shader.h: src/shaders/interference.glsl tools/embed.py
	@mkdir -p $(@D)
	$(PYTHON) tools/embed.py --minify $< $@

define COMPILE_FULL
build/$(1)/main.o: src/main.c src/platform.h src/audio.h build/$(1)/shader.h
	$(CC) $(FULL_CFLAGS) -DDEMO=$(2) -Ibuild/$(1) -Isrc -c $$< -o $$@
build/$(1)/audio.o: src/audio.c src/audio.h src/platform.h
	@mkdir -p $$(@D)
	$(CC) $(FULL_CFLAGS) -DDEMO=$(2) -Isrc -c $$< -o $$@
build/$(1)/start.o: src/start.S
	@mkdir -p $$(@D)
	$(CC) -c -fno-pie -fcf-protection=none $$< -o $$@
endef

$(eval $(call COMPILE_FULL,64k,0))
$(eval $(call COMPILE_FULL,16k,1))

build/64k/payload: build/64k/main.o build/64k/audio.o
	$(CC) -no-pie -L$(LIBDIR) -Wl,--gc-sections -Wl,--build-id=none -Wl,--as-needed -Wl,--dynamic-linker=/lib64/ld-linux-x86-64.so.2 $^ -lSDL2 -lm -o $@
	strip --strip-all $@
	patchelf --remove-rpath $@

build/16k/payload: build/16k/start.o build/16k/main.o build/16k/audio.o
	$(CLEAN_LD) $(DYNAMIC_FLAGS) -o $@ $^ $(LIBDIR)/libSDL2-2.0.so.0 $(LIBDIR)/libm.so.6 $(LIBDIR)/libc.so.6
	objcopy --remove-section=.note.gnu.property $@
	strip --strip-all $@
	$(PYTHON) tools/sstrip.py $@

build/4k/tiny.o: src/tiny.c src/platform.h build/4k/shader.h
	$(CC) $(PACK_CFLAGS) -Ibuild/4k -Isrc -c $< -o $@

build/4k/start.o: src/start.S
	@mkdir -p $(@D)
	$(CC) -c -fno-pie -fcf-protection=none $< -o $@

build/4k/payload: build/4k/start.o build/4k/tiny.o
	$(CLEAN_LD) $(DYNAMIC_FLAGS) -o $@ $^ $(LIBDIR)/libSDL2-2.0.so.0 $(LIBDIR)/libc.so.6
	objcopy --remove-section=.note.gnu.property $@
	strip --strip-all $@
	$(PYTHON) tools/sstrip.py $@

build/%/packed.h: build/%/payload tools/pack.py tools/lzr.py
	$(PYTHON) tools/pack.py $< $@ build/$*/payload.lzr

build/%/stub.o: src/packstub.c build/%/packed.h
	$(CC) $(PACK_CFLAGS) -Ibuild/$* -c $< -o $@
	objcopy --remove-section=.note.gnu.property $@

build/%/packed: build/%/stub.o src/packstub.ld tools/sstrip.py
	$(LD) -T src/packstub.ld --gc-sections --build-id=none --no-eh-frame-hdr -o $@ $<
	strip --strip-all $@
	$(PYTHON) tools/sstrip.py $@

morphdemo-64k: build/64k/payload tools/pad.py
	$(PYTHON) tools/pad.py $< $@ 65536

morphdemo-16k: build/16k/packed tools/pad.py
	$(PYTHON) tools/pad.py $< $@ 16384

morphdemo-4k: build/4k/packed tools/pad.py
	$(PYTHON) tools/pad.py $< $@ 4096

clean:
	rm -rf build morphdemo-64k morphdemo-16k morphdemo-4k
