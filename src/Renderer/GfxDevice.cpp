#include "GfxDevice.hpp"

#include <iostream>
#include <limits>

#include <vulkan/vulkan.h>

#include <volk.h>

#include <vk_mem_alloc.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include "Vulkan/GPUBuffer.hpp"
#include "Vulkan/GPUImage.hpp"
#include "Vulkan/Init.hpp"
#include "Vulkan/Pipelines.hpp"
#include "Vulkan/Util.hpp"

#include "ImageLoader.hpp"
#include "MipMapGeneration.hpp"

#include <tracy/Tracy.hpp>

namespace
{
static constexpr auto NO_TIMEOUT = std::numeric_limits<std::uint64_t>::max();
}

VkFormat GfxDevice::getSwapchainFormat()
{
	uint32_t formatCount = 0;
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, 0));
	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> formats(formatCount);
	VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()));

	if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
		return VK_FORMAT_R8G8B8A8_UNORM;

	for (uint32_t i = 0; i < formatCount; ++i)
		if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM || formats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			return formats[i].format;

	return formats[0].format;
}


GfxDevice::GfxDevice() : imageCache(*this)
{}

void GfxDevice::init(SDL_Window* window, const char* appName, const Version& version, bool vSync)
{
    initVulkan(window, appName, version);
    executor = createImmediateExecutor();

    swapchain.initSyncStructures(device);

    this->vSync = vSync;

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    swapchainFormat = getSwapchainFormat();
    swapchain.create(device, swapchainFormat, (std::uint32_t)w, (std::uint32_t)h, vSync);

    createCommandBuffers();
    imageCache.bindlessSetManager.init(device, getMaxAnisotropy()); // TODO: init when number of images is known

    { // create white texture
        std::uint32_t pixel = 0xFFFFFFFF;
        whiteImageId = createImage(
            {
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .extent = VkExtent3D{1, 1, 1},
            },
            "white texture",
            &pixel);
    }

    { // create error texture (black/magenta checker)
        const auto black = 0xFF000000;
        const auto magenta = 0xFFFF00FF;

        std::array<std::uint32_t, 4> pixels{black, magenta, magenta, black};
        errorImageId = createImage(
            {
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                .extent = VkExtent3D{2, 2, 1},
            },
            "error texture",
            pixels.data());
        imageCache.setErrorImageId(errorImageId);
    }

    for (std::size_t i = 0; i < graphics::FRAME_OVERLAP; ++i) {
        frames[i].tracyVkCtx =
            TracyVkContext(physicalDevice, device, graphicsQueue, frames[i].mainCommandBuffer);
    }
}

