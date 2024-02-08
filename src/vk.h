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

#endif
