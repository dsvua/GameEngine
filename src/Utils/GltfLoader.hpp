#pragma once

#include <filesystem>
#include <vulkan/vulkan.h>
#include <vector>
#include "../Renderer/Asset.hpp"
#include "../Math/Math.hpp"

namespace util
{
bool loadScene(Geometry& geometry, 
        std::vector<Material>& materials, 
        std::vector<MeshDraw>& draws, 
        std::vector<std::string>& texturePaths, 
        std::vector<Animation>& animations, 
        Camera& camera, 
        glm::vec3& sunDirection, 
        const char* path, 
        bool buildMeshlets, 
        bool fast = false);
}
