#pragma once

#include "WebGPUGraphics.h"
#include <juce_opengl/juce_opengl.h>

// Platform-specific headers for direct GPU-to-GPU copy
#ifdef __APPLE__
    #include <Metal/Metal.h>
    #include <IOSurface/IOSurface.h>
    #include <OpenGL/CGLIOSurface.h>
    #include <webgpu/webgpu.h>
#endif

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
    
#ifdef __APPLE__
    // Direct GPU-to-GPU copy on macOS using Metal-OpenGL interop
    bool updateOpenGLTextureDirect();
    
    // Create IOSurface-backed Metal texture for sharing
    bool createSharedIOSurfaceTexture(int width, int height);
    
    // Update WebGPU to render into our shared IOSurface texture
    bool configureWebGPUForSharedTexture();
#endif
    
    // Fallback CPU path (current implementation)
    void updateOpenGLTextureCPU();

    // Render the OpenGL texture as a quad
    void renderTextureQuad();

    std::shared_ptr<WebGPUGraphics> webgpuGraphics;
    std::unique_ptr<juce::OpenGLTexture> openglTexture;
    std::unique_ptr<juce::OpenGLShaderProgram> shaderProgram;

    // Simple quad vertices for texture rendering
    GLuint vertexBuffer = 0;
    GLuint indexBuffer = 0;

    // Shader attribute locations
    GLint positionAttribLocation = -1;
    GLint texCoordAttribLocation = -1;

#ifdef __APPLE__
    // macOS-specific resources for direct GPU copy
    IOSurfaceRef ioSurface = nullptr;
    GLuint ioSurfaceTextureId = 0;
    id<MTLTexture> sharedMetalTexture = nil;
    id<MTLDevice> metalDevice = nil;
    bool directCopySupported = false;
    
    // Shared texture dimensions for IOSurface
    int sharedTextureWidth = 0;
    int sharedTextureHeight = 0;
#endif

    bool isInitialized = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenGLWebGPUComponent)
};