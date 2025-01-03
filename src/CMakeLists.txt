# add_subdirectory(plugins)

ADD_EXECUTABLE(${PROJECT_NAME} 

Engine.cpp
main.cpp

#Math
Math/IndexRange2.cpp
Math/Transform.cpp
Math/Util.cpp
# Math/
# Math/
# Renderer
Renderer/Vulkan/VolkImpl.cpp
Renderer/Vulkan/BindlessSetManager.cpp
Renderer/Vulkan/Descriptors.cpp
Renderer/Vulkan/Init.cpp
Renderer/Vulkan/Pipelines.cpp
Renderer/Vulkan/Swapchain.cpp
Renderer/Vulkan/Util.cpp
Renderer/Vulkan/VmaImpl.cpp
Renderer/Vulkan/VulkanImmediateExecutor.cpp
Renderer/ImageLoader.cpp
Renderer/Color.cpp
Renderer/GfxDevice.cpp
Renderer/ImageCache.cpp
Renderer/MeshCache.cpp
Renderer/MaterialCache.cpp
Renderer/MipMapGeneration.cpp
Renderer/NBuffer.cpp
Renderer/Scene.cpp

Utils/GltfLoader.cpp
)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

target_link_libraries(${PROJECT_NAME} 
  PUBLIC 
    # volk_headers
    nlohmann_json::nlohmann_json
    volk
    libfork::libfork 
    SDL3::SDL3
    glm::glm
    Tracy::TracyClient
    vk-bootstrap::vk-bootstrap
  PRIVATE
    tinygltf::tinygltf
    stb::image
)

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    VK_NO_PROTOTYPES
    VMA_VULKAN_VERSION=1003000
)


if (MSVC)
  target_compile_definitions(TracyClient PUBLIC
    $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_ENABLE>
    $<$<CONFIG:Debug,RelWithDebInfo>:TRACY_DELAYED_INIT> # for unit tests to work
  )
else()
  if(NOT CMAKE_BUILD_TYPE OR
    CMAKE_BUILD_TYPE STREQUAL "Debug" OR
    CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(TracyClient PUBLIC
        TRACY_ENABLE
        TRACY_DELAYED_INIT # needed for unit tests to work
    )
  endif()
endif()

symlink_assets(${PROJECT_NAME})