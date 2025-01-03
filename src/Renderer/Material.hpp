#pragma once

#include <string>

#include <glm/vec4.hpp>
#include <glm/vec3.hpp>

#include "Color.hpp"
#include "IdTypes.hpp"

struct alignas(16) MaterialData {
    glm::vec4 diffuseFactor{1.f, 1.f, 1.f, 1.f}; //baseColor
    glm::vec4 specularFactor{1.f, 1.f, 1.f, 1.f};
    glm::vec3 emissiveFactor{1.f, 1.f, 1.f}; // = vec3(material->emissive_factor[0], material->emissive_factor[1], material->emissive_factor[2]);
    float emissiveStrength{1.f};

    std::uint32_t diffuseTex;
    std::uint32_t normalTex;
    std::uint32_t metallicRoughnessTex;
    std::uint32_t emissiveTex;
};

struct Material {
    glm::vec4 diffuseFactor{1.f, 1.f, 1.f, 1.f}; //baseColor
    glm::vec4 specularFactor{1.f, 1.f, 1.f, 1.f};
    glm::vec3 emissiveFactor{1.f, 1.f, 1.f}; // = vec3(material->emissive_factor[0], material->emissive_factor[1], material->emissive_factor[2]);
    float emissiveStrength{1.f};

    ImageId diffuseTexture{NULL_IMAGE_ID}; // albedoTexture
    ImageId normalMapTexture{NULL_IMAGE_ID};
    ImageId metallicRoughnessTexture{NULL_IMAGE_ID}; // specularTexture
    ImageId emissiveTexture{NULL_IMAGE_ID};

    std::string name;
};
