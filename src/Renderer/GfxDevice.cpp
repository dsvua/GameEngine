#include "GfxDevice.h"

#include "niagara/shaders.h"
#include "niagara/device.h"
#include "niagara/swapchain.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>


VkSemaphore createSemaphore(VkDevice device)
{
	VkSemaphoreCreateInfo createInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

	VkSemaphore semaphore = 0;
	VK_CHECK(vkCreateSemaphore(device, &createInfo, 0, &semaphore));

	return semaphore;
}

VkFence createFence(VkDevice device)
{
	VkFenceCreateInfo createInfo = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence fence = 0;
	VK_CHECK(vkCreateFence(device, &createInfo, 0, &fence));

	return fence;
}

VkCommandPool createCommandPool(VkDevice device, uint32_t familyIndex)
{
	VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	createInfo.queueFamilyIndex = familyIndex;

	VkCommandPool commandPool = 0;
	VK_CHECK(vkCreateCommandPool(device, &createInfo, 0, &commandPool));

	return commandPool;
}

VkQueryPool createQueryPool(VkDevice device, uint32_t queryCount, VkQueryType queryType)
{
	VkQueryPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	createInfo.queryType = queryType;
	createInfo.queryCount = queryCount;

	if (queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS)
	{
		createInfo.pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT;
	}

	VkQueryPool queryPool = 0;
	VK_CHECK(vkCreateQueryPool(device, &createInfo, 0, &queryPool));

	return queryPool;
}

template <typename PushConstants, size_t PushDescriptors>
void dispatch(VkCommandBuffer commandBuffer, const Program& program, uint32_t threadCountX, uint32_t threadCountY, const PushConstants& pushConstants, const DescriptorInfo (&pushDescriptors)[PushDescriptors])
{
	assert(program.pushConstantSize == sizeof(pushConstants));
	assert(program.pushDescriptorCount == PushDescriptors);

	if (program.pushConstantStages)
		vkCmdPushConstants(commandBuffer, program.layout, program.pushConstantStages, 0, sizeof(pushConstants), &pushConstants);

	if (program.pushDescriptorCount)
		vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer, program.updateTemplate, program.layout, 0, pushDescriptors);

	vkCmdDispatch(commandBuffer, getGroupCount(threadCountX, program.localSizeX), getGroupCount(threadCountY, program.localSizeY), 1);
}

GfxDevice initDevice()
{
    GfxDevice result;
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD) < 0) {
        printf("SDL could not initialize. SDL Error: %s\n", SDL_GetError());
        std::exit(1);
    }

	VK_CHECK(volkInitialize());

	printf("Creating SDL window\n");
    // TODO:
    // Add version to window title.
    result.m_window = SDL_CreateWindow(
        "Game",
        // size
        1024,
        768,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!result.m_window) {
        printf("Failed to create window. SDL Error: %s\n", SDL_GetError());
        std::exit(1);
    }

	printf("Creating instance\n");
	result.m_instance = createInstance();
	assert(result.m_instance);

	printf("Volk load instance\n");
	volkLoadInstanceOnly(result.m_instance);

	printf("Registering debug callback\n");
	result.debugCallback = registerDebugCallback(result.m_instance);
	registerDebugCallback(result.m_instance);
	VkPhysicalDevice physicalDevices[16];
	uint32_t physicalDeviceCount = sizeof(physicalDevices) / sizeof(physicalDevices[0]);
	VK_CHECK(vkEnumeratePhysicalDevices(result.m_instance, &physicalDeviceCount, physicalDevices));

	printf("Selecting physical device\n");
	result.m_physicalDevice = pickPhysicalDevice(physicalDevices, physicalDeviceCount);
	assert(result.m_physicalDevice);

	uint32_t extensionCount = 0;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(result.m_physicalDevice, 0, &extensionCount, 0));

	std::vector<VkExtensionProperties> extensions(extensionCount);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(result.m_physicalDevice, 0, &extensionCount, extensions.data()));

	bool meshShadingSupported = false;
	bool raytracingSupported = false;

	for (auto& ext : extensions)
	{
		meshShadingSupported = meshShadingSupported || strcmp(ext.extensionName, VK_EXT_MESH_SHADER_EXTENSION_NAME) == 0;
		raytracingSupported = raytracingSupported || strcmp(ext.extensionName, VK_KHR_RAY_QUERY_EXTENSION_NAME) == 0;
	}

    assert(meshShadingSupported);
    assert(raytracingSupported);

	vkGetPhysicalDeviceProperties(result.m_physicalDevice, &result.props);
	assert(result.props.limits.timestampComputeAndGraphics);

	result.m_familyIndex = getGraphicsFamilyIndex(result.m_physicalDevice);
	assert(result.m_familyIndex != VK_QUEUE_FAMILY_IGNORED);

	result.m_device = createDevice(result.m_instance, result.m_physicalDevice, result.m_familyIndex, meshShadingSupported, raytracingSupported);
	assert(result.m_device);

	volkLoadDevice(result.m_device);

	result.surface = createSurface(result.m_instance, result.m_window);
	assert(result.surface);

	VkBool32 presentSupported = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(result.m_physicalDevice, result.m_familyIndex, result.surface, &presentSupported));
	assert(presentSupported);

	result.swapchainFormat = getSwapchainFormat(result.m_physicalDevice, result.surface);
	result.depthFormat = VK_FORMAT_D32_SFLOAT;

	vkGetPhysicalDeviceMemoryProperties(result.m_physicalDevice, &result.m_memoryProperties);

	createSwapchain(result.swapchain, result.m_physicalDevice, result.m_device, result.surface, result.m_familyIndex, result.m_window, result.swapchainFormat);

    return result;
}

bool shouldQuit()
{
	SDL_Event e;
	while (SDL_PollEvent(&e) != 0);
	{
		if(e.type == SDL_EVENT_QUIT)
			return true;
	}
	return false;
}

void destroyGfxWindow(SDL_Window *window)
{
	SDL_DestroyWindow(window);
}