#include "RendererUtils.h"
#include <math.h>

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

pcg32_random_t rngstate = PCG32_INITIALIZER;

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

double rand01()
{
    return pcg32_random_r(&rngstate) / double(1ull << 32);
}

uint32_t rand32()
{
    return pcg32_random_r(&rngstate);
}

VkRenderingAttachmentInfo createColorAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, VkAttachmentLoadOp loadOp, const VkClearColorValue& clearValue)
{
    VkRenderingAttachmentInfo attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachment.imageView = imageView;
    attachment.imageLayout = imageLayout;
    attachment.loadOp = loadOp;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.color = clearValue;
    return attachment;
}

VkRenderingAttachmentInfo createDepthAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, VkAttachmentLoadOp loadOp, const VkClearDepthStencilValue& clearValue)
{
    VkRenderingAttachmentInfo attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    attachment.imageView = imageView;
    attachment.imageLayout = imageLayout;
    attachment.loadOp = loadOp;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.clearValue.depthStencil = clearValue;
    return attachment;
}

VkViewport createViewport(float width, float height)
{
    return { 0, height, width, -height, 0, 1 };
}

VkRect2D createScissorRect(uint32_t width, uint32_t height)
{
    return { { 0, 0 }, { width, height } };
}

void createRenderTargetBarriers(VkImage depthImage, const VkImage* colorImages, uint32_t colorCount, VkImageMemoryBarrier2* barriers)
{
    barriers[0] = imageBarrier(depthImage,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_DEPTH_BIT);
    
    for (uint32_t i = 0; i < colorCount; ++i)
        barriers[i + 1] = imageBarrier(colorImages[i],
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
}

mat4 calculateViewMatrix(const Camera& camera)
{
    mat4 view = glm::mat4_cast(camera.orientation);
    view[3] = vec4(camera.position, 1.0f);
    view = inverse(view);
    // Flip Z for Vulkan coordinate system
    return glm::scale(glm::identity<glm::mat4>(), vec3(1, 1, -1)) * view;
}

CullData setupCullingData(
    const mat4& view, 
    const mat4& projection,
    const Camera& camera,
    float drawDistance,
    uint32_t drawCount,
    float depthPyramidWidth,
    float depthPyramidHeight,
    float screenHeight)
{
    mat4 projectionT = transpose(projection);
    vec4 frustumX = normalizePlane(projectionT[3] + projectionT[0]); // x + w < 0
    vec4 frustumY = normalizePlane(projectionT[3] + projectionT[1]); // y + w < 0
    
    CullData cullData = {};
    cullData.view = view;
    cullData.P00 = projection[0][0];
    cullData.P11 = projection[1][1];
    cullData.znear = camera.znear;
    cullData.zfar = drawDistance;
    cullData.frustum[0] = frustumX.x;
    cullData.frustum[1] = frustumX.z;
    cullData.frustum[2] = frustumY.y;
    cullData.frustum[3] = frustumY.z;
    cullData.drawCount = drawCount;
    cullData.cullingEnabled = true;
    cullData.lodEnabled = true;
    cullData.occlusionEnabled = true;
    cullData.lodTarget = (2 / cullData.P11) * (1.f / screenHeight) * (1 << 0); // 1px
    cullData.pyramidWidth = depthPyramidWidth;
    cullData.pyramidHeight = depthPyramidHeight;
    cullData.clusterOcclusionEnabled = true;
    
    return cullData;
}

void clearBuffer(
    VkCommandBuffer commandBuffer, 
    VkBuffer buffer, 
    VkDeviceSize size, 
    VkPipelineStageFlags2 srcStageMask,
    VkPipelineStageFlags2 dstStageMask,
    VkAccessFlags2 dstAccessMask)
{
    // Fill the buffer with zeros
    vkCmdFillBuffer(commandBuffer, buffer, 0, size, 0);
    
    // Create and execute barrier to ensure the fill is complete before other operations
    VkBufferMemoryBarrier2 fillBarrier = bufferBarrier(buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        dstStageMask, dstAccessMask);
    
    pipelineBarrier(commandBuffer, 0, 1, &fillBarrier, 0, nullptr);
}

VkRenderingInfo createRenderingInfo(
    uint32_t width,
    uint32_t height,
    const VkRenderingAttachmentInfo* colorAttachments,
    uint32_t colorAttachmentCount,
    const VkRenderingAttachmentInfo* depthAttachment)
{
    VkRenderingInfo renderInfo = { VK_STRUCTURE_TYPE_RENDERING_INFO };
    renderInfo.renderArea.extent.width = width;
    renderInfo.renderArea.extent.height = height;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = colorAttachmentCount;
    renderInfo.pColorAttachments = colorAttachments;
    renderInfo.pDepthAttachment = depthAttachment;
    return renderInfo;
}