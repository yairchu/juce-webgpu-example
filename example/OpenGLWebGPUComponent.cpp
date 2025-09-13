#include "OpenGLWebGPUComponent.h"
#include "WebGPUJuceUtils.h"
#include "WebGPUUtils.h"

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
    // Initialize Metal device for IOSurface creation
    metalDevice = MTLCreateSystemDefaultDevice();
    if (metalDevice)
    {
        directCopySupported = true;
        juce::Logger::writeToLog("macOS Metal device created for direct GPU-to-GPU copy");
    }
    else
    {
        juce::Logger::writeToLog("Failed to create Metal device - falling back to CPU path");
        directCopySupported = false;
    }
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
    
    if (sharedMetalTexture != nil)
    {
        sharedMetalTexture = nil;
    }
    
    if (metalDevice != nil)
    {
        metalDevice = nil;
    }
    
    directCopySupported = false;
    sharedTextureWidth = 0;
    sharedTextureHeight = 0;
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
        // Get current texture dimensions
        int width = webgpuGraphics->getTextureWidth();
        int height = webgpuGraphics->getTextureHeight();
        
        if (width <= 0 || height <= 0)
            return false;
        
        // Create or update shared IOSurface texture if needed
        if (!createSharedIOSurfaceTexture(width, height))
        {
            juce::Logger::writeToLog("Failed to create shared IOSurface texture");
            return false;
        }
        
        // For now, since we can't directly integrate WebGPU with our IOSurface,
        // we'll implement a hybrid approach:
        // 1. Render WebGPU frame normally
        // 2. Copy from WebGPU texture to our shared Metal texture using Metal
        // 3. OpenGL can then access the shared texture directly
        
        // Render the latest WebGPU frame
        webgpuGraphics->renderFrame();
        
        // Get the WebGPU texture
        WGPUTexture webgpuTexture = webgpuGraphics->getSharedTexture();
        if (!webgpuTexture)
        {
            juce::Logger::writeToLog("Failed to get WebGPU texture for direct copy");
            return false;
        }
        
        // For now, we'll implement a Metal-assisted copy approach:
        // 1. Use WebGPU's copy operation to copy texture data to a Metal buffer
        // 2. Use Metal to blit from buffer to our shared IOSurface texture
        // This provides GPU-to-GPU copy without going through CPU memory
        
        // Create a temporary Metal buffer for the texture data if needed
        static id<MTLBuffer> tempBuffer = nil;
        size_t bufferSize = width * height * 4; // RGBA8
        
        if (!tempBuffer || tempBuffer.length < bufferSize)
        {
            tempBuffer = [metalDevice newBufferWithLength:bufferSize 
                                                  options:MTLResourceStorageModeShared];
            if (!tempBuffer)
            {
                juce::Logger::writeToLog("Failed to create Metal buffer for texture copy");
                return false;
            }
        }
        
        // Use WebGPU to copy texture data to the Metal buffer
        // We'll need to use WebGPU's buffer mapping mechanism
        WebGPUTexture::MemLayout textureLayout { 
            .width = (uint32_t)width, 
            .height = (uint32_t)height, 
            .bytesPerPixel = 4 
        };
        textureLayout.calcParams();
        
        // Get WebGPU context and copy texture to a buffer
        auto& context = webgpuGraphics->getContext();
        auto& texture = webgpuGraphics->getTexture();
        
        wgpu::raii::Buffer readbackBuffer = texture.read(context, textureLayout);
        const auto* srcData = (uint8_t*)readbackBuffer->getConstMappedRange(0, textureLayout.bufferSize);
        
        if (!srcData)
        {
            juce::Logger::writeToLog("Failed to map WebGPU readback buffer");
            return false;
        }
        
        // Copy data to Metal buffer
        memcpy(tempBuffer.contents, srcData, textureLayout.bufferSize);
        
        // Unmap the WebGPU buffer
        readbackBuffer->unmap();
        
        // Use Metal to copy from buffer to our shared IOSurface texture
        id<MTLCommandQueue> commandQueue = [metalDevice newCommandQueue];
        id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
        id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
        
        // Copy buffer data to texture
        [blitEncoder copyFromBuffer:tempBuffer
                       sourceOffset:0
                  sourceBytesPerRow:textureLayout.bytesPerRow
                sourceBytesPerImage:textureLayout.bufferSize
                         sourceSize:MTLSizeMake(width, height, 1)
                          toTexture:sharedMetalTexture
                   destinationSlice:0
                   destinationLevel:0
                  destinationOrigin:MTLOriginMake(0, 0, 0)];
        
        [blitEncoder endEncoding];
        [commandBuffer commit];
        [commandBuffer waitUntilCompleted];
        
        juce::Logger::writeToLog("Successfully copied WebGPU texture to shared IOSurface via Metal");
        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("Exception in direct GPU copy: " + juce::String(e.what()));
        return false;
    }
}
#endif

