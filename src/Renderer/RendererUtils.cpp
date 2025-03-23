#include "RendererUtils.h"
#include <math.h>

/**
 * Creates a perspective projection matrix for rendering.
 * Uses reverse-Z for better precision in the distance.
 * 
 * @param fovY Vertical field of view in radians
 * @param aspectWbyH Aspect ratio (width divided by height)
 * @param zNear Near clip plane distance
 * @return Perspective projection matrix
 */
mat4 perspectiveProjection(float fovY, float aspectWbyH, float zNear)
{
    float f = 1.0f / tanf(fovY / 2.0f);
    return mat4(
        f / aspectWbyH, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, zNear, 0.0f);
}

/**
 * Normalizes a plane equation by dividing by the length of its normal vector.
 * 
 * @param p Plane equation in ax+by+cz+d=0 form
 * @return Normalized plane equation
 */
vec4 normalizePlane(vec4 p)
{
    return p / length(vec3(p));
}

/**
 * Finds the largest power of 2 that is less than or equal to the input value.
 * 
 * @param v Input value
 * @return Largest power of 2 less than or equal to v
 */
uint32_t previousPow2(uint32_t v)
{
    uint32_t r = 1;

    while (r * 2 < v)
        r *= 2;

    return r;
}

/**
 * Global random number generator state.
 */
pcg32_random_t rngstate = PCG32_INITIALIZER;

/**
 * Generates a random 32-bit unsigned integer using PCG algorithm.
 * PCG is a family of simple fast space-efficient statistically good algorithms 
 * for random number generation.
 * 
 * @param rng Pointer to the random number generator state
 * @return Random 32-bit unsigned integer
 */
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

/**
 * Generates a random double in the range [0,1).
 * Uses the PCG algorithm with the global random state.
 * 
 * @return Random double between 0 (inclusive) and 1 (exclusive)
 */
double rand01()
{
    return pcg32_random_r(&rngstate) / double(1ull << 32);
}

/**
 * Generates a random 32-bit unsigned integer.
 * Uses the PCG algorithm with the global random state.
 * 
 * @return Random 32-bit unsigned integer
 */
uint32_t rand32()
{
    return pcg32_random_r(&rngstate);
}

/**
 * Creates a VkRenderingAttachmentInfo structure for color attachments.
 * 
 * @param imageView The image view to attach
 * @param imageLayout The layout of the image
 * @param loadOp The load operation to perform
 * @param clearValue The clear value to use
 * @return Initialized VkRenderingAttachmentInfo structure
 */
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

/**
 * Creates a VkRenderingAttachmentInfo structure for depth attachments.
 * 
 * @param imageView The image view to attach
 * @param imageLayout The layout of the image
 * @param loadOp The load operation to perform
 * @param clearValue The clear value to use
 * @return Initialized VkRenderingAttachmentInfo structure
 */
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

/**
 * Creates a VkViewport structure.
 * 
 * @param width Viewport width
 * @param height Viewport height
 * @return Initialized VkViewport structure
 */
VkViewport createViewport(float width, float height)
{
    return { 0, height, width, -height, 0, 1 };
}

/**
 * Creates a VkRect2D structure for scissor test.
 * 
 * @param width Scissor width
 * @param height Scissor height
 * @return Initialized VkRect2D structure
 */
VkRect2D createScissorRect(uint32_t width, uint32_t height)
{
    return { { 0, 0 }, { width, height } };
}

/**
 * Creates image barriers for transitioning render targets from undefined to attachment optimal.
 * 
 * @param depthImage The depth image to transition
 * @param colorImages Array of color images to transition
 * @param colorCount Number of color images
 * @param barriers Output array to store the barriers (should be large enough for colorCount + 1)
 */
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

/**
 * Calculates the view matrix from camera position and orientation.
 * 
 * @param camera The camera with position and orientation
 * @return The view matrix for rendering
 */
mat4 calculateViewMatrix(const Camera& camera)
{
    mat4 view = glm::mat4_cast(camera.orientation);
    view[3] = vec4(camera.position, 1.0f);
    view = inverse(view);
    // Flip Z for Vulkan coordinate system
    return glm::scale(glm::identity<glm::mat4>(), vec3(1, 1, -1)) * view;
}

/**
 * Sets up culling data for the renderer based on camera and view parameters.
 * 
 * @param view The view matrix
 * @param projection The projection matrix 
 * @param camera The camera data
 * @param drawDistance Maximum render distance
 * @param drawCount Number of objects to potentially render
 * @param depthPyramidWidth Width of the depth pyramid texture
 * @param depthPyramidHeight Height of the depth pyramid texture 
 * @param screenHeight Screen height for LOD calculations
 * @return Initialized CullData structure
 */
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

/**
 * Clears a buffer using a command buffer and sets up necessary pipeline barriers.
 * 
 * @param commandBuffer The command buffer to record commands to
 * @param buffer The buffer to clear
 * @param size Size of the data to clear (in bytes)
 * @param srcStageMask Source pipeline stage mask for the barrier
 * @param dstStageMask Destination pipeline stage mask for the barrier
 * @param dstAccessMask Destination access mask for the barrier
 */
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

/**
 * Creates a VkRenderingInfo structure for dynamic rendering.
 * 
 * @param width Width of the render area
 * @param height Height of the render area
 * @param colorAttachments Array of color attachments
 * @param colorAttachmentCount Number of color attachments
 * @param depthAttachment Depth attachment
 * @return Initialized VkRenderingInfo structure
 */
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