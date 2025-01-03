cmake_minimum_required(VERSION 3.14)

SET(PROJECT_NAME GameEngine)

project(
    ${PROJECT_NAME}
#   VERSION ${version_major}.${version_minor}.${version_patch}
  DESCRIPTION
    "A bleeding-edge game engine built on C++20's coroutines."
  HOMEPAGE_URL "https://github.com/dsvua/gameengine"
  LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AssetManagement)

add_subdirectory(external)
add_subdirectory(src)
