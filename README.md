# JUCE with WebGPU utilities library and example

This is both a reusable library and example application using CMake + JUCE + [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution) to demo using its WebGPU RAII wrappers in a JUCE app.

Currently the image is rendered and copied to CPU memory to construct a `juce::Image` to render,
but I intend to skip this step and keep rendering fully in GPU memory, probably requiring JUCE's OpenGL integration and platform specific calls to use the webgpu texture in OpenGL.

## Building the Example

Set up with CMake and build/run using your preferred backend, for example for using Xcode:

```bash
cmake -B build -G Xcode
open build/JuceWebGPU.xcodeproj
```

## Usage as Library Dependency

When using this as a CPM dependency, ensure your project provides the required dependencies first:

- **JUCE** (tested with 7.0.9+) with targets: `juce::juce_gui_basics`, `juce::juce_core`, `juce::juce_graphics`
- **WebGPU-distribution** with target: `webgpu`

```cmake
# Your project's CMakeLists.txt

# Add dependencies first
CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.3  # Choose your version
)

CPMAddPackage(
    NAME webgpu_dist
    GITHUB_REPOSITORY eliemichel/WebGPU-distribution
    GIT_TAG 17dcd42a7683355e7a40ac4e97e77f36dff5b5ab  # Choose your version
)
set(WEBGPU_BACKEND WGPU CACHE STRING "WebGPU backend")  # Choose wgpu or dawn backend

# Then add this library
CPMAddPackage(
    NAME juce-webgpu
    GITHUB_REPOSITORY yairchu/juce-webgpu
    GIT_TAG main
)

# Use in your target, with some sources embedded to avoid JUCE-CMake interaction problems
target_link_libraries(your_target PRIVATE juce-webgpu)
target_sources(your_target PRIVATE ${JUCE_WEBGPU_SOURCES})
```

The example application will only be built when this project is the main project being built, not when used as a dependency.
