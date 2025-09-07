#define WEBGPU_CPP_IMPLEMENTATION

#include <webgpu/webgpu.hpp>

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

static void wait_until(std::atomic<bool>& flag, WGPUInstance instance = nullptr) {
    while (!flag.load(std::memory_order_acquire)) {
        if (instance) {
            wgpuInstanceProcessEvents(instance);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main() {
    wgpu::Instance instance = wgpu::createInstance({});
    assert(instance);

    wgpu::Adapter adapter = instance.requestAdapter({});
    assert(adapter);

    wgpu::Device device = adapter.requestDevice({});
    assert(device);

    // Note: wgpuDeviceSetUncapturedErrorCallback has been removed in newer WebGPU versions

    wgpu::Queue queue = device.getQueue();

    std::string wgsl = load_text_file("shaders/comp.wgsl");
    assert(!wgsl.empty());

    const WGPUShaderSourceWGSL wgslSource {
        .chain = {.sType = WGPUSType_ShaderSourceWGSL},
        .code = WGPUStringView {.data = wgsl.c_str(), .length = wgsl.length()},
    };
    const WGPUShaderModuleDescriptor shaderDesc {
        .nextInChain = &wgslSource.chain,
        .label = WGPUStringView {.data = "comp.wgsl", .length = 9},
    };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    assert(shaderModule);

    const uint64_t bufferSize = sizeof(uint32_t);
    
    // Storage buffer for compute shader
    wgpu::Buffer storage = device.createBuffer (
        WGPUBufferDescriptor {
            .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc,
            .size = bufferSize,
            .mappedAtCreation = false,
        });
    assert(storage);
    
    // Staging buffer for reading results
    wgpu::Buffer staging = device.createBuffer (
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
    const WGPUBindGroupLayoutDescriptor bglDesc {.entryCount = 1, .entries = &bglEntry};
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    const WGPUPipelineLayoutDescriptor plDesc {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bgl,
    };
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    const WGPUComputePipelineDescriptor cpDesc {
        .layout = pipelineLayout,
        .compute = {
            .module = shaderModule,
            .entryPoint = WGPUStringView {.data = "main", .length = 4},
        }};
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &cpDesc);
    assert(pipeline);

    const WGPUBindGroupEntry bgEntry {
        .binding = 0,
        .buffer = storage,
        .offset = 0,
        .size = bufferSize,
    };
    const WGPUBindGroupDescriptor bgDesc {
        .layout = bgl,
        .entryCount = 1,
        .entries = &bgEntry,
    };
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    const WGPUCommandEncoderDescriptor encDesc {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encDesc);
    const WGPUComputePassDescriptor passDesc {};
    WGPUComputePassEncoder pass = wgpuCommandEncoderBeginComputePass(encoder, &passDesc);
    wgpuComputePassEncoderSetPipeline(pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    wgpuComputePassEncoderDispatchWorkgroups(pass, 1, 1, 1);
    wgpuComputePassEncoderEnd(pass);
    WGPUCommandBufferDescriptor cbDesc = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cbDesc);
    wgpuQueueSubmit(queue, 1, &cmd);

    // Copy from storage buffer to staging buffer
    {
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
        wgpuCommandEncoderCopyBufferToBuffer(encoder, storage, 0, staging, 0, bufferSize);
        const WGPUCommandBufferDescriptor cbDesc = {};
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cbDesc);
        wgpuQueueSubmit(queue, 1, &cmd);
    }

    std::atomic<bool> mapped {false};
    wgpuBufferMapAsync (
        staging,
        WGPUMapMode_Read,
        0,
        bufferSize,
        {
            .callback =
                [] (WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* userdata2) {
                    auto* flag = reinterpret_cast<std::atomic<bool>*> (userdata1);
                    flag->store (true, std::memory_order_release);
                },
            .userdata1 = &mapped,
        });
    wait_until(mapped, instance);

    const void* ptr = wgpuBufferGetConstMappedRange(staging, 0, bufferSize);
    uint32_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint32_t));
    std::printf("Compute result: %u\n", value);
    wgpuBufferUnmap(staging);

    return 0;
}
