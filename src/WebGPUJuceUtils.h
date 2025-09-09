#pragma once

struct WebGPUContext;
struct WebGPUTexture;
namespace juce
{
class Image;
}

struct WebGPUJuceUtils
{
    // Read back RGBA texture data into a JUCE Image.
    // Image and texture sizes must match!
    static void readTextureToImage (WebGPUContext&, WebGPUTexture&, juce::Image&);
};
