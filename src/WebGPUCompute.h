#pragma once

#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu-raii.hpp>
#include <juce_core/juce_core.h>
#include <memory>
#include <atomic>
#include <functional>

class WebGPUCompute
{
public:
    WebGPUCompute();
    ~WebGPUCompute();

    bool initialize();
    void shutdown();
    
    // Run the compute shader asynchronously
    void runComputeAsync(std::function<void(uint32_t)> callback);
    
    // Run the compute shader synchronously
    uint32_t runComputeSync();
    
    bool isInitialized() const { return initialized; }

private:
    static juce::String loadTextFile(const juce::String& path);
    
    bool initialized = false;
    
    wgpu::raii::Instance instance;
    wgpu::raii::Device device;
    wgpu::raii::Queue queue;
    wgpu::raii::ShaderModule shaderModule;
    wgpu::raii::Buffer storage;
    wgpu::raii::Buffer staging;
    wgpu::raii::BindGroupLayout bgl;
    wgpu::raii::PipelineLayout pipelineLayout;
    wgpu::raii::ComputePipeline pipeline;
    wgpu::raii::BindGroup bindGroup;
    
    static constexpr uint64_t bufferSize = sizeof(uint32_t);
};
