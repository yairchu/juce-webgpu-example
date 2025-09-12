#include "OpenGLWebGPUComponent.h"
#include "WebGPUJuceUtils.h"

namespace
{
// Simple vertex shader for rendering a textured quad (OpenGL 2.1 compatible)
const char* vertexShader = R"(
    attribute vec2 position;
    attribute vec2 texCoord;
    varying vec2 fragmentTexCoord;
    
    void main()
    {
        gl_Position = vec4(position, 0.0, 1.0);
        fragmentTexCoord = texCoord;
    }
)";

// Simple fragment shader for rendering a texture (OpenGL 2.1 compatible)
const char* fragmentShader = R"(
    varying vec2 fragmentTexCoord;
    uniform sampler2D textureSampler;
    
    void main()
    {
        gl_FragColor = texture2D(textureSampler, fragmentTexCoord);
    }
)";

// Quad vertices: position (x, y) and texture coordinates (u, v)
const float quadVertices[] = {
    // positions   // texCoords
    -1.0f,
    1.0f,
    0.0f,
    1.0f, // top left
    -1.0f,
    -1.0f,
    0.0f,
    0.0f, // bottom left
    1.0f,
    -1.0f,
    1.0f,
    0.0f, // bottom right
    1.0f,
    1.0f,
    1.0f,
    1.0f // top right
};

const unsigned int quadIndices[] = {
    0,
    1,
    2, // first triangle
    0,
    2,
    3 // second triangle
};
} // namespace

OpenGLWebGPUComponent::OpenGLWebGPUComponent()
{
    // OpenGL context will be created when component becomes visible
}

OpenGLWebGPUComponent::~OpenGLWebGPUComponent()
{
    openGLContext.detach();
}

void OpenGLWebGPUComponent::setWebGPUGraphics (std::shared_ptr<WebGPUGraphics> graphics)
{
    webgpuGraphics = graphics;
}

void OpenGLWebGPUComponent::initialise()
{
    // Create shader program
    shaderProgram = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);

    if (shaderProgram->addVertexShader (vertexShader) && shaderProgram->addFragmentShader (fragmentShader) && shaderProgram->link())
    {
        juce::Logger::writeToLog ("OpenGL shaders compiled successfully");

        // Get attribute locations
        positionAttribLocation = openGLContext.extensions.glGetAttribLocation (shaderProgram->getProgramID(), "position");
        texCoordAttribLocation = openGLContext.extensions.glGetAttribLocation (shaderProgram->getProgramID(), "texCoord");

        if (positionAttribLocation < 0 || texCoordAttribLocation < 0)
        {
            juce::Logger::writeToLog ("Failed to get shader attribute locations");
            return;
        }
    }
    else
    {
        juce::Logger::writeToLog ("Failed to compile OpenGL shaders: " + shaderProgram->getLastError());
        return;
    }

    // Create vertex buffer
    openGLContext.extensions.glGenBuffers (1, &vertexBuffer);
    openGLContext.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, vertexBuffer);
    openGLContext.extensions.glBufferData (juce::gl::GL_ARRAY_BUFFER,
                                           sizeof (quadVertices),
                                           quadVertices,
                                           juce::gl::GL_STATIC_DRAW);

    // Create index buffer
    openGLContext.extensions.glGenBuffers (1, &indexBuffer);
    openGLContext.extensions.glBindBuffer (juce::gl::GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    openGLContext.extensions.glBufferData (juce::gl::GL_ELEMENT_ARRAY_BUFFER,
                                           sizeof (quadIndices),
                                           quadIndices,
                                           juce::gl::GL_STATIC_DRAW);

    // Create OpenGL texture
    openglTexture = std::make_unique<juce::OpenGLTexture>();

    isInitialized = true;
    
#ifdef __APPLE__
    // Check if direct GPU copy is supported on macOS
    // For now, we'll enable it by default and handle failures gracefully
    directCopySupported = true;
    juce::Logger::writeToLog ("macOS direct GPU-to-GPU copy enabled");
#endif
    
    juce::Logger::writeToLog ("OpenGL WebGPU component initialized");
}

void OpenGLWebGPUComponent::shutdown()
{
    shaderProgram.reset();
    openglTexture.reset();

    if (vertexBuffer != 0)
    {
        openGLContext.extensions.glDeleteBuffers (1, &vertexBuffer);
        vertexBuffer = 0;
    }

    if (indexBuffer != 0)
    {
        openGLContext.extensions.glDeleteBuffers (1, &indexBuffer);
        indexBuffer = 0;
    }

#ifdef __APPLE__
    // Clean up macOS-specific resources
    if (ioSurfaceTextureId != 0)
    {
        juce::gl::glDeleteTextures(1, &ioSurfaceTextureId);
        ioSurfaceTextureId = 0;
    }
    
    if (ioSurface != nullptr)
    {
        CFRelease(ioSurface);
        ioSurface = nullptr;
    }
    
    directCopySupported = false;
#endif

    isInitialized = false;
    juce::Logger::writeToLog ("OpenGL WebGPU component shutdown");
}

