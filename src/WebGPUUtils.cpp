#define WEBGPU_CPP_IMPLEMENTATION

#include "WebGPUUtils.h"

#include <thread>

bool WebGPUContext::init()
{
    instance = wgpu::createInstance();
    if (! instance)
        return false;

    device = wgpu::raii::Adapter (instance->requestAdapter ({}))->requestDevice ({});
    if (! device)
        return false;

    queue = device->getQueue();
    return queue;
}

wgpu::raii::ShaderModule WebGPUContext::loadWgslShader (const char* source, const char* name)
{
    const WGPUShaderSourceWGSL vertexWgslSource {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = wgpu::StringView (source),
    };
    const WGPUShaderModuleDescriptor vertexShaderDesc {
        .nextInChain = &vertexWgslSource.chain,
        .label = name == nullptr ? WGPUStringView {} : wgpu::StringView ("vertex_shader"),
    };
    return wgpu::raii::ShaderModule (wgpuDeviceCreateShaderModule (*device, &vertexShaderDesc));
}

bool WebGPUTexture::init (WebGPUContext& context, const WGPUTextureDescriptor& desc)
{
    texture = context.device->createTexture (desc);
    if (! texture)
        return false;
    view = texture->createView();
    return view;
}

void WebGPUTexture::MemLayout::calcParams()
{
    const uint32_t unalignedBytesPerRow = width * bytesPerPixel;
    const uint32_t alignment = 256;
    bytesPerRow = ((unalignedBytesPerRow + alignment - 1) / alignment) * alignment;
    bufferSize = bytesPerRow * height;
}

wgpu::raii::Buffer WebGPUTexture::read (WebGPUContext& context, const MemLayout& layout)
{
    wgpu::raii::Buffer readbackBuffer = context.device->createBuffer (WGPUBufferDescriptor {
        .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
        .size = layout.bufferSize,
        .mappedAtCreation = false,
    });

    {
        wgpu::raii::CommandEncoder encoder = context.device->createCommandEncoder();
        encoder->copyTextureToBuffer (
            WGPUTexelCopyTextureInfo {
                .texture = *texture,
                .mipLevel = 0,
                .origin = { 0, 0, 0 },
                .aspect = WGPUTextureAspect_All,
            },
            WGPUTexelCopyBufferInfo {
                .buffer = *readbackBuffer,
                .layout = {
                    .offset = 0,
                    .bytesPerRow = layout.bytesPerRow,
                    .rowsPerImage = layout.height,
                },
            },
            WGPUExtent3D {
                .width = layout.width,
                .height = layout.height,
                .depthOrArrayLayers = 1,
            });
        context.queue->submit (1, &*wgpu::raii::CommandBuffer (encoder->finish()));
    }

    std::atomic<bool> mapped { false };
    readbackBuffer->mapAsync (
        WGPUMapMode_Read, 0, layout.bufferSize, WGPUBufferMapCallbackInfo {
                                                    .callback = [] (WGPUMapAsyncStatus, WGPUStringView, void* userdata1, void*)
                                                    {
                                                        auto* flag = reinterpret_cast<std::atomic<bool>*> (userdata1);
                                                        flag->store (true, std::memory_order_release);
                                                    },
                                                    .userdata1 = &mapped,
                                                });

    // Wait for mapping to complete, but check for shutdown
    while (! mapped.load (std::memory_order_acquire))
    {
        context.instance->processEvents();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    return readbackBuffer;
}
