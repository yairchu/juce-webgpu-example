cmake_minimum_required(VERSION 3.20)
project(JuceWebGPU VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only build example if this is the main project (not a dependency)
option(JUCE_WEBGPU_BUILD_EXAMPLE "Build the example application" ${PROJECT_IS_TOP_LEVEL})

# When used as dependency, require that these targets already exist
# When building standalone, fetch them ourselves
if(PROJECT_IS_TOP_LEVEL AND JUCE_WEBGPU_BUILD_EXAMPLE)
    # Download CPM.cmake
    file(
      DOWNLOAD
      https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.0/CPM.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
      EXPECTED_HASH SHA256=2020b4fc42dba44817983e06342e682ecfc3d2f484a581f11cc5731fbe4dce8a
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
endif()

# Create the library target
add_library(juce-webgpu
    src/WebGPUUtils.cpp
    src/WebGPUJuceUtils.cpp
    src/WebGPUExampleScene.cpp
)

target_include_directories(juce-webgpu
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Declare what we need - let the parent project provide it
target_link_libraries(juce-webgpu
    PUBLIC
        juce::juce_gui_basics
        juce::juce_core
        juce::juce_graphics
        webgpu
)

# Build example only if requested and we're the main project
if(NOT PROJECT_IS_TOP_LEVEL OR NOT JUCE_WEBGPU_BUILD_EXAMPLE)
    return()
endif()

# Example application (only built when this is the main project)
juce_add_gui_app(JuceWebGPUExample
    PRODUCT_NAME "JUCE WebGPU Example"
    COMPANY_NAME "Your Company"
    VERSION "1.0.0"
    BUNDLE_ID "com.yourcompany.jucewebgpuexample"
)

# Generate config header
juce_generate_juce_header(JuceWebGPUExample)

# macOS-specific settings to fix code signing issues
if(APPLE)
    set_target_properties(JuceWebGPUExample PROPERTIES
        XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
        XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO"
        XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
        XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
    )
endif()

# Add source files for the example
target_sources(JuceWebGPUExample
    PRIVATE
        example/main.cpp
        example/MainComponent.h
        example/MainComponent.cpp
        example/WebGPUGraphics.h
        example/WebGPUGraphics.cpp
)

# Link JUCE modules and our library
target_link_libraries(JuceWebGPUExample
    PRIVATE
        juce-webgpu
        juce::juce_gui_extra
        juce::juce_events
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compiler-specific options
if (MSVC)
    target_compile_options(JuceWebGPUExample PRIVATE /Zc:__cplusplus)
endif()
