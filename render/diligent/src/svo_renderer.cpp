#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "render/diligent/svo_renderer.hpp"

#include "engine/core/log.hpp"
#include "world/chunk/block_type.hpp"
#include "world/chunk/material.hpp"

#include "detail/render_context_impl.hpp"

#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#if defined(TRACY_ENABLE)
#include <tracy/Tracy.hpp>
#else
#define ZoneScopedN(name)
#endif

namespace render::diligent {

using namespace Diligent;

namespace {

using world::chunk::kBlockTable;
using world::chunk::kMaterialCount;
static_assert(kMaterialCount == 8, "svo_march.psh.hlsl declares g_MaterialColors[8] -- update both together");

// Mirror of svo_march.psh.hlsl's cbuffer MarchConstants -- update both together.
struct MarchConstantsCpu {
    glm::mat4 invViewProj;
    glm::mat4 viewProj;
    glm::vec4 cameraPosWorld; // xyz + time
    glm::vec4 treeOrigin;     // xyz + root edge
    glm::vec4 treeParams;     // lod pixel angle, shadow lod multiplier, finest voxel edge, AO ray length
    glm::uvec4 treeInts;      // V, max brick level, root offset, flags
    std::array<std::array<float, 4>, kMaterialCount> materialColors;
};
static_assert(sizeof(MarchConstantsCpu) == 64 + 64 + 16 * 4 + 16 * 8, "must match the HLSL cbuffer exactly");

constexpr std::uint32_t kFlagShadows = 1u;
constexpr std::uint32_t kFlagLodMarch = 2u;
constexpr std::uint32_t kFlagAO = 4u;
constexpr std::uint32_t kFlagTree = 8u;
constexpr std::uint32_t kFlagSky = 16u;

RefCntAutoPtr<IShader> create_shader(RenderContext::Impl& rc, IShaderSourceInputStreamFactory* factory,
                                     SHADER_TYPE type, const char* file, const char* name) {
    ShaderCreateInfo ci;
    ci.pShaderSourceStreamFactory = factory;
    ci.FilePath = file;
    ci.EntryPoint = "main";
    ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
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

    RefCntAutoPtr<IPipelineState> pso;
    RefCntAutoPtr<IShaderResourceBinding> srb;
    RefCntAutoPtr<IBuffer> constants;
    RefCntAutoPtr<IBuffer> nodes;
    RefCntAutoPtr<IBuffer> bricks;
    std::uint64_t treeBytes = 0;
    bool hasTree = false;

    // A tree being staged onto the GPU across frames (begin_upload / pump_upload).
    struct Pending {
        world::svo::BrickTree tree; // kept alive until every slice has been copied
        RefCntAutoPtr<IBuffer> nodes;
        RefCntAutoPtr<IBuffer> bricks;
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

    void create_pipeline();
    void bind_tree_buffers();
};

void SvoRenderer::Impl::create_pipeline() {
    auto& rc = context->impl();
    RefCntAutoPtr<IShaderSourceInputStreamFactory> factory;
    rc.factory->CreateDefaultShaderSourceStreamFactory(VOXEL_TERRAIN_SHADER_DIR, &factory);
    if (!factory) {
        throw std::runtime_error("failed to create shader source stream factory for svo");
    }
    RefCntAutoPtr<IShader> vs =
        create_shader(rc, factory, SHADER_TYPE_VERTEX, "fullscreen.vsh.hlsl", "SVO march VS");
    RefCntAutoPtr<IShader> ps =
        create_shader(rc, factory, SHADER_TYPE_PIXEL, "svo_march.psh.hlsl", "SVO march PS");

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "SVO march PSO";
    psoCI.pVS = vs;
    psoCI.pPS = ps;
    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] =
        rc.sceneColor ? rc.sceneColor->GetDesc().Format : scDesc.ColorBufferFormat;
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

    BufferDesc cbDesc;
    cbDesc.Name = "SVO MarchConstants CB";
    cbDesc.Size = sizeof(MarchConstantsCpu);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    rc.device->CreateBuffer(cbDesc, nullptr, &constants);
    if (!constants) {
        throw std::runtime_error("svo constants buffer creation failed");
    }
    if (IShaderResourceVariable* var = pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "MarchConstants")) {
        var->Set(constants);
    } else {
        throw std::runtime_error("svo shader variable not found: MarchConstants");
    }
    pso->CreateShaderResourceBinding(&srb, true);
    if (!srb) {
        throw std::runtime_error("svo SRB creation failed");
    }

