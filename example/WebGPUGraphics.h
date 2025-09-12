#pragma once

#include <atomic>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <mutex>
#include <webgpu/webgpu-raii.hpp>

#include "WebGPUExampleScene.h"
#include "WebGPUUtils.h"

class WebGPUGraphics
{
public:
    bool initialize (int width, int height);
    void shutdown();
    void resize (int width, int height);

    void renderFrame();

    // Get the WebGPU texture for sharing with OpenGL
    WGPUTexture getSharedTexture() const { return *texture.texture; }

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

    // Get access to WebGPU resources for OpenGL integration
    WebGPUContext& getContext() { return context; }
    WebGPUTexture& getTexture() { return texture; }

private:
    bool createTexture (int width, int height);

    std::atomic<bool> initialized { false };
    std::atomic<bool> shutdownRequested { false };
    int textureWidth = 0;
    int textureHeight = 0;
    mutable std::mutex textureMutex; // Protects texture dimensions and resources

    WebGPUContext context;
    WebGPUExampleScene scene;
    WebGPUTexture texture;

    static constexpr uint32_t bytesPerPixel = 4; // RGBA8
};
