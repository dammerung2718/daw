#include "vk.h"

#include "die.h"
#include "vulkan/vulkan_core.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// CONFIG

#define VALIDATION_LAYERS 1
const char *validationLayers[VALIDATION_LAYERS] = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
const int enableValidationLayers = 0;
#else
const int enableValidationLayers = 1;
#endif

#define DEVICE_EXTENSIONS 1
const char *deviceExtensions[DEVICE_EXTENSIONS] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// PRIVATE FUNCTIONS

static int checkValidationLayerSupport(void) {
  // get available layers
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, NULL);

  VkLayerProperties *availableLayers =
      malloc(layerCount * sizeof(VkLayerProperties));
  ;
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

  // check if all layers are available
  for (uint32_t i = 0; i < VALIDATION_LAYERS; i++) {
    int found = 0;
    for (uint32_t j = 0; j < layerCount; j++) {
      if (strcmp(validationLayers[i], availableLayers[j].layerName) == 0) {
        found = 1;
        break;
      }
    }
    if (!found) {
      free(availableLayers);
      return 0;
    }
  }

  free(availableLayers);
  return 1;
}

static const char **instanceExtensions(uint32_t *count) {
  const char **extensions = NULL;

  // glfw extensions
  const char **glfwExtensions = glfwGetRequiredInstanceExtensions(count);
  extensions = malloc(*count * sizeof(const char *));
  memcpy(extensions, glfwExtensions, *count * sizeof(const char *));

  // on macOS gotta enable compatibility extension
#ifdef __APPLE__
  extensions = realloc(extensions, (*count + 1) * sizeof(const char *));
  extensions[(*count)++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif

  return extensions;
}

// PUBLIC FUNCTIONS

VkInstance makeVkInstance(char *appName) {
  VkInstance instance;

  // validation layers
  if (enableValidationLayers && !checkValidationLayerSupport()) {
    die("validation layers requested, but not available!\n");
  }

  // app info
  VkApplicationInfo appInfo = {0};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = appName;
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  // required extensions
  uint32_t extensionsCount = 0;
  const char **extensions = instanceExtensions(&extensionsCount);

  // create info
  VkInstanceCreateInfo createInfo = {0};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = extensionsCount;
  createInfo.ppEnabledExtensionNames = extensions;
  createInfo.enabledLayerCount = 0;
#ifdef __APPLE__
  createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  // done
  VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
  if (result != VK_SUCCESS) {
    die("Failed to create instance: %d\n", result);
  }
  free(extensions);
  return instance;
}

VkSurfaceKHR makeVkSurface(VkInstance instance, GLFWwindow *window) {
  VkSurfaceKHR surface;
  if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
    die("Failed to create window surface\n");
  }
  return surface;
}

VkPhysicalDevice pickVkPhysicalDevice(VkInstance instance) {
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(instance, &count, NULL);

  VkPhysicalDevice *devices = malloc(count * sizeof(VkPhysicalDevice));
  vkEnumeratePhysicalDevices(instance, &count, devices);

  // TODO: pick the best device, for now just return the first one
  VkPhysicalDevice device = devices[0];
  free(devices);
  return device;
}

int findVkQueueFamilyIndex(VkPhysicalDevice device, VkSurfaceKHR surface) {
  // get queue families
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

  VkQueueFamilyProperties *queueFamilies =
      malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies);

  // find a queue family that supports both graphics and presentation
  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    int graphicsSupported = queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;

    VkBool32 presentSupported = 0;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupported);

    if (graphicsSupported && presentSupported) {
      free(queueFamilies);
      return i;
    }
  }

  // fail
  die("No suitable queue family found\n");
}

VkDevice makeVkDevice(VkPhysicalDevice physicalDevice, int queueFamilyIndex) {
  VkDevice device;

  // queue create info
  VkDeviceQueueCreateInfo queueCreateInfo = {0};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
  queueCreateInfo.queueCount = 1;
  float queuePriority = 1.0f;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  // device create info
  VkDeviceCreateInfo createInfo = {0};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = 1;
  createInfo.pQueueCreateInfos = &queueCreateInfo;
  createInfo.ppEnabledExtensionNames = deviceExtensions;
  createInfo.enabledExtensionCount = DEVICE_EXTENSIONS;

  // done
  VkResult result = vkCreateDevice(physicalDevice, &createInfo, NULL, &device);
  if (result != VK_SUCCESS) {
    die("Failed to create logical device: %d\n", result);
  }
  return device;
}

