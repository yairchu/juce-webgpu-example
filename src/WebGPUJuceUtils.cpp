#include "WebGPUJuceUtils.h"

#include "WebGPUUtils.h"
#include <juce_graphics/juce_graphics.h>

void WebGPUJuceUtils::readTextureToImage (WebGPUContext& context, WebGPUTexture& texture, juce::Image& image)
{
    WebGPUTexture::MemLayout textureLayout { .width = (uint32_t) image.getWidth(), .height = (uint32_t) image.getHeight(), .bytesPerPixel = 4 };
    textureLayout.calcParams();

    wgpu::raii::Buffer readbackBuffer = texture.read (context, textureLayout);

    // Copy pixel data (WebGPU uses RGBA, JUCE uses ARGB)
    const auto src = (uint8_t*) readbackBuffer->getConstMappedRange (0, textureLayout.bufferSize);
    juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < (int) textureLayout.height; ++y)
        for (int x = 0; x < (int) textureLayout.width; ++x)
        {
            const int srcIndex = y * (int) textureLayout.bytesPerRow + x * 4;
            bitmap.setPixelColour (x, y, juce::Colour::fromRGBA (src[srcIndex + 0], src[srcIndex + 1], src[srcIndex + 2], src[srcIndex + 3]));
        }

    readbackBuffer->unmap();
}