void GfxDevice::initVulkan(SDL_Window* window, const char* appName, const Version& appVersion)
{
    VK_CHECK(volkInitialize());

    instance = vkb::InstanceBuilder{}
                   .set_app_name(appName)
                   .set_app_version(appVersion.major, appVersion.major, appVersion.patch)
                   .request_validation_layers()
                   .use_default_debug_messenger()
                   .require_api_version(1, 3, 0)
                   .build()
                   .value();

    volkLoadInstance(instance);

    auto res = SDL_Vulkan_CreateSurface(window, instance, NULL, &surface);
    if (res != true) {
        std::cout << "Failed to create Vulkan surface: " << SDL_GetError() << std::endl;
        std::exit(1);
    }

    const auto deviceFeatures = VkPhysicalDeviceFeatures{
        .imageCubeArray = true,
        .geometryShader = true, // for im3d
        .multiDrawIndirect = true,
        .depthClamp = true,
        .samplerAnisotropy = true,
        .pipelineStatisticsQuery = true,
        .shaderInt64 = true,
        .shaderInt16 = true,
    };

	VkPhysicalDeviceVulkan11Features features11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
	features11.storageBuffer16BitAccess = true;
	features11.shaderDrawParameters = true;

    auto features12 = VkPhysicalDeviceVulkan12Features{
        .drawIndirectCount = true,
        .storageBuffer8BitAccess = true,
        .uniformAndStorageBuffer8BitAccess = true,
        .shaderFloat16 = true,
        .shaderInt8 = true,
        .descriptorIndexing = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingStorageImageUpdateAfterBind = true,
        .descriptorBindingPartiallyBound = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .samplerFilterMinmax = true,
        .scalarBlockLayout = true,
    };

	// This will only be used if raytracingSupported=true (see below)
	VkPhysicalDeviceRayQueryFeaturesKHR featuresRayQueries = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
	featuresRayQueries.rayQuery = true;

	// This will only be used if raytracingSupported=true (see below)
	VkPhysicalDeviceAccelerationStructureFeaturesKHR featuresAccelerationStructure = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
	featuresAccelerationStructure.accelerationStructure = true;

	// This will only be used if meshShadingSupported=true (see below)
	VkPhysicalDeviceMeshShaderFeaturesEXT featuresMesh = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };
	featuresMesh.taskShader = true;
	featuresMesh.meshShader = true;

    auto features13 = VkPhysicalDeviceVulkan13Features{
        .synchronization2 = true,
        .dynamicRendering = true,
        .maintenance4 = true,
    };

    std::vector<const char*> extensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	};

    auto system_info_ret = vkb::SystemInfo::get_system_info();

    if (!system_info_ret) {
        printf("%s\n", system_info_ret.error().message().c_str());
        std::exit(1);
    }

    auto system_info = system_info_ret.value();
    void** ppNext = &features13.pNext;

    if (system_info.is_extension_available("VK_EXT_MESH_SHADER_EXTENSION_NAME")) {
        meshShadingSupported = true;
		extensions.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);

        *ppNext = &featuresMesh;
		ppNext = &featuresMesh.pNext;
    }

    meshShadingEnabled = meshShadingSupported;

    if (system_info.is_extension_available("VK_KHR_RAY_QUERY_EXTENSION_NAME")) {
        raytracingSupported = true;
		extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		extensions.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
		extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        features12.bufferDeviceAddress = true;

        *ppNext = &featuresRayQueries;
		ppNext = &featuresRayQueries.pNext;

		*ppNext = &featuresAccelerationStructure;
		ppNext = &featuresAccelerationStructure.pNext;
    }

    physicalDevice = vkb::PhysicalDeviceSelector{instance}
                         .set_minimum_version(1, 3)
                         .set_required_features(deviceFeatures)
                         .set_required_features_11(features11)
                         .set_required_features_12(features12)
                         .set_required_features_13(features13)
                         .add_required_extensions(extensions)
                         .set_surface(surface)
                         .select()
                         .value();

    checkDeviceCapabilities();

    device = vkb::DeviceBuilder{physicalDevice}.build().value();

    graphicsQueueFamily = device.get_queue_index(vkb::QueueType::graphics).value();
    graphicsQueue = device.get_queue(vkb::QueueType::graphics).value();

    { // Init VMA
        const auto vulkanFunctions = VmaVulkanFunctions{
            .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
            .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        };

        const auto allocatorInfo = VmaAllocatorCreateInfo{
            .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
            .physicalDevice = physicalDevice,
            .device = device,
            .pVulkanFunctions = &vulkanFunctions,
            .instance = instance,
        };
        vmaCreateAllocator(&allocatorInfo, &allocator);
    }
}

void GfxDevice::checkDeviceCapabilities()
{
    // check limits
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice, &props);

    maxSamplerAnisotropy = props.limits.maxSamplerAnisotropy;

    { // store which sampling counts HW supports
        const auto counts = std::array{
            VK_SAMPLE_COUNT_1_BIT,
            VK_SAMPLE_COUNT_2_BIT,
            VK_SAMPLE_COUNT_4_BIT,
            VK_SAMPLE_COUNT_8_BIT,
            VK_SAMPLE_COUNT_16_BIT,
            VK_SAMPLE_COUNT_32_BIT,
            VK_SAMPLE_COUNT_64_BIT,
        };

        const auto supportedByDepthAndColor =
            props.limits.framebufferColorSampleCounts & props.limits.framebufferDepthSampleCounts;
        supportedSampleCounts = {};
        for (const auto& count : counts) {
            if (supportedByDepthAndColor & count) {
                supportedSampleCounts = (VkSampleCountFlagBits)(supportedSampleCounts | count);
                highestSupportedSamples = count;
            }
        }
    }
}

void GfxDevice::createCommandBuffers()
{
    const auto poolCreateInfo = vkinit::
        commandPoolCreateInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphicsQueueFamily);

    for (std::uint32_t i = 0; i < graphics::FRAME_OVERLAP; ++i) {
        auto& commandPool = frames[i].commandPool;
        VK_CHECK(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &commandPool));

        const auto cmdAllocInfo = vkinit::commandBufferAllocateInfo(commandPool, 1);
        auto& mainCommandBuffer = frames[i].mainCommandBuffer;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &mainCommandBuffer));
    }
}

