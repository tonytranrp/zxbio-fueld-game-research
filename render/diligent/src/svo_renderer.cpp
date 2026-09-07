#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "render/diligent/svo_renderer.hpp"

#include "engine/core/log.hpp"
#include "world/materials/materials.hpp"

#include "detail/material_macros.hpp"
#include "detail/render_context_impl.hpp"
#include "detail/wind_macros.hpp"

#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsTools/interface/DurationQueryHelper.hpp"

#include "render/diligent/gpu_passes.hpp"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#else
#define ZoneScopedN(name)
#endif

namespace render::diligent {

using namespace Diligent;

namespace {

using world::materials::kMaterialCount;

// Mirror of svo_march.psh.hlsl's cbuffer MarchConstants -- update both together. The material
// records at the tail are detail::kMaterialRecords (rgb albedo + shading model in .w); the shader
// sizes that array with the MATERIAL_COUNT macro create_shader passes from the same registry.
struct MarchConstantsCpu {
    glm::mat4 invViewProj;
    glm::mat4 viewProj;
    glm::vec4 cameraPosWorld; // xyz + time
    glm::vec4 treeOrigin;     // xyz + root edge
    glm::vec4 treeParams;     // lod pixel angle, shadow lod multiplier, finest voxel edge, AO radius px
    glm::uvec4 treeInts;      // V, max brick level, root offset, flags | view << 8
    glm::vec4 shadeParams;    // smooth pixels, grain amplitude, AO lod multiplier, raw pixel angle
    glm::vec4 jitter;         // xy pixels, zw 1/size
    // The ONE wind field (world/wind), as tuning rather than shape: the wave constants are shader
    // macros (detail/wind_macros.hpp), these are the numbers --wind-speed/--no-wind move at runtime.
    glm::vec4 windDirSpeed;    // xy = horizontal direction, z = base speed, w = gust amplitude
    glm::vec4 windGustFlutter; // x = gust frequency, y = gust scroll, z = flutter Hz, w = flutter freq
    // The Gerstner field (world/water), DERIVED on the CPU and only summed on the GPU: how a wind
    // speed becomes wavelengths and amplitudes, and how the steepness budget is shared out, stays
    // in one place.
    std::array<glm::vec4, world::water::kWaveCount> waves; // xy = direction, z = amplitude, w = k
    glm::vec4 waveParams; // x = steepness Q (shared), y = goal 266's beam tile size, zw spare
    // Goal 256's cell grid. xyz = dimensions in cells, w = cell edge (metres); x <= 0 means the
    // grid is off and the region is marched as one tree -- which the shader expresses as a grid of
    // exactly one cell rather than as a separate path, so the two cannot drift.
    glm::vec4 gridDims;
    glm::vec4 gridOrigin; // xyz = world min corner of cell (0,0,0)
    std::array<detail::MaterialRecord, kMaterialCount> materials;
};
static_assert(sizeof(MarchConstantsCpu) ==
                  64 + 64 + 16 * 8 + 16 * world::water::kWaveCount + 16 + 16 * 2 + 16 * kMaterialCount,
              "must match the HLSL cbuffer exactly");

// Mirror of svo_beam.psh.hlsl's cbuffer BeamConstants -- update both together.
struct BeamConstantsCpu {
    glm::mat4 invViewProj;
    glm::vec4 camera;     // xyz camera position
    glm::vec4 treeOrigin; // xyz root min corner; w = root edge
    glm::vec4 params;     // x = tan(cone half-angle), y = tile pixels, zw = 1 / screen size
    glm::uvec4 ints;      // x = root node offset, y = tree present
};

// Mirror of svo_taa.psh.hlsl's cbuffer TaaConstants -- update both together.
struct TaaConstantsCpu {
    glm::mat4 invViewProj;
    glm::mat4 prevViewProj;
    glm::vec4 cameraPos;     // xyz + blend weight
    glm::vec4 prevCameraPos; // xyz + history valid
    glm::vec4 jitter;
    glm::vec4 params; // relative distance tolerance, absolute tolerance
};
static_assert(sizeof(TaaConstantsCpu) == 64 + 64 + 16 * 4, "must match the HLSL cbuffer exactly");

constexpr std::uint32_t kFlagShadows = 1u;
constexpr std::uint32_t kFlagLodMarch = 2u;
constexpr std::uint32_t kFlagAO = 4u;
constexpr std::uint32_t kFlagTree = 8u;
constexpr std::uint32_t kFlagSky = 16u;
constexpr std::uint32_t kFlagGrain = 32u;
constexpr std::uint32_t kViewShift = 8u;
constexpr std::uint32_t kJitterSamples = 8u;

// Halton(2,3) sub-pixel offsets in [-0.5, 0.5): the standard TAA jitter sequence (Playdead's TRAA
// used 16; 8 matches the 1/8 blend so every sample position recurs once per history length).
glm::vec2 halton_jitter(std::uint32_t index) noexcept {
    const auto radical = [](std::uint32_t i, std::uint32_t base) {
        float f = 1.0f;
        float r = 0.0f;
        for (std::uint32_t n = i + 1; n > 0; n /= base) {
            f /= static_cast<float>(base);
            r += f * static_cast<float>(n % base);
        }
        return r;
    };
    return glm::vec2{radical(index, 2) - 0.5f, radical(index, 3) - 0.5f};
}

RefCntAutoPtr<IShader> create_shader(RenderContext::Impl& rc, IShaderSourceInputStreamFactory* factory,
                                     SHADER_TYPE type, const char* file, const char* name) {
    // The material registry's shader macros (MATERIAL_COUNT, MAT_SHADING_*); the helper owns the
    // array CreateShader reads, so it lives until the call returns.
    ShaderMacroHelper macros;
    detail::add_material_macros(macros);
    detail::add_wind_macros(macros);

    ShaderCreateInfo ci;
    ci.pShaderSourceStreamFactory = factory;
    ci.FilePath = file;
    ci.EntryPoint = "main";
    ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ci.Macros = macros;
    ci.Desc.ShaderType = type;
    ci.Desc.Name = name;
    ci.Desc.UseCombinedTextureSamplers = true;
    RefCntAutoPtr<IShader> shader;
    RefCntAutoPtr<IDataBlob> output;
    rc.device->CreateShader(ci, &shader, &output);
    if (!shader) {
        std::string message = std::string("svo shader compilation failed: ") + file;
        if (output && output->GetSize() > 0) {
            message += "\n";
            message += static_cast<const char*>(output->GetConstDataPtr());
        }
        throw std::runtime_error(message);
    }
    return shader;
}

// A structured uint buffer sized for `count` words (at least one), filled later by UpdateBuffer in
// slices -- USAGE_DEFAULT, no initial data, so creating a 400 MB buffer costs no CPU copy.
RefCntAutoPtr<IBuffer> create_word_buffer(IRenderDevice* device, const char* name, std::size_t count) {
    BufferDesc desc;
    desc.Name = name;
    desc.Size = static_cast<Uint64>(count == 0 ? 1 : count) * sizeof(std::uint32_t);
    desc.Usage = USAGE_DEFAULT;
    desc.BindFlags = BIND_SHADER_RESOURCE;
    desc.Mode = BUFFER_MODE_STRUCTURED;
    desc.ElementByteStride = sizeof(std::uint32_t);
    RefCntAutoPtr<IBuffer> buffer;
    device->CreateBuffer(desc, nullptr, &buffer);
    if (!buffer) {
        throw std::runtime_error(std::string("svo buffer creation failed: ") + name);
    }
    return buffer;
}

// Goal 256: the per-cell record buffer -- a uint4 each, so the shader can hold one fetch per cell.
RefCntAutoPtr<IBuffer> create_cell_buffer(IRenderDevice* device, const char* name, std::size_t count) {
    BufferDesc desc;
    desc.Name = name;
    desc.Size = static_cast<Uint64>(count == 0 ? 1 : count) * 4u * sizeof(std::uint32_t);
    desc.Usage = USAGE_DEFAULT;
    desc.BindFlags = BIND_SHADER_RESOURCE;
    desc.Mode = BUFFER_MODE_STRUCTURED;
    desc.ElementByteStride = 4u * sizeof(std::uint32_t);
    RefCntAutoPtr<IBuffer> buffer;
    device->CreateBuffer(desc, nullptr, &buffer);
    if (!buffer) {
        throw std::runtime_error(std::string("svo cell buffer creation failed: ") + name);
    }
    return buffer;
}

RefCntAutoPtr<IBuffer> create_constant_buffer(IRenderDevice* device, const char* name, std::size_t bytes) {
    BufferDesc desc;
    desc.Name = name;
    desc.Size = static_cast<Uint64>(bytes);
    desc.Usage = USAGE_DYNAMIC;
    desc.BindFlags = BIND_UNIFORM_BUFFER;
    desc.CPUAccessFlags = CPU_ACCESS_WRITE;
    RefCntAutoPtr<IBuffer> buffer;
    device->CreateBuffer(desc, nullptr, &buffer);
    if (!buffer) {
        throw std::runtime_error(std::string("svo constants buffer creation failed: ") + name);
    }
    return buffer;
}

RefCntAutoPtr<ITexture> create_target(IRenderDevice* device, const char* name, Uint32 width, Uint32 height,
                                      TEXTURE_FORMAT format) {
    TextureDesc desc;
    desc.Name = name;
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.MipLevels = 1;
    desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RefCntAutoPtr<ITexture> tex;
    device->CreateTexture(desc, nullptr, &tex);
    if (!tex) {
        throw std::runtime_error(std::string("svo target creation failed: ") + name);
    }
    return tex;
}

// Copies up to `budgetBytes` of `words` (from word offset `done`) into `buffer`; advances `done`.
std::size_t upload_slice(IDeviceContext* ctx, IBuffer* buffer, const std::vector<std::uint32_t>& words,
                         std::size_t& done, std::size_t budgetBytes) {
    if (done >= words.size() || budgetBytes < sizeof(std::uint32_t)) {
        return 0;
    }
    const std::size_t count = std::min(words.size() - done, budgetBytes / sizeof(std::uint32_t));
    ctx->UpdateBuffer(buffer, static_cast<Uint64>(done) * sizeof(std::uint32_t),
                      static_cast<Uint64>(count) * sizeof(std::uint32_t), words.data() + done,
                      RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    done += count;
    return count * sizeof(std::uint32_t);
}

} // namespace

struct SvoRenderer::Impl {
    RenderContext* context = nullptr;
    GpuAllocationTracker tracker;
    Settings settings;

