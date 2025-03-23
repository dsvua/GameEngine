# Coding Style Consistency Checklist

## Member Variables (`m_` Prefix)

### GfxDevice.h
- [x] Rename `window` to `m_window`
- [x] Rename `physicalDevice` to `m_physicalDevice`
- [x] Rename `instance` to `m_instance`
- [ ] Rename `gbufferInfo` to `m_gbufferInfo`
- [ ] Rename `memoryProperties` to `m_memoryProperties`
- [ ] Rename `familyIndex` to `m_familyIndex`
- [ ] Rename `device` to `m_device`
- [ ] Rename `surface` to `m_surface`
- [ ] Rename `swapchain` to `m_swapchain`
- [ ] Rename `swapchainFormat` to `m_swapchainFormat`
- [ ] Rename `depthFormat` to `m_depthFormat`
- [ ] Rename `props` to `m_props`
- [ ] Rename `debugCallback` to `m_debugCallback`
- [ ] Rename `swapchainImageViews` to `m_swapchainImageViews`

### Renderer.h - FrameData Struct
- [ ] Rename `waitSemaphore` to `m_waitSemaphore`
- [ ] Rename `signalSemaphore` to `m_signalSemaphore`
- [ ] Rename `renderFence` to `m_renderFence`
- [ ] Rename `frameTimeStamp` to `m_frameTimeStamp`
- [ ] Rename `deltaTime` to `m_deltaTime`
- [ ] Rename `commandPool` to `m_commandPool`
- [ ] Rename `commandBuffer` to `m_commandBuffer`

### Renderer.h - Pipelines Struct
- [ ] Rename `debugtextPipeline` to `m_debugtextPipeline`
- [ ] Rename `drawcullPipeline` to `m_drawcullPipeline`
- [ ] Rename `drawculllatePipeline` to `m_drawculllatePipeline`
- [ ] Rename `taskcullPipeline` to `m_taskcullPipeline`
- [ ] Rename `taskculllatePipeline` to `m_taskculllatePipeline`
- [ ] Rename `tasksubmitPipeline` to `m_tasksubmitPipeline`
- [ ] Rename `clustersubmitPipeline` to `m_clustersubmitPipeline`
- [ ] Rename `clustercullPipeline` to `m_clustercullPipeline`
- [ ] Rename `clusterculllatePipeline` to `m_clusterculllatePipeline`
- [ ] Rename `depthreducePipeline` to `m_depthreducePipeline`
- [ ] Rename `meshPipeline` to `m_meshPipeline`
- [ ] Rename `meshpostPipeline` to `m_meshpostPipeline`
- [ ] Rename `meshtaskPipeline` to `m_meshtaskPipeline`
- [ ] Rename `meshtasklatePipeline` to `m_meshtasklatePipeline`
- [ ] Rename `meshtaskpostPipeline` to `m_meshtaskpostPipeline`
- [ ] Rename `clusterPipeline` to `m_clusterPipeline`
- [ ] Rename `clusterpostPipeline` to `m_clusterpostPipeline`
- [ ] Rename `blitPipeline` to `m_blitPipeline`
- [ ] Rename `finalPipeline` to `m_finalPipeline`
- [ ] Rename `shadowlqPipeline` to `m_shadowlqPipeline`
- [ ] Rename `shadowhqPipeline` to `m_shadowhqPipeline`
- [ ] Rename `shadowfillPipeline` to `m_shadowfillPipeline`
- [ ] Rename `shadowblurPipeline` to `m_shadowblurPipeline`
- [ ] Rename `pipelines` to `m_pipelines`
- [ ] Rename `pipelineCache` to `m_pipelineCache`

### Renderer.h - Programs Struct
- [ ] Rename `debugtextProgram` to `m_debugtextProgram`
- [ ] Rename `drawcullProgram` to `m_drawcullProgram`
- [ ] Rename `tasksubmitProgram` to `m_tasksubmitProgram`
- [ ] Rename `clustersubmitProgram` to `m_clustersubmitProgram`
- [ ] Rename `clustercullProgram` to `m_clustercullProgram`
- [ ] Rename `depthreduceProgram` to `m_depthreduceProgram`
- [ ] Rename `meshProgram` to `m_meshProgram`
- [ ] Rename `meshtaskProgram` to `m_meshtaskProgram`
- [ ] Rename `clusterProgram` to `m_clusterProgram`
- [ ] Rename `finalProgram` to `m_finalProgram`
- [ ] Rename `shadowProgram` to `m_shadowProgram`
- [ ] Rename `shadowfillProgram` to `m_shadowfillProgram`
- [ ] Rename `shadowblurProgram` to `m_shadowblurProgram`

