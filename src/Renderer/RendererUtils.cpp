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