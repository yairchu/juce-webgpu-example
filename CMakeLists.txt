cmake_minimum_required(VERSION 3.20)
project(JuceWebGPUExample C CXX)

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

# Pull JUCE using CPM
CPMAddPackage(
  NAME JUCE
  GITHUB_REPOSITORY juce-framework/JUCE
  GIT_TAG 8.0.3
)

# Pull WebGPU-distribution using CPM
CPMAddPackage(
  NAME webgpu_dist
  GITHUB_REPOSITORY eliemichel/WebGPU-distribution
  GIT_TAG 17dcd42a7683355e7a40ac4e97e77f36dff5b5ab
)

# Force backend to wgpu-native (WGPU)
set(WEBGPU_BACKEND WGPU CACHE STRING "WebGPU backend")

# Create JUCE GUI application
juce_add_gui_app(JuceWebGPUExample
    PRODUCT_NAME "JUCE WebGPU Example"
    COMPANY_NAME "Your Company"
    VERSION "1.0.0"
    BUNDLE_ID "com.yourcompany.jucewebgpuexample"
)

# Generate config header
juce_generate_juce_header(JuceWebGPUExample)

# Add source files
target_sources(JuceWebGPUExample
    PRIVATE
        src/main.cpp
        src/MainComponent.h
        src/MainComponent.cpp
        src/WebGPUCompute.h
        src/WebGPUCompute.cpp
)

# Link JUCE modules
target_link_libraries(JuceWebGPUExample
    PRIVATE
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_core
        juce::juce_events
        juce::juce_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Link WebGPU
target_link_libraries(JuceWebGPUExample PRIVATE webgpu)

# Compiler-specific options
if (MSVC)
    target_compile_options(JuceWebGPUExample PRIVATE /Zc:__cplusplus)
endif()

# Copy shaders to build directory
add_custom_command(TARGET JuceWebGPUExample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:JuceWebGPUExample>/shaders)

# Set binary data directory for JUCE
juce_add_binary_data(SharedBinaryData
    SOURCES
        shaders/comp.wgsl
)
