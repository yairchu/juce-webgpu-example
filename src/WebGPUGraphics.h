#pragma once

#include <atomic>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <mutex>
#include <webgpu/webgpu-raii.hpp>

class WebGPUGraphics
{
public:
    WebGPUGraphics();
    ~WebGPUGraphics();

    bool initialize (int width, int height);
    void shutdown();
    void resize (int width, int height);

    void renderFrame();

    // Get the WebGPU texture for sharing with OpenGL
    WGPUTexture getSharedTexture() const { return *renderTexture; }

    // Legacy method for CPU readback (renamed from renderFrame to avoid confusion)
    juce::Image renderFrameToImage();

    bool isInitialized() const { return initialized; }
    int getTextureWidth() const
    {
        std::lock_guard<std::mutex> lock (textureMutex);
        return textureWidth;
    }
    int getTextureHeight() const
    {
        std::lock_guard<std::mutex> lock (textureMutex);
        return textureHeight;
    }

private:
    bool createTexture (int width, int height);
    bool createPipeline();
    bool createVertexBuffer();

    std::atomic<bool> initialized { false };
    std::atomic<bool> shutdownRequested { false };
    int textureWidth = 0;
    int textureHeight = 0;
    mutable std::mutex textureMutex; // Protects texture dimensions and resources

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
