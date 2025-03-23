#include "Renderer.h"
#include "RendererUtils.h"
#include "niagara/math.h"
#include "niagara/common.h"
#include "niagara/config.h"
#include "niagara/scene.h"
#include "niagara/textures.h"
#include "niagara/scenert.h"
#include <stdarg.h>
#include <string.h>
// #include "volk.h"


Renderer::Renderer()
{
    m_gfxDevice = initDevice();
    m_gfxDevice.m_gbufferInfo.colorAttachmentCount = GBUFFER_COUNT;
	m_gfxDevice.m_gbufferInfo.pColorAttachmentFormats = m_gbufferFormats;
	m_gfxDevice.m_gbufferInfo.depthAttachmentFormat = m_gfxDevice.m_depthFormat;
    printf("Setting depth format to: %d\n", m_gfxDevice.m_depthFormat);

    createPrograms();
    createPipelines();
    createFramesData();
    vkGetDeviceQueue(m_gfxDevice.m_device, m_gfxDevice.m_familyIndex, 0, &m_queue);

    m_camera.position = { 0.0f, 0.0f, 0.0f };
	m_camera.orientation = { 0.0f, 0.0f, 0.0f, 1.0f };
	m_camera.fovY = glm::radians(70.f);
	m_camera.znear = 0.1f;

    // material index 0 is always dummy
	m_materials.resize(1);
	m_materials[0].diffuseFactor = vec4(1);

    m_sunDirection = normalize(vec3(1.0f, 1.0f, 1.0f));

    createBuffer(m_buffers.m_scratch, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    loadGLTFScene("../../../Documents/github/niagara_bistro/bistrox.gltf");
}

void Renderer::createPrograms()
{
	bool rcs = loadShaders(m_shaders, "", "shaders/");
	assert(rcs);

    m_textureSetLayout = createDescriptorArrayLayout(m_gfxDevice.m_device);
    m_pipelines.m_pipelineCache = 0;

    m_programs.m_drawcullProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["drawcull.comp"] }, sizeof(CullData));
    m_programs.m_tasksubmitProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["tasksubmit.comp"] }, 0);
    m_programs.m_clustersubmitProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["clustersubmit.comp"] }, 0);
    m_programs.m_clustercullProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["clustercull.comp"] }, sizeof(CullData));
    m_programs.m_depthreduceProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["depthreduce.comp"] }, sizeof(vec4));
    m_programs.m_meshtaskProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_GRAPHICS, { &m_shaders["meshlet.task"], &m_shaders["meshlet.mesh"], &m_shaders["mesh.frag"] }, sizeof(Globals), m_textureSetLayout);
    m_programs.m_finalProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["final.comp"] }, sizeof(ShadeData));
}

void Renderer::createPipelines()
{
    auto replace = [&](VkPipeline& pipeline, VkPipeline newPipeline)
    {
        if (pipeline)
            vkDestroyPipeline(m_gfxDevice.m_device, pipeline, 0);
        assert(newPipeline);
        pipeline = newPipeline;
        m_pipelines.m_pipelines.push_back(newPipeline);
    };

    m_pipelines.m_pipelines.clear();

    replace(m_pipelines.m_taskcullPipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_programs.m_drawcullProgram, { /* LATE= */ false, /* TASK= */ true }));
    replace(m_pipelines.m_taskculllatePipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_programs.m_drawcullProgram, { /* LATE= */ true, /* TASK= */ true }));

    replace(m_pipelines.m_tasksubmitPipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_programs.m_tasksubmitProgram));

    replace(m_pipelines.m_depthreducePipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_programs.m_depthreduceProgram));

    replace(m_pipelines.m_meshtaskPipeline, createGraphicsPipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_gfxDevice.m_gbufferInfo, m_programs.m_meshtaskProgram, { /* LATE= */ false, /* TASK= */ true }));
    replace(m_pipelines.m_meshtasklatePipeline, createGraphicsPipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_gfxDevice.m_gbufferInfo, m_programs.m_meshtaskProgram, { /* LATE= */ true, /* TASK= */ true }));
    replace(m_pipelines.m_meshtaskpostPipeline, createGraphicsPipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_gfxDevice.m_gbufferInfo, m_programs.m_meshtaskProgram, { /* LATE= */ true, /* TASK= */ true, /* POST= */ 1 }));

    replace(m_pipelines.m_finalPipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.m_pipelineCache, m_programs.m_finalProgram));

}

