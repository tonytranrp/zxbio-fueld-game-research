#include <cstddef>
#include <stdexcept>

#include "detail/terrain_renderer_impl.hpp"

#include "world/chunk/material.hpp"

#include "Graphics/GraphicsEngine/interface/Shader.h"

namespace render::diligent {

namespace {

using namespace Diligent;

// The material palette cbuffer (float4 per MaterialID) is sized to the actual material count from
// world/chunk -- adding a material means updating this array AND g_MaterialColors[] in
// terrain.psh.hlsl together (task 12).
constexpr std::size_t kMaterialCount = static_cast<std::size_t>(world::chunk::MaterialID::Water) + 1;
static_assert(kMaterialCount == 4, "terrain.psh.hlsl declares g_MaterialColors[4] -- update both together");

// Linear-space albedo per MaterialID; index 0 (Air) is never sampled by a real fragment but keeps
// indexing direct.
constexpr float kMaterialColors[kMaterialCount][4] = {
    {0.0f, 0.0f, 0.0f, 1.0f},    // Air (unused)
    {0.55f, 0.55f, 0.58f, 1.0f}, // Stone
    {0.45f, 0.32f, 0.18f, 1.0f}, // Dirt
    {0.13f, 0.35f, 0.72f, 1.0f}, // Water
};

// The GPU input layout is a byte-for-byte contract with world::meshing::Vertex -- freeze it here
// so a Vertex change breaks the build instead of the rendered image.
using world::meshing::Vertex;
static_assert(sizeof(Vertex) == 28);
static_assert(offsetof(Vertex, position) == 0);
static_assert(offsetof(Vertex, normal) == 12);
static_assert(offsetof(Vertex, material) == 24);

RefCntAutoPtr<IShader> create_shader(TerrainRenderer::Impl& impl, IShaderSourceInputStreamFactory* streamFactory,
                                     SHADER_TYPE type, const char* file, const char* name) {
    ShaderCreateInfo shaderCI;
    shaderCI.pShaderSourceStreamFactory = streamFactory;
    shaderCI.FilePath = file;
    shaderCI.EntryPoint = "main";
    shaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL; // default is GLSL on Vulkan -- must be explicit
    shaderCI.Desc.ShaderType = type;
    shaderCI.Desc.Name = name;

    RefCntAutoPtr<IShader> shader;
    RefCntAutoPtr<IDataBlob> compilerOutput;
    impl.context->impl().device->CreateShader(shaderCI, &shader, &compilerOutput);
    if (!shader) {
        std::string message = std::string("terrain shader compilation failed: ") + file;
        if (compilerOutput && compilerOutput->GetSize() > 0) {
            message += "\n";
            message += static_cast<const char*>(compilerOutput->GetConstDataPtr());
        }
        throw std::runtime_error(message);
    }
    return shader;
}

RefCntAutoPtr<IBuffer> create_dynamic_uniform_buffer(IRenderDevice* device, Uint64 size, const char* name) {
    BufferDesc desc;
    desc.Name = name;
    desc.Size = size;
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    RefCntAutoPtr<IBuffer> buffer;
    device->CreateBuffer(desc, nullptr, &buffer);
    if (!buffer) {
        throw std::runtime_error(std::string("failed to create uniform buffer: ") + name);
    }
    return buffer;
}

} // namespace

void create_terrain_pipeline(TerrainRenderer::Impl& impl) {
    auto& rc = impl.context->impl();
    IRenderDevice* device = rc.device;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> streamFactory;
    rc.factory->CreateDefaultShaderSourceStreamFactory(VOXEL_TERRAIN_SHADER_DIR, &streamFactory);
    if (!streamFactory) {
        throw std::runtime_error("failed to create shader source stream factory for " VOXEL_TERRAIN_SHADER_DIR);
    }

    RefCntAutoPtr<IShader> vs = create_shader(impl, streamFactory, SHADER_TYPE_VERTEX, "terrain.vsh.hlsl", "Terrain VS");
    RefCntAutoPtr<IShader> ps = create_shader(impl, streamFactory, SHADER_TYPE_PIXEL, "terrain.psh.hlsl", "Terrain PS");

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "Terrain PSO";
    psoCI.pVS = vs;
    psoCI.pPS = ps;

    // Interleaved AoS vertex fetch (PHASE_1_BRIEF.md §2.6): a vertex shader touches every field of
    // one vertex at once -- the stated exception to the SoA default. Explicit offsets/stride, not
    // LAYOUT_ELEMENT_AUTO_*, because Vertex carries 3 tail padding bytes auto-packing would elide.
    LayoutElement layout[3];
    layout[0].InputIndex = 0; // ATTRIB0: position
    layout[0].NumComponents = 3;
    layout[0].ValueType = VT_FLOAT32;
    layout[0].RelativeOffset = static_cast<Uint32>(offsetof(Vertex, position));
    layout[0].Stride = sizeof(Vertex);
    layout[1].InputIndex = 1; // ATTRIB1: normal
    layout[1].NumComponents = 3;
    layout[1].ValueType = VT_FLOAT32;
    layout[1].RelativeOffset = static_cast<Uint32>(offsetof(Vertex, normal));
    layout[1].Stride = sizeof(Vertex);
    layout[2].InputIndex = 2; // ATTRIB2: material id
    layout[2].NumComponents = 1;
    layout[2].ValueType = VT_UINT8;
    layout[2].IsNormalized = False; // integer attribute, read as uint in HLSL
    layout[2].RelativeOffset = static_cast<Uint32>(offsetof(Vertex, material));
    layout[2].Stride = sizeof(Vertex);
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
    psoCI.GraphicsPipeline.InputLayout.NumElements = 3;

    // Fixed-function defaults are already correct for opaque terrain (research/
    // diligent-core-api-surface.md Task 2): solid fill, back-face cull, depth test+write LESS.
    // Only the attachment formats are mandatory -- read from the real swap chain, never assumed.
    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] = scDesc.ColorBufferFormat;
    psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    // All three cbuffers are STATIC-tier variables (research Task 1): the *binding* never changes
    // -- per-frame/per-draw variability lives in the dynamic buffers' contents, not in rebinding.
    psoCI.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    device->CreateGraphicsPipelineState(psoCI, &impl.pso);
    if (!impl.pso) {
        throw std::runtime_error("terrain PSO creation failed");
    }

