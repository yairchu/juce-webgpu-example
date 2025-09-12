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
    if (openglTexture->getTextureID() != 0)
        renderTextureQuad();
}

void OpenGLWebGPUComponent::updateOpenGLTexture()
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
    // TODO: In a future optimization, this could be done with direct GPU-to-GPU copy
    // using platform-specific WebGPU-OpenGL interop APIs
    juce::Image tempImage (juce::Image::ARGB, width, height, true);

    // Read WebGPU texture to the temporary image
    // This is still the CPU path, but it's isolated to this component
    WebGPUJuceUtils::readTextureToImage (webgpuGraphics->getContext(), webgpuGraphics->getTexture(), tempImage);

    // Upload the image data to OpenGL texture
    openglTexture->loadImage (tempImage);
}

void OpenGLWebGPUComponent::renderTextureQuad()
{
    if (! shaderProgram || ! openglTexture)
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
