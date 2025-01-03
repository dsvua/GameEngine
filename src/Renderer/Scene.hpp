#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "CPUMesh.hpp"
#include "Light.hpp"
#include "Material.hpp"
#include "SkeletalAnimation.hpp"
#include "Skeleton.hpp"
#include "../Math/AABB.hpp"
#include "../Math/Transform.hpp"

struct SceneNode {
    std::string name;
    Transform transform;

    int meshIndex{-1}; // index in Scene::meshes
    int skinId{-1}; // index in Scene::skeletons
    int lightId{-1}; // index in Scene::lights
    int cameraId{-1}; // unused for now

    // hierarchy
    SceneNode* parent{nullptr};
    std::vector<std::unique_ptr<SceneNode>> children;
};

struct SceneMesh {
    std::vector<MeshId> primitives;
    std::vector<MaterialId> primitiveMaterials;
};

struct Scene {
    std::filesystem::path path;

    // root nodes
    std::vector<std::unique_ptr<SceneNode>> nodes;

    std::vector<SceneMesh> meshes;
    std::vector<Skeleton> skeletons;
    std::unordered_map<std::string, SkeletalAnimation> animations;
    std::vector<Light> lights;
    std::unordered_map<MeshId, CPUMesh> cpuMeshes;
};

namespace edbr
{
math::AABB calculateBoundingBoxLocal(const Scene& scene, const std::vector<MeshId> meshes);
}
