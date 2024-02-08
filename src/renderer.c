#include "renderer.h"

#include "die.h"
#include "vk.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct Renderer {
  // window
  char *title;
  int width, height;
  GLFWwindow *window;

  // vulkan
  VkInstance instance;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  int queueFamilyIndex; // on macOS, this supports both graphics and
                        // presentation
  VkDevice device;
  VkQueue queue;

  // swapchain
  struct SwapchainSettings swapchainSettings;
  VkSwapchainKHR swapchain;
  VkImage *swapchainImages;

  // swapchain image views
  VkImageView *imageViews;

  // shaders
  VkShaderModule vertShader;
  VkShaderModule fragShader;
};

Renderer makeRenderer(char *title, int width, int height) {
  Renderer r = malloc(sizeof(struct Renderer));
  r->title = title;
  r->width = width;
  r->height = height;

  // init windowing lib
  if (glfwInit() != GLFW_TRUE) {
    die("Failed to initialize GLFW\n");
  }

  // check for vulkan support
  if (glfwVulkanSupported() != GLFW_TRUE) {
    die("Vulkan not supported\n");
  }

  // create window
  glfwWindowHint(GLFW_CLIENT_API,
                 GLFW_NO_API); // don't create an OpenGL context
  glfwWindowHint(GLFW_RESIZABLE,
                 GLFW_FALSE); // resizing breaks vulkan for now, so disable it
  r->window = glfwCreateWindow(r->width, r->height, r->title, NULL, NULL);
  if (!r->window) {
    glfwTerminate();
    die("Failed to create window\n");
  }

  // vulkan
  r->instance = makeVkInstance(r->title);
  r->surface = makeVkSurface(r->instance, r->window);
  r->physicalDevice = pickVkPhysicalDevice(r->instance);
  r->queueFamilyIndex = findVkQueueFamilyIndex(r->physicalDevice, r->surface);
  r->device = makeVkDevice(r->physicalDevice, r->queueFamilyIndex);
  vkGetDeviceQueue(r->device, r->queueFamilyIndex, 0, &r->queue);

  // swapchain
  r->swapchainSettings =
      makeSwapchainSettings(r->physicalDevice, r->surface, r->width, r->height);
  r->swapchain = makeVkSwapchain(r->swapchainSettings, r->device, r->surface);
  r->swapchainImages =
      getVkSwapchainImages(r->device, r->swapchainSettings, r->swapchain);
  r->imageViews =
      makeVkImageViews(r->device, r->swapchainSettings, r->swapchainImages);

  // shaders
  r->vertShader = makeVkShaderModule(r->device, "bin/vert.spv");
  r->fragShader = makeVkShaderModule(r->device, "bin/frag.spv");

  // return
  return r;
}

void mainLoop(Renderer r) {
  // main loop
  while (!glfwWindowShouldClose(r->window)) {
    glfwPollEvents(); // TODO: event handling
  }
}

void freeRenderer(Renderer r) {
  // shaders
  vkDestroyShaderModule(r->device, r->fragShader, NULL);
  vkDestroyShaderModule(r->device, r->vertShader, NULL);

  // swapchain
  for (uint32_t i = 0; i < r->swapchainSettings.imageCount; i++) {
    vkDestroyImageView(r->device, r->imageViews[i], NULL);
  }
  vkDestroySwapchainKHR(r->device, r->swapchain, NULL);

  // vulkan
  vkDestroyDevice(r->device, NULL);
  vkDestroySurfaceKHR(r->instance, r->surface, NULL);
  vkDestroyInstance(r->instance, NULL);

  // window
  glfwDestroyWindow(r->window);
  glfwTerminate();
}