    impl.frameConstants =
        create_dynamic_uniform_buffer(device, sizeof(detail::FrameConstantsCpu), "Terrain FrameConstants CB");
    impl.chunkConstants =
        create_dynamic_uniform_buffer(device, sizeof(detail::ChunkConstantsCpu), "Terrain ChunkConstants CB");

    {
        BufferDesc desc;
        desc.Name = "Terrain MaterialPalette CB";
        desc.Size = sizeof(kMaterialColors);
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = BIND_UNIFORM_BUFFER;
        BufferData initial{kMaterialColors, sizeof(kMaterialColors)};
        impl.context->impl().device->CreateBuffer(desc, &initial, &impl.materialPalette);
        if (!impl.materialPalette) {
            throw std::runtime_error("material palette buffer creation failed");
        }
    }

    const auto bind_static = [&](SHADER_TYPE stage, const char* variable, IBuffer* buffer) {
        if (IShaderResourceVariable* var = impl.pso->GetStaticVariableByName(stage, variable)) {
            var->Set(buffer);
        } else {
            // A missing variable here means the shader no longer declares (or no longer uses) the
            // cbuffer -- fail loudly instead of drawing with an unbound resource.
            throw std::runtime_error(std::string("terrain shader variable not found: ") + variable);
        }
    };
    bind_static(SHADER_TYPE_VERTEX, "FrameConstants", impl.frameConstants);
    bind_static(SHADER_TYPE_VERTEX, "ChunkConstants", impl.chunkConstants);
    bind_static(SHADER_TYPE_PIXEL, "MaterialPalette", impl.materialPalette);

    impl.pso->CreateShaderResourceBinding(&impl.srb, true); // true: copy the static bindings in
    if (!impl.srb) {
        throw std::runtime_error("terrain SRB creation failed");
    }
}

} // namespace render::diligent