struct SwapchainSettings makeSwapchainSettings(VkPhysicalDevice physicalDevice,
                                               VkSurfaceKHR surface, int width,
                                               int height) {
  // get details
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       NULL);
  assert(formatCount > 0);
  VkSurfaceFormatKHR *formats =
      malloc(formatCount * sizeof(VkSurfaceFormatKHR));
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       formats);

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, NULL);
  assert(presentModeCount > 0);
  VkPresentModeKHR *presentModes =
      malloc(presentModeCount * sizeof(VkPresentModeKHR));
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, presentModes);

  // find best settings
  VkSurfaceFormatKHR selectedFormat = formats[0];
  for (uint32_t i = 0; i < formatCount; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      selectedFormat = formats[i];
      break;
    }
  }
  free(formats);

  VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
  for (uint32_t i = 0; i < presentModeCount; i++) {
    if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      selectedPresentMode = presentModes[i];
      break;
    }
  }
  free(presentModes);

  VkExtent2D selectedExtent = capabilities.currentExtent;
  if (capabilities.currentExtent.width == UINT32_MAX) {
    selectedExtent.width = fmax(capabilities.minImageExtent.width,
                                fmin(capabilities.maxImageExtent.width, width));
    selectedExtent.height =
        fmax(capabilities.minImageExtent.height,
             fmin(capabilities.maxImageExtent.height, height));
  }

  // decide image count
  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  // done
  return (struct SwapchainSettings){
      .imageCount = imageCount,
      .currentTransform = capabilities.currentTransform,
      .selectedFormat = selectedFormat,
      .selectedPresentMode = selectedPresentMode,
      .selectedExtent = selectedExtent,
  };
}

VkSwapchainKHR makeVkSwapchain(struct SwapchainSettings settings,
                               VkDevice device, VkSurfaceKHR surface) {
  VkSwapchainKHR swapchain;

  // create swapchain
  VkSwapchainCreateInfoKHR createInfo = {0};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = surface;
  createInfo.minImageCount = settings.imageCount;
  createInfo.imageFormat = settings.selectedFormat.format;
  createInfo.imageColorSpace = settings.selectedFormat.colorSpace;
  createInfo.imageExtent = settings.selectedExtent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  createInfo.preTransform = settings.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = settings.selectedPresentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  // done
  VkResult result = vkCreateSwapchainKHR(device, &createInfo, NULL, &swapchain);
  if (result != VK_SUCCESS) {
    die("Failed to create swap chain: %d\n", result);
  }
  return swapchain;
}

VkImage *getVkSwapchainImages(VkDevice device,
                              struct SwapchainSettings settings,
                              VkSwapchainKHR swapchain) {
  VkImage *images = malloc(settings.imageCount * sizeof(VkImage));
  vkGetSwapchainImagesKHR(device, swapchain, &settings.imageCount, images);
  return images;
}

VkImageView *makeVkImageViews(VkDevice device,
                              struct SwapchainSettings settings,
                              VkImage *images) {
  VkImageView *views = malloc(settings.imageCount * sizeof(VkImageView));

  for (uint32_t i = 0; i < settings.imageCount; i++) {
    VkImageViewCreateInfo createInfo = {0};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = settings.selectedFormat.format;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VkResult result = vkCreateImageView(device, &createInfo, NULL, &views[i]);
    if (result != VK_SUCCESS) {
      die("Failed to create image views: %d\n", result);
    }
  }

  return views;
}

VkShaderModule makeVkShaderModule(VkDevice device, char *path) {
  // read file
  FILE *fp = fopen(path, "rb");
  assert(fp != NULL);

  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  rewind(fp);

  char *code = malloc(size);
  fread(code, 1, size, fp);
  fclose(fp);

  // create shader
  VkShaderModuleCreateInfo createInfo = {0};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = size;
  createInfo.pCode = (uint32_t *)code;

  // done
  VkShaderModule shaderModule;
  VkResult result =
      vkCreateShaderModule(device, &createInfo, NULL, &shaderModule);
  if (result != VK_SUCCESS) {
    die("Failed to create shader module: %d\n", result);
  }
  free(code);
  return shaderModule;
}
