#pragma once

#include "GfxDevice.h"
#include "GfxTypes.h"
#include "niagara/shaders.h"
#include "niagara/resources.h"
#include <chrono>

static const size_t gbufferCount = 2UL;

struct FrameData {
    VkSemaphore waitSemaphore, signalSemaphore;
    VkFence renderFence;
	std::chrono::_V2::system_clock::time_point frameTimeStamp;
	int64_t deltaTime; // number of milliseconds

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
};

struct Pipelines {
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
	VkPipeline finalPipeline = 0;
	VkPipeline shadowlqPipeline = 0;
	VkPipeline shadowhqPipeline = 0;
	VkPipeline shadowfillPipeline = 0;
	VkPipeline shadowblurPipeline = 0;

	std::vector<VkPipeline> pipelines;
    VkPipelineCache pipelineCache = 0;
};

struct Programs {
	Program debugtextProgram = {};
	Program drawcullProgram = {};
	Program tasksubmitProgram = {};
	Program clustersubmitProgram = {};
	Program clustercullProgram = {};
	Program depthreduceProgram = {};
	Program meshProgram = {};
	Program meshtaskProgram = {};
	Program clusterProgram = {};
	Program finalProgram = {};
	Program shadowProgram = {};
	Program shadowfillProgram = {};
	Program shadowblurProgram = {};
};

struct Samplers {
    VkSampler textureSampler;
    VkSampler readSampler;
    VkSampler depthSampler;
};

struct Buffers {
	Buffer scratch = {};
	Buffer meshesh = {}; // mb
	Buffer materials = {}; // mtb
	Buffer vertices = {}; // vb
	Buffer indices = {}; // ib
	Buffer meshlets = {}; // mlb
	Buffer meshletdata = {}; // mdb
	Buffer draw = {}; // db
	Buffer DrawVisibility = {}; // dvb
	Buffer TaskCommands = {}; // dcb
	Buffer CommandCount = {}; // dccb
	Buffer MeshletVisibility = {}; // mvb
	Buffer blasBuffer = {};
	Buffer tlasBuffer = {};
	Buffer tlasScratchBuffer = {};
	Buffer tlasInstanceBuffer = {};
	VkAccelerationStructureKHR tlas = nullptr;
	bool DrawVisibilityCleared = false;
	bool MeshletVisibilityCleared = false;
	uint32_t meshletVisibilityBytes;
};

struct Timestamps {
	std::chrono::_V2::system_clock::time_point frameTimestamp;
	std::chrono::_V2::system_clock::time_point frameCpuBegin;
	std::chrono::_V2::system_clock::time_point frameGpuBegin;
	std::chrono::_V2::system_clock::time_point frameGpuEnd;
};

class Renderer
{
public:
    Renderer();
    GfxDevice m_gfxDevice;
    FrameData m_frames[FRAMES_COUNT];
    VkDescriptorSetLayout m_textureSetLayout;
	std::pair<VkDescriptorPool, VkDescriptorSet> m_textureSet;
    VkQueryPool m_queryPoolTimestamp;
    VkQueryPool m_queryPoolPipeline;
	VkQueue m_queue = 0;

    uint64_t m_frameIndex = 0;
    uint64_t m_currentFrameIndex = 0;
    uint64_t m_lastFrameIndex = 0;
    Buffers m_buffers;
    Samplers m_samplers;
    Programs m_programs;
    Pipelines m_pipelines;
    ShaderSet m_shaders;
    Camera m_camera;
    vec3 m_sunDirection;
	float m_drawDistance = 200;
    CullData m_cullData = {};
    Globals m_globals = {};
	std::vector<Image> m_images;
    mat4 m_projection, m_projectionT, m_frustumX, m_frustumY, m_view;

	Geometry m_geometry;
	std::vector<Material> m_materials;
	std::vector<MeshDraw> m_draws;
	std::vector<Animation> m_animations;
	std::vector<std::string> m_texturePaths;

	const VkFormat m_gbufferFormats[gbufferCount] = {
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	};

	Image m_gbufferTargets[gbufferCount] = {};
	Image m_depthTarget = {};
	Image m_shadowTarget = {};
	Image m_shadowblurTarget = {};
	Image m_depthPyramid = {};
	VkImageView m_depthPyramidMips[16] = {};
	uint32_t m_depthPyramidWidth = 0;
	uint32_t m_depthPyramidHeight = 0;
	uint32_t m_depthPyramidLevels = 0;
	uint32_t m_meshPostPasses = 0;
	uint32_t m_imageIndex = 0;
    VkClearColorValue m_colorClear = { 135.f / 255.f, 206.f / 255.f, 250.f / 255.f, 15.f / 255.f };
    VkClearDepthStencilValue m_depthClear = { 0.f, 0 };
	uint64_t m_timestampResults[23] = {};
	uint64_t m_pipelineResults[3] = {};
	double m_frameGpuAvg = 0;
	double m_frameCpuAvg = 0;
	size_t m_imageMemory = 0;

    // immediate submit structures
    VkFence m_immFence;
    VkCommandBuffer m_immCommandBuffer;
    VkCommandPool m_immCommandPool;
    
	std::vector<VkAccelerationStructureKHR> m_blas;
	std::vector<VkDeviceAddress> m_blasAddresses;
	VkAccelerationStructureKHR m_tlas = nullptr;
	bool m_tlasNeedsRebuild = true;

    void createPrograms();
    void createPipelines();
    void createFramesData();

	bool beginFrame();
	void drawCull(VkPipeline pipeline, uint32_t timestamp, const char* phase, bool late, unsigned int postPass = 0);
    void drawRender(bool late, const VkClearColorValue& colorClear, const VkClearDepthStencilValue& depthClear, uint32_t query, uint32_t timestamp, const char* phase, unsigned int postPass = 0);
    void drawPyramid(uint32_t timestamp);
	void drawDebug();
	void drawShadow();
	void draw();
	void endFrame();
	void cleanup();

	void niagaraShadows();
	bool loadGLTFScene(std::string filename);
};