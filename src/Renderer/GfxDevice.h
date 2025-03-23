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
    VkDevice m_device;
    VkSurfaceKHR m_surface;
    Swapchain m_swapchain;
	VkFormat m_swapchainFormat;
	VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;
	VkPhysicalDeviceProperties m_props = {};
	VkDebugReportCallbackEXT m_debugCallback;

	std::vector<VkImageView> m_swapchainImageViews;

};

/**
 * Creates a Vulkan semaphore for synchronization operations.
 * @param device The Vulkan device to create the semaphore on
 * @return The created semaphore handle
 */
VkSemaphore createSemaphore(VkDevice device);

/**
 * Creates a Vulkan fence for CPU-GPU synchronization.
 * @param device The Vulkan device to create the fence on
 * @return The created fence handle with signaled state
 */
VkFence createFence(VkDevice device);

/**
 * Creates a command pool for allocating command buffers.
 * @param device The Vulkan device to create the command pool on
 * @param familyIndex The graphics queue family index
 * @return The created command pool with transient flag
 */
VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex);

/**
 * Creates a query pool for collecting performance data.
 * @param device The Vulkan device to create the query pool on
 * @param queryCount Number of queries in the pool
 * @param queryType Type of query (timestamp or pipeline statistics)
 * @return The created query pool handle
 */
VkQueryPool createQueryPool(VkDevice device, uint32_t queryCount, VkQueryType queryType);

/**
 * Destroys the SDL window used for rendering.
 * @param window Pointer to the SDL window to destroy
 */
void destroyGfxWindow(SDL_Window * window);

/**
 * Initializes the graphics device and Vulkan context.
 * Creates window, instance, physical device, logical device, and swapchain.
 * @return Initialized GfxDevice structure
 */
GfxDevice initDevice();

/**
 * Checks if application should quit by polling SDL events.
 * @return True if quit event received, false otherwise
 */
bool shouldQuit(GfxDevice gfxDevice);