### Renderer.h - Samplers Struct
- [ ] Rename `textureSampler` to `m_textureSampler`
- [ ] Rename `readSampler` to `m_readSampler`
- [ ] Rename `depthSampler` to `m_depthSampler`

### Renderer.h - Buffers Struct
- [ ] Rename `scratch` to `m_scratch`
- [ ] Rename `meshesh` to `m_meshesh`
- [ ] Rename `materials` to `m_materials`
- [ ] Rename `vertices` to `m_vertices`
- [ ] Rename `indices` to `m_indices`
- [ ] Rename `meshlets` to `m_meshlets`
- [ ] Rename `meshletdata` to `m_meshletdata`
- [ ] Rename `draw` to `m_draw`
- [ ] Rename `DrawVisibility` to `m_drawVisibility` (also fix capitalization)
- [ ] Rename `TaskCommands` to `m_taskCommands` (also fix capitalization)
- [ ] Rename `CommandCount` to `m_commandCount` (also fix capitalization)
- [ ] Rename `MeshletVisibility` to `m_meshletVisibility` (also fix capitalization)
- [ ] Rename `blasBuffer` to `m_blasBuffer`
- [ ] Rename `tlasBuffer` to `m_tlasBuffer`
- [ ] Rename `tlasScratchBuffer` to `m_tlasScratchBuffer`
- [ ] Rename `tlasInstanceBuffer` to `m_tlasInstanceBuffer`
- [ ] Rename `tlas` to `m_tlas`
- [ ] Rename `DrawVisibilityCleared` to `m_drawVisibilityCleared` (also fix capitalization)
- [ ] Rename `MeshletVisibilityCleared` to `m_meshletVisibilityCleared` (also fix capitalization)
- [ ] Rename `meshletVisibilityBytes` to `m_meshletVisibilityBytes`

### Renderer.h - Timestamps Struct
- [ ] Rename `frameTimestamp` to `m_frameTimestamp`
- [ ] Rename `frameCpuBegin` to `m_frameCpuBegin`
- [ ] Rename `frameGpuBegin` to `m_frameGpuBegin`
- [ ] Rename `frameGpuEnd` to `m_frameGpuEnd`

## Constants and Macro Naming (UPPER_CASE)

- [ ] Rename `gbufferCount` to `GBUFFER_COUNT` in Renderer.h (line 9)
- [ ] Check codebase for any other non-uppercased constants/macros

## Variable and Function Naming (camelCase)

### thread_name.hpp
- [ ] Rename `thread_name` to `threadName` (line 13)
- [ ] Rename `thread_id` to `threadId` (line 14)
- [ ] Rename `hook_init_ex_cpu_thread_name` to `hookInitExCpuThreadName` (line 18)
- [ ] Rename `hook_init_ex_cpu_thread_id` to `hookInitExCpuThreadId` (line 25)
- [ ] Rename `get_thread_name` to `getThreadName` (line 34)
- [ ] Rename `get_thread_id` to `getThreadId` (line 45)
- [ ] Rename `print_thread_name` to `printThreadName` (line 49)

### niagara/common.h
- [ ] Rename `countof_helper` to `countofHelper` (line 44)

### Update Related References
- [ ] Update all references to the renamed functions and variables in main.cpp
- [ ] Update all references to the renamed functions and variables in Renderer.cpp
- [ ] Check all other files for references to renamed identifiers

## Indentation Consistency (4 spaces, no tabs)

### Files to check and fix:
- [ ] GfxDevice.h (line 14 has inconsistent indentation)
- [ ] Renderer.h (check all indentation)
- [ ] main.cpp (check all indentation)
- [ ] Renderer.cpp (check all indentation)
- [ ] thread_name.hpp (check all indentation)
- [ ] All other source files in src/ directory

## File Extension Consistency (.hpp/.cpp for C++, .h for C-compatible)

- [ ] No issues found with file extensions (thread_name already uses .hpp)
- [ ] Verify if any other files in the codebase need extension updates

## Braces Style (Allman - new line)

- [ ] No issues found with brace style
- [ ] Verify braces style consistency in Renderer.cpp (not fully checked due to file size)

## Error Handling with VK_CHECK

- [ ] Verify all Vulkan API calls use VK_CHECK or appropriate variants
- [ ] Check for any direct result checking that should use VK_CHECK instead

## Include Guards (#pragma once)

- [ ] No issues found with include guards (all headers have #pragma once)

## Documentation 

- [ ] Verify all function declarations have docstring comments
- [ ] Add missing docstring comments to functions in Renderer.h and GfxDevice.h

## Line Length (under 120 characters)

- [ ] Check all files for lines exceeding 120 characters

## General Organization

- [ ] Review all files for additional style inconsistencies
- [ ] Ensure code follows established patterns in the project