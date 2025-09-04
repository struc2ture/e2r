CFLAGS = -g -I/opt/homebrew/include -I/usr/local/include -I../../../shared/stb
LFLAGS = -L/opt/homebrew/lib -L/usr/local/lib -lglfw -lvulkan

export VK_ICD_FILENAMES = /usr/local/share/vulkan/icd.d/MoltenVK_icd.json
export VK_LAYER_PATH = /usr/local/share/vulkan/explicit_layer.d
export DYLD_LIBRARY_PATH = /usr/local/lib:$DYLD_LIBRARY_PATH

debug: bin/test
	lldb bin/test -o run

bin/test: $(wildcard src/*)
	clang $(CFLAGS) src/main.c src/e2r_core.c -o bin/test $(LFLAGS)