#include "vertex.h"

#include <stdlib.h>

VkVertexInputBindingDescription getVertexBindingDescription(void) {
  VkVertexInputBindingDescription bindingDescription = {0};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(struct Vertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return bindingDescription;
}

uint32_t getVertexAttributeDescriptionCount(void) { return 1; }

VkVertexInputAttributeDescription *getVertexAttributeDescriptions(void) {
  VkVertexInputAttributeDescription *attributeDescriptions =
      malloc(sizeof(VkVertexInputAttributeDescription) *
             getVertexAttributeDescriptionCount());

  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(struct Vertex, pos);

  return attributeDescriptions;
}