void OpenGLWebGPUComponent::render()
{
    if (! isInitialized || ! webgpuGraphics || ! webgpuGraphics->isInitialized())
        return;

    // Update the OpenGL texture with latest WebGPU content
    updateOpenGLTexture();

    // Clear the background
    juce::OpenGLHelpers::clear (juce::Colours::black);

    // Render the texture
#ifdef __APPLE__
    if ((directCopySupported && ioSurfaceTextureId != 0) || 
        (openglTexture && openglTexture->getTextureID() != 0))
#else
    if (openglTexture && openglTexture->getTextureID() != 0)
#endif
    {
        renderTextureQuad();
    }
}

void OpenGLWebGPUComponent::updateOpenGLTexture()
{
#ifdef __APPLE__
    // Try direct GPU-to-GPU copy first on macOS
    if (directCopySupported && updateOpenGLTextureDirect())
    {
        return;
    }
#endif
    
    // Fallback to CPU path
    updateOpenGLTextureCPU();
}

void OpenGLWebGPUComponent::renderTextureQuad()
{
    if (! shaderProgram)
        return;

#ifdef __APPLE__
    // Check if we have a direct GPU copy texture to render
    if (directCopySupported && ioSurfaceTextureId != 0)
    {
        // Use our shader program
        shaderProgram->use();

        // Bind the IOSurface-backed texture
        juce::gl::glActiveTexture(GL_TEXTURE0);
        juce::gl::glBindTexture(GL_TEXTURE_RECTANGLE, ioSurfaceTextureId);
        shaderProgram->setUniform ("textureSampler", 0);

        // Set up vertex attributes
        openGLContext.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, vertexBuffer);

        // Position attribute
        if (positionAttribLocation >= 0)
        {
            openGLContext.extensions.glVertexAttribPointer ((GLuint) positionAttribLocation, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 4 * sizeof (float), (void*) nullptr);
            openGLContext.extensions.glEnableVertexAttribArray ((GLuint) positionAttribLocation);
        }

        // Texture coordinate attribute
        if (texCoordAttribLocation >= 0)
        {
            openGLContext.extensions.glVertexAttribPointer ((GLuint) texCoordAttribLocation, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 4 * sizeof (float), (void*) (2 * sizeof (float)));
            openGLContext.extensions.glEnableVertexAttribArray ((GLuint) texCoordAttribLocation);
        }

        // Render the quad
        openGLContext.extensions.glBindBuffer (juce::gl::GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        juce::gl::glDrawElements (juce::gl::GL_TRIANGLES, 6, juce::gl::GL_UNSIGNED_INT, nullptr);

        // Cleanup
        if (positionAttribLocation >= 0)
            openGLContext.extensions.glDisableVertexAttribArray ((GLuint) positionAttribLocation);
        if (texCoordAttribLocation >= 0)
            openGLContext.extensions.glDisableVertexAttribArray ((GLuint) texCoordAttribLocation);
        juce::gl::glBindTexture(GL_TEXTURE_RECTANGLE, 0);
        
        return;
    }
#endif

    // Fallback to regular texture rendering (CPU path)
    if (! openglTexture)
        return;

    // Use our shader program
    shaderProgram->use();

    // Bind texture
    openglTexture->bind();
    shaderProgram->setUniform ("textureSampler", 0);

    // Set up vertex attributes
    openGLContext.extensions.glBindBuffer (juce::gl::GL_ARRAY_BUFFER, vertexBuffer);

    // Position attribute
    if (positionAttribLocation >= 0)
    {
        openGLContext.extensions.glVertexAttribPointer ((GLuint) positionAttribLocation, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 4 * sizeof (float), (void*) nullptr);
        openGLContext.extensions.glEnableVertexAttribArray ((GLuint) positionAttribLocation);
    }

    // Texture coordinate attribute
    if (texCoordAttribLocation >= 0)
    {
        openGLContext.extensions.glVertexAttribPointer ((GLuint) texCoordAttribLocation, 2, juce::gl::GL_FLOAT, juce::gl::GL_FALSE, 4 * sizeof (float), (void*) (2 * sizeof (float)));
        openGLContext.extensions.glEnableVertexAttribArray ((GLuint) texCoordAttribLocation);
    }

    // Render the quad
    openGLContext.extensions.glBindBuffer (juce::gl::GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    juce::gl::glDrawElements (juce::gl::GL_TRIANGLES, 6, juce::gl::GL_UNSIGNED_INT, nullptr);

    // Cleanup
    if (positionAttribLocation >= 0)
        openGLContext.extensions.glDisableVertexAttribArray ((GLuint) positionAttribLocation);
    if (texCoordAttribLocation >= 0)
        openGLContext.extensions.glDisableVertexAttribArray ((GLuint) texCoordAttribLocation);
    openglTexture->unbind();
}

void OpenGLWebGPUComponent::paint (juce::Graphics&)
{
    // OpenGL rendering is handled in render() method
    // This paint method is called by JUCE but we don't need to draw anything here
    // since OpenGL handles the rendering
}

void OpenGLWebGPUComponent::resized()
{
    if (isInitialized)
        juce::gl::glViewport (0, 0, getWidth(), getHeight());
}

#ifdef __APPLE__
bool OpenGLWebGPUComponent::updateOpenGLTextureDirect()
{
    if (! webgpuGraphics || ! webgpuGraphics->isInitialized())
        return false;

    try
    {
        // Render the latest WebGPU frame
        webgpuGraphics->renderFrame();

        // Get the WebGPU texture
        WGPUTexture webgpuTexture = webgpuGraphics->getSharedTexture();
        if (!webgpuTexture)
        {
            juce::Logger::writeToLog("Failed to get WebGPU texture for direct copy");
            return false;
        }

        // Get the underlying Metal texture
        id<MTLTexture> metalTexture = getMetalTextureFromWebGPU(webgpuTexture);
        if (!metalTexture)
        {
            juce::Logger::writeToLog("Failed to get Metal texture from WebGPU");
            return false;
        }

        // Get the IOSurface from the Metal texture
        IOSurfaceRef surface = metalTexture.iosurface;
        if (!surface)
        {
            juce::Logger::writeToLog("Metal texture doesn't have an IOSurface");
            return false;
        }

        // Create or update IOSurface-backed OpenGL texture
        if (ioSurfaceTextureId == 0)
        {
            juce::gl::glGenTextures(1, &ioSurfaceTextureId);
        }

        // Bind IOSurface to OpenGL texture
        juce::gl::glBindTexture(GL_TEXTURE_RECTANGLE, ioSurfaceTextureId);
        
        CGLContextObj cglContext = CGLGetCurrentContext();
        if (!cglContext)
        {
            juce::Logger::writeToLog("No current CGL context for IOSurface binding");
            return false;
        }

        CGLError err = CGLTexImageIOSurface2D(
            cglContext,
            GL_TEXTURE_RECTANGLE,
            GL_RGBA8,
            (GLsizei)IOSurfaceGetWidth(surface),
            (GLsizei)IOSurfaceGetHeight(surface),
            GL_BGRA,
            GL_UNSIGNED_INT_8_8_8_8_REV,
            surface,
            0
        );

        if (err != kCGLNoError)
        {
            juce::Logger::writeToLog("Failed to bind IOSurface to OpenGL texture: " + juce::String(err));
            return false;
        }

        // Set texture parameters for rectangle texture
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        juce::gl::glBindTexture(GL_TEXTURE_RECTANGLE, 0);

        // Store reference to the surface for cleanup
        if (ioSurface != surface)
        {
            if (ioSurface)
                CFRelease(ioSurface);
            ioSurface = surface;
            CFRetain(ioSurface);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("Exception in direct GPU copy: " + juce::String(e.what()));
        return false;
    }
}

id<MTLTexture> OpenGLWebGPUComponent::getMetalTextureFromWebGPU(WGPUTexture webgpuTexture)
{
    // This is the critical part - accessing the underlying Metal texture from WebGPU
    // 
    // Unfortunately, the standard WebGPU API doesn't provide this functionality.
    // We would need platform-specific extensions from the WebGPU implementation.
    //
    // Potential solutions for wgpu-native:
    // 1. wgpu_texture_as_hal() - unsafe API to get HAL texture
    // 2. wgpu_metal_texture_get_texture() - if such function exists
    // 3. Custom WebGPU context creation with shared resources
    //
    // For Dawn (Google's implementation):
    // 1. dawn::native::metal::ExportMetalTexture()
    // 2. dawn::native::GetMetalTextureFromWGPUTexture()
    //
    // Implementation example (if API was available):
    // ```
    // // Hypothetical API call
    // void* platformHandle = wgpu_texture_get_platform_handle(webgpuTexture);
    // id<MTLTexture> metalTexture = (__bridge id<MTLTexture>)platformHandle;
    // return metalTexture;
    // ```
    //
    // Since we don't have access to these APIs in the current setup,
    // we'll return nil to indicate that direct copy is not available.
    // The framework is ready and will work once platform access is implemented.
    
    juce::Logger::writeToLog("Direct Metal texture access not implemented - WebGPU API limitations");
    return nil;
}
#endif

void OpenGLWebGPUComponent::updateOpenGLTextureCPU()
{
    if (! webgpuGraphics || ! openglTexture)
        return;

    // Render the latest WebGPU frame
    webgpuGraphics->renderFrame();

    // Get texture dimensions
    int width = webgpuGraphics->getTextureWidth();
    int height = webgpuGraphics->getTextureHeight();

    if (width <= 0 || height <= 0)
        return;

    // Create a temporary image to transfer the data
    // This is the CPU fallback path
    juce::Image tempImage (juce::Image::ARGB, width, height, true);

    // Read WebGPU texture to the temporary image
    WebGPUJuceUtils::readTextureToImage (webgpuGraphics->getContext(), webgpuGraphics->getTexture(), tempImage);

    // Upload the image data to OpenGL texture
    openglTexture->loadImage (tempImage);
}
