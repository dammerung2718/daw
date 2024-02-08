#include "renderer.h"

#include "die.h"
#include "vk.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <pthread.h>

struct Renderer {
  // state
  int running;

  // ui
  struct Vertex *vertices;
  int vertexCount;

  // window
  char *title;
  int width, height;
  GLFWwindow *window;
  int resized;

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

  // graphics pipeline
  VkShaderModule vertShader;
  VkShaderModule fragShader;
  VkRenderPass renderPass;
  VkPipelineLayout pipelineLayout;
  VkPipeline pipeline;

  // framebuffers
  VkFramebuffer *framebuffers;

  // command buffer
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;

  // sync objects
  struct SyncObjects sync;

  // vertex buffer
  VkBuffer vertexBuffer;
  VkDeviceMemory vertexMemory;
};

static void recordCommandBuffer(Renderer r, uint32_t imageIndex) {
  VkResult result;

  // reset
  vkResetCommandBuffer(r->commandBuffer, 0);

  // begin recording
  VkCommandBufferBeginInfo beginInfo = {0};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  result = vkBeginCommandBuffer(r->commandBuffer, &beginInfo);
  if (result != VK_SUCCESS) {
    die("Failed to begin recording command buffer: %d\n", result);
  }

  // begin render pass
  VkRenderPassBeginInfo renderPassInfo = {0};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = r->renderPass;
  renderPassInfo.framebuffer = r->framebuffers[imageIndex];
  renderPassInfo.renderArea.offset.x = 0;
  renderPassInfo.renderArea.offset.y = 0;
  renderPassInfo.renderArea.extent.width =
      r->swapchainSettings.selectedExtent.width;
  renderPassInfo.renderArea.extent.height =
      r->swapchainSettings.selectedExtent.height;

  VkClearValue clearValue = {{{1.0f, 1.0f, 1.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearValue;

  vkCmdBeginRenderPass(r->commandBuffer, &renderPassInfo,
                       VK_SUBPASS_CONTENTS_INLINE);

  // bind pipeline
  vkCmdBindPipeline(r->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    r->pipeline);

  // push constants
  struct PushConstants pushConstants = {{r->width, r->height}};
  vkCmdPushConstants(r->commandBuffer, r->pipelineLayout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(struct PushConstants), &pushConstants);

  // draw the loaded vertices
  VkBuffer vertexBuffers[] = {r->vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(r->commandBuffer, 0, 1, vertexBuffers, offsets);
  vkCmdDraw(r->commandBuffer, r->vertexCount, 1, 0, 0);

  // end render pass
  vkCmdEndRenderPass(r->commandBuffer);

  // end recording
  result = vkEndCommandBuffer(r->commandBuffer);
  if (result != VK_SUCCESS) {
    die("Failed to end recording command buffer: %d\n", result);
  }
}

static void framebufferResizeCallback(GLFWwindow *window, int width,
                                      int height) {
  Renderer r = glfwGetWindowUserPointer(window);
  r->width = width;
  r->height = height;
  r->resized = 1;
}

static void recreateSwapchain(Renderer r) {
  glfwGetFramebufferSize(r->window, &r->width, &r->height);
  while (r->width == 0 || r->height == 0) {
    glfwGetFramebufferSize(r->window, &r->width, &r->height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(r->device);

  r->swapchainSettings =
      makeSwapchainSettings(r->physicalDevice, r->surface, r->width, r->height);
  r->swapchain = makeVkSwapchain(r->swapchainSettings, r->device, r->surface);
  r->imageViews =
      makeVkImageViews(r->device, r->swapchainSettings, r->swapchainImages);
  r->framebuffers = makeVkFramebuffers(r->device, r->swapchainSettings,
                                       r->imageViews, r->renderPass);
}

static void cleanupSwapchain(Renderer r) {
  for (uint32_t i = 0; i < r->swapchainSettings.imageCount; i++) {
    vkDestroyFramebuffer(r->device, r->framebuffers[i], NULL);
  }
  free(r->framebuffers);

  for (uint32_t i = 0; i < r->swapchainSettings.imageCount; i++) {
    vkDestroyImageView(r->device, r->imageViews[i], NULL);
  }
  free(r->imageViews);

  vkDestroySwapchainKHR(r->device, r->swapchain, NULL);
}

static void render(Renderer r) {
  while (r->running) {
    // wait for previous frame to finish
    vkWaitForFences(r->device, 1, &r->sync.inFlight, VK_TRUE, UINT64_MAX);

    // get next image to render to
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(r->device, r->swapchain, UINT64_MAX,
                                            r->sync.imageAvailable,
                                            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreateSwapchain(r);
      continue;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      die("failed to acquire swap chain image!: %d\n", result);
    }

    // record command buffer
    vkResetFences(r->device, 1, &r->sync.inFlight);
    recordCommandBuffer(r, imageIndex);

    // submit command buffer
    VkSubmitInfo submitInfo = {0};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {r->sync.imageAvailable};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &r->commandBuffer;

    VkSemaphore signalSemaphores[] = {r->sync.renderFinished};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    result = vkQueueSubmit(r->queue, 1, &submitInfo, r->sync.inFlight);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      recreateSwapchain(r);
    } else if (result != VK_SUCCESS) {
      die("Failed to submit draw command buffer: %d\n", result);
    }

    // present image
    VkPresentInfoKHR presentInfo = {0};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {r->swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;

    vkQueuePresentKHR(r->queue, &presentInfo);
  }
}

Renderer makeRenderer(char *title, int width, int height,
                      struct Vertex *vertices, int vertexCount) {
  Renderer r = malloc(sizeof(struct Renderer));
  r->running = 1;
  r->vertices = vertices;
  r->vertexCount = vertexCount;
  r->title = title;
  r->width = width;
  r->height = height;
  r->resized = 0;

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
  r->window = glfwCreateWindow(r->width, r->height, r->title, NULL, NULL);
  if (!r->window) {
    glfwTerminate();
    die("Failed to create window\n");
  }

  // resize callback
  glfwSetWindowUserPointer(r->window, r);
  glfwSetFramebufferSizeCallback(r->window, framebufferResizeCallback);
  glfwGetFramebufferSize(r->window, &r->width, &r->height);

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

  // graphics pipeline
  r->vertShader = makeVkShaderModule(r->device, "bin/vert.spv");
  r->fragShader = makeVkShaderModule(r->device, "bin/frag.spv");
  r->renderPass = makeVkRenderPass(r->device, r->swapchainSettings);
  r->pipelineLayout = makeVkPipelineLayout(r->device);
  r->pipeline = makeVkPipeline(r->device, r->swapchainSettings, r->vertShader,
                               r->fragShader, r->renderPass, r->pipelineLayout);

  // framebuffers
  r->framebuffers = makeVkFramebuffers(r->device, r->swapchainSettings,
                                       r->imageViews, r->renderPass);

  // command buffer
  r->commandPool = makeVkCommandPool(r->device, r->queueFamilyIndex);
  r->commandBuffer = makeVkCommandBuffer(r->device, r->commandPool);

  // sync objects
  r->sync = makeVkSyncObjects(r->device);

  // vertex buffer
  struct VertexBufferAndMemory vbam =
      makeVkVertexBuffer(r->physicalDevice, r->device, vertices, vertexCount);
  r->vertexBuffer = vbam.buffer;
  r->vertexMemory = vbam.memory;

  // return
  return r;
}

void mainLoop(Renderer r) {
  pthread_t renderThread;

  pthread_create(&renderThread, NULL, (void *(*)(void *))render, r);
  while (!glfwWindowShouldClose(r->window)) {
    glfwPollEvents(); // TODO: event handling
  }

  r->running = 0;
  pthread_join(renderThread, NULL);
}

void freeRenderer(Renderer r) {
  vkDeviceWaitIdle(r->device);
  cleanupSwapchain(r);

  // vertexBuffer
  vkDestroyBuffer(r->device, r->vertexBuffer, NULL);
  vkFreeMemory(r->device, r->vertexMemory, NULL);

  // sync objects
  vkDestroySemaphore(r->device, r->sync.imageAvailable, NULL);
  vkDestroySemaphore(r->device, r->sync.renderFinished, NULL);
  vkDestroyFence(r->device, r->sync.inFlight, NULL);

  // command buffer
  vkDestroyCommandPool(r->device, r->commandPool, NULL);

  // graphics pipeline
  vkDestroyPipeline(r->device, r->pipeline, NULL);
  vkDestroyPipelineLayout(r->device, r->pipelineLayout, NULL);
  vkDestroyRenderPass(r->device, r->renderPass, NULL);
  vkDestroyShaderModule(r->device, r->fragShader, NULL);
  vkDestroyShaderModule(r->device, r->vertShader, NULL);

  // vulkan
  vkDestroyDevice(r->device, NULL);
  vkDestroySurfaceKHR(r->instance, r->surface, NULL);
  vkDestroyInstance(r->instance, NULL);

  // window
  glfwDestroyWindow(r->window);
  glfwTerminate();
}
