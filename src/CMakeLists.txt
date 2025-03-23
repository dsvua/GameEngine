file(GLOB_RECURSE CPP_SOURCE_FILES "*.h" "*.cpp")
file(GLOB_RECURSE GLSL_SOURCE_FILES "Renderer/shaders/*.glsl")
file(GLOB_RECURSE GLSL_HEADER_FILES "Renderer/shaders/*.h" "Renderer/niagara/config.h")

# SET(PROJECT_NAME TMCTest)

ADD_EXECUTABLE(${PROJECT_NAME} 
  ${CPP_SOURCE_FILES}
  ${GLSL_SOURCE_FILES}
  ${GLSL_HEADER_FILES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/external/TooManyCooks/include ${PROJECT_SOURCE_DIR}/external/cgltf)

target_link_libraries(${PROJECT_NAME} 
  PUBLIC 
    # volk_headers
    nlohmann_json::nlohmann_json
    volk
    # libfork::libfork 
    SDL3::SDL3
    glm::glm
    # Tracy::TracyClient
    # vk-bootstrap::vk-bootstrap
  PRIVATE
    tinygltf::tinygltf
    stb::image
    meshoptimizer
)

if(APPLE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_METAL_EXT)
  set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_METAL_EXT)
elseif(UNIX)
  if(USE_WAYLAND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WAYLAND_KHR)
  else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_XLIB_KHR)
    set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_XLIB_KHR)
  endif()
elseif(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR GLFW_EXPOSE_NATIVE_WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  set(VOLK_STATIC_DEFINES "VK_USE_PLATFORM_WIN32_KHR")
endif()

if(UNIX)
  if(DEFINED ENV{VULKAN_SDK})
    set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/bin/glslangValidator")
  else()
    set(GLSL_VALIDATOR "/usr/bin/glslangValidator")
  endif()
elseif(${CMAKE_HOST_SYSTEM_PROCESSOR} STREQUAL "AMD64")
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin/glslangValidator.exe")
else()
  set(GLSL_VALIDATOR "$ENV{VULKAN_SDK}/Bin32/glslangValidator.exe")
endif()

# Thanks to: https://gist.github.com/evilactually/a0d191701cb48f157b05be7f74d79396
foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(STEM ${GLSL} NAME_WLE)
  set(SPIRV "${SHADERS_OUTPUT_DIR}/${STEM}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_OUTPUT_DIR}"
    COMMAND ${GLSL_VALIDATOR} -V --target-env vulkan1.3 ${GLSL} -o ${SPIRV}
    # COMMAND glslc ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL} ${GLSL_HEADER_FILES})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(compile_shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(${PROJECT_NAME} compile_shaders)

