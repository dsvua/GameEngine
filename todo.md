# Coding Style Consistency Checklist

## Member Variables (`m_` Prefix)

### GfxDevice.h
- [x] Rename `window` to `m_window`
- [x] Rename `physicalDevice` to `m_physicalDevice`
- [x] Rename `instance` to `m_instance`
- [x] Rename `gbufferInfo` to `m_gbufferInfo`
- [x] Rename `memoryProperties` to `m_memoryProperties`
- [x] Rename `familyIndex` to `m_familyIndex`
- [x] Rename `device` to `m_device`
- [x] Rename `surface` to `m_surface`
- [x] Rename `swapchain` to `m_swapchain`
- [x] Rename `swapchainFormat` to `m_swapchainFormat`
- [x] Rename `depthFormat` to `m_depthFormat`
- [x] Rename `props` to `m_props`
- [x] Rename `debugCallback` to `m_debugCallback`
- [x] Rename `swapchainImageViews` to `m_swapchainImageViews`

### Renderer.h - FrameData Struct
- [x] Rename `waitSemaphore` to `m_waitSemaphore`
- [x] Rename `signalSemaphore` to `m_signalSemaphore`
- [x] Rename `renderFence` to `m_renderFence`
- [x] Rename `frameTimeStamp` to `m_frameTimeStamp`
- [x] Rename `deltaTime` to `m_deltaTime`
- [x] Rename `commandPool` to `m_commandPool`
- [x] Rename `commandBuffer` to `m_commandBuffer`

### Renderer.h - Pipelines Struct
- [x] Rename `debugtextPipeline` to `m_debugtextPipeline`
- [x] Rename `drawcullPipeline` to `m_drawcullPipeline`
- [x] Rename `drawculllatePipeline` to `m_drawculllatePipeline`
- [x] Rename `taskcullPipeline` to `m_taskcullPipeline`
- [x] Rename `taskculllatePipeline` to `m_taskculllatePipeline`
- [x] Rename `tasksubmitPipeline` to `m_tasksubmitPipeline`
- [x] Rename `clustersubmitPipeline` to `m_clustersubmitPipeline`
- [x] Rename `clustercullPipeline` to `m_clustercullPipeline`
- [x] Rename `clusterculllatePipeline` to `m_clusterculllatePipeline`
- [x] Rename `depthreducePipeline` to `m_depthreducePipeline`
- [x] Rename `meshPipeline` to `m_meshPipeline`
- [x] Rename `meshpostPipeline` to `m_meshpostPipeline`
- [x] Rename `meshtaskPipeline` to `m_meshtaskPipeline`
- [x] Rename `meshtasklatePipeline` to `m_meshtasklatePipeline`
- [x] Rename `meshtaskpostPipeline` to `m_meshtaskpostPipeline`
- [x] Rename `clusterPipeline` to `m_clusterPipeline`
- [x] Rename `clusterpostPipeline` to `m_clusterpostPipeline`
- [x] Rename `blitPipeline` to `m_blitPipeline`
- [x] Rename `finalPipeline` to `m_finalPipeline`
- [x] Rename `shadowlqPipeline` to `m_shadowlqPipeline`
- [x] Rename `shadowhqPipeline` to `m_shadowhqPipeline`
- [x] Rename `shadowfillPipeline` to `m_shadowfillPipeline`
- [x] Rename `shadowblurPipeline` to `m_shadowblurPipeline`
- [x] Rename `pipelines` to `m_pipelines`
- [x] Rename `pipelineCache` to `m_pipelineCache`

### Renderer.h - Programs Struct
- [x] Rename `debugtextProgram` to `m_debugtextProgram`
- [x] Rename `drawcullProgram` to `m_drawcullProgram`
- [x] Rename `tasksubmitProgram` to `m_tasksubmitProgram`
- [x] Rename `clustersubmitProgram` to `m_clustersubmitProgram`
- [x] Rename `clustercullProgram` to `m_clustercullProgram`
- [x] Rename `depthreduceProgram` to `m_depthreduceProgram`
- [x] Rename `meshProgram` to `m_meshProgram`
- [x] Rename `meshtaskProgram` to `m_meshtaskProgram`
- [x] Rename `clusterProgram` to `m_clusterProgram`
- [x] Rename `finalProgram` to `m_finalProgram`
- [x] Rename `shadowProgram` to `m_shadowProgram`
- [x] Rename `shadowfillProgram` to `m_shadowfillProgram`
- [x] Rename `shadowblurProgram` to `m_shadowblurProgram`