    // Bindable placeholders until the first upload (one zero word each: an empty root).
    nodes = create_word_buffer(rc.device, "SVO nodes (empty)", 0);
    bricks = create_word_buffer(rc.device, "SVO bricks (empty)", 0);
    const std::uint32_t zero = 0u;
    rc.context->UpdateBuffer(nodes, 0, sizeof(zero), &zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    rc.context->UpdateBuffer(bricks, 0, sizeof(zero), &zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    bind_tree_buffers();
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

SvoRenderer::SvoRenderer(RenderContext& context) : impl_(std::make_unique<Impl>()) {
    impl_->context = &context;
    impl_->create_pipeline();
}

SvoRenderer::~SvoRenderer() = default;

void SvoRenderer::begin_upload(world::svo::BrickTree tree) {
    ZoneScopedN("svo begin upload");
    auto& rc = impl_->context->impl();
    auto pending = std::make_unique<Impl::Pending>();
    pending->start = std::chrono::steady_clock::now();
    // Sized now, filled by pump_upload in slices: the previous buffers stay bound and drawn until
    // the whole tree has landed, so a rebuild never shows a half-uploaded world.
    pending->nodes = create_word_buffer(rc.device, "SVO nodes", tree.nodes.size());
    pending->bricks = create_word_buffer(rc.device, "SVO bricks", tree.bricks.size());
    pending->tree = std::move(tree);
    impl_->pending = std::move(pending); // a still-pending older tree is simply dropped
    impl_->pendingFrames = 0;
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

    // Complete: swap in. Diligent keeps the previous buffers alive until the frames referencing
    // them retire, so an in-flight frame never sees a freed buffer.
    if (impl_->treeBytes > 0) {
        impl_->tracker.on_free(impl_->treeBytes);
    }
    impl_->nodes = p.nodes;
    impl_->bricks = p.bricks;
    impl_->treeBytes = static_cast<std::uint64_t>(p.tree.memory_bytes());
    impl_->tracker.on_allocate(impl_->treeBytes);
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

    ITextureView* rtv = rc.sceneColor ? rc.sceneColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)
                                      : rc.swapchain->GetCurrentBackBufferRTV();
    ITextureView* dsv = rc.swapchain->GetDepthBufferDSV();
    ctx->SetRenderTargets(1, &rtv, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const float clear[4] = {0.25f, 0.5f, 0.8f, 1.0f};
    ctx->ClearRenderTarget(rtv, clear, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->ClearDepthStencil(dsv, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    const float aspect =
        scDesc.Height > 0 ? static_cast<float>(scDesc.Width) / static_cast<float>(scDesc.Height) : 1.0f;
    const glm::mat4 viewProj = projection_matrix(camera, aspect) * view_matrix(camera);
    const float animSeconds =
        std::chrono::duration<float>(std::chrono::steady_clock::now() - impl_->animStart).count();
    const Settings& s = impl_->settings;
    {
        MapHelper<MarchConstantsCpu> cb(ctx, impl_->constants, MAP_WRITE, MAP_FLAG_DISCARD);
        cb->invViewProj = glm::inverse(viewProj);
        cb->viewProj = viewProj;
        cb->cameraPosWorld = glm::vec4(camera.position, animSeconds);
        const world::svo::TreeGeometry& g = impl_->geometry;
        cb->treeOrigin = glm::vec4(g.origin, g.root_edge());
        // One pixel's angular size at the image center, scaled by the quality knob.
        const float pixelAngle = 2.0f * std::tan(camera.fov_y_radians * 0.5f) /
                                 static_cast<float>(std::max<Uint32>(scDesc.Height, 1)) * s.lod_quality;
        cb->treeParams = glm::vec4(pixelAngle, s.shadow_lod, g.finest_voxel_edge(), s.ao_ray_length);
        std::uint32_t flags = 0;
        flags |= s.shadows ? kFlagShadows : 0u;
        flags |= s.lod_march ? kFlagLodMarch : 0u;
        flags |= s.ao ? kFlagAO : 0u;
        flags |= impl_->hasTree ? kFlagTree : 0u;
        flags |= s.sky ? kFlagSky : 0u;
        cb->treeInts = glm::uvec4(static_cast<std::uint32_t>(g.voxel_bits()),
                                  static_cast<std::uint32_t>(g.max_brick_level()), impl_->rootOffset, flags);
        for (std::size_t i = 0; i < kMaterialCount; ++i) {
            const auto& c = kBlockTable[i].color;
            cb->materialColors[i] = {c[0], c[1], c[2], 1.0f};
        }
    }

    ctx->SetPipelineState(impl_->pso);
    ctx->CommitShaderResources(impl_->srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1});
}

const GpuAllocationTracker& SvoRenderer::gpu_memory() const noexcept {
    return impl_->tracker;
}

bool SvoRenderer::has_tree() const noexcept {
    return impl_->hasTree;
}

} // namespace render::diligent
