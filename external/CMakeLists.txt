find_package(Vulkan REQUIRED)

add_subdirectory(SDL)
# add_subdirectory(libfork)
# add_subdirectory(vk-bootstrap)
add_subdirectory(glm)
add_subdirectory(vma)
# add_subdirectory(tracy)
add_subdirectory(volk)
add_subdirectory(stb)
add_subdirectory(meshoptimizer)
add_subdirectory(ozz-animation)
# add_subdirectory(quill) # logging library

## json
option(JSON_MultipleHeaders "Use non-amalgamated version of the library." ON)
option(JSON_Install         "Install CMake targets during install step." OFF)
add_subdirectory(json)

## tinygltf
option(TINYGLTF_BUILD_LOADER_EXAMPLE "Build loader_example(load glTF and dump infos)" OFF)
option(TINYGLTF_INSTALL "Install tinygltf files during install step." OFF)
option(TINYGLTF_HEADER_ONLY "On: header-only mode. Off: create tinygltf library(No TINYGLTF_IMPLEMENTATION required in your project)" ON)
add_subdirectory(tinygltf)
add_library(tinygltf::tinygltf ALIAS tinygltf)
