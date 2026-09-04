#include <array>
#include <memory>
#include <stdexcept>

#include "render/diligent/post_process.hpp"

#include "engine/core/log.hpp"

#include "detail/render_context_impl.hpp"

#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

// DiligentFX Bloom + the shared post-FX context. The HLSL-mirror struct comes in through the
// same include convention DiligentFX's own samples use.
#include "PostProcess/Bloom/interface/Bloom.hpp"
#include "PostProcess/Common/interface/PostFXContext.hpp"

namespace Diligent::HLSL {
#include "Shaders/Common/public/ShaderDefinitions.fxh"
#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PostProcess/Bloom/public/BloomStructures.fxh"
} // namespace Diligent::HLSL

namespace render::diligent {

using namespace Diligent;

namespace {

// Mirror of composite.psh.hlsl's cbuffer CompositeConstants -- update both together.
struct CompositeConstantsCpu {
    float tonemapEnabled = 1.0f;
    float pad0 = 0.0f;
    float pad1 = 0.0f;
    float pad2 = 0.0f;
};
static_assert(sizeof(CompositeConstantsCpu) == 16, "must match the 16-byte HLSL cbuffer");

} // namespace

struct PostProcessor::Impl {
    RenderContext* context = nullptr;

    std::unique_ptr<PostFXContext> postfx;
    std::unique_ptr<Bloom> bloom;

    RefCntAutoPtr<IPipelineState> compositePso;
    RefCntAutoPtr<IShaderResourceBinding> compositeSrb;
    RefCntAutoPtr<IBuffer> compositeConstants;

    std::uint32_t targetWidth = 0;
    std::uint32_t targetHeight = 0;

    bool bloomEnabled = true;
    bool tonemapEnabled = true;

