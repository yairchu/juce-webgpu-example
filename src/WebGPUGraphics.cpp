#include "WebGPUGraphics.h"
#include <cassert>
#include <chrono>
#include <cstring>
#include <thread>

WebGPUGraphics::WebGPUGraphics() = default;
WebGPUGraphics::~WebGPUGraphics() = default;

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

    instance = wgpu::createInstance();
    if (! instance)
        return false;

    auto adapter = wgpu::raii::Adapter (instance->requestAdapter ({}));
    if (! adapter)
        return false;

    device = adapter->requestDevice ({});
    if (! device)
        return false;

    queue = device->getQueue();

    const WGPUShaderSourceWGSL vertexWgslSource {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = wgpu::StringView (vertexShaderSource),
    };
    const WGPUShaderModuleDescriptor vertexShaderDesc {
        .nextInChain = &vertexWgslSource.chain,
        .label = wgpu::StringView ("vertex_shader"),
    };
    vertexShader = wgpu::raii::ShaderModule (wgpuDeviceCreateShaderModule (*device, &vertexShaderDesc));

    const WGPUShaderSourceWGSL fragmentWgslSource {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = wgpu::StringView (fragmentShaderSource),
    };
    const WGPUShaderModuleDescriptor fragmentShaderDesc {
        .nextInChain = &fragmentWgslSource.chain,
        .label = wgpu::StringView ("fragment_shader"),
    };
    fragmentShader = wgpu::raii::ShaderModule (wgpuDeviceCreateShaderModule (*device, &fragmentShaderDesc));

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
    // Create render texture
    renderTexture = device->createTexture (WGPUTextureDescriptor {
        .usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc,
        .dimension = WGPUTextureDimension_2D,
        .size = { static_cast<uint32_t> (width), static_cast<uint32_t> (height), 1 },
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
    });

    renderTextureView = renderTexture->createView();

    // No longer need readback texture - we copy directly from render texture to buffer
    return renderTexture && renderTextureView;
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

    vertexBuffer = device->createBuffer (WGPUBufferDescriptor {
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

    renderPipeline = device->createRenderPipeline (WGPURenderPipelineDescriptor {
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
    if (! initialized || (width == textureWidth && height == textureHeight))
        return;

    textureWidth = width;
    textureHeight = height;

    // Recreate textures with new size
    createTexture (width, height);
}

juce::Image WebGPUGraphics::renderFrame()
{
    if (! initialized)
        return {};

    // Render to texture
    {
        wgpu::raii::CommandEncoder encoder = device->createCommandEncoder();

        WGPURenderPassColorAttachment colorAttachment = {
            .view = *renderTextureView,
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = { 0.1f, 0.1f, 0.1f, 1.0f }, // Dark gray background
        };

        WGPURenderPassDescriptor renderPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
            .depthStencilAttachment = nullptr,
        };

        wgpu::raii::RenderPassEncoder pass = encoder->beginRenderPass (renderPassDesc);
        pass->setPipeline (*renderPipeline);
        pass->setVertexBuffer (0, *vertexBuffer, 0, WGPU_WHOLE_SIZE);
        pass->draw (3, 1, 0, 0); // Draw 3 vertices
        pass->end();

        queue->submit (1, &*wgpu::raii::CommandBuffer (encoder->finish()));
    }

    // Read back the texture data directly from render texture
    // WebGPU requires bytes per row to be aligned to 256 bytes
    const uint32_t unalignedBytesPerRow = (uint32_t) textureWidth * bytesPerPixel;
    const uint32_t alignment = 256;
    const uint32_t bytesPerRow = ((unalignedBytesPerRow + alignment - 1) / alignment) * alignment;
    const uint32_t bufferSize = bytesPerRow * (uint32_t) textureHeight;

    wgpu::raii::Buffer readbackBuffer = device->createBuffer (WGPUBufferDescriptor {
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
        .size = bufferSize,
        .mappedAtCreation = false,
    });

    // Copy render texture directly to buffer (no intermediate texture needed)
    {
        wgpu::raii::CommandEncoder encoder = device->createCommandEncoder();
        encoder->copyTextureToBuffer (
            WGPUTexelCopyTextureInfo {
                .texture = *renderTexture, // Copy directly from render texture
                .mipLevel = 0,
                .origin = { 0, 0, 0 },
                .aspect = WGPUTextureAspect_All,
            },
            WGPUTexelCopyBufferInfo {
                .buffer = *readbackBuffer,
                .layout = {
                    .offset = 0,
                    .bytesPerRow = bytesPerRow,
                    .rowsPerImage = static_cast<uint32_t> (textureHeight),
                },
            },
            WGPUExtent3D {
                .width = static_cast<uint32_t> (textureWidth),
                .height = static_cast<uint32_t> (textureHeight),
                .depthOrArrayLayers = 1,
            });
        queue->submit (1, &*wgpu::raii::CommandBuffer (encoder->finish()));
    }

    // Map and read the buffer
    std::atomic<bool> mapped { false };
    readbackBuffer->mapAsync (
        WGPUMapMode_Read, 0, bufferSize, WGPUBufferMapCallbackInfo {
                                             .callback = [] (WGPUMapAsyncStatus, WGPUStringView, void* userdata1, void*)
                                             {
                                                 auto* flag = reinterpret_cast<std::atomic<bool>*> (userdata1);
                                                 flag->store (true, std::memory_order_release);
                                             },
                                             .userdata1 = &mapped,
                                         });

    // Wait for mapping to complete
    while (! mapped.load (std::memory_order_acquire))
    {
        instance->processEvents();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    const void* ptr = readbackBuffer->getConstMappedRange (0, bufferSize);

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
            const int srcIndex = (y * (int) bytesPerRow) + (x * 4);
            bitmap.setPixelColour (x, y, juce::Colour::fromRGBA (src[srcIndex + 0], src[srcIndex + 1], src[srcIndex + 2], src[srcIndex + 3]));
        }

    readbackBuffer->unmap();

    return image;
}

void WebGPUGraphics::shutdown()
{
    initialized = false;
    renderTextureView = {};
    renderTexture = {};
    renderPipeline = {};
    vertexBuffer = {};
    fragmentShader = {};
    vertexShader = {};
    queue = {};
    device = {};
    instance = {};
}
