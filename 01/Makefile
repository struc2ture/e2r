CFLAGS = -g -I/opt/homebrew/include -I/usr/local/include -I../../../shared/stb
CFLAGS += -I/Users/struc/dev/jects/font-loader/out
LFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lglfw -lvulkan
LFLAGS += -L/Users/struc/dev/jects/font-loader/out -lfont_loader

SHADERS = bin/shaders/tri.vert.spv bin/shaders/tri.frag.spv
SHADERS += bin/shaders/cubes.vert.spv bin/shaders/cubes.frag.spv
SHADERS += bin/shaders/ui.vert.spv bin/shaders/ui.frag.spv
SHADERS += bin/shaders/text.vert.spv bin/shaders/text.frag.spv

export VK_ICD_FILENAMES = /usr/local/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH = /usr/local/share/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH = /usr/local/lib:/Users/struc/dev/jects/font-loader/out:$$DYLD_LIBRARY_PATH
# export DYLD_PRINT_LIBRARIES = 1

debug: bin/test 
	lldb bin/test -o run

bin/test: $(wildcard src/*) bin/common.o $(SHADERS)
	clang $(CFLAGS) src/main.c src/e2r_core.c src/e2r_camera.c src/e2r_draw.c src/e2r_ui.c bin/common.o -o bin/test $(LFLAGS)

bin/common.o: $(wildcard src/common/*)
	clang -c $(CFLAGS) src/common/common.c -o bin/common.o

bin/shaders/tri.vert.spv: src/shaders/tri.vert
	glslc $< -o $@

bin/shaders/tri.frag.spv: src/shaders/tri.frag
	glslc $< -o $@

bin/shaders/ui.vert.spv: src/shaders/ui.vert
	glslc $< -o $@

bin/shaders/ui.frag.spv: src/shaders/ui.frag
	glslc $< -o $@

bin/shaders/text.vert.spv: src/shaders/text.vert
	glslc $< -o $@

bin/shaders/text.frag.spv: src/shaders/text.frag
	glslc $< -o $@

bin/shaders/cubes.vert.spv: src/shaders/cubes.vert
	glslc $< -o $@

bin/shaders/cubes.frag.spv: src/shaders/cubes.frag
	glslc $< -o $@
