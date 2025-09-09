#include "WebGPUGraphics.h"
#include "WebGPUJuceUtils.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <thread>

bool WebGPUGraphics::initialize (int width, int height)
{
    if (initialized)
        return true;

    textureWidth = width;
    textureHeight = height;

    if (! context.init())
        return false;

    if (! scene.initialize (context))
        return false;

    if (! createTexture (width, height))
        return false;

    initialized = true;
    juce::Logger::writeToLog ("WebGPU graphics initialized successfully");
    return true;
}

bool WebGPUGraphics::createTexture (int width, int height)
{
    return texture.init (context, {
                                      .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
                                      .dimension = WGPUTextureDimension_2D,
                                      .size = { static_cast<uint32_t> (width), static_cast<uint32_t> (height), 1 },
                                      .format = WGPUTextureFormat_RGBA8Unorm,
                                      .mipLevelCount = 1,
                                      .sampleCount = 1,
                                  });
}

void WebGPUGraphics::resize (int width, int height)
{
    std::lock_guard<std::mutex> lock (textureMutex);

    if (! initialized || (width == textureWidth && height == textureHeight))
        return;

    textureWidth = width;
    textureHeight = height;

    // Recreate textures with new size
    createTexture (width, height);
}

void WebGPUGraphics::renderFrame()
{
    std::lock_guard<std::mutex> lock (textureMutex);

    if (! initialized.load() || shutdownRequested.load())
        return;

    scene.render (context, texture);
}

juce::Image WebGPUGraphics::renderFrameToImage()
{
    if (! initialized.load() || shutdownRequested.load())
        return {};

    renderFrame();

    std::lock_guard<std::mutex> lock (textureMutex);
    if (! initialized.load() || shutdownRequested.load())
        return {};

    juce::Image image (juce::Image::ARGB, textureWidth, textureHeight, true);
    WebGPUJuceUtils::readTextureToImage (context, texture, image);

    return image;
}

void WebGPUGraphics::shutdown()
{
    shutdownRequested.store (true);

    // Acquire lock to ensure no rendering operations are in progress
    std::lock_guard<std::mutex> lock (textureMutex);

    if (! initialized.load())
        return;

    juce::Logger::writeToLog ("WebGPU shutdown starting...");

    // Process events to ensure completion
    for (int i = 0; i < 100; ++i) // Max 100ms timeout
    {
        context.instance->processEvents();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }
}
