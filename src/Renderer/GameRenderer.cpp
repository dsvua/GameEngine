#include "GameRenderer.hpp"
#include "GfxDevice.hpp"
// #include "Vulkan/Shaders.hpp"
#include <volk.h>

void GameRenderer::init(GfxDevice& gfxDevice, const glm::ivec2 &drawImageSize)
{
	createPrograms(gfxDevice);
	createPipelines(gfxDevice);
}

void GameRenderer::initSceneData(GfxDevice& gfxDevice, const std::filesystem::path &p)
{
    // loadGLTF and textures here

    // Create GPU buffers

    // Upload GPU buffers
}

void GameRenderer::draw(VkCommandBuffer cmd, GfxDevice &gfxDevice, const Camera &camera, const SceneData &sceneData)
{
    // run pipelines here
}

void GameRenderer::createPipelines(GfxDevice& gfxDevice)
{
	auto replace = [&](VkPipeline& pipeline, VkPipeline newPipeline)
	{
		if (pipeline)
			vkDestroyPipeline(gfxDevice.getDevice(), pipeline, 0);
		assert(newPipeline);
		pipeline = newPipeline;
		pipelines.push_back(newPipeline);
	};

	pipelines.clear();

	replace(debugtextPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, debugtextProgram));

	replace(drawcullPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, drawcullProgram, { /* LATE= */ false, /* TASK= */ false }));
	replace(drawculllatePipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, drawcullProgram, { /* LATE= */ true, /* TASK= */ false }));
	replace(taskcullPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, drawcullProgram, { /* LATE= */ false, /* TASK= */ true }));
	replace(taskculllatePipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, drawcullProgram, { /* LATE= */ true, /* TASK= */ true }));

	replace(tasksubmitPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, tasksubmitProgram));
	replace(clustersubmitPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, clustersubmitProgram));

	replace(clustercullPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, clustercullProgram, { /* LATE= */ false }));
	replace(clusterculllatePipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, clustercullProgram, { /* LATE= */ true }));

	replace(depthreducePipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, depthreduceProgram));

	replace(meshPipeline, createGraphicsPipeline(gfxDevice.getDevice(), pipelineCache, gbufferInfo, meshProgram));
	replace(meshpostPipeline, createGraphicsPipeline(gfxDevice.getDevice(), pipelineCache, gbufferInfo, meshProgram, { /* LATE= */ false, /* TASK= */ false, /* POST= */ 1 }));

	if (gfxDevice.meshShadingSupported)
	{
		replace(meshtaskPipeline, createGraphicsPipeline(gfxDevice.getDevice(), pipelineCache, gbufferInfo, meshtaskProgram, { /* LATE= */ false, /* TASK= */ true }));
		replace(meshtasklatePipeline, createGraphicsPipeline(gfxDevice.getDevice(), pipelineCache, gbufferInfo, meshtaskProgram, { /* LATE= */ true, /* TASK= */ true }));
		replace(meshtaskpostPipeline, createGraphicsPipeline(gfxDevice.getDevice(), pipelineCache, gbufferInfo, meshtaskProgram, { /* LATE= */ true, /* TASK= */ true, /* POST= */ 1 }));

		replace(clusterPipeline, createGraphicsPipeline(gfxDevice.getDevice(), pipelineCache, gbufferInfo, clusterProgram, { /* LATE= */ false, /* TASK= */ false }));
		replace(clusterpostPipeline, createGraphicsPipeline(gfxDevice.getDevice(), pipelineCache, gbufferInfo, clusterProgram, { /* LATE= */ false, /* TASK= */ false, /* POST= */ 1 }));
	}

	replace(shadePipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, shadeProgram));

	if (gfxDevice.raytracingSupported)
	{
		replace(shadowlqPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, shadowProgram, { /* QUALITY= */ 0 }));
		replace(shadowhqPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, shadowProgram, { /* QUALITY= */ 1 }));
		replace(shadowfillPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, shadowfillProgram));
		replace(shadowblurPipeline, createComputePipeline(gfxDevice.getDevice(), pipelineCache, shadowblurProgram));
	}
}

void GameRenderer::createPrograms(GfxDevice& gfxDevice) 
{
	gbufferInfo.colorAttachmentCount = gbufferCount;
	gbufferInfo.pColorAttachmentFormats = gbufferFormats;
	gbufferInfo.depthAttachmentFormat = depthFormat;

	bool rcs = loadShaders(shaders, gfxDevice.getDevice(), "../", "spirv/");
	assert(rcs);

	textureSetLayout = createDescriptorArrayLayout(gfxDevice.getDevice());

	Program debugtextProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["debugtext.comp"] }, sizeof(TextData));

	Program drawcullProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["drawcull.comp"] }, sizeof(CullData));
	Program tasksubmitProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["tasksubmit.comp"] }, 0);
	Program clustersubmitProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["clustersubmit.comp"] }, 0);
	Program clustercullProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["clustercull.comp"] }, sizeof(CullData));
	Program depthreduceProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["depthreduce.comp"] }, sizeof(vec4));
	Program meshProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_GRAPHICS, { &shaders["mesh.vert"], &shaders["mesh.frag"] }, sizeof(Globals), textureSetLayout);

	Program meshtaskProgram = {};
	Program clusterProgram = {};
	if (gfxDevice.meshShadingSupported)
	{
		meshtaskProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_GRAPHICS, { &shaders["meshlet.task"], &shaders["meshlet.mesh"], &shaders["mesh.frag"] }, sizeof(Globals), textureSetLayout);
		clusterProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_GRAPHICS, { &shaders["meshlet.mesh"], &shaders["mesh.frag"] }, sizeof(Globals), textureSetLayout);
	}

	Program shadeProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["shade.comp"] }, sizeof(ShadeData));

	Program shadowProgram = {};
	Program shadowfillProgram = {};
	Program shadowblurProgram = {};
	if (gfxDevice.raytracingSupported)
	{
		shadowProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["shadow.comp"] }, sizeof(ShadowData), textureSetLayout);
		shadowfillProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["shadowfill.comp"] }, sizeof(vec4));
		shadowblurProgram = createProgram(gfxDevice.getDevice(), VK_PIPELINE_BIND_POINT_COMPUTE, { &shaders["shadowblur.comp"] }, sizeof(vec4));
	}

}

