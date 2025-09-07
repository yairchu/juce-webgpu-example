cmake_minimum_required(VERSION 3.20)
project(MyWebGPUCompute C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Pull WebGPU-distribution (pin to a tag/commit you trust)
FetchContent_Declare(
  webgpu_dist
  GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution.git
  GIT_TAG        v0.2.0
)
FetchContent_MakeAvailable(webgpu_dist)

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
