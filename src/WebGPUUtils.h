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
    struct MemLayout
    {
        uint32_t width, height, bytesPerPixel; // inputs
        uint32_t bytesPerRow, bufferSize; // calculated

        void calcParams();
    };

    wgpu::raii::Texture texture;
    wgpu::raii::TextureView view;

    bool init (WebGPUContext&, const WGPUTextureDescriptor&);
    wgpu::raii::Buffer read (WebGPUContext&, const MemLayout&);
};
