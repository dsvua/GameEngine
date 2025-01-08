#pragma once

#include "../Math/Math.hpp"
#include "Vulkan/Shaders.hpp"
#include <filesystem>
#include <vulkan/vulkan.h>

struct SDL_Window;

class Camera;
class GfxDevice;
class SceneData;

struct MeshDrawCommand
{
	uint32_t drawId;
	VkDrawIndexedIndirectCommand indirect; // 5 uint32_t
};

struct MeshTaskCommand
{
	uint32_t drawId;
	uint32_t taskOffset;
	uint32_t taskCount;
	uint32_t lateDrawVisibility;
	uint32_t meshletVisibilityOffset;
};

struct alignas(16) CullData
{
	mat4 view;

	float P00, P11, znear, zfar;       // symmetric projection parameters
	float frustum[4];                  // data for left/right/top/bottom frustum planes
	float lodTarget;                   // lod target error at z=1
	float pyramidWidth, pyramidHeight; // depth pyramid size in texels

	uint32_t drawCount;

	int cullingEnabled;
	int lodEnabled;
	int occlusionEnabled;
	int clusterOcclusionEnabled;
	int clusterBackfaceEnabled;

	uint32_t postPass;
};

struct alignas(16) Globals
{
	mat4 projection;
	CullData cullData;
	float screenWidth, screenHeight;
};

struct alignas(16) ShadowData
{
	vec3 sunDirection;
	float sunJitter;

	mat4 inverseViewProjection;

	vec2 imageSize;
	unsigned int checkerboard;
};

struct alignas(16) ShadeData
{
	vec3 cameraPosition;
	float pad0;
	vec3 sunDirection;
	int shadowsEnabled;

	mat4 inverseViewProjection;

	vec2 imageSize;
};

struct alignas(16) TextData
{
	int offsetX, offsetY;
	int scale;
	unsigned int color;

	char data[112];
};

class GameRenderer {
public:
    void init(GfxDevice& gfxDevice, const glm::ivec2& drawImageSize);
    void initSceneData(GfxDevice& gfxDevice, const std::filesystem::path& p);
    void createPipelines(GfxDevice& gfxDevice);
    void createPrograms(GfxDevice& gfxDevice);
    void draw(VkCommandBuffer cmd,
        GfxDevice& gfxDevice,
        const Camera& camera,
        const SceneData& sceneData
        );
private:

	static const size_t gbufferCount = 2;
	const VkFormat gbufferFormats[gbufferCount] = {
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	};
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
    VkDescriptorSetLayout textureSetLayout;

    VkPipelineRenderingCreateInfo gbufferInfo = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    Program debugtextProgram = {};
    Program drawcullProgram = {};
    Program tasksubmitProgram = {};
    Program clustersubmitProgram = {};
    Program clustercullProgram = {};
    Program depthreduceProgram = {};
    Program meshProgram = {};
    Program meshtaskProgram = {};
    Program clusterProgram = {};
    Program shadeProgram = {};
    Program shadowProgram = {};
    Program shadowfillProgram = {};
    Program shadowblurProgram = {};

    ShaderSet shaders;

    VkPipelineCache pipelineCache = 0;
    VkPipeline debugtextPipeline = 0;
    VkPipeline drawcullPipeline = 0;
    VkPipeline drawculllatePipeline = 0;
    VkPipeline taskcullPipeline = 0;
    VkPipeline taskculllatePipeline = 0;
    VkPipeline tasksubmitPipeline = 0;
    VkPipeline clustersubmitPipeline = 0;
    VkPipeline clustercullPipeline = 0;
    VkPipeline clusterculllatePipeline = 0;
    VkPipeline depthreducePipeline = 0;
    VkPipeline meshPipeline = 0;
    VkPipeline meshpostPipeline = 0;
    VkPipeline meshtaskPipeline = 0;
    VkPipeline meshtasklatePipeline = 0;
    VkPipeline meshtaskpostPipeline = 0;
    VkPipeline clusterPipeline = 0;
    VkPipeline clusterpostPipeline = 0;
    VkPipeline blitPipeline = 0;
    VkPipeline shadePipeline = 0;
    VkPipeline shadowlqPipeline = 0;
    VkPipeline shadowhqPipeline = 0;
    VkPipeline shadowfillPipeline = 0;
    VkPipeline shadowblurPipeline = 0;

    std::vector<VkPipeline> pipelines;
};