### Renderer.h - Samplers Struct
- [x] Rename `textureSampler` to `m_textureSampler`
- [x] Rename `readSampler` to `m_readSampler`
- [x] Rename `depthSampler` to `m_depthSampler`

### Renderer.h - Buffers Struct
- [x] Rename `scratch` to `m_scratch`
- [x] Rename `meshesh` to `m_meshesh`
- [x] Rename `materials` to `m_materials`
- [x] Rename `vertices` to `m_vertices`
- [x] Rename `indices` to `m_indices`
- [x] Rename `meshlets` to `m_meshlets`
- [x] Rename `meshletdata` to `m_meshletdata`
- [x] Rename `draw` to `m_draw`
- [x] Rename `DrawVisibility` to `m_drawVisibility` (also fix capitalization)
- [x] Rename `TaskCommands` to `m_taskCommands` (also fix capitalization)
- [x] Rename `CommandCount` to `m_commandCount` (also fix capitalization)
- [x] Rename `MeshletVisibility` to `m_meshletVisibility` (also fix capitalization)
- [x] Rename `blasBuffer` to `m_blasBuffer`
- [x] Rename `tlasBuffer` to `m_tlasBuffer`
- [x] Rename `tlasScratchBuffer` to `m_tlasScratchBuffer`
- [x] Rename `tlasInstanceBuffer` to `m_tlasInstanceBuffer`
- [x] Rename `tlas` to `m_tlas`
- [x] Rename `DrawVisibilityCleared` to `m_drawVisibilityCleared` (also fix capitalization)
- [x] Rename `MeshletVisibilityCleared` to `m_meshletVisibilityCleared` (also fix capitalization)
- [x] Rename `meshletVisibilityBytes` to `m_meshletVisibilityBytes`

### Renderer.h - Timestamps Struct
- [x] Rename `frameTimestamp` to `m_frameTimestamp`
- [x] Rename `frameCpuBegin` to `m_frameCpuBegin`
- [x] Rename `frameGpuBegin` to `m_frameGpuBegin`
- [x] Rename `frameGpuEnd` to `m_frameGpuEnd`

## Constants and Macro Naming (UPPER_CASE)

- [x] Rename `gbufferCount` to `GBUFFER_COUNT` in Renderer.h (line 9)
- [ ] Check codebase for any other non-uppercased constants/macros

## Variable and Function Naming (camelCase)

### thread_name.hpp
- [x] Rename `thread_name` to `threadName` (line 13)
- [x] Rename `thread_id` to `threadId` (line 14)
- [x] Rename `hook_init_ex_cpu_thread_name` to `hookInitExCpuThreadName` (line 18)
- [x] Rename `hook_init_ex_cpu_thread_id` to `hookInitExCpuThreadId` (line 25)
- [x] Rename `get_thread_name` to `getThreadName` (line 34)
- [x] Rename `get_thread_id` to `getThreadId` (line 45)
- [x] Rename `print_thread_name` to `printThreadName` (line 49)

### niagara/common.h
- [ ] Rename `countof_helper` to `countofHelper` (line 44)

### Update Related References
- [x] Update all references to the renamed functions and variables in main.cpp
- [x] Update all references to the renamed functions and variables in Renderer.cpp
- [ ] Check all other files for references to renamed identifiers

## Indentation Consistency (4 spaces, no tabs)

### Files to check and fix:
- [ ] GfxDevice.h (line 14 has inconsistent indentation)
- [ ] Renderer.h (check all indentation)
- [ ] main.cpp (check all indentation)
- [ ] Renderer.cpp (check all indentation)
- [x] thread_name.hpp (check all indentation)
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

- [x] Verify all function declarations have docstring comments
- [x] Add missing docstring comments to functions in Renderer.h and GfxDevice.h

## Line Length (under 120 characters)

- [ ] Check all files for lines exceeding 120 characters

## General Organization

- [ ] Review all files for additional style inconsistencies
- [ ] Ensure code follows established patterns in the project