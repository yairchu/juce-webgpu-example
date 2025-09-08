#include "WebGPUGraphics.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <thread>

namespace
{

const char* vertexShaderSource = R"(
    struct VertexOutput {
        @builtin(position) position: vec4<f32>,
        @location(0) color: vec3<f32>,
    }

    @vertex
    fn vs_main(@location(0) position: vec2<f32>, @location(1) color: vec3<f32>) -> VertexOutput {
        var output: VertexOutput;
        output.position = vec4<f32>(position, 0.0, 1.0);
        output.color = color;
        return output;
    }
)";

const char* fragmentShaderSource = R"(
    @fragment
    fn fs_main(@location(0) color: vec3<f32>) -> @location(0) vec4<f32> {
        return vec4<f32>(color, 1.0);
    }
)";

} // namespace

bool WebGPUGraphics::initialize (int width, int height)
{
    if (initialized)
        return true;

    textureWidth = width;
    textureHeight = height;

    if (! context.init())
        return false;

    vertexShader = context.loadWgslShader (vertexShaderSource);
    fragmentShader = context.loadWgslShader (fragmentShaderSource);

    if (! vertexShader || ! fragmentShader)
        return false;

    if (! createTexture (width, height) || ! createVertexBuffer() || ! createPipeline())
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

bool WebGPUGraphics::createVertexBuffer()
{
    // Triangle vertices with positions and colors
    struct Vertex
    {
        float position[2];
        float color[3];
    };

    const Vertex vertices[] = {
        { { 0.0f, 0.8f }, { 1.0f, 0.0f, 0.0f } }, // Top vertex - red
        { { -0.8f, -0.8f }, { 0.0f, 1.0f, 0.0f } }, // Bottom left - green
        { { 0.8f, -0.8f }, { 0.0f, 0.0f, 1.0f } }, // Bottom right - blue
    };

    vertexBuffer = context.device->createBuffer (WGPUBufferDescriptor {
        .usage = wgpu::BufferUsage::Vertex,
        .size = sizeof (vertices),
        .mappedAtCreation = true,
    });

    void* mappedData = vertexBuffer->getMappedRange (0, sizeof (vertices));
    std::memcpy (mappedData, vertices, sizeof (vertices));
    vertexBuffer->unmap();

    return *vertexBuffer != nullptr;
}

bool WebGPUGraphics::createPipeline()
{
    // Vertex attributes
    WGPUVertexAttribute attributes[2] = {
        {
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
            .shaderLocation = 0,
        },
        {
            .format = WGPUVertexFormat_Float32x3,
            .offset = 2 * sizeof (float),
            .shaderLocation = 1,
        }
    };

    WGPUVertexBufferLayout vertexBufferLayout = {
        .arrayStride = 5 * sizeof (float), // 2 floats for position + 3 floats for color
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 2,
        .attributes = attributes,
    };

    WGPUColorTargetState colorTarget = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .blend = nullptr,
        .writeMask = WGPUColorWriteMask_All,
    };

    WGPUFragmentState fragmentState = {
        .module = *fragmentShader,
        .entryPoint = wgpu::StringView ("fs_main"),
        .targetCount = 1,
        .targets = &colorTarget,
    };

    renderPipeline = context.device->createRenderPipeline (WGPURenderPipelineDescriptor {
        .layout = nullptr, // Auto layout
        .vertex = {
            .module = *vertexShader,
            .entryPoint = wgpu::StringView ("vs_main"),
            .bufferCount = 1,
            .buffers = &vertexBufferLayout,
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .stripIndexFormat = WGPUIndexFormat_Undefined,
            .frontFace = WGPUFrontFace_CCW,
            .cullMode = WGPUCullMode_None,
        },
        .fragment = &fragmentState,
        .multisample = {
            .count = 1,
            .mask = 0xFFFFFFFF,
            .alphaToCoverageEnabled = false,
        },
    });

    return *renderPipeline != nullptr;
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

    // GPU-only rendering - no CPU readback
    wgpu::raii::CommandEncoder encoder = context.device->createCommandEncoder();

    WGPURenderPassColorAttachment colorAttachment = {
        .view = *texture.view,
        .resolveTarget = nullptr,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = { 0.1f, 0.1f, 0.1f, 1.0f }, // Dark gray background
    };

    wgpu::raii::RenderPassEncoder pass = encoder->beginRenderPass (WGPURenderPassDescriptor {
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
        .depthStencilAttachment = nullptr,
    });
    pass->setPipeline (*renderPipeline);
    pass->setVertexBuffer (0, *vertexBuffer, 0, WGPU_WHOLE_SIZE);
    pass->draw (3, 1, 0, 0); // Draw 3 vertices
    pass->end();

    context.queue->submit (1, &*wgpu::raii::CommandBuffer (encoder->finish()));
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
