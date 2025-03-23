#pragma once

#include "GfxDevice.h"
#include "GfxTypes.h"
#include "Camera.h"
#include "niagara/shaders.h"
#include "niagara/resources.h"
#include <chrono>

static const size_t GBUFFER_COUNT = 2UL;

struct FrameData {
    VkSemaphore m_waitSemaphore, m_signalSemaphore;
    VkFence m_renderFence;
	std::chrono::_V2::system_clock::time_point m_frameTimeStamp;
	int64_t m_deltaTime; // number of milliseconds

    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffer;
};

struct Pipelines {
	VkPipeline m_debugtextPipeline = 0;
	VkPipeline m_drawcullPipeline = 0;
	VkPipeline m_drawculllatePipeline = 0;
	VkPipeline m_taskcullPipeline = 0;
	VkPipeline m_taskculllatePipeline = 0;
	VkPipeline m_tasksubmitPipeline = 0;
	VkPipeline m_clustersubmitPipeline = 0;
	VkPipeline m_clustercullPipeline = 0;
	VkPipeline m_clusterculllatePipeline = 0;
	VkPipeline m_depthreducePipeline = 0;
	VkPipeline m_meshPipeline = 0;
	VkPipeline m_meshpostPipeline = 0;
	VkPipeline m_meshtaskPipeline = 0;
	VkPipeline m_meshtasklatePipeline = 0;
	VkPipeline m_meshtaskpostPipeline = 0;
	VkPipeline m_clusterPipeline = 0;
	VkPipeline m_clusterpostPipeline = 0;
	VkPipeline m_blitPipeline = 0;
	VkPipeline m_finalPipeline = 0;
	VkPipeline m_shadowlqPipeline = 0;
	VkPipeline m_shadowhqPipeline = 0;
	VkPipeline m_shadowfillPipeline = 0;
	VkPipeline m_shadowblurPipeline = 0;

	std::vector<VkPipeline> m_pipelines;
    VkPipelineCache m_pipelineCache = 0;
};

struct Programs {
	Program m_debugtextProgram = {};
	Program m_drawcullProgram = {};
	Program m_tasksubmitProgram = {};
	Program m_clustersubmitProgram = {};
	Program m_clustercullProgram = {};
	Program m_depthreduceProgram = {};
	Program m_meshProgram = {};
	Program m_meshtaskProgram = {};
	Program m_clusterProgram = {};
	Program m_finalProgram = {};
	Program m_shadowProgram = {};
	Program m_shadowfillProgram = {};
	Program m_shadowblurProgram = {};
};

struct Samplers {
    VkSampler m_textureSampler;
    VkSampler m_readSampler;
    VkSampler m_depthSampler;
};

struct Buffers {
	Buffer m_scratch = {};
	Buffer m_meshesh = {}; // mb
	Buffer m_materials = {}; // mtb
	Buffer m_vertices = {}; // vb
	Buffer m_indices = {}; // ib
	Buffer m_meshlets = {}; // mlb
	Buffer m_meshletdata = {}; // mdb
	Buffer m_draw = {}; // db
	Buffer m_drawVisibility = {}; // dvb (also fixed capitalization)
	Buffer m_taskCommands = {}; // dcb (also fixed capitalization)
	Buffer m_commandCount = {}; // dccb (also fixed capitalization)
	Buffer m_meshletVisibility = {}; // mvb (also fixed capitalization)
	Buffer m_blasBuffer = {};
	Buffer m_tlasBuffer = {};
	Buffer m_tlasScratchBuffer = {};
	Buffer m_tlasInstanceBuffer = {};
	VkAccelerationStructureKHR m_tlas = nullptr;
	bool m_drawVisibilityCleared = false; // also fixed capitalization
	bool m_meshletVisibilityCleared = false; // also fixed capitalization
	uint32_t m_meshletVisibilityBytes;
};

struct Timestamps {
	std::chrono::_V2::system_clock::time_point m_frameTimestamp;
	std::chrono::_V2::system_clock::time_point m_frameCpuBegin;
	std::chrono::_V2::system_clock::time_point m_frameGpuBegin;
	std::chrono::_V2::system_clock::time_point m_frameGpuEnd;
};

class Renderer
{
public:
    /**
     * Initializes the renderer with default settings.
     * Creates the graphics device, shaders, pipelines, and frame data.
     */
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

	const VkFormat m_gbufferFormats[GBUFFER_COUNT] = {
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	};

	Image m_gbufferTargets[GBUFFER_COUNT] = {};
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

    /**
     * Creates shader programs used in the renderer.
     * Initializes compute and graphics shader combinations.
     */
    void createPrograms();
    
    /**
     * Creates rendering pipelines for all render passes.
     * Sets up pipeline state and shader bindings.
     */
    void createPipelines();
    
    /**
     * Creates per-frame data structures for multi-buffered rendering.
     * Initializes semaphores, fences, and command buffers.
     */
    void createFramesData();

    /**
     * Begins a new frame for rendering.
     * Acquires next swapchain image and prepares command buffers.
     * @return True if frame setup was successful, false otherwise
     */
	bool beginFrame();
	
	/**
     * Performs visibility culling for mesh draws.
     * @param pipeline The culling pipeline to use
     * @param timestamp Query index for performance timing
     * @param phase Debug name for the culling phase
     * @param late Whether this is late culling (after main render)
     * @param postPass Post-processing pass index
     */
	void drawCull(VkPipeline pipeline, uint32_t timestamp, const char* phase, bool late, unsigned int postPass = 0);
    
    /**
     * Renders the scene with current visibility data.
     * @param late Whether this is late rendering (after main render)
     * @param colorClear Color value used for clearing color attachments
     * @param depthClear Depth value used for clearing depth attachment
     * @param query Query index for pipeline statistics
     * @param timestamp Query index for performance timing
     * @param phase Debug name for the render phase
     * @param postPass Post-processing pass index
     */
    void drawRender(bool late, const VkClearColorValue& colorClear, const VkClearDepthStencilValue& depthClear, uint32_t query, uint32_t timestamp, const char* phase, unsigned int postPass = 0);
    
    /**
     * Generates mip chain for depth pyramid used in hierarchical Z-buffer.
     * @param timestamp Query index for performance timing
     */
    void drawPyramid(uint32_t timestamp);
    
    /**
     * Renders debug visualization overlays.
     * Displays performance statistics and debug information.
     */
	void drawDebug();
	
	/**
     * Renders shadow maps for the scene.
     * Handles shadow cascades and filtering.
     */
	void drawShadow();
	
	/**
     * Main draw function that renders a complete frame.
     * Orchestrates the entire rendering pipeline.
     */
	void draw();
	
	/**
     * Completes frame rendering and presents to display.
     * Submits command buffers and handles swapchain presentation.
     */
	void endFrame();
	
	/**
     * Releases all Vulkan resources.
     * Called during shutdown to prevent resource leaks.
     */
	void cleanup();

    /**
     * Sets up shadow rendering for the Niagara engine.
     * Configures shadow mapping resources and parameters.
     */
	void niagaraShadows();
	
	/**
     * Loads a 3D scene from a GLTF file.
     * @param filename Path to the GLTF file
     * @return True if loading was successful, false otherwise
     */
	bool loadGLTFScene(std::string filename);
};