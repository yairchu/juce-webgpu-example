# JUCE WebGPU Example

This is a vibe-coded application using CMake + JUCE + [WebGPU-distribution](https://github.com/eliemichel/WebGPU-distribution) to demo using WebGPU's RAII wrappers in a JUCE app.

Currently the image is rendered and copied to CPU memory to construct a `juce::Image` to render,
but I intend to skip this step and keep rendering fully in GPU memory, probably requiring JUCE's OpenGL integration and platform specific calls to use the webgpu texture in OpenGL.
