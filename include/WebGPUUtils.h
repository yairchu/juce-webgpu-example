#pragma once

#include <webgpu/webgpu-raii.hpp>

// To use WebGPU you first need to initialize the context.
// It includes objects you will always need.
struct WebGPUContext
{
    wgpu::raii::Instance instance;
    wgpu::raii::Device device;
    wgpu::raii::Queue queue;

    bool init();
    wgpu::raii::ShaderModule loadWgslShader (const char* source, const char* name = nullptr);
};

struct WebGPUTexture
{
    wgpu::raii::Texture texture;
    wgpu::raii::TextureView view;

    // The descriptor contains texture size and format
    WGPUTextureDescriptor descriptor;

    bool init (WebGPUContext&, const WGPUTextureDescriptor&);
    wgpu::raii::Buffer read (WebGPUContext&);
    int bytesPerRow() const;
};

struct WebGPUPassThroughFragmentShader
{
    static const char* wgslSource;
    static const char* entryPoint;
};