    // March pass.
    RefCntAutoPtr<IPipelineState> pso;
    RefCntAutoPtr<IShaderResourceBinding> srb;
    RefCntAutoPtr<IBuffer> constants;
    // The current tree's buffers and their capacities (words); the spare pair is the previous
    // tree's, kept for the next upload so a steady-state swap allocates nothing (goal 170: the
    // swap frame was 30-39 ms of creating two ~250 MB buffers; reuse makes it a pointer swap).
    RefCntAutoPtr<IBuffer> nodes;
    RefCntAutoPtr<IBuffer> bricks;
    // Goal 256: one uint4 per cell (node base, brick base, root offset, flags), mirroring
    // world::svo::FlatCell. A single zeroed element until a grid is uploaded, so the variable is
    // always bindable.
    RefCntAutoPtr<IBuffer> cellRecords;
    std::size_t cellRecordsCapacity = 0;
    std::size_t nodesCapacity = 0;
    std::size_t bricksCapacity = 0;
    RefCntAutoPtr<IBuffer> spareNodes;
    RefCntAutoPtr<IBuffer> spareBricks;
    std::size_t spareNodesCapacity = 0;
    std::size_t spareBricksCapacity = 0;
    std::uint64_t treeBytes = 0;
    bool hasTree = false;

    // Goal 266: the coarse start-t pre-pass. One conservative cone bound per screen tile, at
    // 1/beam_tile resolution, mirroring world::svo::beam_start_t.
    RefCntAutoPtr<IPipelineState> beamPso;
    RefCntAutoPtr<IShaderResourceBinding> beamSrb;
    RefCntAutoPtr<IBuffer> beamConstants;
    RefCntAutoPtr<ITexture> beamStart;
    Uint32 beamWidth = 0;
    Uint32 beamHeight = 0;
    int beamTile = 0; // the tile size beamStart was sized for; 0 = no target yet

