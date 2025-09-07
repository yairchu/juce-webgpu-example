#include <webgpu/webgpu.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

static std::string load_text_file(const char* path) {
    std::ifstream f(path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void wait_until(std::atomic<bool>& flag) {
    while (!flag.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main() {
    WGPUInstanceDescriptor instanceDesc = {};
    WGPUInstance instance = wgpuCreateInstance(&instanceDesc);
    assert(instance);

    std::atomic<bool> gotAdapter{false};
    WGPUAdapter adapter = nullptr;
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.powerPreference = WGPUPowerPreference_HighPerformance;

    auto onAdapter = [](WGPURequestAdapterStatus status, WGPUAdapter a, const char*,
                        void* userdata) {
        auto* out = reinterpret_cast<std::pair<WGPUAdapter*, std::atomic<bool>*>*>(userdata);
        if (status == WGPURequestAdapterStatus_Success) {
            *out->first = a;
        }
        out->second->store(true, std::memory_order_release);
    };
    std::pair<WGPUAdapter*, std::atomic<bool>*> adapterOut{&adapter, &gotAdapter};
    wgpuInstanceRequestAdapter(instance, &adapterOpts, onAdapter, &adapterOut);
    wait_until(gotAdapter);
    assert(adapter);

    std::atomic<bool> gotDevice{false};
    WGPUDevice device = nullptr;
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label = "MyDevice";

    auto onDevice = [](WGPURequestDeviceStatus status, WGPUDevice d, const char*,
                       void* userdata) {
        auto* out = reinterpret_cast<std::pair<WGPUDevice*, std::atomic<bool>*>*>(userdata);
        if (status == WGPURequestDeviceStatus_Success) {
            *out->first = d;
        }
        out->second->store(true, std::memory_order_release);
    };
    std::pair<WGPUDevice*, std::atomic<bool>*> deviceOut{&device, &gotDevice};
    wgpuAdapterRequestDevice(adapter, &deviceDesc, onDevice, &deviceOut);
    wait_until(gotDevice);
    assert(device);

    wgpuDeviceSetUncapturedErrorCallback(device,
        [](WGPUErrorType type, const char* msg, void*) {
            std::fprintf(stderr, "[WebGPU Error %d] %s\n", (int)type, msg ? msg : "");
        }, nullptr);

    WGPUQueue queue = wgpuDeviceGetQueue(device);

    std::string wgsl = load_text_file("shaders/comp.wgsl");
    assert(!wgsl.empty());
    WGPUShaderModuleWGSLDescriptor wgslDesc = {};
    wgslDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgslDesc.code = wgsl.c_str();
    WGPUShaderModuleDescriptor shaderDesc = {};
    shaderDesc.nextInChain = reinterpret_cast<const WGPUChainedStruct*>(&wgslDesc);
    shaderDesc.label = "comp.wgsl";
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
    assert(shaderModule);

    const uint64_t bufferSize = sizeof(uint32_t);
    WGPUBufferDescriptor bufDesc = {};
    bufDesc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapRead;
    bufDesc.size  = bufferSize;
    bufDesc.mappedAtCreation = false;
    WGPUBuffer storage = wgpuDeviceCreateBuffer(device, &bufDesc);
    assert(storage);

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
    stage.entryPoint = "main";
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

    std::atomic<bool> mapped{false};
    auto onMap = [](WGPUBufferMapAsyncStatus status, void* userdata) {
        auto* flag = reinterpret_cast<std::atomic<bool>*>(userdata);
        flag->store(true, std::memory_order_release);
    };
    wgpuBufferMapAsync(storage, WGPUMapMode_Read, 0, bufferSize, onMap, &mapped);
    wait_until(mapped);

    const void* ptr = wgpuBufferGetConstMappedRange(storage, 0, bufferSize);
    uint32_t value = 0;
    std::memcpy(&value, ptr, sizeof(uint32_t));
    std::printf("Compute result: %u\n", value);
    wgpuBufferUnmap(storage);

    return 0;
}
