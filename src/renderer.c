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
};

void recordCommandBuffer(Renderer r, uint32_t imageIndex) {
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

  // TODO: draw the loaded vertices

  // end render pass
  vkCmdEndRenderPass(r->commandBuffer);

  // end recording
  result = vkEndCommandBuffer(r->commandBuffer);
  if (result != VK_SUCCESS) {
    die("Failed to end recording command buffer: %d\n", result);
  }
}

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

  // return
  return r;
}

void mainLoop(Renderer r) {
  while (!glfwWindowShouldClose(r->window)) {
    glfwPollEvents(); // TODO: event handling

    // wait for previous frame to finish
    vkWaitForFences(r->device, 1, &r->sync.inFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(r->device, 1, &r->sync.inFlight);

    // get next image to render to
    uint32_t imageIndex;
    vkAcquireNextImageKHR(r->device, r->swapchain, UINT64_MAX,
                          r->sync.imageAvailable, VK_NULL_HANDLE, &imageIndex);

    // record command buffer
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

    VkResult result = vkQueueSubmit(r->queue, 1, &submitInfo, r->sync.inFlight);
    if (result != VK_SUCCESS) {
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

void freeRenderer(Renderer r) {
  vkDeviceWaitIdle(r->device);

  // sync objects
  vkDestroySemaphore(r->device, r->sync.imageAvailable, NULL);
  vkDestroySemaphore(r->device, r->sync.renderFinished, NULL);
  vkDestroyFence(r->device, r->sync.inFlight, NULL);

  // command buffer
  vkDestroyCommandPool(r->device, r->commandPool, NULL);

  // framebuffers
  for (uint32_t i = 0; i < r->swapchainSettings.imageCount; i++) {
    vkDestroyFramebuffer(r->device, r->framebuffers[i], NULL);
  }

  // graphics pipeline
  vkDestroyPipeline(r->device, r->pipeline, NULL);
  vkDestroyPipelineLayout(r->device, r->pipelineLayout, NULL);
  vkDestroyRenderPass(r->device, r->renderPass, NULL);
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
