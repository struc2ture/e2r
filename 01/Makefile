CFLAGS = -g -I/opt/homebrew/include -I/usr/local/include -I../../../shared/stb
LFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lglfw -lvulkan

export VK_ICD_FILENAMES = /usr/local/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH = /usr/local/share/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH = /usr/local/lib:$DYLD_LIBRARY_PATH

debug: bin/test bin/shaders/tri.vert.spv bin/shaders/tri.frag.spv
	lldb bin/test -o run

bin/test: $(wildcard src/*) bin/common.o
	clang $(CFLAGS) src/main.c src/e2r_core.c bin/common.o -o bin/test $(LFLAGS)

bin/common.o: $(wildcard src/common/*)
	clang -c $(CFLAGS) src/common/common.c -o bin/common.o

bin/shaders/tri.vert.spv: src/shaders/tri.vert
	glslc $< -o $@

bin/shaders/tri.frag.spv: src/shaders/tri.frag
	glslc $< -o $@