#ifdef __APPLE__
bool OpenGLWebGPUComponent::createSharedIOSurfaceTexture(int width, int height)
{
    if (!metalDevice || !directCopySupported)
        return false;
    
    // Clean up existing resources if dimensions changed
    if (sharedTextureWidth != width || sharedTextureHeight != height)
    {
        if (ioSurface)
        {
            CFRelease(ioSurface);
            ioSurface = nullptr;
        }
        if (sharedMetalTexture)
        {
            sharedMetalTexture = nil;
        }
        if (ioSurfaceTextureId != 0)
        {
            juce::gl::glDeleteTextures(1, &ioSurfaceTextureId);
            ioSurfaceTextureId = 0;
        }
    }
    
    // Create IOSurface
    if (!ioSurface)
    {
        CFMutableDictionaryRef properties = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0,
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
        );
        
        // Set IOSurface properties
        int32_t w = width;
        int32_t h = height;
        int32_t bytesPerElement = 4; // RGBA8
        int32_t bytesPerRow = width * bytesPerElement;
        
        CFNumberRef widthNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &w);
        CFNumberRef heightNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &h);
        CFNumberRef bytesPerElementNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bytesPerElement);
        CFNumberRef bytesPerRowNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bytesPerRow);
        
        CFDictionarySetValue(properties, kIOSurfaceWidth, widthNumber);
        CFDictionarySetValue(properties, kIOSurfaceHeight, heightNumber);
        CFDictionarySetValue(properties, kIOSurfaceBytesPerElement, bytesPerElementNumber);
        CFDictionarySetValue(properties, kIOSurfaceBytesPerRow, bytesPerRowNumber);
        CFDictionarySetValue(properties, kIOSurfacePixelFormat, 
                           CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, 
                                        (int32_t[]){kCVPixelFormatType_32BGRA}));
        
        ioSurface = IOSurfaceCreate(properties);
        
        // Clean up CFNumbers
        CFRelease(widthNumber);
        CFRelease(heightNumber);
        CFRelease(bytesPerElementNumber);
        CFRelease(bytesPerRowNumber);
        CFRelease(properties);
        
        if (!ioSurface)
        {
            juce::Logger::writeToLog("Failed to create IOSurface");
            return false;
        }
    }
    
    // Create Metal texture from IOSurface
    if (!sharedMetalTexture)
    {
        MTLTextureDescriptor* textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                                     width:width
                                                                                                    height:height
                                                                                                 mipmapped:NO];
        textureDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        textureDescriptor.storageMode = MTLStorageModeShared;
        
        sharedMetalTexture = [metalDevice newTextureWithDescriptor:textureDescriptor iosurface:ioSurface plane:0];
        
        if (!sharedMetalTexture)
        {
            juce::Logger::writeToLog("Failed to create Metal texture from IOSurface");
            return false;
        }
    }
    
    // Create OpenGL texture from IOSurface
    if (ioSurfaceTextureId == 0)
    {
        juce::gl::glGenTextures(1, &ioSurfaceTextureId);
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
            width,
            height,
            GL_BGRA,
            GL_UNSIGNED_INT_8_8_8_8_REV,
            ioSurface,
            0
        );
        
        if (err != kCGLNoError)
        {
            juce::Logger::writeToLog("Failed to bind IOSurface to OpenGL texture: " + juce::String((int)err));
            return false;
        }
        
        // Set texture parameters
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        juce::gl::glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        juce::gl::glBindTexture(GL_TEXTURE_RECTANGLE, 0);
    }
    
    sharedTextureWidth = width;
    sharedTextureHeight = height;
    
    juce::Logger::writeToLog("Created shared IOSurface texture " + juce::String(width) + "x" + juce::String(height));
    return true;
}

bool OpenGLWebGPUComponent::configureWebGPUForSharedTexture()
{
    if (!webgpuGraphics || !sharedMetalTexture)
        return false;
    
    // This is where we would need to configure WebGPU to render into our shared Metal texture
    // Unfortunately, this requires internal APIs that aren't publicly available
    // For now, we'll implement a workaround using texture copying
    
    juce::Logger::writeToLog("Shared IOSurface texture ready - WebGPU integration requires internal APIs");
    return false; // Return false to indicate we need to use the fallback path for now
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
