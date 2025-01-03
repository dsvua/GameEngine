#pragma once

#include <cstdint>

// #include <volk.h>
#include <vulkan/vulkan.h>

namespace graphics
{
void generateMipmaps(
    VkCommandBuffer cmd,
    VkImage image,
    VkExtent2D imageSize,
    std::uint32_t mipLevels);
}
