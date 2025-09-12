#include "WebGPUJuceUtils.h"

#include "WebGPUUtils.h"
#include <juce_graphics/juce_graphics.h>

void WebGPUJuceUtils::readTextureToImage (WebGPUContext& context, WebGPUTexture& texture, juce::Image& image)
{
    jassert (texture.descriptor.size.width == (uint32_t) image.getWidth());
    jassert (texture.descriptor.size.height == (uint32_t) image.getHeight());

    wgpu::raii::Buffer readbackBuffer = texture.read (context);

    // Copy pixel data (WebGPU uses RGBA, JUCE uses ARGB)
    const int bytesPerRow = texture.bytesPerRow();
    const auto src = (uint8_t*) readbackBuffer->getConstMappedRange (0, bytesPerRow * texture.descriptor.size.height);
    juce::Image::BitmapData bitmap (image, juce::Image::BitmapData::writeOnly);

    for (int y = 0; y < (int) texture.descriptor.size.height; ++y)
        std::memcpy (bitmap.getLinePointer (y), src + y * bytesPerRow, (size_t) texture.descriptor.size.width * 4);

    readbackBuffer->unmap();
}
