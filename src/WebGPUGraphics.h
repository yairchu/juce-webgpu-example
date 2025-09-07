#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <webgpu/webgpu-raii.hpp>

class WebGPUGraphics
{
public:
    WebGPUGraphics();
    ~WebGPUGraphics();

    bool initialize (int width, int height);
    void shutdown();
    void resize (int width, int height);

    // Render a frame and return the rendered image
    juce::Image renderFrame();

    bool isInitialized() const { return initialized; }

private:
    bool createTexture (int width, int height);
    bool createPipeline();
    bool createVertexBuffer();

    bool initialized = false;
    int textureWidth = 0;
    int textureHeight = 0;

    wgpu::raii::Instance instance;
    wgpu::raii::Device device;
    wgpu::raii::Queue queue;
    wgpu::raii::ShaderModule vertexShader;
    wgpu::raii::ShaderModule fragmentShader;
    wgpu::raii::Texture renderTexture;
    wgpu::raii::TextureView renderTextureView;
    wgpu::raii::Buffer vertexBuffer;
    wgpu::raii::RenderPipeline renderPipeline;

    static constexpr uint32_t bytesPerPixel = 4; // RGBA8
};
