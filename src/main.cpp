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

    WGPUQueue queue = wgpuDeviceGetQueue(device);

    std::string wgsl = load_text_file("shaders/comp.wgsl");
    assert(!wgsl.empty());
    
    WGPUShaderSourceWGSL wgslSource = {};
    wgslSource.chain.next = nullptr;
    wgslSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslSource.code = WGPUStringView{.data = wgsl.c_str(), .length = wgsl.length()};
    
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = &wgslSource.chain;
    shaderDesc.label = WGPUStringView{.data = "comp.wgsl", .length = 9};
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    assert(shaderModule);

    const uint64_t bufferSize = sizeof(uint32_t);
    
    // Storage buffer for compute shader
    WGPUBufferDescriptor storageDesc = {};
    storageDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    storageDesc.size = bufferSize;
    storageDesc.mappedAtCreation = false;
    WGPUBuffer storage = wgpuDeviceCreateBuffer(device, &storageDesc);
    assert(storage);
    
    // Staging buffer for reading results
    WGPUBufferDescriptor stagingDesc = {};
    stagingDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    stagingDesc.size = bufferSize;
    stagingDesc.mappedAtCreation = false;
    WGPUBuffer staging = wgpuDeviceCreateBuffer(device, &stagingDesc);
    assert(staging);

    WGPUBindGroupLayoutEntry bglEntry = {};
    bglEntry.binding = 0;
    bglEntry.visibility = WGPUShaderStage_Compute;
    bglEntry.buffer.type = WGPUBufferBindingType_Storage;
    bglEntry.buffer.hasDynamicOffset = false;
    bglEntry.buffer.minBindingSize = bufferSize;
    WGPUBindGroupLayoutDescriptor bglDesc = {};
    bglDesc.entryCount = 1;
    bglDesc.entries = &bglEntry;
    WGPUBindGroupLayout bgl = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);

    WGPUPipelineLayoutDescriptor plDesc = {};
    plDesc.bindGroupLayoutCount = 1;
    plDesc.bindGroupLayouts = &bgl;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &plDesc);

    WGPUComputePipelineDescriptor cpDesc = {};
    cpDesc.layout = pipelineLayout;
    WGPUProgrammableStageDescriptor stage = {};
    stage.module = shaderModule;
    stage.entryPoint = WGPUStringView{.data = "main", .length = 4};
    cpDesc.compute = stage;
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &cpDesc);
    assert(pipeline);

    WGPUBindGroupEntry bgEntry = {};
    bgEntry.binding = 0;
    bgEntry.buffer = storage;
    bgEntry.offset = 0;
    bgEntry.size   = bufferSize;
    WGPUBindGroupDescriptor bgDesc = {};
    bgDesc.layout = bgl;
    bgDesc.entryCount = 1;
    bgDesc.entries = &bgEntry;
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bgDesc);

    WGPUCommandEncoderDescriptor encDesc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encDesc);
    WGPUComputePassDescriptor passDesc = {};
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
        WGPUCommandBufferDescriptor cbDesc = {};
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cbDesc);
        wgpuQueueSubmit(queue, 1, &cmd);
    }

    std::atomic<bool> mapped{false};
    auto onMap = [](WGPUMapAsyncStatus status, WGPUStringView message, void* userdata1, void* userdata2) {
        auto* flag = reinterpret_cast<std::atomic<bool>*>(userdata1);
        flag->store(true, std::memory_order_release);
    };
    WGPUBufferMapCallbackInfo mapCallback = {};
    mapCallback.callback = onMap;
    mapCallback.userdata1 = &mapped;
    mapCallback.userdata2 = nullptr;
    wgpuBufferMapAsync(staging, WGPUMapMode_Read, 0, bufferSize, mapCallback);
    wait_until(mapped, instance);

    const void* ptr = wgpuBufferGetConstMappedRange(staging, 0, bufferSize);
    uint32_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint32_t));
    std::printf("Compute result: %u\n", value);
    wgpuBufferUnmap(staging);

    return 0;
}
