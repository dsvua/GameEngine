# add_subdirectory(plugins)

file(GLOB_RECURSE GLSL_SOURCE_FILES "shaders/*.glsl")
file(GLOB_RECURSE GLSL_HEADER_FILES "shaders/*.h" "Config.hpp")
file(GLOB_RECURSE CPP_SOURCE_FILES "*.hpp" "*.cpp")

ADD_EXECUTABLE(${PROJECT_NAME} 
  ${CPP_SOURCE_FILES}
  ${GLSL_SOURCE_FILES}
  ${GLSL_HEADER_FILES}
)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

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
    meshoptimizer
)

target_compile_definitions(${PROJECT_NAME}
  PUBLIC
    VK_NO_PROTOTYPES
    VMA_VULKAN_VERSION=1003000
)

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/external/cgltf)

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
# symlink_shaders(${PROJECT_NAME})

if(UNIX)
  if(DEFINED ENV{VULKAN_SDK})
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")
  else()
    set(GLSL_VALIDATOR "glslangValidator")
  endif()
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

# Thanks to: https://gist.github.com/evilactually/a0d191701cb48f157b05be7f74d79396
set(SPIRV_OUTPUT_DIR "${PROJECT_BINARY_DIR}/spirv/")
foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(STEM ${GLSL} NAME_WLE)
  set(SPIRV "${SPIRV_OUTPUT_DIR}${STEM}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_OUTPUT_DIR}"
    COMMAND ${GLSL_VALIDATOR} -V --target-env vulkan1.3 --quiet ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL} ${GLSL_HEADER_FILES})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

add_custom_target(compile_shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} compile_shaders)

