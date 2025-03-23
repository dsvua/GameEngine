#pragma once

#include "niagara/math.h"
#include "niagara/common.h"
#include "niagara/shaders.h"
#include <stdint.h>

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