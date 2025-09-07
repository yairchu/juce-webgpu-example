cmake_minimum_required(VERSION 3.20)
project(MyWebGPUCompute C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download CPM.cmake
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.38.3/CPM.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
  EXPECTED_HASH SHA256=cc155ce02e7945e7b8967ddfaff0b050e958a723ef7aad3766d368940cb15494
)
include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)

# Pull WebGPU-distribution using CPM
CPMAddPackage(
  NAME webgpu_dist
  GITHUB_REPOSITORY eliemichel/WebGPU-distribution
  GIT_TAG 17dcd42a7683355e7a40ac4e97e77f36dff5b5ab
)

# Force backend to wgpu-native (WGPU)
set(WEBGPU_BACKEND WGPU CACHE STRING "WebGPU backend")

add_executable(my_compute src/main.cpp)

# Link the 'webgpu' target provided by WebGPU-distribution
target_link_libraries(my_compute PRIVATE webgpu)

if (MSVC)
  target_compile_options(my_compute PRIVATE /Zc:__cplusplus)
endif()

add_custom_command(TARGET my_compute POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/shaders
          $<TARGET_FILE_DIR:my_compute>/shaders)