void Renderer::createFramesData()
{
    m_queryPoolTimestamp = createQueryPool(m_gfxDevice.m_device, 128, VK_QUERY_TYPE_TIMESTAMP);
	assert(m_queryPoolTimestamp);

	m_queryPoolPipeline = createQueryPool(m_gfxDevice.m_device, 4, VK_QUERY_TYPE_PIPELINE_STATISTICS);
	assert(m_queryPoolPipeline);

    m_gfxDevice.m_swapchainImageViews.resize(m_gfxDevice.m_swapchain.imageCount);

    for (int i = 0; i < FRAMES_COUNT; i++)
    {

        m_frames[i].m_commandPool = createCommandPool(m_gfxDevice.m_device, m_gfxDevice.m_familyIndex);
        assert(m_frames[i].m_commandPool);

        VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocateInfo.commandPool = m_frames[i].m_commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
    
        m_frames[i].m_commandBuffer = 0;
        VK_CHECK(vkAllocateCommandBuffers(m_gfxDevice.m_device, &allocateInfo, &m_frames[i].m_commandBuffer));

        m_frames[i].m_waitSemaphore = createSemaphore(m_gfxDevice.m_device); // acquireSemaphore
        assert(m_frames[i].m_waitSemaphore);
    
        m_frames[i].m_signalSemaphore = createSemaphore(m_gfxDevice.m_device); // releaseSemaphore
        assert(m_frames[i].m_signalSemaphore);

        m_frames[i].m_renderFence = createFence(m_gfxDevice.m_device); // frameFence
        assert(m_frames[i].m_renderFence);
        // VK_CHECK(vkResetFences(m_gfxDevice.m_device, 1, &m_frames[i].m_renderFence));

    }

    m_immCommandPool = createCommandPool(m_gfxDevice.m_device, m_gfxDevice.m_familyIndex);
    VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocateInfo.commandPool = m_immCommandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(m_gfxDevice.m_device, &allocateInfo, &m_immCommandBuffer));

	m_samplers.m_textureSampler = createSampler(m_gfxDevice.m_device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	assert(m_samplers.m_textureSampler);

	m_samplers.m_readSampler = createSampler(m_gfxDevice.m_device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	assert(m_samplers.m_readSampler);

	m_samplers.m_depthSampler = createSampler(m_gfxDevice.m_device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_REDUCTION_MODE_MIN);
	assert(m_samplers.m_depthSampler);

}

bool Renderer::beginFrame()
{
    printf("vkWaitForFences \n");
    VK_CHECK(vkWaitForFences(m_gfxDevice.m_device, 1, &m_frames[m_currentFrameIndex].m_renderFence, VK_TRUE, ~0ull));
    printf("vkResetFences \n");
    VK_CHECK(vkResetFences(m_gfxDevice.m_device, 1, &m_frames[m_currentFrameIndex].m_renderFence));

    m_currentFrameIndex = m_frameIndex % FRAMES_COUNT;
    m_lastFrameIndex = (m_frameIndex + FRAMES_COUNT - 1) % FRAMES_COUNT;
    m_frames[m_currentFrameIndex].m_frameTimeStamp = std::chrono::system_clock::now();
    
    m_frames[m_currentFrameIndex].m_deltaTime = (std::chrono::duration_cast<std::chrono::milliseconds>(m_frames[m_lastFrameIndex].m_frameTimeStamp - m_frames[m_currentFrameIndex].m_frameTimeStamp)).count();

    SwapchainStatus swapchainStatus = updateSwapchain(m_gfxDevice.m_swapchain, m_gfxDevice.m_physicalDevice, m_gfxDevice.m_device, m_gfxDevice.m_surface, m_gfxDevice.m_familyIndex, m_gfxDevice.m_window, m_gfxDevice.m_swapchainFormat);

    if (swapchainStatus == Swapchain_NotReady)
        return false;

    if (swapchainStatus == Swapchain_Resized || !m_depthTarget.image)
    {
        printf("Swapchain: %dx%d\n", m_gfxDevice.m_swapchain.width, m_gfxDevice.m_swapchain.height);

        for (Image& image : m_gbufferTargets)
            if (image.image)
                destroyImage(image, m_gfxDevice.m_device);
        if (m_depthTarget.image)
            destroyImage(m_depthTarget, m_gfxDevice.m_device);

        if (m_depthPyramid.image)
        {
            for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
                vkDestroyImageView(m_gfxDevice.m_device, m_depthPyramidMips[i], 0);
            destroyImage(m_depthPyramid, m_gfxDevice.m_device);
        }

        if (m_shadowTarget.image)
            destroyImage(m_shadowTarget, m_gfxDevice.m_device);
        if (m_shadowblurTarget.image)
            destroyImage(m_shadowblurTarget, m_gfxDevice.m_device);

        for (uint32_t i = 0; i < GBUFFER_COUNT; ++i)
            createImage(m_gbufferTargets[i], m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.m_swapchain.width, m_gfxDevice.m_swapchain.height, 1, m_gbufferFormats[i], VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        createImage(m_depthTarget, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.m_swapchain.width, m_gfxDevice.m_swapchain.height, 1, m_gfxDevice.m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        createImage(m_shadowTarget, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.m_swapchain.width, m_gfxDevice.m_swapchain.height, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        createImage(m_shadowblurTarget, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.m_swapchain.width, m_gfxDevice.m_swapchain.height, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        // Note: previousPow2 makes sure all reductions are at most by 2x2 which makes sure they are conservative
        m_depthPyramidWidth = previousPow2(m_gfxDevice.m_swapchain.width);
        m_depthPyramidHeight = previousPow2(m_gfxDevice.m_swapchain.height);
        m_depthPyramidLevels = getImageMipLevels(m_depthPyramidWidth, m_depthPyramidHeight);

        createImage(m_depthPyramid, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_depthPyramidWidth, m_depthPyramidHeight, m_depthPyramidLevels, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
        {
            m_depthPyramidMips[i] = createImageView(m_gfxDevice.m_device, m_depthPyramid.image, VK_FORMAT_R32_SFLOAT, i, 1);
            assert(m_depthPyramidMips[i]);
        }

        for (uint32_t i = 0; i < m_gfxDevice.m_swapchain.imageCount; ++i)
        {
            if (m_gfxDevice.m_swapchainImageViews[i])
                vkDestroyImageView(m_gfxDevice.m_device, m_gfxDevice.m_swapchainImageViews[i], 0);

                m_gfxDevice.m_swapchainImageViews[i] = createImageView(m_gfxDevice.m_device, m_gfxDevice.m_swapchain.images[i], m_gfxDevice.m_swapchainFormat, 0, 1);
        }
    }

    VkResult acquireResult = vkAcquireNextImageKHR(m_gfxDevice.m_device, m_gfxDevice.m_swapchain.swapchain, ~0ull, m_frames[m_currentFrameIndex].m_waitSemaphore, VK_NULL_HANDLE, &m_imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        return false; // attempting to render to an out-of-date swapchain would break semaphore synchronization
    VK_CHECK_SWAPCHAIN(acquireResult);

    VK_CHECK(vkResetCommandPool(m_gfxDevice.m_device, m_frames[m_currentFrameIndex].m_commandPool, 0));

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(m_frames[m_currentFrameIndex].m_commandBuffer, &beginInfo));

    vkCmdResetQueryPool(m_frames[m_currentFrameIndex].m_commandBuffer, m_queryPoolTimestamp, 0, 128);
    vkCmdWriteTimestamp(m_frames[m_currentFrameIndex].m_commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, 0);

    if (!m_buffers.m_drawVisibilityCleared)
    {
        // Use the new free function to clear buffers
        clearBuffer(
            m_frames[m_currentFrameIndex].m_commandBuffer,
            m_buffers.m_drawVisibility.buffer,
            sizeof(uint32_t) * m_draws.size(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        m_buffers.m_drawVisibilityCleared = true;
    }

    if (!m_buffers.m_meshletVisibilityCleared)
    {
        // Use the new free function to clear buffers
        clearBuffer(
            m_frames[m_currentFrameIndex].m_commandBuffer,
            m_buffers.m_meshletVisibility.buffer,
            m_buffers.m_meshletVisibilityBytes,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        m_buffers.m_meshletVisibilityCleared = true;
    }

    // raytracing
    uint32_t timestamp = 21;

    vkCmdWriteTimestamp(m_frames[m_currentFrameIndex].m_commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    if (m_tlasNeedsRebuild)
    {
        buildTLAS(m_gfxDevice.m_device, m_frames[m_currentFrameIndex].m_commandBuffer, m_buffers.m_tlas, m_buffers.m_tlasBuffer, m_buffers.m_tlasScratchBuffer, m_buffers.m_tlasInstanceBuffer, m_draws.size(), VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR);
        m_tlasNeedsRebuild = false;
    }

    vkCmdWriteTimestamp(m_frames[m_currentFrameIndex].m_commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);


    // Use the new free function to calculate view matrix
    m_view = calculateViewMatrix(m_camera);
    
    // Calculate projection matrix
    m_projection = perspectiveProjection(m_camera.fovY, float(m_gfxDevice.m_swapchain.width) / float(m_gfxDevice.m_swapchain.height), m_camera.znear);

    m_projectionT = transpose(m_projection);

    // Use the new free function to set up culling data
    m_cullData = setupCullingData(
        m_view,
        m_projection,
        m_camera,
        m_drawDistance,
        uint32_t(m_draws.size()),
        float(m_depthPyramidWidth),
        float(m_depthPyramidHeight),
        float(m_gfxDevice.m_swapchain.height)
    );

    Globals m_globals = {};
    m_globals.projection = m_projection;
    m_globals.cullData = m_cullData;
    m_globals.screenWidth = float(m_gfxDevice.m_swapchain.width);
    m_globals.screenHeight = float(m_gfxDevice.m_swapchain.height);

    // Use the new free function to create image barriers for render targets
    VkImageMemoryBarrier2 renderBeginBarriers[GBUFFER_COUNT + 1];
    
    // Prepare color image array for the function
    VkImage colorImages[GBUFFER_COUNT];
    for (uint32_t i = 0; i < GBUFFER_COUNT; ++i)
        colorImages[i] = m_gbufferTargets[i].image;
    
    createRenderTargetBarriers(m_depthTarget.image, colorImages, GBUFFER_COUNT, renderBeginBarriers);
    
    pipelineBarrier(m_frames[m_currentFrameIndex].m_commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, COUNTOF(renderBeginBarriers), renderBeginBarriers);

    vkCmdResetQueryPool(m_frames[m_currentFrameIndex].m_commandBuffer, m_queryPoolPipeline, 0, 4);

    return true;
}

void Renderer::drawCull(VkPipeline pipeline, uint32_t timestamp, const char *phase, bool late, unsigned int postPass)
{
    uint32_t rasterizationStage = VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;

    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].m_commandBuffer;

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    VkBufferMemoryBarrier2 prefillBarrier = bufferBarrier(m_buffers.m_commandCount.buffer,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    pipelineBarrier(commandBuffer, 0, 1, &prefillBarrier, 0, nullptr);

    // printf("vkCmdFillBuffer \n");
    vkCmdFillBuffer(commandBuffer, m_buffers.m_commandCount.buffer, 0, 4, 0);

    // pyramid barrier is tricky: our frame sequence is cull -> render -> pyramid -> cull -> render
    // the first cull (late=0) doesn't read pyramid data BUT the read in the shader is guarded by a push constant value (which could be specialization constant but isn't due to AMD bug)
    // the second cull (late=1) does read pyramid data that was written in the pyramid stage
    // as such, second cull needs to transition GENERAL->GENERAL with a COMPUTE->COMPUTE barrier, but the first cull needs to have a dummy transition because pyramid starts in UNDEFINED state on first frame
    VkImageMemoryBarrier2 pyramidBarrier = imageBarrier(m_depthPyramid.image,
        late ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0, late ? VK_ACCESS_SHADER_WRITE_BIT : 0, late ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);

    VkBufferMemoryBarrier2 fillBarriers[] = {
        bufferBarrier(m_buffers.m_taskCommands.buffer,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | rasterizationStage, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT),
        bufferBarrier(m_buffers.m_commandCount.buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT),
    };
    // printf("pipelineBarrier \n");
    pipelineBarrier(commandBuffer, 0, COUNTOF(fillBarriers), fillBarriers, 1, &pyramidBarrier);

    {
        CullData passData = m_cullData;
        passData.clusterBackfaceEnabled = postPass == 0;
        passData.postPass = postPass;

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        DescriptorInfo pyramidDesc(m_samplers.m_depthSampler, m_depthPyramid.imageView, VK_IMAGE_LAYOUT_GENERAL);
        DescriptorInfo descriptors[] = { m_buffers.m_draw.buffer, m_buffers.m_meshesh.buffer, m_buffers.m_taskCommands.buffer, m_buffers.m_commandCount.buffer, m_buffers.m_drawVisibility.buffer, pyramidDesc };

        printf("dispatch %lu \n", m_draws.size());
        dispatch(commandBuffer, m_programs.m_drawcullProgram, uint32_t(m_draws.size()), 1, passData, descriptors);
        printf("dispatch completed \n");
    }

    VkBufferMemoryBarrier2 syncBarrier = bufferBarrier(m_buffers.m_commandCount.buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

    printf("pipelineBarrier \n");
    pipelineBarrier(commandBuffer, 0, 1, &syncBarrier, 0, nullptr);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.m_tasksubmitPipeline);

    DescriptorInfo descriptors[] = { m_buffers.m_commandCount.buffer, m_buffers.m_taskCommands.buffer };
    vkCmdPushDescriptorSetWithTemplate(commandBuffer, m_programs.m_tasksubmitProgram.updateTemplate, m_programs.m_tasksubmitProgram.layout, 0, descriptors);

    printf("vkCmdDispatch \n");
    vkCmdDispatch(commandBuffer, 1, 1, 1);

    VkBufferMemoryBarrier2 cullBarriers[] = {
        bufferBarrier(m_buffers.m_taskCommands.buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | rasterizationStage, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT),
        bufferBarrier(m_buffers.m_commandCount.buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
    };

    printf("pipelineBarrier \n");
    pipelineBarrier(commandBuffer, 0, COUNTOF(cullBarriers), cullBarriers, 0, nullptr);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);
}

void Renderer::drawRender(bool late, const VkClearColorValue &colorClear, const VkClearDepthStencilValue &depthClear, uint32_t query, uint32_t timestamp, const char *phase, unsigned int postPass)
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].m_commandBuffer;

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    vkCmdBeginQuery(commandBuffer, m_queryPoolPipeline, query, 0);

    // Use the new free functions to create attachments
    VkRenderingAttachmentInfo gbufferAttachments[GBUFFER_COUNT];
    for (uint32_t i = 0; i < GBUFFER_COUNT; ++i)
    {
        gbufferAttachments[i] = createColorAttachmentInfo(
            m_gbufferTargets[i].imageView,
            VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            late ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
            colorClear
        );
    }

    VkRenderingAttachmentInfo depthAttachment = createDepthAttachmentInfo(
        m_depthTarget.imageView,
        VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        late ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR,
        depthClear
    );

    // Use the new free function for creating render info
    VkRenderingInfo passInfo = createRenderingInfo(
        m_gfxDevice.m_swapchain.width,
        m_gfxDevice.m_swapchain.height,
        gbufferAttachments,
        GBUFFER_COUNT,
        &depthAttachment
    );

    vkCmdBeginRendering(commandBuffer, &passInfo);

    // Use the new free functions for viewport and scissor
    VkViewport viewport = createViewport(float(m_gfxDevice.m_swapchain.width), float(m_gfxDevice.m_swapchain.height));
    VkRect2D scissor = createScissorRect(uint32_t(m_gfxDevice.m_swapchain.width), uint32_t(m_gfxDevice.m_swapchain.height));

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdSetCullMode(commandBuffer, postPass == 0 ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    vkCmdSetDepthBias(commandBuffer, postPass == 0 ? 0 : 16, 0, postPass == 0 ? 0 : 1);

    Globals passGlobals = m_globals;
    passGlobals.cullData.postPass = postPass;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postPass >= 1 ? m_pipelines.m_meshtaskpostPipeline : late ? m_pipelines.m_meshtasklatePipeline
                                                                                                                    : m_pipelines.m_meshtaskPipeline);

    DescriptorInfo pyramidDesc(m_samplers.m_depthSampler, m_depthPyramid.imageView, VK_IMAGE_LAYOUT_GENERAL);
    DescriptorInfo descriptors[] = { m_buffers.m_taskCommands.buffer, m_buffers.m_draw.buffer, m_buffers.m_meshlets.buffer, m_buffers.m_meshletdata.buffer, m_buffers.m_vertices.buffer, m_buffers.m_meshletVisibility.buffer, pyramidDesc, m_samplers.m_textureSampler, m_buffers.m_materials.buffer };
    vkCmdPushDescriptorSetWithTemplate(commandBuffer, m_programs.m_meshtaskProgram.updateTemplate, m_programs.m_meshtaskProgram.layout, 0, descriptors);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_programs.m_meshtaskProgram.layout, 1, 1, &m_textureSet.second, 0, nullptr);

    vkCmdPushConstants(commandBuffer, m_programs.m_meshtaskProgram.layout, m_programs.m_meshtaskProgram.pushConstantStages, 0, sizeof(m_globals), &passGlobals);
    vkCmdDrawMeshTasksIndirectEXT(commandBuffer, m_buffers.m_commandCount.buffer, 4, 1, 0);

    vkCmdEndRendering(commandBuffer);

    vkCmdEndQuery(commandBuffer, m_queryPoolPipeline, query);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);
}

void Renderer::drawPyramid(uint32_t timestamp)
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].m_commandBuffer;

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    VkImageMemoryBarrier2 depthBarriers[] = {
        imageBarrier(m_depthTarget.image,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT),
        imageBarrier(m_depthPyramid.image,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL)
    };

    pipelineBarrier(commandBuffer, 0, 0, nullptr, COUNTOF(depthBarriers), depthBarriers);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.m_depthreducePipeline);

    for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
    {
        DescriptorInfo sourceDepth = (i == 0)
                                         ? DescriptorInfo(m_samplers.m_depthSampler, m_depthTarget.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         : DescriptorInfo(m_samplers.m_depthSampler, m_depthPyramidMips[i - 1], VK_IMAGE_LAYOUT_GENERAL);

        DescriptorInfo descriptors[] = { { m_depthPyramidMips[i], VK_IMAGE_LAYOUT_GENERAL }, sourceDepth };

        uint32_t levelWidth = std::max(1u, m_depthPyramidWidth >> i);
        uint32_t levelHeight = std::max(1u, m_depthPyramidHeight >> i);

        vec4 reduceData = vec4(levelWidth, levelHeight, 0, 0);

        dispatch(commandBuffer, m_programs.m_depthreduceProgram, levelWidth, levelHeight, reduceData, descriptors);

        VkImageMemoryBarrier2 reduceBarrier = imageBarrier(m_depthPyramid.image,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_ASPECT_COLOR_BIT, i, 1);

        pipelineBarrier(commandBuffer, 0, 0, nullptr, 1, &reduceBarrier);
    }

    VkImageMemoryBarrier2 depthWriteBarrier = imageBarrier(m_depthTarget.image,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    pipelineBarrier(commandBuffer, 0, 0, nullptr, 1, &depthWriteBarrier);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);
}

void Renderer::drawDebug()
{
}

void Renderer::drawShadow()
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].m_commandBuffer;

}

void Renderer::draw()
{
    // at the beginning we are checking if resolution changed
    // and should skip this cycle until swapchain is good.
    printf("beginFrame \n");
    if (!beginFrame())
    {
        printf("Something wrong with beginFrame \n");
        return;
    }
    printf("drawCull \n");
    drawCull(m_pipelines.m_taskcullPipeline, 2, "early cull", /* late= */ false);
    // drawCull(m_pipelines.drawcullPipeline, 2, "early cull", /* late= */ false);
    printf("drawRender \n");
    drawRender(/* late= */ false, m_colorClear, m_depthClear, 0, 4, "early render");
    printf("drawPyramid \n");
    drawPyramid(6);
    printf("drawCull \n");
    drawCull(m_pipelines.m_taskculllatePipeline, 8, "late cull", /* late= */ true);
    printf("drawRender \n");
    drawRender(/* late= */ true, m_colorClear, m_depthClear, 1, 10, "late render");

    if (m_meshPostPasses >> 1)
    {
        // post cull: frustum + occlusion cull & fill extra objects
        printf(" \n");
        drawCull(m_pipelines.m_taskculllatePipeline , 12, "post cull", /* late= */ true, /* postPass= */ 1);

        // post render: render extra objects
        printf(" \n");
        drawRender(/* late= */ true, m_colorClear, m_depthClear, 2, 14, "post render", /* postPass= */ 1);
    }

    // should be replaced by raytracing
    {
        auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].m_commandBuffer;

        VkImageMemoryBarrier2 dummyBarrier =
            imageBarrier(m_shadowTarget.image,
                0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);

        pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &dummyBarrier);

        uint32_t timestamp = 16; // todo: we need an actual profiler
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 2);
    }

    {
        auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].m_commandBuffer;
        uint32_t timestamp = 19;

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.m_finalPipeline);

            DescriptorInfo descriptors[] = { { m_gfxDevice.m_swapchainImageViews[m_imageIndex], VK_IMAGE_LAYOUT_GENERAL }, { m_samplers.m_readSampler, m_gbufferTargets[0].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, { m_samplers.m_readSampler, m_gbufferTargets[1].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, { m_samplers.m_readSampler, m_depthTarget.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, { m_samplers.m_readSampler, m_shadowTarget.imageView, VK_IMAGE_LAYOUT_GENERAL } };

            ShadeData shadeData = {};
            shadeData.cameraPosition = m_camera.position;
            shadeData.sunDirection = m_sunDirection;
            shadeData.shadowsEnabled = false;
            shadeData.inverseViewProjection = inverse(m_projection * m_view);
            shadeData.imageSize = vec2(float(m_gfxDevice.m_swapchain.width), float(m_gfxDevice.m_swapchain.height));

            dispatch(commandBuffer, m_programs.m_finalProgram, m_gfxDevice.m_swapchain.width, m_gfxDevice.m_swapchain.height, shadeData, descriptors);
        }

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);
    }

    // Import Global illumination here
    // drawShadow();

    // drawDebug();

    printf("endFrame \n");
    endFrame();
}

