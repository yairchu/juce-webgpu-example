#include "WebGPUExampleScene.h"

#include "WebGPUUtils.h"

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

bool WebGPUExampleScene::initialize (WebGPUContext& context)
{
    vertexShader = context.loadWgslShader (vertexShaderSource);
    fragmentShader = context.loadWgslShader (fragmentShaderSource);
    if (! fragmentShader || ! vertexShader)
        return false;

    return createVertexBuffer (context) && createPipeline (context);
}

void WebGPUExampleScene::render (WebGPUContext& context, WebGPUTexture& texture)
{
    wgpu::raii::CommandEncoder encoder = context.device->createCommandEncoder();

    {
        WGPURenderPassColorAttachment colorAttachment {
            .view = *texture.view,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = { 0.2f, 0.2f, 0.2f, 1.0f }, // Dark gray background
        };
        wgpu::raii::RenderPassEncoder renderPass = encoder->beginRenderPass (WGPURenderPassDescriptor {
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
        });

        renderPass->setPipeline (*renderPipeline);
        renderPass->setVertexBuffer (0, *vertexBuffer, 0, WGPU_WHOLE_SIZE);
        renderPass->draw (3, 1, 0, 0); // Draw 3 vertices (triangle)
        renderPass->end();
    }

    wgpu::raii::CommandBuffer commands = encoder->finish();
    context.queue->submit (1, &*commands);
}

bool WebGPUExampleScene::createVertexBuffer (WebGPUContext& context)
{
    struct Vertex
    {
        float position[2];
        float color[3];
    };

    const Vertex vertices[] {
        { { 0.0f, 0.8f }, { 1.0f, 0.0f, 0.0f } }, // Top vertex - red
        { { -0.8f, -0.8f }, { 0.0f, 1.0f, 0.0f } }, // Bottom left - green
        { { 0.8f, -0.8f }, { 0.0f, 0.0f, 1.0f } }, // Bottom right - blue
    };

    vertexBuffer = context.device->createBuffer (WGPUBufferDescriptor {
        .usage = wgpu::BufferUsage::Vertex,
        .size = sizeof (vertices),
        .mappedAtCreation = true,
    });

    std::memcpy (vertexBuffer->getMappedRange (0, sizeof (vertices)), vertices, sizeof (vertices));
    vertexBuffer->unmap();

    return vertexBuffer;
}

bool WebGPUExampleScene::createPipeline (WebGPUContext& context)
{
    WGPUVertexAttribute attributes[2] {
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

    WGPUVertexBufferLayout vertexBufferLayout {
        .arrayStride = 5 * sizeof (float), // 2 floats for position + 3 floats for color
        .stepMode = WGPUVertexStepMode_Vertex,
        .attributeCount = 2,
        .attributes = attributes,
    };

    WGPUColorTargetState colorTarget {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .blend = nullptr,
        .writeMask = WGPUColorWriteMask_All,
    };

    WGPUFragmentState fragmentState {
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
            .mask = UINT32_MAX,
            .alphaToCoverageEnabled = false,
        },
    });

    return renderPipeline;
}
