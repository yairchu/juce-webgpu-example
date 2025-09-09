#include "WebGPUGraphics.h"
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

    // Double-check after acquiring lock
    if (! initialized.load() || shutdownRequested.load())
        return;

    scene.render (context, texture);
}

juce::Image WebGPUGraphics::renderFrameToImage()
{
    if (! initialized.load() || shutdownRequested.load())
        return {};

    // First render to GPU texture
    renderFrame();

    std::lock_guard<std::mutex> lock (textureMutex);

    // Double-check after acquiring lock
    if (! initialized.load() || shutdownRequested.load())
        return {};

    // Read back the texture data directly from render texture
    // WebGPU requires bytes per row to be aligned to 256 bytes
    WebGPUTexture::MemLayout textureLayout { .width = (uint32_t) textureWidth, .height = (uint32_t) textureHeight, .bytesPerPixel = bytesPerPixel };
    textureLayout.calcParams();

    wgpu::raii::Buffer readbackBuffer = texture.read (context, textureLayout);

    const void* ptr = readbackBuffer->getConstMappedRange (0, textureLayout.bufferSize);

    // Create JUCE image from the data
    juce::Image image (juce::Image::ARGB, textureWidth, textureHeight, true);
    juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);

    // Copy pixel data (WebGPU uses RGBA, JUCE uses ARGB)
    // Account for potential row padding due to alignment
    const uint8_t* src = static_cast<const uint8_t*> (ptr);
    for (int y = 0; y < textureHeight; ++y)
        for (int x = 0; x < textureWidth; ++x)
        {
            // Use aligned bytes per row for source indexing
            const int srcIndex = y * (int) textureLayout.bytesPerRow + x * 4;
            bitmap.setPixelColour (x, y, juce::Colour::fromRGBA (src[srcIndex + 0], src[srcIndex + 1], src[srcIndex + 2], src[srcIndex + 3]));
        }

    readbackBuffer->unmap();

    return image;
}

void WebGPUGraphics::shutdown()
{
    // Signal shutdown to all operations first
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
