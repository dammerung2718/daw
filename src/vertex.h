#ifndef VERTEX_H
#define VERTEX_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Vec2 {
  float x, y;
};

struct Vertex {
  struct Vec2 pos;
};

VkVertexInputBindingDescription getVertexBindingDescription(void);
uint32_t getVertexAttributeDescriptionCount(void);
VkVertexInputAttributeDescription *getVertexAttributeDescriptions(void);

#endif
