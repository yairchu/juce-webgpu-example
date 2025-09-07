#define WEBGPU_CPP_IMPLEMENTATION

#include "WebGPUCompute.h"
#include <cassert>
#include <cstring>
#include <chrono>
#include <thread>

#include "BinaryData.h"
#include <juce_events/juce_events.h>

WebGPUCompute::WebGPUCompute() = default;
WebGPUCompute::~WebGPUCompute() = default;

bool WebGPUCompute::initialize()
{
    if (initialized)
        return true;
        
    try
    {
        // Create WebGPU instance
        instance = wgpu::createInstance();
        if (!instance)
        {
            juce::Logger::writeToLog("Failed to create WebGPU instance");
            return false;
        }

        // Get adapter and device
        auto adapter = wgpu::raii::Adapter(instance->requestAdapter({}));
        if (!adapter)
        {
            juce::Logger::writeToLog("Failed to get WebGPU adapter");
            return false;
        }

        device = adapter->requestDevice({});
        if (!device)
        {
            juce::Logger::writeToLog("Failed to get WebGPU device");
            return false;
        }

        queue = device->getQueue();

        // Load shader from binary data
        juce::String wgsl = juce::String::createStringFromData(
            BinaryData::comp_wgsl, BinaryData::comp_wgslSize);
        if (wgsl.isEmpty())
        {
          juce::Logger::writeToLog("Failed to load shader from binary data");
          return false;
        }

        const WGPUShaderSourceWGSL wgslSource{
            .chain = {.sType = WGPUSType_ShaderSourceWGSL},
            .code = wgpu::StringView(wgsl.toRawUTF8()),
        };
        const WGPUShaderModuleDescriptor shaderDesc{
            .nextInChain = &wgslSource.chain,
            .label = wgpu::StringView("comp.wgsl"),
        };
        
        shaderModule = wgpu::raii::ShaderModule(wgpuDeviceCreateShaderModule(*device, &shaderDesc));
        if (!shaderModule)
        {
            juce::Logger::writeToLog("Failed to create shader module");
            return false;
        }

        // Create buffers
        storage = device->createBuffer(WGPUBufferDescriptor{
            .usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc,
            .size = bufferSize,
            .mappedAtCreation = false,
        });

        staging = device->createBuffer(WGPUBufferDescriptor{
            .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
            .size = bufferSize,
            .mappedAtCreation = false,
        });

        // Create bind group layout
        const WGPUBindGroupLayoutEntry bglEntry{
            .binding = 0,
            .visibility = WGPUShaderStage_Compute,
            .buffer = {
                .type = WGPUBufferBindingType_Storage,
                .hasDynamicOffset = false,
                .minBindingSize = bufferSize,
            },
        };
        bgl = device->createBindGroupLayout(WGPUBindGroupLayoutDescriptor{
            .entryCount = 1, 
            .entries = &bglEntry
        });

        // Create pipeline layout
        pipelineLayout = device->createPipelineLayout(WGPUPipelineLayoutDescriptor{
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &(WGPUBindGroupLayout&)*bgl,
        });

        // Create compute pipeline
        pipeline = device->createComputePipeline(WGPUComputePipelineDescriptor{
            .layout = *pipelineLayout,
            .compute = {
                .module = *shaderModule,
                .entryPoint = wgpu::StringView("main"),
            }
        });

        // Create bind group
        const WGPUBindGroupEntry bgEntry{
            .binding = 0,
            .buffer = *storage,
            .offset = 0,
            .size = bufferSize,
        };
        bindGroup = device->createBindGroup(WGPUBindGroupDescriptor{
            .layout = *bgl,
            .entryCount = 1,
            .entries = &bgEntry,
        });

        initialized = true;
        juce::Logger::writeToLog("WebGPU compute initialized successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("Exception during WebGPU initialization: " + juce::String(e.what()));
        return false;
    }
}

void WebGPUCompute::shutdown()
{
    initialized = false;
    bindGroup = {};
    pipeline = {};
    pipelineLayout = {};
    bgl = {};
    staging = {};
    storage = {};
    shaderModule = {};
    queue = {};
    device = {};
    instance = {};
}

uint32_t WebGPUCompute::runComputeSync()
{
    if (!initialized)
        return 0;

    try
    {
        // Run compute shader
        {
            wgpu::raii::CommandEncoder encoder = device->createCommandEncoder();
            wgpu::raii::ComputePassEncoder pass = encoder->beginComputePass();
            pass->setPipeline(*pipeline);
            pass->setBindGroup(0, *bindGroup, 0, nullptr);
            pass->dispatchWorkgroups(1, 1, 1);
            pass->end();
            queue->submit(1, &*wgpu::raii::CommandBuffer(encoder->finish()));
        }

        // Copy result to staging buffer
        {
            wgpu::raii::CommandEncoder encoder = device->createCommandEncoder();
            encoder->copyBufferToBuffer(*storage, 0, *staging, 0, bufferSize);
            queue->submit(1, &*wgpu::raii::CommandBuffer(encoder->finish()));
        }

        // Map and read result
        std::atomic<bool> mapped{false};
        staging->mapAsync(
            WGPUMapMode_Read, 0, bufferSize,
            WGPUBufferMapCallbackInfo{
                .callback =
                    [](WGPUMapAsyncStatus, WGPUStringView, void *userdata1,
                       void *) {
                      auto *flag =
                          reinterpret_cast<std::atomic<bool> *>(userdata1);
                      flag->store(true, std::memory_order_release);
                    },
                .userdata1 = &mapped,
            });

        // Wait for mapping to complete
        while (!mapped.load(std::memory_order_acquire))
        {
            instance->processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        const void* ptr = staging->getConstMappedRange(0, bufferSize);
        uint32_t value = 0;
        std::memcpy(&value, ptr, sizeof(uint32_t));
        staging->unmap();

        return value;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("Exception during compute execution: " + juce::String(e.what()));
        return 0;
    }
}

void WebGPUCompute::runComputeAsync(std::function<void(uint32_t)> callback)
{
    if (!initialized || !callback)
        return;

    // Run compute on background thread
    std::thread([this, callback]() {
        uint32_t result = runComputeSync();
        juce::MessageManager::callAsync([callback, result]() {
            callback(result);
        });
    }).detach();
}
