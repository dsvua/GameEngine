#include "Renderer.h"
#include "niagara/math.h"
#include "niagara/common.h"
#include "niagara/config.h"
#include "niagara/scene.h"
#include "niagara/textures.h"
#include "niagara/scenert.h"
#include <stdarg.h>
#include <string.h>
// #include "volk.h"

mat4 perspectiveProjection(float fovY, float aspectWbyH, float zNear)
{
	float f = 1.0f / tanf(fovY / 2.0f);
	return mat4(
	    f / aspectWbyH, 0.0f, 0.0f, 0.0f,
	    0.0f, f, 0.0f, 0.0f,
	    0.0f, 0.0f, 0.0f, 1.0f,
	    0.0f, 0.0f, zNear, 0.0f);
}

vec4 normalizePlane(vec4 p)
{
	return p / length(vec3(p));
}

uint32_t previousPow2(uint32_t v)
{
	uint32_t r = 1;

	while (r * 2 < v)
		r *= 2;

	return r;
}

struct pcg32_random_t
{
	uint64_t state;
	uint64_t inc;
};

#define PCG32_INITIALIZER \
	{ \
		0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL \
	}

uint32_t pcg32_random_r(pcg32_random_t* rng)
{
	uint64_t oldstate = rng->state;
	// Advance internal state
	rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
	// Calculate output function (XSH RR), uses old state for max ILP
	uint32_t xorshifted = uint32_t(((oldstate >> 18u) ^ oldstate) >> 27u);
	uint32_t rot = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((32 - rot) & 31));
}

pcg32_random_t rngstate = PCG32_INITIALIZER;

double rand01()
{
	return pcg32_random_r(&rngstate) / double(1ull << 32);
}

uint32_t rand32()
{
	return pcg32_random_r(&rngstate);
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

Renderer::Renderer()
{
    m_gfxDevice = initDevice();
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

    m_gfxDevice.m_gbufferInfo.colorAttachmentCount = gbufferCount;
	m_gfxDevice.m_gbufferInfo.pColorAttachmentFormats = m_gbufferFormats;
	m_gfxDevice.m_gbufferInfo.depthAttachmentFormat = m_gfxDevice.depthFormat;

    createBuffer(m_buffers.scratch, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, 128 * 1024 * 1024, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    loadGLTFScene("../../../Documents/github/niagara_bistro/bistrox.gltf");
}

void Renderer::createPrograms()
{
	bool rcs = loadShaders(m_shaders, "", "shaders/");
	assert(rcs);

    m_textureSetLayout = createDescriptorArrayLayout(m_gfxDevice.m_device);
    m_pipelines.pipelineCache = 0;

    m_programs.drawcullProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["drawcull.comp"] }, sizeof(CullData));
    m_programs.tasksubmitProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["tasksubmit.comp"] }, 0);
    m_programs.clustersubmitProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["clustersubmit.comp"] }, 0);
    m_programs.clustercullProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["clustercull.comp"] }, sizeof(CullData));
    m_programs.depthreduceProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["depthreduce.comp"] }, sizeof(vec4));
    m_programs.meshtaskProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_GRAPHICS, { &m_shaders["meshlet.task"], &m_shaders["meshlet.mesh"], &m_shaders["mesh.frag"] }, sizeof(Globals), m_textureSetLayout);
    m_programs.finalProgram = createProgram(m_gfxDevice.m_device, VK_PIPELINE_BIND_POINT_COMPUTE, { &m_shaders["final.comp"] }, sizeof(ShadeData));
}

void Renderer::createPipelines()
{
    auto replace = [&](VkPipeline& pipeline, VkPipeline newPipeline)
    {
        if (pipeline)
            vkDestroyPipeline(m_gfxDevice.m_device, pipeline, 0);
        assert(newPipeline);
        pipeline = newPipeline;
        m_pipelines.pipelines.push_back(newPipeline);
    };

    m_pipelines.pipelines.clear();

    replace(m_pipelines.taskcullPipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_programs.drawcullProgram, { /* LATE= */ false, /* TASK= */ true }));
    replace(m_pipelines.taskculllatePipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_programs.drawcullProgram, { /* LATE= */ true, /* TASK= */ true }));

    replace(m_pipelines.tasksubmitPipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_programs.tasksubmitProgram));

    replace(m_pipelines.depthreducePipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_programs.depthreduceProgram));

    replace(m_pipelines.meshtaskPipeline, createGraphicsPipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_gfxDevice.m_gbufferInfo, m_programs.meshtaskProgram, { /* LATE= */ false, /* TASK= */ true }));
    replace(m_pipelines.meshtasklatePipeline, createGraphicsPipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_gfxDevice.m_gbufferInfo, m_programs.meshtaskProgram, { /* LATE= */ true, /* TASK= */ true }));
    replace(m_pipelines.meshtaskpostPipeline, createGraphicsPipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_gfxDevice.m_gbufferInfo, m_programs.meshtaskProgram, { /* LATE= */ true, /* TASK= */ true, /* POST= */ 1 }));

    replace(m_pipelines.finalPipeline, createComputePipeline(m_gfxDevice.m_device, m_pipelines.pipelineCache, m_programs.finalProgram));

}

