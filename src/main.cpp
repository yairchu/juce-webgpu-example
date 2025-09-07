#define WEBGPU_CPP_IMPLEMENTATION

#include <webgpu/webgpu-raii.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static std::string load_text_file(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main() {
    wgpu::raii::Instance instance = wgpu::createInstance();
    assert(instance);

    wgpu::raii::Adapter adapter = instance->requestAdapter ({});
    assert(adapter);

    wgpu::raii::Device device = adapter->requestDevice ({});
    assert(device);

    // Note: wgpuDeviceSetUncapturedErrorCallback has been removed in newer WebGPU versions

    wgpu::Queue queue = device->getQueue();

    std::string wgsl = load_text_file("shaders/comp.wgsl");
    assert(!wgsl.empty());

    const WGPUShaderSourceWGSL wgslSource {
        .chain = {.sType = WGPUSType_ShaderSourceWGSL},
        .code = wgpu::StringView (wgsl),
    };
    const WGPUShaderModuleDescriptor shaderDesc {
        .nextInChain = &wgslSource.chain,
        .label = wgpu::StringView ("comp.wgsl"),
    };
    // For a mysterious reason this fails when using device->createShaderModule(shaderDesc)
    wgpu::ShaderModule shaderModule = wgpuDeviceCreateShaderModule (*device, &shaderDesc);
    assert(shaderModule);

    const uint64_t bufferSize = sizeof(uint32_t);
    
    // Storage buffer for compute shader
    wgpu::Buffer storage = device->createBuffer (
        WGPUBufferDescriptor {
            .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc,
            .size = bufferSize,
            .mappedAtCreation = false,
        });
    assert(storage);
    
    // Staging buffer for reading results
    wgpu::Buffer staging = device->createBuffer (
        WGPUBufferDescriptor {
            .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
            .size = bufferSize,
            .mappedAtCreation = false,
        });
    assert(staging);

    const WGPUBindGroupLayoutEntry bglEntry {
        .binding = 0,
        .visibility = WGPUShaderStage_Compute,
        .buffer =
            {
                .type = WGPUBufferBindingType_Storage,
                .hasDynamicOffset = false,
                .minBindingSize = bufferSize,
            },
    };
    wgpu::BindGroupLayout bgl =
        device->createBindGroupLayout (WGPUBindGroupLayoutDescriptor {.entryCount = 1, .entries = &bglEntry});

    wgpu::PipelineLayout pipelineLayout = device->createPipelineLayout (
        WGPUPipelineLayoutDescriptor {
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &(WGPUBindGroupLayout&) bgl,
        });
    wgpu::ComputePipeline pipeline = device->createComputePipeline (
        WGPUComputePipelineDescriptor {
            .layout = pipelineLayout,
            .compute = {
                .module = shaderModule,
                .entryPoint = wgpu::StringView ("main"),
            }});
    assert(pipeline);

    const WGPUBindGroupEntry bgEntry {
        .binding = 0,
        .buffer = storage,
        .offset = 0,
        .size = bufferSize,
    };
    wgpu::BindGroup bindGroup = device->createBindGroup (
        WGPUBindGroupDescriptor {
            .layout = bgl,
            .entryCount = 1,
            .entries = &bgEntry,
        });

    wgpu::CommandEncoder encoder = device->createCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.beginComputePass();
    pass.setPipeline (pipeline);
    pass.setBindGroup (0, bindGroup, 0, nullptr);
    pass.dispatchWorkgroups (1, 1, 1);
    pass.end();
    wgpu::CommandBuffer cmd = encoder.finish();
    queue.submit (1, &cmd);

    // Copy from storage buffer to staging buffer
    {
        wgpu::CommandEncoder encoder = device->createCommandEncoder();
        encoder.copyBufferToBuffer (storage, 0, staging, 0, bufferSize);
        wgpu::CommandBuffer cmd = encoder.finish();
        queue.submit (1, &cmd);
    }

    std::atomic<bool> mapped {false};
    staging.mapAsync (
        WGPUMapMode_Read,
        0,
        bufferSize,
        WGPUBufferMapCallbackInfo {
            .callback =
                [] (WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* userdata2) {
                    auto* flag = reinterpret_cast<std::atomic<bool>*> (userdata1);
                    flag->store (true, std::memory_order_release);
                },
            .userdata1 = &mapped,
        });

    while (! mapped.load (std::memory_order_acquire)) {
        instance->processEvents();
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    }

    const void* ptr = staging.getConstMappedRange (0, bufferSize);
    uint32_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint32_t));
    std::printf("Compute result: %u\n", value);
    staging.unmap();

    return 0;
}