    void ensure_scene_target();
    void create_composite_pipeline();
    void composite(ITextureView* sourceSRV);
};

void PostProcessor::Impl::ensure_scene_target() {
    auto& rc = context->impl();
    const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
    if (rc.sceneColor && targetWidth == scDesc.Width && targetHeight == scDesc.Height) {
        return;
    }
    targetWidth = scDesc.Width;
    targetHeight = scDesc.Height;

    TextureDesc desc;
    desc.Name = "PostProcess scene color (HDR)";
    desc.Type = RESOURCE_DIM_TEX_2D;
    desc.Width = targetWidth;
    desc.Height = targetHeight;
    // 16F: the scene pass writes real HDR-ish values and Bloom's bright-pass needs the headroom
    // above 1.0 to have anything to bloom on.
    desc.Format = TEX_FORMAT_RGBA16_FLOAT;
    desc.MipLevels = 1;
    desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

    rc.sceneColor.Release();
    rc.device->CreateTexture(desc, nullptr, &rc.sceneColor);
    if (!rc.sceneColor) {
        throw std::runtime_error("post-process: scene color target creation failed");
    }
    // The composite SRB binds the scene SRV; rebind on target recreation.
    if (compositeSrb) {
        compositeSrb.Release();
        compositePso->CreateShaderResourceBinding(&compositeSrb, true);
    }
}

void PostProcessor::Impl::create_composite_pipeline() {
    auto& rc = context->impl();

    RefCntAutoPtr<IShaderSourceInputStreamFactory> streamFactory;
    rc.factory->CreateDefaultShaderSourceStreamFactory(VOXEL_TERRAIN_SHADER_DIR, &streamFactory);

    auto createShader = [&](SHADER_TYPE type, const char* file, const char* name) {
        ShaderCreateInfo ci;
        ci.pShaderSourceStreamFactory = streamFactory;
        ci.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ci.Desc.ShaderType = type;
        ci.Desc.Name = name;
        ci.Desc.UseCombinedTextureSamplers = true;
        ci.EntryPoint = "main";
        ci.FilePath = file;
        RefCntAutoPtr<IShader> shader;
        rc.device->CreateShader(ci, &shader);
        if (!shader) {
            throw std::runtime_error(std::string("post-process: shader creation failed: ") + file);
        }
        return shader;
    };

    RefCntAutoPtr<IShader> vs = createShader(SHADER_TYPE_VERTEX, "fullscreen.vsh.hlsl", "composite VS");
    RefCntAutoPtr<IShader> ps = createShader(SHADER_TYPE_PIXEL, "composite.psh.hlsl", "composite PS");

    GraphicsPipelineStateCreateInfo psoCI;
    psoCI.PSODesc.Name = "composite PSO";
    psoCI.pVS = vs;
    psoCI.pPS = ps;
    psoCI.GraphicsPipeline.NumRenderTargets = 1;
    psoCI.GraphicsPipeline.RTVFormats[0] = rc.swapchain->GetDesc().ColorBufferFormat;
    // The swap-chain depth buffer STAYS BOUND through this pass (depth test/write disabled) so
    // the overlay's ImGui PSO -- created against the swap-chain DSV format -- still sees the
    // depth attachment it expects afterwards.
    psoCI.GraphicsPipeline.DSVFormat = rc.swapchain->GetDesc().DepthBufferFormat;
    psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

    ShaderResourceVariableDesc vars[] = {
        {SHADER_TYPE_PIXEL, "g_SourceColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL, "CompositeConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
    };
    psoCI.PSODesc.ResourceLayout.Variables = vars;
    psoCI.PSODesc.ResourceLayout.NumVariables = 2;

    SamplerDesc linearClamp;
    linearClamp.MinFilter = FILTER_TYPE_LINEAR;
    linearClamp.MagFilter = FILTER_TYPE_LINEAR;
    linearClamp.MipFilter = FILTER_TYPE_LINEAR;
    linearClamp.AddressU = TEXTURE_ADDRESS_CLAMP;
    linearClamp.AddressV = TEXTURE_ADDRESS_CLAMP;
    ImmutableSamplerDesc samplers[] = {{SHADER_TYPE_PIXEL, "g_SourceColor", linearClamp}};
    psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
    psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

    rc.device->CreateGraphicsPipelineState(psoCI, &compositePso);
    if (!compositePso) {
        throw std::runtime_error("post-process: composite PSO creation failed");
    }

    BufferDesc cbDesc;
    cbDesc.Name = "CompositeConstants";
    cbDesc.Size = sizeof(CompositeConstantsCpu);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    rc.device->CreateBuffer(cbDesc, nullptr, &compositeConstants);
    if (!compositeConstants) {
        throw std::runtime_error("post-process: composite constants creation failed");
    }
    compositePso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "CompositeConstants")->Set(compositeConstants);
    compositePso->CreateShaderResourceBinding(&compositeSrb, true);
}

void PostProcessor::Impl::composite(ITextureView* sourceSRV) {
    auto& rc = context->impl();
    IDeviceContext* ctx = rc.context;

    {
        MapHelper<CompositeConstantsCpu> constants(ctx, compositeConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        constants->tonemapEnabled = tonemapEnabled ? 1.0f : 0.0f;
    }

    ITextureView* rtv = rc.swapchain->GetCurrentBackBufferRTV();
    ITextureView* dsv = rc.swapchain->GetDepthBufferDSV();
    ctx->SetRenderTargets(1, &rtv, dsv, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->SetPipelineState(compositePso);
    compositeSrb->GetVariableByName(SHADER_TYPE_PIXEL, "g_SourceColor")->Set(sourceSRV);
    ctx->CommitShaderResources(compositeSrb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1});
}

PostProcessor::PostProcessor(RenderContext& context) : impl_(std::make_unique<Impl>()) {
    impl_->context = &context;
    auto& rc = context.impl();

    PostFXContext::CreateInfo postfxCI;
    impl_->postfx = std::make_unique<PostFXContext>(rc.device, postfxCI);

    Bloom::CreateInfo bloomCI; // sync shader creation: first-use hitch over a black first frame
    impl_->bloom = std::make_unique<Bloom>(rc.device, bloomCI);

    impl_->create_composite_pipeline();
    // Eager: registers the scene target on the shared impl BEFORE TerrainRenderer's constructor
    // creates its PSO, which reads the target's format (the app constructs in that order).
    impl_->ensure_scene_target();

    // ONE warm-up PostFXContext::Execute, then never again. Found empirically (bloom stayed
    // PENDING forever): Bloom::Execute gates on pPostFXContext->IsPSOsReady(), and that flag is
    // set ONLY inside PostFXContext::Execute -- which also DEV_CHECKs depth/motion-vector/camera
    // inputs Bloom never actually consumes, and runs four full-resolution helper passes per call
    // (blue noise, depth reprojection, closest motion, previous depth). Feeding it tiny dummy
    // inputs ONCE flips the readiness flag (nothing ever resets it -- verified in
    // PostFXContext.cpp), unlocking Bloom without paying for machinery only SSAO/SSR/TAA would
    // use. If Group F's gate ever approves SSAO, this becomes a real per-frame Execute with real
    // depth + zeroed motion vectors instead.
    {
        auto makeDummy = [&](TEXTURE_FORMAT format, const char* name) {
            TextureDesc desc;
            desc.Name = name;
            desc.Type = RESOURCE_DIM_TEX_2D;
            desc.Width = 2;
            desc.Height = 2;
            desc.Format = format;
            desc.MipLevels = 1;
            desc.BindFlags = BIND_SHADER_RESOURCE;
            RefCntAutoPtr<ITexture> tex;
            rc.device->CreateTexture(desc, nullptr, &tex);
            if (!tex) {
                throw std::runtime_error("post-process: warm-up dummy texture creation failed");
            }
            return tex;
        };
        RefCntAutoPtr<ITexture> dummyDepth = makeDummy(TEX_FORMAT_R32_FLOAT, "PostFX warm-up depth");
        RefCntAutoPtr<ITexture> dummyMotion = makeDummy(TEX_FORMAT_RG16_FLOAT, "PostFX warm-up motion");

        PostFXContext::FrameDesc warmupFrame;
        warmupFrame.Index = 0;
        warmupFrame.Width = 2;
        warmupFrame.Height = 2;
        warmupFrame.OutputWidth = 2;
        warmupFrame.OutputHeight = 2;
        impl_->postfx->PrepareResources(rc.device, warmupFrame, PostFXContext::FEATURE_FLAG_NONE);

        HLSL::CameraAttribs cameraCurr{};
        HLSL::CameraAttribs cameraPrev{};
        PostFXContext::RenderAttributes warmup;
        warmup.pDevice = rc.device;
        warmup.pDeviceContext = rc.context;
        warmup.pCurrDepthBufferSRV = dummyDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        warmup.pPrevDepthBufferSRV = dummyDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        warmup.pMotionVectorsSRV = dummyMotion->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        warmup.pCurrCamera = &cameraCurr;
        warmup.pPrevCamera = &cameraPrev;
        impl_->postfx->Execute(warmup);
        if (!impl_->postfx->IsPSOsReady()) {
            engine::core::log(engine::core::LogLevel::Warn,
                              "post-process: PostFXContext PSOs still not ready after warm-up -- bloom will stay off");
        }
    }
}

PostProcessor::~PostProcessor() {
    // The offscreen target is registered on the shared context impl -- deregister it so the
    // renderer falls back to direct swap-chain drawing if it outlives this post-processor.
    if (impl_ && impl_->context != nullptr) {
        impl_->context->impl().sceneColor.Release();
    }
}

void PostProcessor::set_bloom_enabled(bool enabled) noexcept { impl_->bloomEnabled = enabled; }
void PostProcessor::set_tonemap_enabled(bool enabled) noexcept { impl_->tonemapEnabled = enabled; }

void PostProcessor::execute(std::uint32_t frameIndex) {
    auto& rc = impl_->context->impl();
    impl_->ensure_scene_target();

    ITextureView* sceneSRV = rc.sceneColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ITextureView* compositeSource = sceneSRV;

    if (impl_->bloomEnabled) {
        PostFXContext::FrameDesc frame;
        frame.Index = frameIndex;
        frame.Width = impl_->targetWidth;
        frame.Height = impl_->targetHeight;
        frame.OutputWidth = impl_->targetWidth;
        frame.OutputHeight = impl_->targetHeight;
        // PrepareResources only -- PostFXContext::Execute (and its depth/motion-vector inputs) is
        // deliberately never called; Bloom does not need it (see the header note).
        impl_->postfx->PrepareResources(rc.device, frame, PostFXContext::FEATURE_FLAG_NONE);
        impl_->bloom->PrepareResources(rc.device, rc.context, impl_->postfx.get(), Bloom::FEATURE_FLAG_NONE);

        // Tuned against real scene brightness by viewed A/B captures (goal 23), not picked blind:
        // the SKY is this scene's brightest surface (max channel ~0.9 vs sun-lit terrain ~0.8),
        // so a low threshold made the whole frame glow hazy (measured mean +8% everywhere). At
        // 0.8/0.12 the result is a soft atmospheric glow at bright sky/terrain silhouettes;
        // Stage 3's water sun-glint (the first genuinely >1.0 emitter) is the intended real
        // bloom source and gets re-tuned in goal 32.
        HLSL::BloomAttribs attribs;
        attribs.Intensity = 0.12f;
        attribs.Threshold = 0.80f;
        attribs.SoftTreshold = 0.25f; // [sic] -- DiligentFX's own field spelling
        attribs.Radius = 0.65f;
        attribs.AlphaInterpolation = 1.0f; // no fade-in: deterministic captures for --verify-frame

        Bloom::RenderAttributes bloomAttribs;
        bloomAttribs.pDevice = rc.device;
        bloomAttribs.pDeviceContext = rc.context;
        bloomAttribs.pPostFXContext = impl_->postfx.get();
        bloomAttribs.pColorBufferSRV = sceneSRV;
        bloomAttribs.pBloomAttribs = &attribs;
        const POST_FX_EXECUTION_STATUS bloomStatus = impl_->bloom->Execute(bloomAttribs);
        static POST_FX_EXECUTION_STATUS lastStatus = POST_FX_EXECUTION_STATUS_FAILED;
        if (bloomStatus != lastStatus) {
            engine::core::log(engine::core::LogLevel::Info, "bloom execute status now: {}",
                              bloomStatus == POST_FX_EXECUTION_STATUS_READY     ? "READY"
                              : bloomStatus == POST_FX_EXECUTION_STATUS_PENDING ? "PENDING"
                                                                                : "FAILED");
            lastStatus = bloomStatus;
        }

        // The bloom output is the fully-composited scene+bloom image (the final upsample pass
        // does `Source + Intensity * Bloom` itself -- read from Bloom_ComputeUpsampledTexture.fx,
        // not assumed), so the composite pass just tone-maps whichever texture we hand it.
        if (ITextureView* bloomSRV = impl_->bloom->GetBloomTextureSRV()) {
            compositeSource = bloomSRV;
        }
    }

    impl_->composite(compositeSource);
}

} // namespace render::diligent
