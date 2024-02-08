#ifndef VK_H
#define VK_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct SwapchainSettings {
  uint32_t imageCount;
  VkSurfaceTransformFlagBitsKHR currentTransform;
  VkSurfaceFormatKHR selectedFormat;
  VkPresentModeKHR selectedPresentMode;
  VkExtent2D selectedExtent;
};

struct SyncObjects {
  VkSemaphore imageAvailable;
  VkSemaphore renderFinished;
  VkFence inFlight;
};

VkInstance makeVkInstance(char *appName);
VkSurfaceKHR makeVkSurface(VkInstance instance, GLFWwindow *window);
VkPhysicalDevice pickVkPhysicalDevice(VkInstance instance);
int findVkQueueFamilyIndex(VkPhysicalDevice device, VkSurfaceKHR surface);
VkDevice makeVkDevice(VkPhysicalDevice physicalDevice, int queueFamilyIndex);

struct SwapchainSettings makeSwapchainSettings(VkPhysicalDevice physicalDevice,
                                               VkSurfaceKHR surface, int width,
                                               int height);
VkSwapchainKHR makeVkSwapchain(struct SwapchainSettings settings,
                               VkDevice device, VkSurfaceKHR surface);
VkImage *getVkSwapchainImages(VkDevice device,
                              struct SwapchainSettings settings,
                              VkSwapchainKHR swapchain);

VkImageView *makeVkImageViews(VkDevice device,
                              struct SwapchainSettings settings,
                              VkImage *images);

VkShaderModule makeVkShaderModule(VkDevice device, char *path);
VkRenderPass makeVkRenderPass(VkDevice device,
                              struct SwapchainSettings settings);
VkPipelineLayout makeVkPipelineLayout(VkDevice device);
VkPipeline makeVkPipeline(VkDevice device, struct SwapchainSettings settings,
                          VkShaderModule vert, VkShaderModule frag,
                          VkRenderPass renderPass,
                          VkPipelineLayout pipelineLayout);

VkFramebuffer *makeVkFramebuffers(VkDevice device,
                                  struct SwapchainSettings settings,
                                  VkImageView *imageViews,
                                  VkRenderPass renderPass);
VkCommandPool makeVkCommandPool(VkDevice device, int queueFamilyIndex);
VkCommandBuffer makeVkCommandBuffer(VkDevice device, VkCommandPool commandPool);

struct SyncObjects makeVkSyncObjects(VkDevice device);

#endif