void Renderer::endFrame()
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].m_commandBuffer;

    VkImageMemoryBarrier2 presentBarrier = imageBarrier(m_gfxDevice.m_swapchain.images[m_imageIndex],
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
        0, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &presentBarrier);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, 1);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_frames[m_currentFrameIndex].m_waitSemaphore;
    submitInfo.pWaitDstStageMask = &submitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_frames[m_currentFrameIndex].m_signalSemaphore;

    VK_CHECK_FORCE(vkQueueSubmit(m_queue, 1, &submitInfo, m_frames[m_currentFrameIndex].m_renderFence));

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_frames[m_currentFrameIndex].m_signalSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_gfxDevice.m_swapchain.swapchain;
    presentInfo.pImageIndices = &m_imageIndex;

    VK_CHECK_SWAPCHAIN(vkQueuePresentKHR(m_queue, &presentInfo));

    m_frameIndex++;
}

bool Renderer::loadGLTFScene(std::string filename)
{
    // std::string filename = "../../../Documents/github/niagara_bistro/bistrox.gltf";
    if (!loadScene(m_geometry, m_materials, m_draws, m_texturePaths, m_animations, m_camera, m_sunDirection, filename.c_str(), true, false))
    {
        printf("Error: scene %s failed to load\n", filename.c_str());
        std::exit(1);
    }

	for (size_t i = 0; i < m_texturePaths.size(); ++i)
	{
		Image image;
		if (!loadDDSImage(image, m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_gfxDevice.m_memoryProperties, m_buffers.m_scratch, m_texturePaths[i].c_str()))
		{
			printf("Error: image N: %ld %s failed to load\n", i, m_texturePaths[i].c_str());
			return 1;
		}

		m_images.push_back(image);
	}

	// printf("Loaded %d textures (%.2f MB) in %.2f sec\n", int(m_images.size()), double(m_imageMemory) / 1e6, glfwGetTime() - imageTimer);

	uint32_t descriptorCount = uint32_t(m_texturePaths.size() + 1);
	m_textureSet = createDescriptorArray(m_gfxDevice.m_device, m_textureSetLayout, descriptorCount);

	for (size_t i = 0; i < m_texturePaths.size(); ++i)
	{
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageView = m_images[i].imageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = m_textureSet.second;
		write.dstBinding = 0;
		write.dstArrayElement = uint32_t(i + 1);
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		write.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(m_gfxDevice.m_device, 1, &write, 0, nullptr);
	}

    printf("Geometry: VB %.2f MB, IB %.2f MB, meshlets %.2f MB\n",
	    double(m_geometry.vertices.size() * sizeof(Vertex)) / 1e6,
	    double(m_geometry.indices.size() * sizeof(uint32_t)) / 1e6,
	    double(m_geometry.meshlets.size() * sizeof(Meshlet) + m_geometry.meshletdata.size() * sizeof(uint32_t)) / 1e6);

    uint32_t meshletVisibilityCount = 0;

    for (size_t i = 0; i < m_draws.size(); ++i)
    {
        MeshDraw& draw = m_draws[i];
        const Mesh& mesh = m_geometry.meshes[draw.meshIndex];

        draw.meshletVisibilityOffset = meshletVisibilityCount;

        uint32_t meshletCount = 0;
        for (uint32_t i = 0; i < mesh.lodCount; ++i)
            meshletCount = std::max(meshletCount, mesh.lods[i].meshletCount);

        meshletVisibilityCount += meshletCount;
        m_meshPostPasses |= 1 << draw.postPass;
    }

    m_buffers.m_meshletVisibilityBytes = (meshletVisibilityCount + 31) / 32 * sizeof(uint32_t);

    uint32_t raytracingBufferFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    createBuffer(m_buffers.m_meshesh, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.meshes.size() * sizeof(Mesh), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.m_materials, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_materials.size() * sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.m_vertices, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | raytracingBufferFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.m_indices, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | raytracingBufferFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.m_meshlets, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.meshlets.size() * sizeof(Meshlet), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.m_meshletdata, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.meshletdata.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.m_meshesh, m_buffers.m_scratch, m_geometry.meshes.data(), m_geometry.meshes.size() * sizeof(Mesh));
    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.m_materials, m_buffers.m_scratch, m_materials.data(), m_materials.size() * sizeof(Material));

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.m_vertices, m_buffers.m_scratch, m_geometry.vertices.data(), m_geometry.vertices.size() * sizeof(Vertex));
    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.m_indices, m_buffers.m_scratch, m_geometry.indices.data(), m_geometry.indices.size() * sizeof(uint32_t));

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.m_meshlets, m_buffers.m_scratch, m_geometry.meshlets.data(), m_geometry.meshlets.size() * sizeof(Meshlet));
    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.m_meshletdata, m_buffers.m_scratch, m_geometry.meshletdata.data(), m_geometry.meshletdata.size() * sizeof(uint32_t));

    createBuffer(m_buffers.m_draw, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_draws.size() * sizeof(MeshDraw), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    createBuffer(m_buffers.m_drawVisibility, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_draws.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.m_taskCommands, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, TASK_WGLIMIT * sizeof(MeshTaskCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.m_commandCount, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // TODO: there's a way to implement cluster visibility persistence *without* using bitwise storage at all, which may be beneficial on the balance, so we should try that.
    // *if* we do that, we can drop meshletVisibilityOffset et al from everywhere
    createBuffer(m_buffers.m_meshletVisibility, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_buffers.m_meshletVisibilityBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.m_draw, m_buffers.m_scratch, m_draws.data(), m_draws.size() * sizeof(MeshDraw));

    std::vector<VkDeviceSize> compactedSizes;
    buildBLAS(m_gfxDevice.m_device, m_geometry.meshes, m_buffers.m_vertices, m_buffers.m_indices, m_blas, compactedSizes, m_buffers.m_blasBuffer, m_immCommandPool, m_immCommandBuffer, m_queue, m_gfxDevice.m_memoryProperties);
    compactBLAS(m_gfxDevice.m_device, m_blas, compactedSizes, m_buffers.m_blasBuffer, m_immCommandPool, m_immCommandBuffer, m_queue, m_gfxDevice.m_memoryProperties);

    m_blasAddresses.resize(m_blas.size());

    for (size_t i = 0; i < m_blas.size(); ++i)
    {
        VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        info.accelerationStructure = m_blas[i];

        m_blasAddresses[i] = vkGetAccelerationStructureDeviceAddressKHR(m_gfxDevice.m_device, &info);
    }

    createBuffer(m_buffers.m_tlasInstanceBuffer, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, sizeof(VkAccelerationStructureInstanceKHR) * m_draws.size(), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    for (size_t i = 0; i < m_draws.size(); ++i)
    {
        const MeshDraw& draw = m_draws[i];
        assert(draw.meshIndex < m_blas.size());

        VkAccelerationStructureInstanceKHR instance = {};
        fillInstanceRT(instance, draw, uint32_t(i), m_blasAddresses[draw.meshIndex]);

        memcpy(static_cast<VkAccelerationStructureInstanceKHR*>(m_buffers.m_tlasInstanceBuffer.data) + i, &instance, sizeof(VkAccelerationStructureInstanceKHR));
    }

    m_buffers.m_tlas = createTLAS(m_gfxDevice.m_device, m_buffers.m_tlasBuffer, m_buffers.m_tlasScratchBuffer, m_buffers.m_tlasInstanceBuffer, m_draws.size(), m_gfxDevice.m_memoryProperties);

    return true;
}

void Renderer::cleanup()
{
    printf("Doing clenup of resources created by renderer\n");
    vkDestroyDescriptorPool(m_gfxDevice.m_device, m_textureSet.first, 0);

	for (Image& image : m_images)
		destroyImage(image, m_gfxDevice.m_device);

	for (Image& image : m_gbufferTargets)
		if (image.image)
			destroyImage(image, m_gfxDevice.m_device);
	if (m_depthTarget.image)
		destroyImage(m_depthTarget, m_gfxDevice.m_device);

	if (m_depthPyramid.image)
	{
		for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
			vkDestroyImageView(m_gfxDevice.m_device, m_depthPyramidMips[i], 0);
		destroyImage(m_depthPyramid, m_gfxDevice.m_device);
	}

	if (m_shadowTarget.image)
		destroyImage(m_shadowTarget, m_gfxDevice.m_device);
	if (m_shadowblurTarget.image)
		destroyImage(m_shadowblurTarget, m_gfxDevice.m_device);

	for (uint32_t i = 0; i < m_gfxDevice.m_swapchain.imageCount; ++i)
		if (m_gfxDevice.m_swapchainImageViews[i])
			vkDestroyImageView(m_gfxDevice.m_device, m_gfxDevice.m_swapchainImageViews[i], 0);

	destroyBuffer(m_buffers.m_meshesh, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_materials, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_draw, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_drawVisibility, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_taskCommands, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_commandCount, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.m_meshlets, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.m_meshletdata, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.m_meshletVisibility, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.m_tlasBuffer, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.m_blasBuffer, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.m_tlasScratchBuffer, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.m_tlasInstanceBuffer, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_indices, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_vertices, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.m_scratch, m_gfxDevice.m_device);

    vkDestroyAccelerationStructureKHR(m_gfxDevice.m_device, m_buffers.m_tlas, 0);
    for (VkAccelerationStructureKHR as : m_blas)
        vkDestroyAccelerationStructureKHR(m_gfxDevice.m_device, as, 0);

    for (int i=0; i<FRAMES_COUNT; i++)
    {
        vkDestroyCommandPool(m_gfxDevice.m_device, m_frames[i].m_commandPool, 0);
    }

	vkDestroyQueryPool(m_gfxDevice.m_device, m_queryPoolTimestamp, 0);
	vkDestroyQueryPool(m_gfxDevice.m_device, m_queryPoolPipeline, 0);

	destroySwapchain(m_gfxDevice.m_device, m_gfxDevice.m_swapchain);

	for (VkPipeline pipeline : m_pipelines.m_pipelines)
		vkDestroyPipeline(m_gfxDevice.m_device, pipeline, 0);

	destroyProgram(m_gfxDevice.m_device, m_programs.m_debugtextProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.m_drawcullProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.m_tasksubmitProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.m_clustersubmitProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.m_clustercullProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.m_depthreduceProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.m_meshProgram);

    destroyProgram(m_gfxDevice.m_device, m_programs.m_meshtaskProgram);
    destroyProgram(m_gfxDevice.m_device, m_programs.m_clusterProgram);

	destroyProgram(m_gfxDevice.m_device, m_programs.m_finalProgram);

    destroyProgram(m_gfxDevice.m_device, m_programs.m_shadowProgram);
    destroyProgram(m_gfxDevice.m_device, m_programs.m_shadowfillProgram);
    destroyProgram(m_gfxDevice.m_device, m_programs.m_shadowblurProgram);

	vkDestroyDescriptorSetLayout(m_gfxDevice.m_device, m_textureSetLayout, 0);

	vkDestroySampler(m_gfxDevice.m_device, m_samplers.m_textureSampler, 0);
	vkDestroySampler(m_gfxDevice.m_device, m_samplers.m_readSampler, 0);
	vkDestroySampler(m_gfxDevice.m_device, m_samplers.m_depthSampler, 0);

    for (int i=0; i<FRAMES_COUNT; i++)
    {
        vkDestroyFence(m_gfxDevice.m_device, m_frames[i].m_renderFence, 0);
        vkDestroySemaphore(m_gfxDevice.m_device, m_frames[i].m_signalSemaphore, 0);
        vkDestroySemaphore(m_gfxDevice.m_device, m_frames[i].m_waitSemaphore, 0);
    }

	vkDestroySurfaceKHR(m_gfxDevice.m_instance, m_gfxDevice.m_surface, 0);


	vkDestroyDevice(m_gfxDevice.m_device, 0);

	vkDestroyInstance(m_gfxDevice.m_instance, 0);

	volkFinalize();
}
