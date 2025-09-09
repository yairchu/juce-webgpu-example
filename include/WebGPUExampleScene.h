#pragma once

#include <webgpu/webgpu-raii.hpp>

struct WebGPUContext;
struct WebGPUTexture;

// An example scene that draws a colored triangle.
// Can be used for validating that the webgpu setup works.

class WebGPUExampleScene
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