    // Temporal resolve pass.
    RefCntAutoPtr<IPipelineState> taaPso;
    RefCntAutoPtr<IShaderResourceBinding> taaSrb;
    RefCntAutoPtr<IBuffer> taaConstants;
    TEXTURE_FORMAT finalFormat = TEX_FORMAT_UNKNOWN;

    // Per-size targets: the march's raw color + hit distance, and the two-frame history.
    RefCntAutoPtr<ITexture> rawColor;
    RefCntAutoPtr<ITexture> distance;
    std::array<RefCntAutoPtr<ITexture>, 2> history;
    Uint32 targetWidth = 0;
    Uint32 targetHeight = 0;
    std::uint32_t historyIndex = 0; // which history[] holds the previous frame
    bool historyValid = false;
    glm::mat4 prevViewProj{1.0f};
    glm::vec3 prevCamera{0.0f};
    std::uint32_t frameCounter = 0;

    // GPU timing. The whole march+resolve pair this renderer has always had is kept (every number
    // in research/lin-look-log.md was taken with it); goal 220's narrower per-pass ranges go
    // through the shared pools on `context`.
    std::optional<DurationQueryHelper> gpuTimer;
    double lastGpuMs = 0.0;

    // A tree being staged onto the GPU across frames (begin_upload / pump_upload).
    struct Pending {
        // Kept alive until every slice has been copied. A shared handle: the simulation holds
        // the same object for collision (goal 227), and neither owner can mutate it.
        std::shared_ptr<const world::svo::BrickTree> tree;
        // Goal 256: exactly one of these is set. The staging machinery below is identical either
        // way -- it copies two word arrays in slices -- so the only thing that branches is where
        // those arrays come from and what gets bound when the last slice lands.
        std::shared_ptr<const world::svo::FlatCellGrid> grid;
        RefCntAutoPtr<IBuffer> cells;
        std::size_t cellCount = 0;
        RefCntAutoPtr<IBuffer> nodes;
        RefCntAutoPtr<IBuffer> bricks;
        std::size_t nodesCapacity = 0;
        std::size_t bricksCapacity = 0;
        std::size_t nodesDone = 0;
        std::size_t bricksDone = 0;
        std::chrono::steady_clock::time_point start;
    };
    std::unique_ptr<Pending> pending;
    double lastUploadMs = 0.0;
    std::uint32_t lastUploadFrames = 0;
    std::uint32_t pendingFrames = 0;

    world::svo::TreeGeometry geometry;
    std::uint32_t rootOffset = 0;
    // Goal 256: the resident grid's shape, zero when the single tree is resident. gridDims.x <= 0
    // is what tells the shader to march the whole region as one cell.
    glm::vec3 gridDims{0.0f};
    glm::vec3 gridOrigin{0.0f};
    float gridCellEdge = 0.0f;
    std::chrono::steady_clock::time_point animStart = std::chrono::steady_clock::now();