void Renderer::createFramesData()
{
    m_queryPoolTimestamp = createQueryPool(m_gfxDevice.m_device, 128, VK_QUERY_TYPE_TIMESTAMP);
	assert(m_queryPoolTimestamp);

	m_queryPoolPipeline = createQueryPool(m_gfxDevice.m_device, 4, VK_QUERY_TYPE_PIPELINE_STATISTICS);
	assert(m_queryPoolPipeline);

    m_gfxDevice.swapchainImageViews.resize(m_gfxDevice.swapchain.imageCount);

    for (int i = 0; i < FRAMES_COUNT; i++)
    {

        m_frames[i].commandPool = createCommandPool(m_gfxDevice.m_device, m_gfxDevice.m_familyIndex);
        assert(m_frames[i].commandPool);

        VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocateInfo.commandPool = m_frames[i].commandPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
    
        m_frames[i].commandBuffer = 0;
        VK_CHECK(vkAllocateCommandBuffers(m_gfxDevice.m_device, &allocateInfo, &m_frames[i].commandBuffer));

        m_frames[i].waitSemaphore = createSemaphore(m_gfxDevice.m_device); // acquireSemaphore
        assert(m_frames[i].waitSemaphore);
    
        m_frames[i].signalSemaphore = createSemaphore(m_gfxDevice.m_device); // releaseSemaphore
        assert(m_frames[i].signalSemaphore);

        m_frames[i].renderFence = createFence(m_gfxDevice.m_device); // frameFence
        assert(m_frames[i].renderFence);
        // VK_CHECK(vkResetFences(m_gfxDevice.m_device, 1, &m_frames[i].renderFence));

    }

    m_immCommandPool = createCommandPool(m_gfxDevice.m_device, m_gfxDevice.m_familyIndex);
    VkCommandBufferAllocateInfo allocateInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocateInfo.commandPool = m_immCommandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VK_CHECK(vkAllocateCommandBuffers(m_gfxDevice.m_device, &allocateInfo, &m_immCommandBuffer));

	m_samplers.textureSampler = createSampler(m_gfxDevice.m_device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT);
	assert(m_samplers.textureSampler);

	m_samplers.readSampler = createSampler(m_gfxDevice.m_device, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
	assert(m_samplers.readSampler);

	m_samplers.depthSampler = createSampler(m_gfxDevice.m_device, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_REDUCTION_MODE_MIN);
	assert(m_samplers.depthSampler);

}

bool Renderer::beginFrame()
{
    printf("vkWaitForFences \n");
    VK_CHECK(vkWaitForFences(m_gfxDevice.m_device, 1, &m_frames[m_currentFrameIndex].renderFence, VK_TRUE, ~0ull));
    printf("vkResetFences \n");
    VK_CHECK(vkResetFences(m_gfxDevice.m_device, 1, &m_frames[m_currentFrameIndex].renderFence));

    m_currentFrameIndex = m_frameIndex % FRAMES_COUNT;
    m_lastFrameIndex = (m_frameIndex + FRAMES_COUNT - 1) % FRAMES_COUNT;
    m_frames[m_currentFrameIndex].frameTimeStamp = std::chrono::system_clock::now();
    
    m_frames[m_currentFrameIndex].deltaTime = (std::chrono::duration_cast<std::chrono::milliseconds>(m_frames[m_lastFrameIndex].frameTimeStamp - m_frames[m_currentFrameIndex].frameTimeStamp)).count();

    SwapchainStatus swapchainStatus = updateSwapchain(m_gfxDevice.swapchain, m_gfxDevice.m_physicalDevice, m_gfxDevice.m_device, m_gfxDevice.surface, m_gfxDevice.m_familyIndex, m_gfxDevice.m_window, m_gfxDevice.swapchainFormat);

    if (swapchainStatus == Swapchain_NotReady)
        return false;

    if (swapchainStatus == Swapchain_Resized || !m_depthTarget.image)
    {
        printf("Swapchain: %dx%d\n", m_gfxDevice.swapchain.width, m_gfxDevice.swapchain.height);

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

        for (uint32_t i = 0; i < gbufferCount; ++i)
            createImage(m_gbufferTargets[i], m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.swapchain.width, m_gfxDevice.swapchain.height, 1, m_gbufferFormats[i], VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        createImage(m_depthTarget, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.swapchain.width, m_gfxDevice.swapchain.height, 1, m_gfxDevice.depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        createImage(m_shadowTarget, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.swapchain.width, m_gfxDevice.swapchain.height, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        createImage(m_shadowblurTarget, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_gfxDevice.swapchain.width, m_gfxDevice.swapchain.height, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        // Note: previousPow2 makes sure all reductions are at most by 2x2 which makes sure they are conservative
        m_depthPyramidWidth = previousPow2(m_gfxDevice.swapchain.width);
        m_depthPyramidHeight = previousPow2(m_gfxDevice.swapchain.height);
        m_depthPyramidLevels = getImageMipLevels(m_depthPyramidWidth, m_depthPyramidHeight);

        createImage(m_depthPyramid, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_depthPyramidWidth, m_depthPyramidHeight, m_depthPyramidLevels, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

        for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
        {
            m_depthPyramidMips[i] = createImageView(m_gfxDevice.m_device, m_depthPyramid.image, VK_FORMAT_R32_SFLOAT, i, 1);
            assert(m_depthPyramidMips[i]);
        }

        for (uint32_t i = 0; i < m_gfxDevice.swapchain.imageCount; ++i)
        {
            if (m_gfxDevice.swapchainImageViews[i])
                vkDestroyImageView(m_gfxDevice.m_device, m_gfxDevice.swapchainImageViews[i], 0);

                m_gfxDevice.swapchainImageViews[i] = createImageView(m_gfxDevice.m_device, m_gfxDevice.swapchain.images[i], m_gfxDevice.swapchainFormat, 0, 1);
        }
    }

    VkResult acquireResult = vkAcquireNextImageKHR(m_gfxDevice.m_device, m_gfxDevice.swapchain.swapchain, ~0ull, m_frames[m_currentFrameIndex].waitSemaphore, VK_NULL_HANDLE, &m_imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
        return false; // attempting to render to an out-of-date swapchain would break semaphore synchronization
    VK_CHECK_SWAPCHAIN(acquireResult);

    VK_CHECK(vkResetCommandPool(m_gfxDevice.m_device, m_frames[m_currentFrameIndex].commandPool, 0));

    VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(m_frames[m_currentFrameIndex].commandBuffer, &beginInfo));

    vkCmdResetQueryPool(m_frames[m_currentFrameIndex].commandBuffer, m_queryPoolTimestamp, 0, 128);
    vkCmdWriteTimestamp(m_frames[m_currentFrameIndex].commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, 0);

    if (!m_buffers.DrawVisibilityCleared)
    {
        // TODO: this is stupidly redundant
        vkCmdFillBuffer(m_frames[m_currentFrameIndex].commandBuffer, m_buffers.DrawVisibility.buffer, 0, sizeof(uint32_t) * m_draws.size(), 0);

        VkBufferMemoryBarrier2 fillBarrier = bufferBarrier(m_buffers.DrawVisibility.buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        pipelineBarrier(m_frames[m_currentFrameIndex].commandBuffer, 0, 1, &fillBarrier, 0, nullptr);

        m_buffers.DrawVisibilityCleared = true;
    }

    if (!m_buffers.MeshletVisibilityCleared)
    {
        // TODO: this is stupidly redundant
        vkCmdFillBuffer(m_frames[m_currentFrameIndex].commandBuffer, m_buffers.MeshletVisibility.buffer, 0, m_buffers.meshletVisibilityBytes, 0);

        VkBufferMemoryBarrier2 fillBarrier = bufferBarrier(m_buffers.MeshletVisibility.buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        pipelineBarrier(m_frames[m_currentFrameIndex].commandBuffer, 0, 1, &fillBarrier, 0, nullptr);

        m_buffers.MeshletVisibilityCleared = true;
    }

    // raytracing
    uint32_t timestamp = 21;

    vkCmdWriteTimestamp(m_frames[m_currentFrameIndex].commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    if (m_tlasNeedsRebuild)
    {
        buildTLAS(m_gfxDevice.m_device, m_frames[m_currentFrameIndex].commandBuffer, m_buffers.tlas, m_buffers.tlasBuffer, m_buffers.tlasScratchBuffer, m_buffers.tlasInstanceBuffer, m_draws.size(), VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR);
        m_tlasNeedsRebuild = false;
    }

    vkCmdWriteTimestamp(m_frames[m_currentFrameIndex].commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);


    m_view = glm::mat4_cast(m_camera.orientation);
    m_view[3] = vec4(m_camera.position, 1.0f);
    m_view = inverse(m_view);
    m_view = glm::scale(glm::identity<glm::mat4>(), vec3(1, 1, -1)) * m_view;

    m_projection = perspectiveProjection(m_camera.fovY, float(m_gfxDevice.swapchain.width) / float(m_gfxDevice.swapchain.height), m_camera.znear);

    m_projectionT = transpose(m_projection);

    vec4 frustumX = normalizePlane(m_projectionT[3] + m_projectionT[0]); // x + w < 0
    vec4 frustumY = normalizePlane(m_projectionT[3] + m_projectionT[1]); // y + w < 0

    m_cullData = {};
    m_cullData.view = m_view;
    m_cullData.P00 = m_projection[0][0];
    m_cullData.P11 = m_projection[1][1];
    m_cullData.znear = m_camera.znear;
    m_cullData.zfar = m_drawDistance;
    m_cullData.frustum[0] = frustumX.x;
    m_cullData.frustum[1] = frustumX.z;
    m_cullData.frustum[2] = frustumY.y;
    m_cullData.frustum[3] = frustumY.z;
    m_cullData.drawCount = uint32_t(m_draws.size());
    m_cullData.cullingEnabled = true;
    m_cullData.lodEnabled = true;
    m_cullData.occlusionEnabled = true;
    m_cullData.lodTarget = (2 / m_cullData.P11) * (1.f / float(m_gfxDevice.swapchain.height)) * (1 << 0); // 1px
    m_cullData.pyramidWidth = float(m_depthPyramidWidth);
    m_cullData.pyramidHeight = float(m_depthPyramidHeight);
    m_cullData.clusterOcclusionEnabled = true;

    Globals m_globals = {};
    m_globals.projection = m_projection;
    m_globals.cullData = m_cullData;
    m_globals.screenWidth = float(m_gfxDevice.swapchain.width);
    m_globals.screenHeight = float(m_gfxDevice.swapchain.height);

    VkImageMemoryBarrier2 renderBeginBarriers[gbufferCount + 1] = {
        imageBarrier(m_depthTarget.image,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT),
    };
    for (uint32_t i = 0; i < gbufferCount; ++i)
        renderBeginBarriers[i + 1] = imageBarrier(m_gbufferTargets[i].image,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);

    pipelineBarrier(m_frames[m_currentFrameIndex].commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, COUNTOF(renderBeginBarriers), renderBeginBarriers);

    vkCmdResetQueryPool(m_frames[m_currentFrameIndex].commandBuffer, m_queryPoolPipeline, 0, 4);

    return true;
}

void Renderer::drawCull(VkPipeline pipeline, uint32_t timestamp, const char *phase, bool late, unsigned int postPass)
{
    uint32_t rasterizationStage = VK_PIPELINE_STAGE_TASK_SHADER_BIT_EXT | VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT;

    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].commandBuffer;

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    VkBufferMemoryBarrier2 prefillBarrier = bufferBarrier(m_buffers.CommandCount.buffer,
        VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    pipelineBarrier(commandBuffer, 0, 1, &prefillBarrier, 0, nullptr);

    // printf("vkCmdFillBuffer \n");
    vkCmdFillBuffer(commandBuffer, m_buffers.CommandCount.buffer, 0, 4, 0);

    // pyramid barrier is tricky: our frame sequence is cull -> render -> pyramid -> cull -> render
    // the first cull (late=0) doesn't read pyramid data BUT the read in the shader is guarded by a push constant value (which could be specialization constant but isn't due to AMD bug)
    // the second cull (late=1) does read pyramid data that was written in the pyramid stage
    // as such, second cull needs to transition GENERAL->GENERAL with a COMPUTE->COMPUTE barrier, but the first cull needs to have a dummy transition because pyramid starts in UNDEFINED state on first frame
    VkImageMemoryBarrier2 pyramidBarrier = imageBarrier(m_depthPyramid.image,
        late ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : 0, late ? VK_ACCESS_SHADER_WRITE_BIT : 0, late ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);

    VkBufferMemoryBarrier2 fillBarriers[] = {
        bufferBarrier(m_buffers.TaskCommands.buffer,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | rasterizationStage, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT),
        bufferBarrier(m_buffers.CommandCount.buffer,
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

        DescriptorInfo pyramidDesc(m_samplers.depthSampler, m_depthPyramid.imageView, VK_IMAGE_LAYOUT_GENERAL);
        DescriptorInfo descriptors[] = { m_buffers.draw.buffer, m_buffers.meshesh.buffer, m_buffers.TaskCommands.buffer, m_buffers.CommandCount.buffer, m_buffers.DrawVisibility.buffer, pyramidDesc };

        // printf("passData \n");
        // printf("passData.P00 %f \n", passData.P00);
        // printf("passData.P11 %f \n", passData.P11);
        // printf("passData.znear %f \n", passData.znear);
        // printf("passData.zfar %f \n", passData.zfar);

        // printf("passData.frustum[0] %f \n", passData.frustum[0]);
        // printf("passData.frustum[1] %f \n", passData.frustum[1]);
        // printf("passData.frustum[2] %f \n", passData.frustum[2]);
        // printf("passData.frustum[2] %f \n", passData.frustum[3]);

        // printf("passData.lodTarget %f \n", passData.lodTarget);
        // printf("passData.pyramidWidth %f \n", passData.pyramidWidth);
        // printf("passData.pyramidHeight %f \n", passData.pyramidHeight);
        // printf("passData.drawCount %ld \n", passData.drawCount);
        // printf("passData.cullingEnabled %i \n", passData.cullingEnabled);
        // printf("passData.lodEnabled %i \n", passData.lodEnabled);
        // printf("passData.occlusionEnabled %i \n", passData.occlusionEnabled);
        // printf("passData.clusterOcclusionEnabled %i \n", passData.clusterOcclusionEnabled);
        // printf("passData.clusterBackfaceEnabled %i \n", passData.clusterBackfaceEnabled);
        // printf("passData.postPass %d \n", passData.postPass);

        printf("dispatch %lu \n", m_draws.size());
        dispatch(commandBuffer, m_programs.drawcullProgram, uint32_t(m_draws.size()), 1, passData, descriptors);
        printf("dispatch completed \n");
    }

    VkBufferMemoryBarrier2 syncBarrier = bufferBarrier(m_buffers.CommandCount.buffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

    printf("pipelineBarrier \n");
    pipelineBarrier(commandBuffer, 0, 1, &syncBarrier, 0, nullptr);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.tasksubmitPipeline);

    DescriptorInfo descriptors[] = { m_buffers.CommandCount.buffer, m_buffers.TaskCommands.buffer };
    vkCmdPushDescriptorSetWithTemplate(commandBuffer, m_programs.tasksubmitProgram.updateTemplate, m_programs.tasksubmitProgram.layout, 0, descriptors);

    printf("vkCmdDispatch \n");
    vkCmdDispatch(commandBuffer, 1, 1, 1);

    VkBufferMemoryBarrier2 cullBarriers[] = {
        bufferBarrier(m_buffers.TaskCommands.buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | rasterizationStage, VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_SHADER_READ_BIT),
        bufferBarrier(m_buffers.CommandCount.buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_ACCESS_INDIRECT_COMMAND_READ_BIT),
    };

    printf("pipelineBarrier \n");
    pipelineBarrier(commandBuffer, 0, COUNTOF(cullBarriers), cullBarriers, 0, nullptr);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);
}

void Renderer::drawRender(bool late, const VkClearColorValue &colorClear, const VkClearDepthStencilValue &depthClear, uint32_t query, uint32_t timestamp, const char *phase, unsigned int postPass)
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].commandBuffer;

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    vkCmdBeginQuery(commandBuffer, m_queryPoolPipeline, query, 0);

    VkRenderingAttachmentInfo gbufferAttachments[gbufferCount] = {};
    for (uint32_t i = 0; i < gbufferCount; ++i)
    {
        gbufferAttachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        gbufferAttachments[i].imageView = m_gbufferTargets[i].imageView;
        gbufferAttachments[i].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        gbufferAttachments[i].loadOp = late ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        gbufferAttachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        gbufferAttachments[i].clearValue.color = colorClear;
    }

    VkRenderingAttachmentInfo depthAttachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    depthAttachment.imageView = m_depthTarget.imageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = late ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil = depthClear;

    VkRenderingInfo passInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    passInfo.renderArea.extent.width = m_gfxDevice.swapchain.width;
    passInfo.renderArea.extent.height = m_gfxDevice.swapchain.height;
    passInfo.layerCount = 1;
    passInfo.colorAttachmentCount = gbufferCount;
    passInfo.pColorAttachments = gbufferAttachments;
    passInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer, &passInfo);

    VkViewport viewport = { 0, float(m_gfxDevice.swapchain.height), float(m_gfxDevice.swapchain.width), -float(m_gfxDevice.swapchain.height), 0, 1 };
    VkRect2D scissor = { { 0, 0 }, { uint32_t(m_gfxDevice.swapchain.width), uint32_t(m_gfxDevice.swapchain.height) } };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdSetCullMode(commandBuffer, postPass == 0 ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    vkCmdSetDepthBias(commandBuffer, postPass == 0 ? 0 : 16, 0, postPass == 0 ? 0 : 1);

    Globals passGlobals = m_globals;
    passGlobals.cullData.postPass = postPass;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postPass >= 1 ? m_pipelines.meshtaskpostPipeline : late ? m_pipelines.meshtasklatePipeline
                                                                                                                    : m_pipelines.meshtaskPipeline);

    DescriptorInfo pyramidDesc(m_samplers.depthSampler, m_depthPyramid.imageView, VK_IMAGE_LAYOUT_GENERAL);
    DescriptorInfo descriptors[] = { m_buffers.TaskCommands.buffer, m_buffers.draw.buffer, m_buffers.meshlets.buffer, m_buffers.meshletdata.buffer, m_buffers.vertices.buffer, m_buffers.MeshletVisibility.buffer, pyramidDesc, m_samplers.textureSampler, m_buffers.materials.buffer };
    vkCmdPushDescriptorSetWithTemplate(commandBuffer, m_programs.meshtaskProgram.updateTemplate, m_programs.meshtaskProgram.layout, 0, descriptors);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_programs.meshtaskProgram.layout, 1, 1, &m_textureSet.second, 0, nullptr);

    vkCmdPushConstants(commandBuffer, m_programs.meshtaskProgram.layout, m_programs.meshtaskProgram.pushConstantStages, 0, sizeof(m_globals), &passGlobals);
    vkCmdDrawMeshTasksIndirectEXT(commandBuffer, m_buffers.CommandCount.buffer, 4, 1, 0);

    vkCmdEndRendering(commandBuffer);

    vkCmdEndQuery(commandBuffer, m_queryPoolPipeline, query);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);

}

void Renderer::drawPyramid(uint32_t timestamp)
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].commandBuffer;

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

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.depthreducePipeline);

    for (uint32_t i = 0; i < m_depthPyramidLevels; ++i)
    {
        DescriptorInfo sourceDepth = (i == 0)
                                         ? DescriptorInfo(m_samplers.depthSampler, m_depthTarget.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                         : DescriptorInfo(m_samplers.depthSampler, m_depthPyramidMips[i - 1], VK_IMAGE_LAYOUT_GENERAL);

        DescriptorInfo descriptors[] = { { m_depthPyramidMips[i], VK_IMAGE_LAYOUT_GENERAL }, sourceDepth };

        uint32_t levelWidth = std::max(1u, m_depthPyramidWidth >> i);
        uint32_t levelHeight = std::max(1u, m_depthPyramidHeight >> i);

        vec4 reduceData = vec4(levelWidth, levelHeight, 0, 0);

        dispatch(commandBuffer, m_programs.depthreduceProgram, levelWidth, levelHeight, reduceData, descriptors);

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
    // auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].commandBuffer;

    // auto debugtext = [&](int line, uint32_t color, const char* format, ...)
    // #ifdef __GNUC__
    //                                  __attribute__((format(printf, 4, 5)))
    // #endif
    // {
    //     TextData textData = {};
    //     textData.offsetX = 1;
    //     textData.offsetY = line + 1;
    //     textData.scale = 2;
    //     textData.color = color;

    //     va_list args;
    //     va_start(args, format);
    //     vsnprintf(textData.data, sizeof(textData.data), format, args);
    //     va_end(args);

    //     vkCmdPushConstants(commandBuffer, m_programs.debugtextProgram.layout, m_programs.debugtextProgram.pushConstantStages, 0, sizeof(textData), &textData);
    //     vkCmdDispatch(commandBuffer, strlen(textData.data), 1, 1);
    // };

    // VkImageMemoryBarrier2 textBarrier =
    //     imageBarrier(m_gfxDevice.swapchain.images[m_imageIndex],
    //         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
    //         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    // pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &textBarrier);

    // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.debugtextPipeline);

    // DescriptorInfo descriptors[] = { { m_gfxDevice.swapchainImageViews[m_imageIndex], VK_IMAGE_LAYOUT_GENERAL } };
    // vkCmdPushDescriptorSetWithTemplate(commandBuffer, m_programs.debugtextProgram.updateTemplate, m_programs.debugtextProgram.layout, 0, descriptors);

    // // debug text goes here!
    // uint64_t triangleCount = m_pipelineResults[0] + m_pipelineResults[1] + m_pipelineResults[2];

    // double frameGpuBegin = double(m_timestampResults[0]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double frameGpuEnd = double(m_timestampResults[1]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;

    // double cullGpuTime = double(m_timestampResults[3] - m_timestampResults[2]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double renderGpuTime = double(m_timestampResults[5] - m_timestampResults[4]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double pyramidGpuTime = double(m_timestampResults[7] - m_timestampResults[6]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double culllateGpuTime = double(m_timestampResults[9] - m_timestampResults[8]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double renderlateGpuTime = double(m_timestampResults[11] - m_timestampResults[10]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double cullpostGpuTime = double(m_timestampResults[13] - m_timestampResults[12]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double renderpostGpuTime = double(m_timestampResults[15] - m_timestampResults[14]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double shadowsGpuTime = double(m_timestampResults[17] - m_timestampResults[16]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double shadowblurGpuTime = double(m_timestampResults[18] - m_timestampResults[17]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double shadeGpuTime = double(m_timestampResults[20] - m_timestampResults[19]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;
    // double tlasGpuTime = double(m_timestampResults[22] - m_timestampResults[21]) * m_gfxDevice.props.limits.timestampPeriod * 1e-6;

    // double trianglesPerSec = double(triangleCount) / double(m_frameGpuAvg * 1e-3);
    // double drawsPerSec = double(m_draws.size()) / double(m_frameGpuAvg * 1e-3);

    // debugtext(0, ~0u, "cpu: %.2f ms (%d); gpu: %.2f ms", m_frameGpuAvg, m_frames[m_lastFrameIndex].deltaTime, m_frameGpuAvg);

    // debugtext(2, ~0u, "cull: %.2f ms, pyramid: %.2f ms, render: %.2f ms, final: %.2f ms",
    //     cullGpuTime + culllateGpuTime + cullpostGpuTime,
    //     pyramidGpuTime,
    //     renderGpuTime + renderlateGpuTime + renderpostGpuTime,
    //     shadeGpuTime);
    // debugtext(3, ~0u, "tlas: %.2f ms, shadows: %.2f ms, shadow blur: %.2f ms",
    //     tlasGpuTime,
    //     shadowsGpuTime, shadowblurGpuTime);
    // debugtext(4, ~0u, "triangles %.2fM; %.1fB tri / sec, %.1fM draws / sec",
    //     double(triangleCount) * 1e-6, trianglesPerSec * 1e-9, drawsPerSec * 1e-6);

    // debugtext(9, ~0u, "RT shadows: %s, blur %s, quality %d, checkerboard %s",
    //     raytracingSupported && shadowsEnabled ? "ON" : "OFF",
    //     raytracingSupported && shadowblurEnabled ? "ON" : "OFF",
    //     shadowQuality, shadowCheckerboard ? "ON" : "OFF");
    
}

void Renderer::drawShadow()
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].commandBuffer;

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
    drawCull(m_pipelines.taskcullPipeline, 2, "early cull", /* late= */ false);
    // drawCull(m_pipelines.drawcullPipeline, 2, "early cull", /* late= */ false);
    printf("drawRender \n");
    drawRender(/* late= */ false, m_colorClear, m_depthClear, 0, 4, "early render");
    printf("drawPyramid \n");
    drawPyramid(6);
    printf("drawCull \n");
    drawCull(m_pipelines.taskculllatePipeline, 8, "late cull", /* late= */ true);
    printf("drawRender \n");
    drawRender(/* late= */ true, m_colorClear, m_depthClear, 1, 10, "late render");

    if (m_meshPostPasses >> 1)
    {
        // post cull: frustum + occlusion cull & fill extra objects
        printf(" \n");
        drawCull(m_pipelines.taskculllatePipeline , 12, "post cull", /* late= */ true, /* postPass= */ 1);

        // post render: render extra objects
        printf(" \n");
        drawRender(/* late= */ true, m_colorClear, m_depthClear, 2, 14, "post render", /* postPass= */ 1);
    }

    printf("niagaraShadows \n");
    niagaraShadows();

    // Import Global illumination here
    // drawShadow();

    // drawDebug();

    printf("endFrame \n");
    endFrame();
}

void Renderer::niagaraShadows()
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].commandBuffer;


    VkImageMemoryBarrier2 blitBarriers[2 + gbufferCount] = {
        // note: even though the source image has previous state as undef, we need to specify COMPUTE_SHADER to synchronize with submitStageMask below
        imageBarrier(m_gfxDevice.swapchain.images[m_imageIndex],
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL),
        imageBarrier(m_depthTarget.image,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT)
    };

    for (uint32_t i = 0; i < gbufferCount; ++i)
        blitBarriers[i + 2] = imageBarrier(m_gbufferTargets[i].image,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, COUNTOF(blitBarriers), blitBarriers);


    uint32_t timestamp = 16;

    // checkerboard rendering: we dispatch half as many columns and xform them to fill the screen
    bool shadowCheckerboard = false;
    int shadowQuality = 1;
    bool shadowsEnabled = true;
    bool shadowblurEnabled = true;
    int shadowWidthCB = shadowCheckerboard ? (m_gfxDevice.swapchain.width + 1) / 2 : m_gfxDevice.swapchain.width;
    int shadowCheckerboardF = shadowCheckerboard ? 1 : 0;

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

    VkImageMemoryBarrier2 preshadowBarrier =
        imageBarrier(m_shadowTarget.image,
            0, 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

    pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &preshadowBarrier);

    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, shadowQuality == 0 ? m_pipelines.shadowlqPipeline : m_pipelines.shadowhqPipeline);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_programs.shadowProgram.layout, 1, 1, &m_textureSet.second, 0, nullptr);

        DescriptorInfo descriptors[] = { { m_shadowTarget.imageView, VK_IMAGE_LAYOUT_GENERAL }, { m_samplers.readSampler, m_depthTarget.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, m_buffers.tlas, m_buffers.draw.buffer, m_buffers.meshesh.buffer, m_buffers.materials.buffer, m_buffers.vertices.buffer, m_buffers.indices.buffer, m_samplers.textureSampler };

        ShadowData shadowData = {};
        shadowData.sunDirection = m_sunDirection;
        shadowData.sunJitter = shadowblurEnabled ? 1e-2f : 0;
        shadowData.inverseViewProjection = inverse(m_projection * m_view);
        shadowData.imageSize = vec2(float(m_gfxDevice.swapchain.width), float(m_gfxDevice.swapchain.height));
        shadowData.checkerboard = shadowCheckerboardF;

        dispatch(commandBuffer, m_programs.shadowProgram, shadowWidthCB, m_gfxDevice.swapchain.height, shadowData, descriptors);
    }

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);

    for (int pass = 0; pass < (shadowblurEnabled ? 2 : 0); ++pass)
    {
        const Image& blurFrom = pass == 0 ? m_shadowTarget : m_shadowblurTarget;
        const Image& blurTo = pass == 0 ? m_shadowblurTarget : m_shadowTarget;

        VkImageMemoryBarrier2 blurBarriers[] = {
            imageBarrier(blurFrom.image,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL),
            imageBarrier(blurTo.image,
                pass == 0 ? 0 : VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, pass == 0 ? 0 : VK_ACCESS_SHADER_READ_BIT, pass == 0 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL),
        };

        pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, COUNTOF(blurBarriers), blurBarriers);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.shadowblurPipeline);

        DescriptorInfo descriptors[] = { { blurTo.imageView, VK_IMAGE_LAYOUT_GENERAL }, { m_samplers.readSampler, blurFrom.imageView, VK_IMAGE_LAYOUT_GENERAL }, { m_samplers.readSampler, m_depthTarget.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } };

        vec4 blurData = vec4(float(m_gfxDevice.swapchain.width), float(m_gfxDevice.swapchain.height), pass == 0 ? 1 : 0, m_camera.znear);

        dispatch(commandBuffer, m_programs.shadowblurProgram, m_gfxDevice.swapchain.width, m_gfxDevice.swapchain.height, blurData, descriptors);
    }

    VkImageMemoryBarrier2 postblurBarrier =
        imageBarrier(m_shadowTarget.image,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_GENERAL);

    pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &postblurBarrier);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 2);

    {
        uint32_t timestamp = 19;

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 0);

        {
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelines.finalPipeline);

            DescriptorInfo descriptors[] = { { m_gfxDevice.swapchainImageViews[m_imageIndex], VK_IMAGE_LAYOUT_GENERAL }, { m_samplers.readSampler, m_gbufferTargets[0].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, { m_samplers.readSampler, m_gbufferTargets[1].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, { m_samplers.readSampler, m_depthTarget.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, { m_samplers.readSampler, m_shadowTarget.imageView, VK_IMAGE_LAYOUT_GENERAL } };

            ShadeData shadeData = {};
            shadeData.cameraPosition = m_camera.position;
            shadeData.sunDirection = m_sunDirection;
            shadeData.shadowsEnabled = shadowsEnabled;
            shadeData.inverseViewProjection = inverse(m_projection * m_view);
            shadeData.imageSize = vec2(float(m_gfxDevice.swapchain.width), float(m_gfxDevice.swapchain.height));

            dispatch(commandBuffer, m_programs.finalProgram, m_gfxDevice.swapchain.width, m_gfxDevice.swapchain.height, shadeData, descriptors);
        }

        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, timestamp + 1);
    }

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
		if (!loadDDSImage(image, m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_gfxDevice.m_memoryProperties, m_buffers.scratch, m_texturePaths[i].c_str()))
		{
			printf("Error: image N: %ld %s failed to load\n", i, m_texturePaths[i].c_str());
			return 1;
		}

		// VkMemoryRequirements memoryRequirements = {};
		// vkGetImageMemoryRequirements(m_gfxDevice.m_device, image.image, &memoryRequirements);
		// m_imageMemory += memoryRequirements.size;

		m_images.push_back(image);
	}

	// printf("Loaded %d textures (%.2f MB) in %.2f sec\n", int(m_images.size()), double(m_imageMemory) / 1e6, glfwGetTime() - imageTimer);

	uint32_t descriptorCount = uint32_t(m_texturePaths.size() + 1);
	std::pair<VkDescriptorPool, VkDescriptorSet> textureSet = createDescriptorArray(m_gfxDevice.m_device, m_textureSetLayout, descriptorCount);

	for (size_t i = 0; i < m_texturePaths.size(); ++i)
	{
		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageView = m_images[i].imageView;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write.dstSet = textureSet.second;
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

    m_buffers.meshletVisibilityBytes = (meshletVisibilityCount + 31) / 32 * sizeof(uint32_t);

    uint32_t raytracingBufferFlags = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    createBuffer(m_buffers.meshesh, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.meshes.size() * sizeof(Mesh), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.materials, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_materials.size() * sizeof(Material), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.vertices, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | raytracingBufferFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.indices, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.indices.size() * sizeof(uint32_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | raytracingBufferFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.meshlets, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.meshlets.size() * sizeof(Meshlet), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.meshletdata, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_geometry.meshletdata.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.meshesh, m_buffers.scratch, m_geometry.meshes.data(), m_geometry.meshes.size() * sizeof(Mesh));
    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.materials, m_buffers.scratch, m_materials.data(), m_materials.size() * sizeof(Material));

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.vertices, m_buffers.scratch, m_geometry.vertices.data(), m_geometry.vertices.size() * sizeof(Vertex));
    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.indices, m_buffers.scratch, m_geometry.indices.data(), m_geometry.indices.size() * sizeof(uint32_t));

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.meshlets, m_buffers.scratch, m_geometry.meshlets.data(), m_geometry.meshlets.size() * sizeof(Meshlet));
    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.meshletdata, m_buffers.scratch, m_geometry.meshletdata.data(), m_geometry.meshletdata.size() * sizeof(uint32_t));

    createBuffer(m_buffers.draw, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_draws.size() * sizeof(MeshDraw), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    createBuffer(m_buffers.DrawVisibility, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_draws.size() * sizeof(uint32_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.TaskCommands, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, TASK_WGLIMIT * sizeof(MeshTaskCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    createBuffer(m_buffers.CommandCount, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // TODO: there's a way to implement cluster visibility persistence *without* using bitwise storage at all, which may be beneficial on the balance, so we should try that.
    // *if* we do that, we can drop meshletVisibilityOffset et al from everywhere
    createBuffer(m_buffers.MeshletVisibility, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, m_buffers.meshletVisibilityBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    uploadBuffer(m_gfxDevice.m_device, m_immCommandPool, m_immCommandBuffer, m_queue, m_buffers.draw, m_buffers.scratch, m_draws.data(), m_draws.size() * sizeof(MeshDraw));

    std::vector<VkDeviceSize> compactedSizes;
    buildBLAS(m_gfxDevice.m_device, m_geometry.meshes, m_buffers.vertices, m_buffers.indices, m_blas, compactedSizes, m_buffers.blasBuffer, m_immCommandPool, m_immCommandBuffer, m_queue, m_gfxDevice.m_memoryProperties);
    compactBLAS(m_gfxDevice.m_device, m_blas, compactedSizes, m_buffers.blasBuffer, m_immCommandPool, m_immCommandBuffer, m_queue, m_gfxDevice.m_memoryProperties);

    m_blasAddresses.resize(m_blas.size());

    for (size_t i = 0; i < m_blas.size(); ++i)
    {
        VkAccelerationStructureDeviceAddressInfoKHR info = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        info.accelerationStructure = m_blas[i];

        m_blasAddresses[i] = vkGetAccelerationStructureDeviceAddressKHR(m_gfxDevice.m_device, &info);
    }

    createBuffer(m_buffers.tlasInstanceBuffer, m_gfxDevice.m_device, m_gfxDevice.m_memoryProperties, sizeof(VkAccelerationStructureInstanceKHR) * m_draws.size(), VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    for (size_t i = 0; i < m_draws.size(); ++i)
    {
        const MeshDraw& draw = m_draws[i];
        assert(draw.meshIndex < m_blas.size());

        VkAccelerationStructureInstanceKHR instance = {};
        fillInstanceRT(instance, draw, uint32_t(i), m_blasAddresses[draw.meshIndex]);

        memcpy(static_cast<VkAccelerationStructureInstanceKHR*>(m_buffers.tlasInstanceBuffer.data) + i, &instance, sizeof(VkAccelerationStructureInstanceKHR));
    }

    m_buffers.tlas = createTLAS(m_gfxDevice.m_device, m_buffers.tlasBuffer, m_buffers.tlasScratchBuffer, m_buffers.tlasInstanceBuffer, m_draws.size(), m_gfxDevice.m_memoryProperties);

    return true;
}

void Renderer::endFrame()
{
    auto commandBuffer = m_frames[m_frameIndex % FRAMES_COUNT].commandBuffer;

    VkImageMemoryBarrier2 presentBarrier = imageBarrier(m_gfxDevice.swapchain.images[m_imageIndex],
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
        0, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    pipelineBarrier(commandBuffer, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &presentBarrier);

    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, m_queryPoolTimestamp, 1);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkPipelineStageFlags submitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_frames[m_currentFrameIndex].waitSemaphore;
    submitInfo.pWaitDstStageMask = &submitStageMask;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_frames[m_currentFrameIndex].signalSemaphore;

    VK_CHECK_FORCE(vkQueueSubmit(m_queue, 1, &submitInfo, m_frames[m_currentFrameIndex].renderFence));

    VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_frames[m_currentFrameIndex].signalSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_gfxDevice.swapchain.swapchain;
    presentInfo.pImageIndices = &m_imageIndex;

    VK_CHECK_SWAPCHAIN(vkQueuePresentKHR(m_queue, &presentInfo));

    m_frameIndex++;
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

	for (uint32_t i = 0; i < m_gfxDevice.swapchain.imageCount; ++i)
		if (m_gfxDevice.swapchainImageViews[i])
			vkDestroyImageView(m_gfxDevice.m_device, m_gfxDevice.swapchainImageViews[i], 0);

	destroyBuffer(m_buffers.meshesh, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.materials, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.draw, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.DrawVisibility, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.TaskCommands, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.CommandCount, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.meshlets, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.meshletdata, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.MeshletVisibility, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.tlasBuffer, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.blasBuffer, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.tlasScratchBuffer, m_gfxDevice.m_device);
    destroyBuffer(m_buffers.tlasInstanceBuffer, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.indices, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.vertices, m_gfxDevice.m_device);
	destroyBuffer(m_buffers.scratch, m_gfxDevice.m_device);

    vkDestroyAccelerationStructureKHR(m_gfxDevice.m_device, m_buffers.tlas, 0);
    for (VkAccelerationStructureKHR as : m_blas)
        vkDestroyAccelerationStructureKHR(m_gfxDevice.m_device, as, 0);

    for (int i=0; i<FRAMES_COUNT; i++)
    {
        vkDestroyCommandPool(m_gfxDevice.m_device, m_frames[i].commandPool, 0);
    }

	vkDestroyQueryPool(m_gfxDevice.m_device, m_queryPoolTimestamp, 0);
	vkDestroyQueryPool(m_gfxDevice.m_device, m_queryPoolPipeline, 0);

	destroySwapchain(m_gfxDevice.m_device, m_gfxDevice.swapchain);

	for (VkPipeline pipeline : m_pipelines.pipelines)
		vkDestroyPipeline(m_gfxDevice.m_device, pipeline, 0);

	destroyProgram(m_gfxDevice.m_device, m_programs.debugtextProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.drawcullProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.tasksubmitProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.clustersubmitProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.clustercullProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.depthreduceProgram);
	destroyProgram(m_gfxDevice.m_device, m_programs.meshProgram);

    destroyProgram(m_gfxDevice.m_device, m_programs.meshtaskProgram);
    destroyProgram(m_gfxDevice.m_device, m_programs.clusterProgram);

	destroyProgram(m_gfxDevice.m_device, m_programs.finalProgram);

    destroyProgram(m_gfxDevice.m_device, m_programs.shadowProgram);
    destroyProgram(m_gfxDevice.m_device, m_programs.shadowfillProgram);
    destroyProgram(m_gfxDevice.m_device, m_programs.shadowblurProgram);

	vkDestroyDescriptorSetLayout(m_gfxDevice.m_device, m_textureSetLayout, 0);

	vkDestroySampler(m_gfxDevice.m_device, m_samplers.textureSampler, 0);
	vkDestroySampler(m_gfxDevice.m_device, m_samplers.readSampler, 0);
	vkDestroySampler(m_gfxDevice.m_device, m_samplers.depthSampler, 0);

    for (int i=0; i<FRAMES_COUNT; i++)
    {
        vkDestroyFence(m_gfxDevice.m_device, m_frames[i].renderFence, 0);
        vkDestroySemaphore(m_gfxDevice.m_device, m_frames[i].signalSemaphore, 0);
        vkDestroySemaphore(m_gfxDevice.m_device, m_frames[i].waitSemaphore, 0);
    }

	vkDestroySurfaceKHR(m_gfxDevice.m_instance, m_gfxDevice.surface, 0);


	vkDestroyDevice(m_gfxDevice.m_device, 0);

	vkDestroyInstance(m_gfxDevice.m_instance, 0);

	volkFinalize();
}
