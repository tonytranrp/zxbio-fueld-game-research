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
constexpr std::size_t kMaterialCount = static_cast<std::size_t>(world::chunk::MaterialID::Leaves) + 1;
static_assert(kMaterialCount == 6, "terrain.psh.hlsl declares g_MaterialColors[6] -- update both together");

// Linear-space albedo per MaterialID; index 0 (Air) is never sampled by a real fragment but keeps
// indexing direct. Goal 19's pass: richer/more saturated than the original deliberately-muted
// placeholders, judged against viewed before/after dumps (not picked blind) -- stone warmed off
// pure gray, dirt/wood deepened, water toward a real ocean blue, leaves toward meadow green.
constexpr float kMaterialColors[kMaterialCount][4] = {
    {0.0f, 0.0f, 0.0f, 1.0f},    // Air (unused)
    {0.52f, 0.49f, 0.44f, 1.0f}, // Stone (warm gray, not blue-gray)
    {0.44f, 0.28f, 0.14f, 1.0f}, // Dirt (richer brown)
    {0.09f, 0.33f, 0.58f, 1.0f}, // Water (deeper ocean blue)
    {0.36f, 0.22f, 0.09f, 1.0f}, // Wood (tree trunks)
    {0.20f, 0.50f, 0.12f, 1.0f}, // Leaves (livelier canopy green)
};

// The GPU input layout is a byte-for-byte contract with detail::GpuVertexCompressed (built at
// upload time from world::meshing::Vertex) -- freeze it here so a layout change breaks the build
// instead of the rendered image.
using detail::GpuVertexCompressed;
static_assert(sizeof(GpuVertexCompressed) == 12);
static_assert(offsetof(GpuVertexCompressed, px) == 0);
static_assert(offsetof(GpuVertexCompressed, octU) == 8);
static_assert(offsetof(GpuVertexCompressed, material) == 10);
static_assert(offsetof(GpuVertexCompressed, ao) == 11);

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

    // Interleaved AoS vertex fetch (Phase 1 brief §2.6) of the COMPRESSED 12-byte vertex
    // (Group K): the decode happens in fixed-function normalized fetch, not shader bit ops --
    // the two normalized attributes arrive in the VS as plain floats in [0,1], and the only
    // shader-side work is one mad (position) and the octahedral refold (arithmetic). Explicit
    // offsets/stride, not LAYOUT_ELEMENT_AUTO_*, because of the 3 tail padding bytes.
    LayoutElement layout[4];
    // 4 components: DXGI has no 3-component 16-bit vertex format (R16G16B16_UNORM does not
    // exist), so 3x VT_UINT16 asserts in the D3D12 backend while silently working on Vulkan.
    layout[0].InputIndex = 0; // ATTRIB0: position, 4x uint16 UNORM fixed point (1/1024 voxel; w unused)
    layout[0].NumComponents = 4;
    layout[0].ValueType = VT_UINT16;
    layout[0].IsNormalized = True;
    layout[0].RelativeOffset = static_cast<Uint32>(offsetof(GpuVertexCompressed, px));
    layout[0].Stride = sizeof(GpuVertexCompressed);
    layout[1].InputIndex = 1; // ATTRIB1: 16-bit octahedral normal, 2x uint8 UNORM
    layout[1].NumComponents = 2;
    layout[1].ValueType = VT_UINT8;
    layout[1].IsNormalized = True;
    layout[1].RelativeOffset = static_cast<Uint32>(offsetof(GpuVertexCompressed, octU));
    layout[1].Stride = sizeof(GpuVertexCompressed);
    layout[2].InputIndex = 2; // ATTRIB2: material id
    layout[2].NumComponents = 1;
    layout[2].ValueType = VT_UINT8;
    layout[2].IsNormalized = False; // integer attribute, read as uint in HLSL
    layout[2].RelativeOffset = static_cast<Uint32>(offsetof(GpuVertexCompressed, material));
    layout[2].Stride = sizeof(GpuVertexCompressed);
    layout[3].InputIndex = 3; // ATTRIB3: baked AO, uint8 UNORM (the former pad byte -- stride unchanged)
    layout[3].NumComponents = 1;
    layout[3].ValueType = VT_UINT8;
    layout[3].IsNormalized = True;
    layout[3].RelativeOffset = static_cast<Uint32>(offsetof(GpuVertexCompressed, ao));
    layout[3].Stride = sizeof(GpuVertexCompressed);
    psoCI.GraphicsPipeline.InputLayout.LayoutElements = layout;
    psoCI.GraphicsPipeline.InputLayout.NumElements = 4;

    // Fixed-function defaults (solid fill, back-face cull, depth LESS) are correct EXCEPT the
    // winding convention: Phase 1's on-paper derivation concluded the default
    // FrontCounterClockwise=False matched the mesher's output, but the TERRAIN_FIXES ribbon-bug
    // bisection proved it empirically wrong -- the visual capture showed terrain visible from
    // BELOW and invisible from above (up-facing triangles were the ones being back-face culled;
    // the "ribbons" were just steep silhouette slivers that survived). extract_mesh emits
    // counter-clockwise-when-viewed-from-outside triangles under GLM's RH/[0,1] conventions, so
    // the rasterizer must treat CCW as front. The lesson stands in
    // research/terrain-fixes-log.md: a winding derivation is not verified until an actual frame
    // capture has been looked at from both sides.
    psoCI.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
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
