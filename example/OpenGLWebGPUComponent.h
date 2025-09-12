#pragma once

#include <juce_opengl/juce_opengl.h>
#include "WebGPUGraphics.h"

/**
 * OpenGL-based component that displays WebGPU content without CPU memory roundtrip.
 * 
 * This component uses JUCE's OpenGL integration to render WebGPU textures directly
 * on the GPU, avoiding the costly CPU memory copy that the traditional approach requires.
 */
class OpenGLWebGPUComponent : public juce::OpenGLAppComponent
{
public:
    OpenGLWebGPUComponent();
    ~OpenGLWebGPUComponent() override;

    // Initialize with WebGPU graphics instance
    void setWebGPUGraphics (std::shared_ptr<WebGPUGraphics> graphics);

    // OpenGLAppComponent interface
    void initialise() override;
    void shutdown() override;
    void render() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Transfer WebGPU texture data to OpenGL texture
    void updateOpenGLTexture();
    
    // Render the OpenGL texture as a quad
    void renderTextureQuad();

    std::shared_ptr<WebGPUGraphics> webgpuGraphics;
    std::unique_ptr<juce::OpenGLTexture> openglTexture;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;
    
    // Simple quad vertices for texture rendering
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;
    
    bool isInitialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGLWebGPUComponent)
};