    void create_pipelines();
    void bind_tree_buffers();
    void ensure_targets();
    void ensure_beam_target(int tile);
    ITextureView* final_rtv();
};

void SvoRenderer::Impl::create_pipelines() {
    auto& rc = context->impl();
    RefCntAutoPtr<IShaderSourceInputStreamFactory> factory;
    rc.factory->CreateDefaultShaderSourceStreamFactory(VOXEL_TERRAIN_SHADER_DIR, &factory);
    if (!factory) {
        throw std::runtime_error("failed to create shader source stream factory for svo");
    }
    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    finalFormat = rc.sceneColor ? rc.sceneColor->GetDesc().Format : scDesc.ColorBufferFormat;

    RefCntAutoPtr<IShader> vs =
        create_shader(rc, factory, SHADER_TYPE_VERTEX, "fullscreen.vsh.hlsl", "SVO fullscreen VS");
    {
        RefCntAutoPtr<IShader> ps =
            create_shader(rc, factory, SHADER_TYPE_PIXEL, "svo_march.psh.hlsl", "SVO march PS");

        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "SVO march PSO";
        psoCI.pVS = vs;
        psoCI.pPS = ps;
        psoCI.GraphicsPipeline.NumRenderTargets = 2;
        psoCI.GraphicsPipeline.RTVFormats[0] = finalFormat;
        psoCI.GraphicsPipeline.RTVFormats[1] = TEX_FORMAT_R32_FLOAT;
        psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        // Goal 267: the march does NOT write depth any more, and the state says so. It used to
        // write SV_Depth for every pixel "for the overlay", which cost 17% of the march on vk and
        // 10% on d3d12 -- a shader-written depth forces ordered ROP export. Nothing read it: every
        // pass after the march on this path has DepthEnable = False. The DSV stays BOUND (the
        // format below) because ImGui's PSO is created against it and the attachment has to exist.
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

        // DYNAMIC, not MUTABLE: Diligent's mutable variables accept a resource exactly once per SRB,
        // and the tree buffers are replaced on every rebuild. (First run: a MUTABLE re-bind at upload
        // was silently ignored, the shader kept marching the 1-word placeholder, and every pixel was
        // sky -- the CPU reference render at the same pose was the tell that the data, not the
        // traversal, was wrong.)
        ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_PIXEL, "MarchConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
            {SHADER_TYPE_PIXEL, "g_Nodes", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_PIXEL, "g_Bricks", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_PIXEL, "g_BeamStart", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_PIXEL, "g_Cells", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = 5;

        rc.device->CreateGraphicsPipelineState(psoCI, &pso);
        if (!pso) {
            throw std::runtime_error("svo march PSO creation failed");
        }
        constants = create_constant_buffer(rc.device, "SVO MarchConstants CB", sizeof(MarchConstantsCpu));
        if (IShaderResourceVariable* var =
                pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "MarchConstants")) {
            var->Set(constants);
        } else {
            throw std::runtime_error("svo shader variable not found: MarchConstants");
        }
        pso->CreateShaderResourceBinding(&srb, true);
        if (!srb) {
            throw std::runtime_error("svo SRB creation failed");
        }
    }
    {
        RefCntAutoPtr<IShader> ps =
            create_shader(rc, factory, SHADER_TYPE_PIXEL, "svo_beam.psh.hlsl", "SVO beam PS");

        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "SVO beam PSO";
        psoCI.pVS = vs;
        psoCI.pPS = ps;
        psoCI.GraphicsPipeline.NumRenderTargets = 1;
        psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_R32_FLOAT;
        psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN; // no depth: nothing to test or write
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

        // DYNAMIC for the same reason the march's are: the node buffer is replaced on every
        // rebuild, and a MUTABLE variable accepts a resource exactly once per SRB.
        ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_PIXEL, "BeamConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
            {SHADER_TYPE_PIXEL, "g_BeamNodes", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = 2;

        rc.device->CreateGraphicsPipelineState(psoCI, &beamPso);
        if (!beamPso) {
            throw std::runtime_error("svo beam PSO creation failed");
        }
        beamConstants = create_constant_buffer(rc.device, "SVO BeamConstants CB", sizeof(BeamConstantsCpu));
        if (IShaderResourceVariable* var =
                beamPso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "BeamConstants")) {
            var->Set(beamConstants);
        } else {
            throw std::runtime_error("svo shader variable not found: BeamConstants");
        }
        beamPso->CreateShaderResourceBinding(&beamSrb, true);
        if (!beamSrb) {
            throw std::runtime_error("svo beam SRB creation failed");
        }
    }
    {
        RefCntAutoPtr<IShader> ps =
            create_shader(rc, factory, SHADER_TYPE_PIXEL, "svo_taa.psh.hlsl", "SVO temporal resolve PS");

        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = "SVO temporal resolve PSO";
        psoCI.pVS = vs;
        psoCI.pPS = ps;
        psoCI.GraphicsPipeline.NumRenderTargets = 2;
        psoCI.GraphicsPipeline.RTVFormats[0] = finalFormat;
        psoCI.GraphicsPipeline.RTVFormats[1] = TEX_FORMAT_RGBA16_FLOAT;
        // The swap-chain depth buffer stays bound (untouched) so the overlay's PSO still sees it.
        psoCI.GraphicsPipeline.DSVFormat = scDesc.DepthBufferFormat;
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

        ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_PIXEL, "TaaConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
            {SHADER_TYPE_PIXEL, "g_RawColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_PIXEL, "g_Distance", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_PIXEL, "g_History", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = 4;
        SamplerDesc linearClamp;
        linearClamp.MinFilter = FILTER_TYPE_LINEAR;
        linearClamp.MagFilter = FILTER_TYPE_LINEAR;
        linearClamp.MipFilter = FILTER_TYPE_LINEAR;
        linearClamp.AddressU = TEXTURE_ADDRESS_CLAMP;
        linearClamp.AddressV = TEXTURE_ADDRESS_CLAMP;
        ImmutableSamplerDesc samplers[] = {{SHADER_TYPE_PIXEL, "g_History", linearClamp}};
        psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

        rc.device->CreateGraphicsPipelineState(psoCI, &taaPso);
        if (!taaPso) {
            throw std::runtime_error("svo temporal resolve PSO creation failed");
        }
        taaConstants = create_constant_buffer(rc.device, "SVO TaaConstants CB", sizeof(TaaConstantsCpu));
        if (IShaderResourceVariable* var =
                taaPso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "TaaConstants")) {
            var->Set(taaConstants);
        } else {
            throw std::runtime_error("svo shader variable not found: TaaConstants");
        }
        taaPso->CreateShaderResourceBinding(&taaSrb, true);
        if (!taaSrb) {
            throw std::runtime_error("svo temporal resolve SRB creation failed");
        }
    }

    // Bindable placeholders until the first upload (one zero word each: an empty root).
    nodes = create_word_buffer(rc.device, "SVO nodes (empty)", 0);
    bricks = create_word_buffer(rc.device, "SVO bricks (empty)", 0);
    cellRecords = create_cell_buffer(rc.device, "SVO cells (empty)", 1);
    const std::array<std::uint32_t, 4> emptyCell{};
    rc.context->UpdateBuffer(cellRecords, 0, sizeof(emptyCell), emptyCell.data(),
                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    cellRecordsCapacity = 1;
    const std::uint32_t zero = 0u;
    rc.context->UpdateBuffer(nodes, 0, sizeof(zero), &zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    rc.context->UpdateBuffer(bricks, 0, sizeof(zero), &zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    bind_tree_buffers();

    // GPU timestamps need the device feature requested at creation (device_init.cpp asks for it
    // as OPTIONAL); without it the overlay's gpu number stays 0 rather than the app failing.
    if (rc.device->GetDeviceInfo().Features.DurationQueries == DEVICE_FEATURE_STATE_ENABLED) {
        gpuTimer.emplace(rc.device, 3);
    } else {
        engine::core::log(engine::core::LogLevel::Warn,
                          "svo: duration queries unsupported on this device -- GPU frame time unavailable");
    }
}

void SvoRenderer::Impl::bind_tree_buffers() {
    IShaderResourceVariable* nodesVar = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Nodes");
    IShaderResourceVariable* bricksVar = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Bricks");
    if (nodesVar == nullptr || bricksVar == nullptr) {
        throw std::runtime_error("svo shader variables g_Nodes/g_Bricks not found");
    }
    nodesVar->Set(nodes->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    bricksVar->Set(bricks->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    // Goal 266: the beam pass walks the same nodes. Bound here rather than at draw time for the
    // same reason as above -- one rebind per tree, not one per frame.
    if (IShaderResourceVariable* cellsVar = srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Cells")) {
        cellsVar->Set(cellRecords->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    } else {
        throw std::runtime_error("svo shader variable g_Cells not found");
    }

    if (IShaderResourceVariable* beamNodes =
            beamSrb ? beamSrb->GetVariableByName(SHADER_TYPE_PIXEL, "g_BeamNodes") : nullptr) {
        beamNodes->Set(nodes->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    } else {
        throw std::runtime_error("svo shader variable g_BeamNodes not found");
    }
}

void SvoRenderer::Impl::ensure_targets() {
    auto& rc = context->impl();
    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    if (rawColor && targetWidth == scDesc.Width && targetHeight == scDesc.Height) {
        return;
    }
    targetWidth = scDesc.Width;
    targetHeight = scDesc.Height;
    rawColor = create_target(rc.device, "SVO raw color", targetWidth, targetHeight, finalFormat);
    distance = create_target(rc.device, "SVO hit distance", targetWidth, targetHeight, TEX_FORMAT_R32_FLOAT);
    history[0] =
        create_target(rc.device, "SVO history 0", targetWidth, targetHeight, TEX_FORMAT_RGBA16_FLOAT);
    history[1] =
        create_target(rc.device, "SVO history 1", targetWidth, targetHeight, TEX_FORMAT_RGBA16_FLOAT);
    historyValid = false;
    beamTile = 0; // force the beam target to be re-sized against the new dimensions
}

void SvoRenderer::Impl::ensure_beam_target(int tile) {
    if (tile <= 0) {
        return;
    }
    if (beamStart && beamTile == tile && beamWidth == (targetWidth + static_cast<Uint32>(tile) - 1u) /
                                                          static_cast<Uint32>(tile)) {
        return;
    }
    auto& rc = context->impl();
    beamWidth = std::max(1u, (targetWidth + static_cast<Uint32>(tile) - 1u) / static_cast<Uint32>(tile));
    beamHeight = std::max(1u, (targetHeight + static_cast<Uint32>(tile) - 1u) / static_cast<Uint32>(tile));
    beamStart = create_target(rc.device, "SVO beam start-t", beamWidth, beamHeight, TEX_FORMAT_R32_FLOAT);
    beamTile = tile;
}

ITextureView* SvoRenderer::Impl::final_rtv() {
    auto& rc = context->impl();
    return rc.sceneColor ? rc.sceneColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)
                         : rc.swapchain->GetCurrentBackBufferRTV();
}

SvoRenderer::SvoRenderer(RenderContext& context) : impl_(std::make_unique<Impl>()) {
    impl_->context = &context;
    impl_->create_pipelines();
}

SvoRenderer::~SvoRenderer() = default;

void SvoRenderer::begin_upload(std::shared_ptr<const world::svo::BrickTree> tree) {
    ZoneScopedN("svo begin upload");
    auto& rc = impl_->context->impl();
    auto pending = std::make_unique<Impl::Pending>();
    pending->start = std::chrono::steady_clock::now();
    // Filled by pump_upload in slices: the previous buffers stay bound and drawn until the whole
    // tree has landed, so a rebuild never shows a half-uploaded world. The spare pair (the tree
    // before the current one) is reused when it is big enough; otherwise new buffers are created
    // with headroom so the next few growths reuse them too.
    Impl& im = *impl_;
    if (im.spareNodes && im.spareBricks && im.spareNodesCapacity >= tree->nodes.size() &&
        im.spareBricksCapacity >= tree->bricks.size()) {
        pending->nodes = im.spareNodes;
        pending->bricks = im.spareBricks;
        pending->nodesCapacity = im.spareNodesCapacity;
        pending->bricksCapacity = im.spareBricksCapacity;
        im.spareNodes.Release();
        im.spareBricks.Release();
        im.spareNodesCapacity = 0;
        im.spareBricksCapacity = 0;
    } else {
        if (im.spareNodes || im.spareBricks) {
            im.tracker.on_free(static_cast<std::uint64_t>(im.spareNodesCapacity + im.spareBricksCapacity) *
                               sizeof(std::uint32_t));
            im.spareNodes.Release();
            im.spareBricks.Release();
            im.spareNodesCapacity = 0;
            im.spareBricksCapacity = 0;
        }
        pending->nodesCapacity = tree->nodes.size() + tree->nodes.size() / 4;
        pending->bricksCapacity = tree->bricks.size() + tree->bricks.size() / 4;
        pending->nodes = create_word_buffer(rc.device, "SVO nodes", pending->nodesCapacity);
        pending->bricks = create_word_buffer(rc.device, "SVO bricks", pending->bricksCapacity);
        im.tracker.on_allocate(static_cast<std::uint64_t>(pending->nodesCapacity + pending->bricksCapacity) *
                               sizeof(std::uint32_t));
    }
    pending->tree = std::move(tree);
    if (im.pending) {
        // A still-pending older tree is dropped; its buffers become the spare pair.
        im.spareNodes = im.pending->nodes;
        im.spareBricks = im.pending->bricks;
        im.spareNodesCapacity = im.pending->nodesCapacity;
        im.spareBricksCapacity = im.pending->bricksCapacity;
    }
    im.pending = std::move(pending);
    im.pendingFrames = 0;
}

void SvoRenderer::begin_upload(std::shared_ptr<const world::svo::FlatCellGrid> grid) {
    ZoneScopedN("svo begin grid upload");
    auto& rc = impl_->context->impl();
    Impl& im = *impl_;

    auto pending = std::make_unique<Impl::Pending>();
    pending->start = std::chrono::steady_clock::now();

    const std::size_t nodeWords = grid->nodes().size();
    const std::size_t brickWords = grid->bricks().size();
    if (im.spareNodes && im.spareBricks && im.spareNodesCapacity >= nodeWords &&
        im.spareBricksCapacity >= brickWords) {
        pending->nodes = im.spareNodes;
        pending->bricks = im.spareBricks;
        pending->nodesCapacity = im.spareNodesCapacity;
        pending->bricksCapacity = im.spareBricksCapacity;
        im.spareNodes.Release();
        im.spareBricks.Release();
        im.spareNodesCapacity = 0;
        im.spareBricksCapacity = 0;
    } else {
        if (im.spareNodes || im.spareBricks) {
            im.tracker.on_free(static_cast<std::uint64_t>(im.spareNodesCapacity + im.spareBricksCapacity) *
                               sizeof(std::uint32_t));
            im.spareNodes.Release();
            im.spareBricks.Release();
            im.spareNodesCapacity = 0;
            im.spareBricksCapacity = 0;
        }
        pending->nodesCapacity = nodeWords + nodeWords / 4;
        pending->bricksCapacity = brickWords + brickWords / 4;
        pending->nodes = create_word_buffer(rc.device, "SVO nodes", pending->nodesCapacity);
        pending->bricks = create_word_buffer(rc.device, "SVO bricks", pending->bricksCapacity);
        im.tracker.on_allocate(static_cast<std::uint64_t>(pending->nodesCapacity + pending->bricksCapacity) *
                               sizeof(std::uint32_t));
    }

    // The cell records go up in one shot: 4,096 cells is 64 KB, three orders of magnitude below the
    // per-frame slice budget, so slicing them would be machinery with no subject.
    pending->cellCount = grid->cells().size();
    pending->cells = create_cell_buffer(rc.device, "SVO cells", pending->cellCount);
    im.tracker.on_allocate(static_cast<std::uint64_t>(pending->cellCount) * 4u * sizeof(std::uint32_t));
    if (pending->cellCount > 0) {
        rc.context->UpdateBuffer(pending->cells, 0,
                                 static_cast<Uint64>(pending->cellCount) * 4u * sizeof(std::uint32_t),
                                 grid->cells().data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    pending->grid = std::move(grid);
    if (im.pending) {
        im.spareNodes = im.pending->nodes;
        im.spareBricks = im.pending->bricks;
        im.spareNodesCapacity = im.pending->nodesCapacity;
        im.spareBricksCapacity = im.pending->bricksCapacity;
    }
    im.pending = std::move(pending);
    im.pendingFrames = 0;
}

bool SvoRenderer::pump_upload() {
    if (!impl_->pending) {
        return false;
    }
    ZoneScopedN("svo upload slice");
    Impl::Pending& p = *impl_->pending;
    IDeviceContext* ctx = impl_->context->impl().context;
    const std::vector<std::uint32_t>& srcNodes = p.grid ? p.grid->nodes() : p.tree->nodes;
    const std::vector<std::uint32_t>& srcBricks = p.grid ? p.grid->bricks() : p.tree->bricks;
    std::size_t budget = impl_->settings.upload_bytes_per_frame;
    budget -= upload_slice(ctx, p.nodes, srcNodes, p.nodesDone, budget);
    (void)upload_slice(ctx, p.bricks, srcBricks, p.bricksDone, budget);
    ++impl_->pendingFrames;
    if (p.nodesDone < srcNodes.size() || p.bricksDone < srcBricks.size()) {
        return false;
    }

    // Complete: swap in. The buffers the previous tree lived in become the spare pair for the next
    // upload (Diligent keeps them valid for the in-flight frames that still read them either
    // way). The tracker counts buffer CAPACITY resident on the GPU: current + spare + pending.
    if (impl_->hasTree) {
        impl_->spareNodes = impl_->nodes;
        impl_->spareBricks = impl_->bricks;
        impl_->spareNodesCapacity = impl_->nodesCapacity;
        impl_->spareBricksCapacity = impl_->bricksCapacity;
    }
    impl_->nodes = p.nodes;
    impl_->bricks = p.bricks;
    impl_->nodesCapacity = p.nodesCapacity;
    impl_->bricksCapacity = p.bricksCapacity;
    if (p.grid) {
        const world::svo::CellGrid& g = p.grid->grid();
        impl_->cellRecords = p.cells;
        impl_->cellRecordsCapacity = p.cellCount;
        impl_->treeBytes = p.grid->memory_bytes();
        // The geometry a cell is built with -- root edge and voxel size -- so the shader's V and the
        // finest-voxel constant are the CELL's, not the region's.
        impl_->geometry = g.geometry_for(g.origin_cell());
        impl_->rootOffset = 0;
        impl_->gridDims = glm::vec3{g.dims()};
        impl_->gridOrigin = g.world_min();
        impl_->gridCellEdge = g.cell_edge();
        impl_->hasTree = g.present_count() > 0;
    } else {
        impl_->treeBytes = static_cast<std::uint64_t>(p.tree->memory_bytes());
        impl_->geometry = p.tree->geometry;
        impl_->rootOffset = p.tree->root;
        impl_->gridDims = glm::vec3{0.0f};
        impl_->gridCellEdge = 0.0f;
        impl_->hasTree = !p.tree->empty();
    }
    impl_->bind_tree_buffers();
    impl_->lastUploadMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - p.start).count();
    impl_->lastUploadFrames = impl_->pendingFrames;
    impl_->pending.reset();
    return true;
}

bool SvoRenderer::upload_pending() const noexcept {
    return impl_->pending != nullptr;
}

double SvoRenderer::last_upload_ms() const noexcept {
    return impl_->lastUploadMs;
}

std::uint32_t SvoRenderer::last_upload_frames() const noexcept {
    return impl_->lastUploadFrames;
}

double SvoRenderer::last_gpu_ms() const noexcept {
    return impl_->lastGpuMs;
}

void SvoRenderer::set_settings(const Settings& settings) noexcept {
    impl_->settings = settings;
}

const SvoRenderer::Settings& SvoRenderer::settings() const noexcept {
    return impl_->settings;
}

void SvoRenderer::render(const render::interface::Camera& camera) {
    ZoneScopedN("svo render");
    auto& rc = impl_->context->impl();
    IDeviceContext* ctx = rc.context;
    impl_->ensure_targets();
    const Settings& s = impl_->settings;
    const bool taa = s.taa && s.debug_view == SvoDebugView::None;

    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    const float aspect =
        scDesc.Height > 0 ? static_cast<float>(scDesc.Width) / static_cast<float>(scDesc.Height) : 1.0f;
    const glm::mat4 viewProj = projection_matrix(camera, aspect) * view_matrix(camera);
    const glm::mat4 invViewProj = glm::inverse(viewProj);
    const glm::vec2 jitterPx = taa ? halton_jitter(impl_->frameCounter % kJitterSamples) : glm::vec2{0.0f};
    const glm::vec4 jitter{jitterPx.x, jitterPx.y,
                           1.0f / static_cast<float>(std::max<Uint32>(scDesc.Width, 1)),
                           1.0f / static_cast<float>(std::max<Uint32>(scDesc.Height, 1))};
    // One pixel's angular size at the image center; the LOD test uses it scaled by the quality knob.
    const float rawPixelAngle =
        2.0f * std::tan(camera.fov_y_radians * 0.5f) / static_cast<float>(std::max<Uint32>(scDesc.Height, 1));

    // Pass 0 (goal 266): the coarse start-t pre-pass. One conservative cone bound per screen tile,
    // at 1/tile resolution, from which every primary ray in that tile starts. Measured on the CPU
    // reference at 35-53% of primary traversal steps with ZERO pixels changed; the shader is a
    // mirror of world::svo::beam_start_t, whose tests carry the proof that the bound is safe.
    const int beamTile = std::max(0, s.beam_tile);
    const bool beam = beamTile > 0 && impl_->hasTree;
    impl_->ensure_beam_target(beam ? beamTile : 16);
    if (beam) {
        ZoneScopedN("svo beam");
        const GpuPassScope beamScope(*impl_->context, GpuPass::Beam);
        ITextureView* beamRtv = impl_->beamStart->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        ctx->SetRenderTargets(1, &beamRtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        {
            MapHelper<BeamConstantsCpu> cb(ctx, impl_->beamConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            cb->invViewProj = invViewProj;
            cb->camera = glm::vec4(camera.position, 0.0f);
            cb->treeOrigin = glm::vec4(impl_->geometry.origin, impl_->geometry.root_edge());
            // The cone must contain the whole tile: the half-diagonal of `tile` pixels plus a
            // one-pixel margin (footprint + TAA jitter). Same formula as
            // world::svo::tile_tan_half_angle, which the CPU measurement used and whose header
            // carries the reason the margin is exactly this size.
            const float halfSpan = 0.5f * static_cast<float>(beamTile) * rawPixelAngle;
            const float diagonal = std::sqrt(2.0f * halfSpan * halfSpan) + rawPixelAngle;
            cb->params = glm::vec4(std::tan(diagonal), static_cast<float>(beamTile),
                                   1.0f / static_cast<float>(std::max<Uint32>(scDesc.Width, 1)),
                                   1.0f / static_cast<float>(std::max<Uint32>(scDesc.Height, 1)));
            cb->ints = glm::uvec4(impl_->rootOffset, impl_->hasTree ? 1u : 0u, 0u, 0u);
        }
        ctx->SetPipelineState(impl_->beamPso);
        ctx->CommitShaderResources(impl_->beamSrb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1});
    } else {
        // Zero means "start at the root entry", so a cleared target is exactly the old
        // behaviour -- the march needs no branch and the A/B is a real one.
        ITextureView* beamRtv = impl_->beamStart->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        const float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        ctx->SetRenderTargets(1, &beamRtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->ClearRenderTarget(beamRtv, zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // Pass 1: the march, into the raw target (temporal on) or straight into the final target.
    ITextureView* finalRtv = impl_->final_rtv();
    ITextureView* dsv = rc.swapchain->GetDepthBufferDSV();
    const bool timeThisFrame = impl_->gpuTimer && impl_->frameCounter >= 2;
    {
        ITextureView* rtvs[2] = {taa ? impl_->rawColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) : finalRtv,
                                 impl_->distance->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};
        ctx->SetRenderTargets(2, rtvs, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const float clear[4] = {0.25f, 0.5f, 0.8f, 1.0f};
        const float farClear[4] = {1.0e6f, 0.0f, 0.0f, 0.0f};
        ctx->ClearRenderTarget(rtvs[0], clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->ClearRenderTarget(rtvs[1], farClear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        // Timestamps bracket the draws only, like Diligent's own Tutorial18 (Queries), and never
        // on the first frames: with no post-processor warm-up ahead of it, a timestamp as the
        // app's very first Vulkan command faulted inside the NVIDIA driver (vkCmdWriteTimestamp
        // reading a null); D3D12 never cared. Two presents later the query pools have been reset
        // once and it is fine.
        if (timeThisFrame) {
            impl_->gpuTimer->Begin(ctx);
        }

        const float animSeconds = anim_seconds();
        MapHelper<MarchConstantsCpu> cb(ctx, impl_->constants, MAP_WRITE, MAP_FLAG_DISCARD);
        cb->invViewProj = invViewProj;
        cb->viewProj = viewProj;
        cb->cameraPosWorld = glm::vec4(camera.position, animSeconds);
        const world::svo::TreeGeometry& g = impl_->geometry;
        cb->treeOrigin = glm::vec4(g.origin, g.root_edge());
        cb->treeParams =
            glm::vec4(rawPixelAngle * s.lod_quality, s.shadow_lod, g.finest_voxel_edge(), s.ao_radius_px);
        std::uint32_t flags = 0;
        flags |= s.shadows ? kFlagShadows : 0u;
        flags |= s.lod_march ? kFlagLodMarch : 0u;
        flags |= s.ao ? kFlagAO : 0u;
        flags |= impl_->hasTree ? kFlagTree : 0u;
        flags |= s.sky ? kFlagSky : 0u;
        flags |= s.grain ? kFlagGrain : 0u;
        flags |= static_cast<std::uint32_t>(s.debug_view) << kViewShift;
        cb->treeInts = glm::uvec4(static_cast<std::uint32_t>(g.voxel_bits()),
                                  static_cast<std::uint32_t>(g.max_brick_level()), impl_->rootOffset, flags);
        cb->shadeParams = glm::vec4(s.smooth_pixels, s.grain_amplitude, s.ao_lod, rawPixelAngle);
        cb->jitter = jitter;
        const glm::vec3 windDir = world::wind::wind_direction(s.wind);
        cb->windDirSpeed = glm::vec4(windDir.x, windDir.z, s.wind.base_speed, s.wind.gust_amplitude);
        cb->windGustFlutter =
            glm::vec4(s.wind.gust_frequency, s.wind.gust_scroll, s.wind.flutter_hz, s.wind.flutter_frequency);
        // Wind and waves share one field, which is the physically right coupling and also why
        // --no-wind gives glass: make_wave_field returns zero amplitudes for a zero base speed.
        const world::water::WaveField waveField = world::water::make_wave_field(s.wind);
        for (std::size_t i = 0; i < world::water::kWaveCount; ++i) {
            const world::water::GerstnerWave& w = waveField.waves[i];
            cb->waves[i] = glm::vec4(w.direction.x, w.direction.y, w.amplitude, w.wavenumber);
        }
        // .y is goal 266's beam tile size (0 = no seed), taking one of the spare slots this
        // vector was reserved with rather than growing the cbuffer for a single float.
        cb->gridDims = glm::vec4(impl_->gridDims, impl_->gridCellEdge);
        cb->gridOrigin = glm::vec4(impl_->gridOrigin, 0.0f);
        cb->waveParams = glm::vec4(waveField.waves[0].steepness,
                                   beam ? static_cast<float>(beamTile) : 0.0f, 0.0f, 0.0f);
        cb->materials = detail::kMaterialRecords;
    }
    {
        // Goal 220: the march range. The Begin above is the whole march+resolve pair this
        // renderer has always had (kept, because every number in research/lin-look-log.md was
        // taken with it); THIS is the narrower one that says how much of it is the march.
        const GpuPassScope marchScope(*impl_->context, GpuPass::March);
        impl_->srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_BeamStart")
            ->Set(impl_->beamStart->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        ctx->SetPipelineState(impl_->pso);
        ctx->CommitShaderResources(impl_->srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1});
    }

    // Pass 2: temporal resolve into the final target + the next frame's history.
    if (taa) {
        ZoneScopedN("svo taa");
        const GpuPassScope resolveScope(*impl_->context, GpuPass::Resolve);
        const std::uint32_t prev = impl_->historyIndex;
        const std::uint32_t cur = prev ^ 1u;
        ITextureView* rtvs[2] = {finalRtv, impl_->history[cur]->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};
        ctx->SetRenderTargets(2, rtvs, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        {
            MapHelper<TaaConstantsCpu> cb(ctx, impl_->taaConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            cb->invViewProj = invViewProj;
            cb->prevViewProj = impl_->prevViewProj;
            cb->cameraPos = glm::vec4(camera.position, s.taa_blend);
            cb->prevCameraPos = glm::vec4(impl_->prevCamera, impl_->historyValid ? 1.0f : 0.0f);
            cb->jitter = jitter;
            // A reprojected sample survives when the previous frame saw within 2% (+5 cm) of the
            // distance it should have: LOD cubes shift a hit by less, real disocclusions by more.
            cb->params = glm::vec4(0.02f, 0.05f, 0.0f, 0.0f);
        }
        impl_->taaSrb->GetVariableByName(SHADER_TYPE_PIXEL, "g_RawColor")
            ->Set(impl_->rawColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        impl_->taaSrb->GetVariableByName(SHADER_TYPE_PIXEL, "g_Distance")
            ->Set(impl_->distance->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        impl_->taaSrb->GetVariableByName(SHADER_TYPE_PIXEL, "g_History")
            ->Set(impl_->history[prev]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        ctx->SetPipelineState(impl_->taaPso);
        ctx->CommitShaderResources(impl_->taaSrb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1});
        impl_->historyIndex = cur;
        impl_->historyValid = true;
    } else {
        impl_->historyValid = false;
    }
    impl_->prevViewProj = viewProj;
    impl_->prevCamera = camera.position;
    ++impl_->frameCounter;

    if (timeThisFrame) {
        double seconds = 0.0;
        if (impl_->gpuTimer->End(ctx, seconds)) {
            impl_->lastGpuMs = seconds * 1000.0;
        }
    }
}

const GpuAllocationTracker& SvoRenderer::gpu_memory() const noexcept {
    return impl_->tracker;
}

bool SvoRenderer::has_tree() const noexcept {
    return impl_->hasTree;
}

float SvoRenderer::anim_seconds() const noexcept {
    return std::chrono::duration<float>(std::chrono::steady_clock::now() - impl_->animStart).count();
}

} // namespace render::diligent