void GfxDevice::recreateSwapchain(std::uint32_t swapchainWidth, std::uint32_t swapchainHeight)
{
    assert(swapchainWidth != 0 && swapchainHeight != 0);
    waitIdle();
    swapchain.recreate(
        device,
        swapchainFormat,
        (std::uint32_t)swapchainWidth,
        (std::uint32_t)swapchainHeight,
        vSync);
}

VkCommandBuffer GfxDevice::beginFrame()
{
    swapchain.beginFrame(device, getCurrentFrameIndex());

    const auto& frame = getCurrentFrame();
    const auto& cmd = frame.mainCommandBuffer;
    const auto cmdBeginInfo = VkCommandBufferBeginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    return cmd;
}

void GfxDevice::endFrame(VkCommandBuffer cmd, const GPUImage& drawImage, const EndFrameProps& props)
{
    // get swapchain image
    const auto [swapchainImage, swapchainImageIndex] = swapchain.acquireImage(device, getCurrentFrameIndex());
    if (swapchainImage == VK_NULL_HANDLE) {
        return;
    }

    // Fences are reset here to prevent the deadlock in case swapchain becomes dirty
    swapchain.resetFences(device, getCurrentFrameIndex());

    auto swapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    {
        // clear swapchain image
        VkImageSubresourceRange clearRange =
            vkinit::imageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
        vkutil::transitionImage(cmd, swapchainImage, swapchainLayout, VK_IMAGE_LAYOUT_GENERAL);
        swapchainLayout = VK_IMAGE_LAYOUT_GENERAL;

        const auto clearValue = VkClearColorValue{
            {props.clearColor.r, props.clearColor.g, props.clearColor.b, props.clearColor.a}};
        vkCmdClearColorImage(
            cmd, swapchainImage, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
    }

    if (props.copyImageIntoSwapchain) {
        // copy from draw image into swapchain
        vkutil::transitionImage(
            cmd,
            drawImage.image,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        vkutil::transitionImage(
            cmd, swapchainImage, swapchainLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        swapchainLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        const auto filter = props.drawImageLinearBlit ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        if (props.drawImageBlitRect != glm::ivec4{}) {
            vkutil::copyImageToImage(
                cmd,
                drawImage.image,
                swapchainImage,
                drawImage.getExtent2D(),
                props.drawImageBlitRect.x,
                props.drawImageBlitRect.y,
                props.drawImageBlitRect.z,
                props.drawImageBlitRect.w,
                filter);
        } else {
            // will stretch image to swapchain
            vkutil::copyImageToImage(
                cmd,
                drawImage.image,
                swapchainImage,
                drawImage.getExtent2D(),
                getSwapchainExtent(),
                filter);
        }
    }

    // prepare for present
    vkutil::transitionImage(cmd, swapchainImage, swapchainLayout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    swapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    {
        // TODO: don't collect every frame?
        auto& frame = getCurrentFrame();
        TracyVkCollect(frame.tracyVkCtx, frame.mainCommandBuffer);
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    swapchain.submitAndPresent(cmd, graphicsQueue, getCurrentFrameIndex(), swapchainImageIndex);

    frameNumber++;
}

void GfxDevice::cleanup()
{
    imageCache.destroyImages();
    imageCache.bindlessSetManager.cleanup(device);

    for (auto& frame : frames) {
        vkDestroyCommandPool(device, frame.commandPool, 0);
        TracyVkDestroy(frame.tracyVkCtx);
    }

    swapchain.cleanup(device);

    executor.cleanup(device);

    vkb::destroy_surface(instance, surface);
    vmaDestroyAllocator(allocator);
    vkb::destroy_device(device);
    vkb::destroy_instance(instance);
}

GPUBuffer GfxDevice::createBuffer(
    std::size_t allocSize,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage) const
{
    const auto bufferInfo = VkBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = usage,
    };

    const auto allocInfo = VmaAllocationCreateInfo{
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                 // TODO: allow to set VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT when needed
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = memoryUsage,
    };

    GPUBuffer buffer{};
    VK_CHECK(vmaCreateBuffer(
        allocator, &bufferInfo, &allocInfo, &buffer.buffer, &buffer.allocation, &buffer.info));
    if ((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) != 0) {
        const auto deviceAdressInfo = VkBufferDeviceAddressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = buffer.buffer,
        };
        buffer.address = vkGetBufferDeviceAddress(device, &deviceAdressInfo);
    }

    return buffer;
}

VkDeviceAddress GfxDevice::getBufferAddress(const GPUBuffer& buffer) const
{
    const auto deviceAdressInfo = VkBufferDeviceAddressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer.buffer,
    };
    return vkGetBufferDeviceAddress(device, &deviceAdressInfo);
}

void GfxDevice::destroyBuffer(const GPUBuffer& buffer) const
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

GfxDevice::FrameData& GfxDevice::getCurrentFrame()
{
    return frames[getCurrentFrameIndex()];
}

const TracyVkCtx& GfxDevice::getTracyVkCtx() const
{
    return frames[getCurrentFrameIndex()].tracyVkCtx;
}

std::uint32_t GfxDevice::getCurrentFrameIndex() const
{
    return frameNumber % graphics::FRAME_OVERLAP;
}

bool GfxDevice::deviceSupportsSamplingCount(VkSampleCountFlagBits sample) const
{
    return (supportedSampleCounts & sample) != 0;
}

VkSampleCountFlagBits GfxDevice::getMaxSupportedSamplingCount() const
{
    return highestSupportedSamples;
}

VulkanImmediateExecutor GfxDevice::createImmediateExecutor() const
{
    VulkanImmediateExecutor executor;
    executor.init(device, graphicsQueueFamily, graphicsQueue);
    return executor;
}

void GfxDevice::immediateSubmit(std::function<void(VkCommandBuffer)>&& f) const
{
    executor.immediateSubmit(std::move(f));
}

void GfxDevice::waitIdle() const
{
    VK_CHECK(vkDeviceWaitIdle(device));
}

BindlessSetManager& GfxDevice::getBindlessSetManager()
{
    return imageCache.bindlessSetManager;
}

VkDescriptorSetLayout GfxDevice::getBindlessDescSetLayout() const
{
    return imageCache.bindlessSetManager.getDescSetLayout();
}

const VkDescriptorSet& GfxDevice::getBindlessDescSet() const
{
    return imageCache.bindlessSetManager.getDescSet();
}

void GfxDevice::bindBindlessDescSet(VkCommandBuffer cmd, VkPipelineLayout layout) const
{
    vkCmdBindDescriptorSets(
        cmd,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout,
        0,
        1,
        &imageCache.bindlessSetManager.getDescSet(),
        0,
        nullptr);
}

ImageId GfxDevice::createImage(
    const vkutil::CreateImageInfo& createInfo,
    const char* debugName,
    void* pixelData,
    ImageId imageId)
{
    auto image = createImageRaw(createInfo);
    if (debugName) {
        vkutil::addDebugLabel(device, image.image, debugName);
        image.debugName = debugName;
    }
    if (pixelData) {
        uploadImageData(image, pixelData);
    }
    if (imageId != NULL_IMAGE_ID) {
        return imageCache.addImage(imageId, std::move(image));
    }
    return addImageToCache(std::move(image));
}

ImageId GfxDevice::createDrawImage(
    VkFormat format,
    glm::ivec2 size,
    const char* debugName,
    ImageId imageId)
{
    assert(size.x > 0 && size.y > 0);
    const auto extent = VkExtent3D{
        .width = (std::uint32_t)size.x,
        .height = (std::uint32_t)size.y,
        .depth = 1,
    };

    VkImageUsageFlags usages{};
    usages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    usages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    usages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

    const auto createImageInfo = vkutil::CreateImageInfo{
        .format = format,
        .usage = usages,
        .extent = extent,
    };
    return createImage(createImageInfo, debugName, nullptr, imageId);
}

ImageId GfxDevice::loadImageFromFile(
    const std::filesystem::path& path,
    VkFormat format,
    VkImageUsageFlags usage,
    bool mipMap)
{
    return imageCache.loadImageFromFile(path, format, usage, mipMap);
}

const GPUImage& GfxDevice::getImage(ImageId id) const
{
    return imageCache.getImage(id);
}

ImageId GfxDevice::addImageToCache(GPUImage img)
{
    return imageCache.addImage(std::move(img));
}

GPUImage GfxDevice::createImageRaw(
    const vkutil::CreateImageInfo& createInfo,
    std::optional<VmaAllocationCreateInfo> customAllocationCreateInfo) const
{
    std::uint32_t mipLevels = 1;
    if (createInfo.mipMap) {
        const auto maxExtent = std::max(createInfo.extent.width, createInfo.extent.height);
        mipLevels = (std::uint32_t)std::floor(std::log2(maxExtent)) + 1;
    }

    if (createInfo.isCubemap) {
        assert(createInfo.numLayers % 6 == 0);
        assert(!createInfo.mipMap);
        assert((createInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0);
    }

    auto imgInfo = VkImageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = createInfo.flags,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = createInfo.format,
        .extent = createInfo.extent,
        .mipLevels = mipLevels,
        .arrayLayers = createInfo.numLayers,
        .samples = createInfo.samples,
        .tiling = createInfo.tiling,
        .usage = createInfo.usage,
    };

    static const auto defaultAllocInfo = VmaAllocationCreateInfo{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    const auto allocInfo = customAllocationCreateInfo.has_value() ?
                               customAllocationCreateInfo.value() :
                               defaultAllocInfo;

    GPUImage image{};
    image.format = createInfo.format;
    image.usage = createInfo.usage;
    image.extent = createInfo.extent;
    image.mipLevels = mipLevels;
    image.numLayers = createInfo.numLayers;
    image.isCubemap = createInfo.isCubemap;

    VK_CHECK(
        vmaCreateImage(allocator, &imgInfo, &allocInfo, &image.image, &image.allocation, nullptr));

    // create view only when usage flags allow it
    bool shouldCreateView = ((createInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0) ||
                            ((createInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) ||
                            ((createInfo.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0) ||
                            ((createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);

    if (shouldCreateView) {
        VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
        if (createInfo.format == VK_FORMAT_D32_SFLOAT) { // TODO: support other depth formats
            aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
        }

        auto viewType =
            createInfo.numLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        if (createInfo.isCubemap && createInfo.numLayers == 6) {
            viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        }

        const auto viewCreateInfo = VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image.image,
            .viewType = viewType,
            .format = createInfo.format,
            .subresourceRange =
                VkImageSubresourceRange{
                    .aspectMask = aspectFlag,
                    .baseMipLevel = 0,
                    .levelCount = mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = createInfo.numLayers,
                },
        };

        VK_CHECK(vkCreateImageView(device, &viewCreateInfo, nullptr, &image.imageView));
    }

    return image;
}

void GfxDevice::uploadImageData(const GPUImage& image, void* pixelData, std::uint32_t layer) const
{
    int numChannels = 4;
    if (image.format == VK_FORMAT_R8_UNORM) {
        // FIXME: support more types
        numChannels = 1;
    }
    const auto dataSize =
        image.extent.depth * image.extent.width * image.extent.height * numChannels;

    const auto uploadBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    memcpy(uploadBuffer.info.pMappedData, pixelData, dataSize);

    executor.immediateSubmit([&](VkCommandBuffer cmd) {
        assert(
            (image.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0 &&
            "Image needs to have VK_IMAGE_USAGE_TRANSFER_DST_BIT to upload data to it");
        vkutil::transitionImage(
            cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        const auto copyRegion = VkBufferImageCopy{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = layer,
                    .layerCount = 1,
                },
            .imageExtent = image.extent,
        };

        vkCmdCopyBufferToImage(
            cmd,
            uploadBuffer.buffer,
            image.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion);

        if (image.mipLevels > 1) {
            assert(
                (image.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0 &&
                (image.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0 &&
                "Image needs to have VK_IMAGE_USAGE_TRANSFER_{DST,SRC}_BIT to generate mip maps");
            graphics::generateMipmaps(
                cmd,
                image.image,
                VkExtent2D{image.extent.width, image.extent.height},
                image.mipLevels);
        } else {
            vkutil::transitionImage(
                cmd,
                image.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    });

    destroyBuffer(uploadBuffer);
}

GPUImage GfxDevice::loadImageFromFileRaw(
    const std::filesystem::path& path,
    VkFormat format,
    VkImageUsageFlags usage,
    bool mipMap) const
{
    auto data = util::loadImage(path);
    if (!data.pixels) {
        // fmt::println("[error] failed to load image from '{}'", path.string());
        return getImage(errorImageId);
    }

    auto ext = path.extension();
    if(ext == "dds" || ext == "DDS")
        format = data.format;

    auto image = createImageRaw({
        .format = format,
        .usage = usage | //
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | // for uploading pixel data to image
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT, // for generating mips
        .extent =
            VkExtent3D{
                .width = (std::uint32_t)data.width,
                .height = (std::uint32_t)data.height,
                .depth = 1,
            },
        .mipMap = mipMap,
    });
    uploadImageData(image, data.pixels);

    image.debugName = path.string();
    vkutil::addDebugLabel(device, image.image, path.string().c_str());

    return image;
}

void GfxDevice::destroyImage(const GPUImage& image) const
{
    vkDestroyImageView(device, image.imageView, nullptr);
    vmaDestroyImage(allocator, image.image, image.allocation);
    // TODO: if image has bindless id, update the set
}
