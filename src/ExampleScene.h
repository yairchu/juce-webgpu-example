#pragma once

#include <webgpu/webgpu-raii.hpp>

struct WebGPUContext;

class ExampleScene
{
public:
    bool initialize (WebGPUContext& context);
    void render (WebGPUContext& context, wgpu::raii::TextureView& renderTarget);
    void shutdown();

private:
    bool createVertexBuffer (WebGPUContext& context);
    bool createPipeline (WebGPUContext& context);

    wgpu::raii::ShaderModule vertexShader;
    wgpu::raii::ShaderModule fragmentShader;
    wgpu::raii::Buffer vertexBuffer;
    wgpu::raii::RenderPipeline renderPipeline;
};
