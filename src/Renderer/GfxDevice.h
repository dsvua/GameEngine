#pragma once

#include "niagara/common.h"
#include "niagara/swapchain.h"

struct SDL_Window;
struct Program;
struct PushConstants;
struct DescriptorInfo;

struct GfxDevice {
    SDL_Window * m_window;
    VkPhysicalDevice m_physicalDevice;
	VkInstance m_instance;
    VkPipelineRenderingCreateInfo m_gbufferInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    uint32_t m_familyIndex;
    VkDevice device;
    VkSurfaceKHR surface;
    Swapchain swapchain;
	VkFormat swapchainFormat;
	VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
	VkPhysicalDeviceProperties props = {};
	VkDebugReportCallbackEXT debugCallback;

	std::vector<VkImageView> swapchainImageViews;

};

VkSemaphore createSemaphore(VkDevice device);

VkFence createFence(VkDevice device);

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex);

VkQueryPool createQueryPool(VkDevice device, uint32_t queryCount, VkQueryType queryType);

void destroyGfxWindow(SDL_Window * window);

GfxDevice initDevice();

bool shouldQuit();
