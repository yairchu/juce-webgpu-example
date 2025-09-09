#pragma once

#include <webgpu/webgpu-raii.hpp>

struct WebGPUContext;
struct WebGPUTexture;

class ExampleScene
{
public:
    bool initialize (WebGPUContext& context);
    void render (WebGPUContext& context, WebGPUTexture& renderTarget);
    void shutdown();

private:
    bool createVertexBuffer (WebGPUContext& context);
    bool createPipeline (WebGPUContext& context);

    wgpu::raii::ShaderModule vertexShader;
    wgpu::raii::ShaderModule fragmentShader;
    wgpu::raii::Buffer vertexBuffer;
    wgpu::raii::RenderPipeline renderPipeline;
};
