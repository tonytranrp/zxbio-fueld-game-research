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

#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsTools/interface/DurationQueryHelper.hpp"
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
    std::array<detail::MaterialRecord, kMaterialCount> materials;
};
static_assert(sizeof(MarchConstantsCpu) == 64 + 64 + 16 * 6 + 16 * kMaterialCount,
              "must match the HLSL cbuffer exactly");

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
    std::size_t nodesCapacity = 0;
    std::size_t bricksCapacity = 0;
    RefCntAutoPtr<IBuffer> spareNodes;
    RefCntAutoPtr<IBuffer> spareBricks;
    std::size_t spareNodesCapacity = 0;
    std::size_t spareBricksCapacity = 0;
    std::uint64_t treeBytes = 0;
    bool hasTree = false;

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

    // GPU timing.
    std::optional<DurationQueryHelper> gpuTimer;
    double lastGpuMs = 0.0;

    // A tree being staged onto the GPU across frames (begin_upload / pump_upload).
    struct Pending {
        world::svo::BrickTree tree; // kept alive until every slice has been copied
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
    std::chrono::steady_clock::time_point animStart = std::chrono::steady_clock::now();

    void create_pipelines();
    void bind_tree_buffers();
    void ensure_targets();
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
        // The pass writes SV_Depth for every pixel (hits and far-plane sky) into a freshly cleared
        // depth buffer, so the test is ALWAYS; what matters is that the WRITE lands, for the overlay.
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_ALWAYS;

        // DYNAMIC, not MUTABLE: Diligent's mutable variables accept a resource exactly once per SRB,
        // and the tree buffers are replaced on every rebuild. (First run: a MUTABLE re-bind at upload
        // was silently ignored, the shader kept marching the 1-word placeholder, and every pixel was
        // sky -- the CPU reference render at the same pose was the tell that the data, not the
        // traversal, was wrong.)
        ShaderResourceVariableDesc vars[] = {
            {SHADER_TYPE_PIXEL, "MarchConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
            {SHADER_TYPE_PIXEL, "g_Nodes", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_PIXEL, "g_Bricks", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };
        psoCI.PSODesc.ResourceLayout.Variables = vars;
        psoCI.PSODesc.ResourceLayout.NumVariables = 3;

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

void SvoRenderer::begin_upload(world::svo::BrickTree tree) {
    ZoneScopedN("svo begin upload");
    auto& rc = impl_->context->impl();
    auto pending = std::make_unique<Impl::Pending>();
    pending->start = std::chrono::steady_clock::now();
    // Filled by pump_upload in slices: the previous buffers stay bound and drawn until the whole
    // tree has landed, so a rebuild never shows a half-uploaded world. The spare pair (the tree
    // before the current one) is reused when it is big enough; otherwise new buffers are created
    // with headroom so the next few growths reuse them too.
    Impl& im = *impl_;
    if (im.spareNodes && im.spareBricks && im.spareNodesCapacity >= tree.nodes.size() &&
        im.spareBricksCapacity >= tree.bricks.size()) {
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
        pending->nodesCapacity = tree.nodes.size() + tree.nodes.size() / 4;
        pending->bricksCapacity = tree.bricks.size() + tree.bricks.size() / 4;
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

bool SvoRenderer::pump_upload() {
    if (!impl_->pending) {
        return false;
    }
    ZoneScopedN("svo upload slice");
    Impl::Pending& p = *impl_->pending;
    IDeviceContext* ctx = impl_->context->impl().context;
    std::size_t budget = impl_->settings.upload_bytes_per_frame;
    budget -= upload_slice(ctx, p.nodes, p.tree.nodes, p.nodesDone, budget);
    (void)upload_slice(ctx, p.bricks, p.tree.bricks, p.bricksDone, budget);
    ++impl_->pendingFrames;
    if (p.nodesDone < p.tree.nodes.size() || p.bricksDone < p.tree.bricks.size()) {
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
    impl_->treeBytes = static_cast<std::uint64_t>(p.tree.memory_bytes());
    impl_->geometry = p.tree.geometry;
    impl_->rootOffset = p.tree.root;
    impl_->hasTree = !p.tree.empty();
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

        const float animSeconds =
            std::chrono::duration<float>(std::chrono::steady_clock::now() - impl_->animStart).count();
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
        cb->materials = detail::kMaterialRecords;
    }
    ctx->SetPipelineState(impl_->pso);
    ctx->CommitShaderResources(impl_->srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1});

    // Pass 2: temporal resolve into the final target + the next frame's history.
    if (taa) {
        ZoneScopedN("svo taa");
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

} // namespace render::diligent
