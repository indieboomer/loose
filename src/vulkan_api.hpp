#pragma once
#include <stdexcept>
#include <string>
#include <vulkan/vulkan.h>
// Explicit platform-loader entry points. No SDK import library is needed.
#define LOOSE_VK_FUNCTIONS(X) \
 X(vkCreateInstance) X(vkDestroyInstance) X(vkEnumerateInstanceLayerProperties) X(vkEnumerateInstanceExtensionProperties) \
 X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceMemoryProperties) X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceFormatProperties) \
 X(vkGetPhysicalDeviceSurfaceSupportKHR) X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) X(vkGetPhysicalDeviceSurfaceFormatsKHR) X(vkGetPhysicalDeviceSurfacePresentModesKHR) \
 X(vkEnumerateDeviceExtensionProperties) X(vkCreateDevice) X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkDeviceWaitIdle) X(vkQueueWaitIdle) X(vkDestroySurfaceKHR) \
 X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) X(vkGetSwapchainImagesKHR) X(vkAcquireNextImageKHR) X(vkQueuePresentKHR) \
 X(vkCreateImageView) X(vkDestroyImageView) X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory) X(vkBindImageMemory) \
 X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) X(vkMapMemory) X(vkUnmapMemory) \
 X(vkCreateRenderPass) X(vkDestroyRenderPass) X(vkCreateFramebuffer) X(vkDestroyFramebuffer) X(vkCreateShaderModule) X(vkDestroyShaderModule) \
 X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) X(vkCreateGraphicsPipelines) X(vkDestroyPipeline) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) \
 X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateSampler) X(vkDestroySampler) \
 X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandBuffer) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) \
 X(vkCmdBeginRenderPass) X(vkCmdEndRenderPass) X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) X(vkCmdPushConstants) X(vkCmdBindVertexBuffers) X(vkCmdDraw) \
 X(vkCmdSetViewport) X(vkCmdSetScissor) X(vkCmdPipelineBarrier) X(vkCmdCopyImageToBuffer) \
 X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences) X(vkResetFences) X(vkCreateSemaphore) X(vkDestroySemaphore) X(vkQueueSubmit)
namespace loose {
#define DECLARE(name) inline PFN_##name name=nullptr;
LOOSE_VK_FUNCTIONS(DECLARE)
#undef DECLARE
inline PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr=nullptr;
inline void vkCheck(VkResult result){
    if(result==VK_ERROR_INCOMPATIBLE_DRIVER)throw std::runtime_error("No compatible Vulkan 1.1 driver. Install or update a Vulkan-capable GPU driver.");
    if(result==VK_ERROR_DEVICE_LOST)throw std::runtime_error("The Vulkan GPU device was lost. Restart the game and check the GPU driver.");
    if(result!=VK_SUCCESS)throw std::runtime_error("Vulkan call failed: "+std::to_string(result));
}
}
