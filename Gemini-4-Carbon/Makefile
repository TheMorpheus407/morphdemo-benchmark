CC       := gcc
PATCHELF := patchelf
STRIP    := strip
PYTHON   := python3 -B

COMMON_CFLAGS := \
	-Oz -fomit-frame-pointer -fno-plt -fno-pie -no-pie -nostdlib \
	-fno-asynchronous-unwind-tables -fno-unwind-tables -fno-ident \
	-fno-stack-protector -fcf-protection=none -fno-math-errno -fno-trapping-math \
	-Wa,--generate-missing-build-notes=no \
	-Ibuild -Isrc/common

COMMON_LDFLAGS := \
	-Wl,--gc-sections -Wl,--no-eh-frame-hdr -Wl,-z,norelro \
	-Wl,--build-id=none -Wl,--hash-style=sysv -Wl,-z,noseparate-code \
	-Wl,-z,max-page-size=0x1000 \
	-Wl,--dynamic-linker,/lib64/ld-linux-x86-64.so.2

STRIP_FLAGS := \
	-s -R .comment -R .gnu.version -R .note.ABI-tag \
	-R .note.gnu.property -R .eh_frame -R .eh_frame_hdr

STUB_DEPS := src/common/stub.S src/common/stub.ld tools/pack.py

.PHONY: all clean

all: morphdemo-4k morphdemo-16k morphdemo-64k

build/demo4k_shaders.h: src/demo4k/scene.frag tools/pack.py
	@mkdir -p build
	$(PYTHON) tools/pack.py minify-shaders $@ \
		SHADER_SCENE_4K=src/demo4k/scene.frag

morphdemo-4k: src/demo4k/main.c src/common/platform.h build/demo4k_shaders.h $(STUB_DEPS)
	$(CC) $(COMMON_CFLAGS) $(COMMON_LDFLAGS) src/demo4k/main.c -lc -o build/demo4k.inner
	$(PATCHELF) --remove-rpath build/demo4k.inner
	$(STRIP) $(STRIP_FLAGS) build/demo4k.inner
	$(PYTHON) tools/pack.py pack build/demo4k.inner src/common/stub.S src/common/stub.ld $@ 4096

build/demo16k_shaders.h: src/demo16k/scene.frag src/demo16k/post.frag tools/pack.py
	@mkdir -p build
	$(PYTHON) tools/pack.py minify-shaders $@ \
		SHADER_SCENE_16K=src/demo16k/scene.frag \
		SHADER_POST_16K=src/demo16k/post.frag

morphdemo-16k: src/demo16k/main.c src/common/platform.h build/demo16k_shaders.h $(STUB_DEPS)
	$(CC) $(COMMON_CFLAGS) $(COMMON_LDFLAGS) src/demo16k/main.c -lc -lm -o build/demo16k.inner
	$(PATCHELF) --remove-rpath build/demo16k.inner
	$(STRIP) $(STRIP_FLAGS) build/demo16k.inner
	$(PYTHON) tools/pack.py pack build/demo16k.inner src/common/stub.S src/common/stub.ld $@ 16384

build/demo64k_shaders.h: src/demo64k/quad.vert src/demo64k/sim.frag src/demo64k/scene.frag src/demo64k/particles.vert src/demo64k/particles.frag src/demo64k/bloom.frag src/demo64k/composite.frag tools/pack.py
	@mkdir -p build
	$(PYTHON) tools/pack.py minify-shaders $@ \
		SHADER_QUAD_VERT=src/demo64k/quad.vert \
		SHADER_SIM_64K=src/demo64k/sim.frag \
		SHADER_SCENE_64K=src/demo64k/scene.frag \
		SHADER_PART_VERT:vCol,vUv=src/demo64k/particles.vert \
		SHADER_PART_FRAG:vCol,vUv=src/demo64k/particles.frag \
		SHADER_BLOOM_64K=src/demo64k/bloom.frag \
		SHADER_COMP_64K=src/demo64k/composite.frag

morphdemo-64k: src/demo64k/main.c src/demo64k/synth.h src/common/platform.h build/demo64k_shaders.h $(STUB_DEPS)
	$(CC) $(COMMON_CFLAGS) $(COMMON_LDFLAGS) src/demo64k/main.c -lc -lm -o build/demo64k.inner
	$(PATCHELF) --remove-rpath build/demo64k.inner
	$(STRIP) $(STRIP_FLAGS) build/demo64k.inner
	$(PYTHON) tools/pack.py pack build/demo64k.inner src/common/stub.S src/common/stub.ld $@ 65536

clean:
	rm -rf build morphdemo-4k morphdemo-16k morphdemo-64k
