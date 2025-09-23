CFLAGS = -g -I/opt/homebrew/include -I/usr/local/include -I../../../shared/stb
CFLAGS += -I/Users/struc/dev/jects/font-loader/out
LFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lglfw -lvulkan
LFLAGS += -L/Users/struc/dev/jects/font-loader/out -lfont_loader

SHADERS = ui.vert ui.frag cubes.vert cubes.frag
SHADER_SPV_NAMES = $(addsuffix .spv, $(addprefix bin/shaders/, $(SHADERS)))

export VK_ICD_FILENAMES = /usr/local/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH = /usr/local/share/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH = /usr/local/lib:/Users/struc/dev/jects/font-loader/out:$$DYLD_LIBRARY_PATH

debug: bin/test 
	lldb bin/test -o run

bin/test: $(wildcard src/*) bin/common.o $(SHADER_SPV_NAMES)
	clang $(CFLAGS) src/main.c src/e2r_core.c src/e2r_camera.c src/e2r_draw.c src/e2r_ui.c src/e2r_input.c bin/common.o -o bin/test $(LFLAGS)

bin/common.o: $(wildcard src/common/*)
	clang -c $(CFLAGS) src/common/common.c -o bin/common.o

bin/shaders/%.spv: src/shaders/%
	glslc $< -o $@
