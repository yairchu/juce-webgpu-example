cmake_minimum_required(VERSION 3.20)
project(JuceWebGPU VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(juce-webgpu
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WebGPUUtils.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WebGPUExampleScene.cpp"
)
target_include_directories(juce-webgpu
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(juce-webgpu
    PUBLIC
        webgpu
)

set(WEBGPU_LINK_TYPE STATIC CACHE INTERNAL "WebGPU library linking can be SHARED or STATIC")

# Modules that depend on JUCE cannot be compiled as part of the library.
# Instead we export a list of source files that can be added to the user's target.
set(JUCE_WEBGPU_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/WebGPUJuceUtils.cpp"
    CACHE INTERNAL "JUCE WebGPU source files for embedding in dependent projects"
)

if(PROJECT_IS_TOP_LEVEL)
    target_sources(juce-webgpu PRIVATE ${JUCE_WEBGPU_SOURCES})

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
      GITHUB_REPOSITORY talaviram/WebGPU-distribution
      GIT_TAG d20157b3b0fa87c3978e8e201414ea331c7fe0ea
      OPTIONS "WEBGPU_LINK_TYPE ${WEBGPU_LINK_TYPE}"
    )

    # Force backend to wgpu-native (WGPU)
    set(WEBGPU_BACKEND WGPU CACHE STRING "WebGPU backend")

    # Declare what we need - let the parent project provide it
    target_link_libraries(juce-webgpu
        PUBLIC
            juce::juce_gui_basics
            juce::juce_core
            juce::juce_graphics
    )

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
            example/OpenGLWebGPUComponent.h
            example/OpenGLWebGPUComponent.cpp
    )

    # Link JUCE modules and our library
    target_link_libraries(JuceWebGPUExample
        PRIVATE
            juce-webgpu
            juce::juce_events
            juce::juce_opengl
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
    
    # macOS-specific frameworks for Metal-OpenGL interop
    if(APPLE)
        target_link_libraries(JuceWebGPUExample PRIVATE
            "-framework Metal"
            "-framework IOSurface"
            "-framework OpenGL"
        )
    endif()
    
    # Disable web browser and curl to avoid extra dependencies
    target_compile_definitions(JuceWebGPUExample PRIVATE 
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )

    # Compiler-specific options
    if (MSVC)
        target_compile_options(JuceWebGPUExample PRIVATE /Zc:__cplusplus)
    endif()
endif()
