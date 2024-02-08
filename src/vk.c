#include "vk.h"

#include "die.h"
#include "vertex.h"
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

static float clamp(float d, float min, float max) {
  const float t = d < min ? min : d;
  return t > max ? max : t;
}

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
    selectedExtent.width = clamp(width, capabilities.minImageExtent.width,
                                 capabilities.maxImageExtent.width);
    selectedExtent.width = clamp(height, capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height);
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

VkRenderPass makeVkRenderPass(VkDevice device,
                              struct SwapchainSettings settings) {
  VkAttachmentDescription colorAttachment = {0};
  colorAttachment.format = settings.selectedFormat.format;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {0};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass = {0};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  VkSubpassDependency dependency = {0};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo = {0};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  VkRenderPass renderPass;
  VkResult result =
      vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass);
  if (result != VK_SUCCESS) {
    die("Failed to create render pass: %d\n", result);
  }
  return renderPass;
}

VkPipelineLayout makeVkPipelineLayout(VkDevice device) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {0};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount = 0;         // Optional
  pipelineLayoutInfo.pSetLayouts = NULL;         // Optional
  pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
  pipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

  VkPipelineLayout pipelineLayout;
  VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL,
                                           &pipelineLayout);
  if (result != VK_SUCCESS) {
    die("failed to create pipeline layout!: %d\n", result);
  }
  return pipelineLayout;
}

VkPipeline makeVkPipeline(VkDevice device, struct SwapchainSettings settings,
                          VkShaderModule vert, VkShaderModule frag,
                          VkRenderPass renderPass,
                          VkPipelineLayout pipelineLayout) {
// dynamic state
#define DYNAMIC_STATES 2
  const VkDynamicState dynamicStates[DYNAMIC_STATES] = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState = {0};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = DYNAMIC_STATES;
  dynamicState.pDynamicStates = dynamicStates;

  // vertex input state
  VkVertexInputBindingDescription bindingDescription =
      getVertexBindingDescription();
  VkVertexInputAttributeDescription *attributeDescriptions =
      getVertexAttributeDescriptions();

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {0};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount = 1;
  vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount =
      getVertexAttributeDescriptionCount();
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

  // input assembly stage
  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {0};
  inputAssembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  // vertex shader stage
  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {0};
  vertShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vert;
  vertShaderStageInfo.pName = "main";

  // fragment shader stage
  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {0};
  fragShaderStageInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = frag;
  fragShaderStageInfo.pName = "main";

// shader stages
#define SHADER_STAGES 2
  VkPipelineShaderStageCreateInfo shaderStages[SHADER_STAGES] = {
      vertShaderStageInfo, fragShaderStageInfo};

  // viewport and scissor state
  VkViewport viewport = {0};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)settings.selectedExtent.width;
  viewport.height = (float)settings.selectedExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {0};
  scissor.offset = (VkOffset2D){0, 0};
  scissor.extent = settings.selectedExtent;

  VkPipelineViewportStateCreateInfo viewportState = {0};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  // rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer = {0};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f; // Optional
  rasterizer.depthBiasClamp = 0.0f;          // Optional
  rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

  // multisampling (anti-alising)
  VkPipelineMultisampleStateCreateInfo multisampling = {0};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading = 1.0f;          // Optional
  multisampling.pSampleMask = NULL;               // Optional
  multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
  multisampling.alphaToOneEnable = VK_FALSE;      // Optional

  // color blending
  VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
  colorBlendAttachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable = VK_TRUE;
  colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  colorBlendAttachment.dstColorBlendFactor =
      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
  colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

  VkPipelineColorBlendStateCreateInfo colorBlending = {0};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f; // Optional
  colorBlending.blendConstants[1] = 0.0f; // Optional
  colorBlending.blendConstants[2] = 0.0f; // Optional
  colorBlending.blendConstants[3] = 0.0f; // Optional

  // pipeline
  VkGraphicsPipelineCreateInfo pipelineInfo = {0};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = SHADER_STAGES;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = NULL; // Optional
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
  pipelineInfo.basePipelineIndex = -1;              // Optional

  // done
  VkPipeline graphicsPipeline;
  VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline);
  if (result != VK_SUCCESS) {
    die("failed to create graphics pipeline!: %d\n", result);
  }
  return graphicsPipeline;
}

VkFramebuffer *makeVkFramebuffers(VkDevice device,
                                  struct SwapchainSettings settings,
                                  VkImageView *imageViews,
                                  VkRenderPass renderPass) {
  VkFramebuffer *framebuffers =
      malloc(settings.imageCount * sizeof(VkFramebuffer));

  for (uint32_t i = 0; i < settings.imageCount; i++) {
    VkImageView attachments[] = {imageViews[i]};
    VkFramebufferCreateInfo framebufferInfo = {0};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = settings.selectedExtent.width;
    framebufferInfo.height = settings.selectedExtent.height;
    framebufferInfo.layers = 1;

    VkResult result =
        vkCreateFramebuffer(device, &framebufferInfo, NULL, &framebuffers[i]);
    if (result != VK_SUCCESS) {
      die("failed to create framebuffer!: %d\n", result);
    }
  }

  return framebuffers;
}

VkCommandPool makeVkCommandPool(VkDevice device, int queueFamilyIndex) {
  VkCommandPoolCreateInfo poolInfo = {0};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndex;
  poolInfo.flags = 0; // Optional

  VkCommandPool commandPool;
  VkResult result = vkCreateCommandPool(device, &poolInfo, NULL, &commandPool);
  if (result != VK_SUCCESS) {
    die("failed to create command pool!: %d\n", result);
  }
  return commandPool;
}

VkCommandBuffer makeVkCommandBuffer(VkDevice device,
                                    VkCommandPool commandPool) {
  VkCommandBufferAllocateInfo allocInfo = {0};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  VkResult result =
      vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
  if (result != VK_SUCCESS) {
    die("failed to allocate command buffers!: %d", result);
  }
  return commandBuffer;
}

struct SyncObjects makeVkSyncObjects(VkDevice device) {
  struct SyncObjects sync = {0};
  VkResult result;

  VkSemaphoreCreateInfo semaphoreInfo = {0};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {0};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  // imageAvailable
  result =
      vkCreateSemaphore(device, &semaphoreInfo, NULL, &sync.imageAvailable);
  if (result != VK_SUCCESS) {
    die("failed to create imageAvailable semaphore!: %d\n", result);
  }

  // renderFinished
  result =
      vkCreateSemaphore(device, &semaphoreInfo, NULL, &sync.renderFinished);
  if (result != VK_SUCCESS) {
    die("failed to create renderFinished semaphore!: %d\n", result);
  }

  // inFlight
  result = vkCreateFence(device, &fenceInfo, NULL, &sync.inFlight);
  if (result != VK_SUCCESS) {
    die("failed to create inFlight fence!: %d\n", result);
  }

  // done
  return sync;
}
