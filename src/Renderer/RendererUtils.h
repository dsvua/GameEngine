#pragma once

#include "niagara/math.h"
#include "niagara/common.h"
#include "niagara/shaders.h"
#include "niagara/resources.h"
#include "GfxTypes.h"
#include <stdint.h>

// Forward declaration
class Camera;

/**
 * PCG (Permuted Congruential Generator) random number generator state.
 * Holds the current state and increment for the generator.
 */
struct pcg32_random_t
{
    uint64_t state;
    uint64_t inc;
};

/**
 * Default initializer for PCG random number generator.
 * Uses pre-defined seed values for state and increment.
 */
#define PCG32_INITIALIZER \
    { \
        0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL \
    }

/**
 * Creates a perspective projection matrix for rendering.
 * Uses reverse-Z for better precision in the distance.
 * 
 * @param fovY Vertical field of view in radians
 * @param aspectWbyH Aspect ratio (width divided by height)
 * @param zNear Near clip plane distance
 * @return Perspective projection matrix
 */
mat4 perspectiveProjection(float fovY, float aspectWbyH, float zNear);

/**
 * Normalizes a plane equation by dividing by the length of its normal vector.
 * 
 * @param p Plane equation in ax+by+cz+d=0 form
 * @return Normalized plane equation
 */
vec4 normalizePlane(vec4 p);

/**
 * Finds the largest power of 2 that is less than or equal to the input value.
 * 
 * @param v Input value
 * @return Largest power of 2 less than or equal to v
 */
uint32_t previousPow2(uint32_t v);

/**
 * Generates a random 32-bit unsigned integer using PCG algorithm.
 * PCG is a family of simple fast space-efficient statistically good algorithms 
 * for random number generation.
 * 
 * @param rng Pointer to the random number generator state
 * @return Random 32-bit unsigned integer
 */
uint32_t pcg32_random_r(pcg32_random_t* rng);

/**
 * Generates a random double in the range [0,1).
 * Uses the PCG algorithm with the global random state.
 * 
 * @return Random double between 0 (inclusive) and 1 (exclusive)
 */
double rand01();

/**
 * Generates a random 32-bit unsigned integer.
 * Uses the PCG algorithm with the global random state.
 * 
 * @return Random 32-bit unsigned integer
 */
uint32_t rand32();

/**
 * Creates a VkRenderingAttachmentInfo structure for color attachments.
 * 
 * @param imageView The image view to attach
 * @param imageLayout The layout of the image
 * @param loadOp The load operation to perform
 * @param clearValue The clear value to use
 * @return Initialized VkRenderingAttachmentInfo structure
 */
VkRenderingAttachmentInfo createColorAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, VkAttachmentLoadOp loadOp, const VkClearColorValue& clearValue);

/**
 * Creates a VkRenderingAttachmentInfo structure for depth attachments.
 * 
 * @param imageView The image view to attach
 * @param imageLayout The layout of the image
 * @param loadOp The load operation to perform
 * @param clearValue The clear value to use
 * @return Initialized VkRenderingAttachmentInfo structure
 */
VkRenderingAttachmentInfo createDepthAttachmentInfo(VkImageView imageView, VkImageLayout imageLayout, VkAttachmentLoadOp loadOp, const VkClearDepthStencilValue& clearValue);

/**
 * Creates a VkViewport structure.
 * 
 * @param width Viewport width
 * @param height Viewport height
 * @return Initialized VkViewport structure
 */
VkViewport createViewport(float width, float height);

/**
 * Creates a VkRect2D structure for scissor test.
 * 
 * @param width Scissor width
 * @param height Scissor height
 * @return Initialized VkRect2D structure
 */
VkRect2D createScissorRect(uint32_t width, uint32_t height);

/**
 * Creates image barriers for transitioning render targets from undefined to attachment optimal.
 * 
 * @param depthImage The depth image to transition
 * @param colorImages Array of color images to transition
 * @param colorCount Number of color images
 * @param barriers Output array to store the barriers (should be large enough for colorCount + 1)
 */
void createRenderTargetBarriers(VkImage depthImage, const VkImage* colorImages, uint32_t colorCount, VkImageMemoryBarrier2* barriers);

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
    VkAccessFlags2 dstAccessMask);

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
    const VkRenderingAttachmentInfo* depthAttachment);

/**
 * Dispatches a compute shader with push constants and descriptors.
 * Handles workgroup calculations and descriptor binding.
 * 
 * @tparam PushConstants Type of the push constants structure
 * @tparam PushDescriptors Number of push descriptors
 * @param commandBuffer Command buffer to record the dispatch command to
 * @param program Shader program to execute
 * @param threadCountX Number of threads in X dimension
 * @param threadCountY Number of threads in Y dimension
 * @param pushConstants Push constants data to send to the shader
 * @param pushDescriptors Array of descriptor infos to bind
 */
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