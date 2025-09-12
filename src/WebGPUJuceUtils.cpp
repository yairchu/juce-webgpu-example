#include "WebGPUJuceUtils.h"

#include "WebGPUUtils.h"
#include <juce_graphics/juce_graphics.h>

void WebGPUJuceUtils::readTextureToImage (WebGPUContext& context, WebGPUTexture& texture, juce::Image& image)
{
    wgpu::raii::Buffer readbackBuffer = texture.read (context);

    // Copy pixel data (WebGPU uses RGBA, JUCE uses ARGB)
    const int bytesPerRow = texture.bytesPerRow();
    const auto src = (uint8_t*) readbackBuffer->getConstMappedRange (0, bytesPerRow * texture.descriptor.size.height);
    juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < (int) texture.descriptor.size.height; ++y)
        for (int x = 0; x < (int) texture.descriptor.size.width; ++x)
        {
            const int srcIndex = y * bytesPerRow + x * 4;
            bitmap.setPixelColour (x, y, juce::Colour::fromRGBA (src[srcIndex + 0], src[srcIndex + 1], src[srcIndex + 2], src[srcIndex + 3]));
        }

    readbackBuffer->unmap();
}
