VULKAN_SDK_PATH = $$HOME/VulkanSDK/1.3.275.0/macOS

CFLAGS += -Wall -Wextra -pedantic -Werror    \
					-std=c11 -g3 -O0                   \
					`pkg-config --cflags --libs glfw3` \
					-I$(VULKAN_SDK_PATH)/include       \
					-L$(VULKAN_SDK_PATH)/lib           \
					-lvulkan
.PHONY: run
run: bin/vert.spv bin/frag.spv bin/daw
	DYLD_LIBRARY_PATH=$(VULKAN_SDK_PATH)/lib bin/daw

.PHONY: clean
clean:
	rm -rf bin

bin/daw: src/main.c src/renderer.c src/vk.c src/vertex.c
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

bin/vert.spv: assets/shader.vert
	$(VULKAN_SDK_PATH)/bin/glslc -o $@ $^

bin/frag.spv: assets/shader.frag
	$(VULKAN_SDK_PATH)/bin/glslc -o $@ $^
