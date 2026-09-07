#include "render/diligent/auto_exposure.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

#include "Graphics/GraphicsEngine/interface/GraphicsTypes.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#include "detail/render_context_impl.hpp"

using namespace Diligent;

namespace render::diligent {

namespace {

// The two metering targets. 64x64 is enough spatial resolution for the mask to have a shape
// (the 6-degree pool is ~9% of the frame's height at a 70-degree FOV, so ~6 texels across) and
// small enough that the second pass is one 8x8 box reduction.
constexpr Uint32 kMeterSize = 64;
constexpr Uint32 kReducedSize = 8;
// How many frames the readback runs behind. Three staging textures means the GPU is never waited
// on; at 120 fps that is ~25 ms, under 3% of the fast adaptation time constant.
constexpr std::size_t kReadbackLatency = 3;

// Mirror of meter.psh.hlsl's cbuffer MeterConstants -- update both together.
struct MeterConstantsCpu {
    float poolingTangent = 0.0f;
    float fovTangent = 1.0f;
    float aspect = 1.0f;
    float crosshairMetering = 1.0f;
};
static_assert(sizeof(MeterConstantsCpu) == 16, "must match the 16-byte HLSL cbuffer");

} // namespace

float pooling_tangent(float poolingHalfAngleDeg) noexcept {
    constexpr float kDegToRad = 3.14159265358979f / 180.0f;
    return std::tan(std::max(poolingHalfAngleDeg, 0.01f) * kDegToRad);
}

float adapt_log_luminance(float adapted, float measured, const ExposureSettings& settings,
                          float dt) noexcept {
    const float target = std::clamp(measured, settings.min_log_luminance, settings.max_log_luminance);
    // "Fast up, slow down": brightening is the urgent direction. An exponential approach rather
    // than a linear rate, because the eye's own adaptation is a settling process and because an
    // exponential cannot overshoot however large dt gets -- the huge-dt case that has broken two
    // other integrators in this pass.
    const float tau = target > adapted ? settings.adapt_up_seconds : settings.adapt_down_seconds;
    // A zero time constant means "instant"; a zero time step means "nothing happened". Those are
    // opposite answers and the first draft of this function gave both of them the same one, so a
    // paused frame or a first frame would have SNAPPED the exposure -- exactly when nothing should
    // change. Caught by its own test, whose comment disagreed with its assertion.
    if (dt <= 0.0f) {
        return adapted;
    }
    if (tau <= 0.0f) {
        return target;
    }
    const float alpha = 1.0f - std::exp(-dt / tau);
    return adapted + (target - adapted) * alpha;
}

struct AutoExposure::Impl {
    RenderContext* context = nullptr;
    ExposureSettings settings;
    ExposureSample sample;

    RefCntAutoPtr<IPipelineState> meterPso;
    RefCntAutoPtr<IShaderResourceBinding> meterSrb;
    RefCntAutoPtr<IBuffer> meterConstants;
    RefCntAutoPtr<IPipelineState> reducePso;
    RefCntAutoPtr<IShaderResourceBinding> reduceSrb;

    RefCntAutoPtr<ITexture> meterTarget;  // 64x64 RG16F
    RefCntAutoPtr<ITexture> reduceTarget; // 8x8  RG16F
    std::array<RefCntAutoPtr<ITexture>, kReadbackLatency> staging;
    std::array<RefCntAutoPtr<IFence>, kReadbackLatency> fences;
    std::array<Uint64, kReadbackLatency> fenceValues{};
    std::size_t frameIndex = 0;

    float adapted = 0.0f;
    bool adaptedSeeded = false;

    void ensure_resources();
    void create_pipelines();
    [[nodiscard]] bool read_back(float& measuredLog2);
};

void AutoExposure::Impl::ensure_resources() {
    if (meterTarget) {
        return;
    }
    auto& rc = context->impl();

    auto makeTarget = [&](Uint32 size, const char* name, RefCntAutoPtr<ITexture>& out) {
        TextureDesc desc;
        desc.Name = name;
        desc.Type = RESOURCE_DIM_TEX_2D;
        desc.Width = size;
        desc.Height = size;
        // RG32F, not RG16F: these targets are 64x64 and 8x8, so full float costs 20 KB total and
        // removes both a half-to-float conversion on the readback and any question about whether
        // an 11-bit mantissa is enough for a sum of 64 log2 values.
        desc.Format = TEX_FORMAT_RG32_FLOAT;
        desc.MipLevels = 1;
        desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        rc.device->CreateTexture(desc, nullptr, &out);
        if (!out) {
            throw std::runtime_error(std::string("auto-exposure: target creation failed: ") + name);
        }
    };
    makeTarget(kMeterSize, "AutoExposure meter (64x64)", meterTarget);
    makeTarget(kReducedSize, "AutoExposure reduce (8x8)", reduceTarget);

    for (std::size_t i = 0; i < kReadbackLatency; ++i) {
        TextureDesc desc;
        desc.Name = "AutoExposure staging";
        desc.Type = RESOURCE_DIM_TEX_2D;
        desc.Width = kReducedSize;
        desc.Height = kReducedSize;
        desc.Format = TEX_FORMAT_RG32_FLOAT;
        desc.MipLevels = 1;
        desc.Usage = USAGE_STAGING;
        desc.CPUAccessFlags = CPU_ACCESS_READ;
        rc.device->CreateTexture(desc, nullptr, &staging[i]);
        if (!staging[i]) {
            throw std::runtime_error("auto-exposure: staging texture creation failed");
        }
        FenceDesc fenceDesc;
        fenceDesc.Name = "AutoExposure readback fence";
        rc.device->CreateFence(fenceDesc, &fences[i]);
        if (!fences[i]) {
            throw std::runtime_error("auto-exposure: fence creation failed");
        }
        fenceValues[i] = 0;
    }
}

void AutoExposure::Impl::create_pipelines() {
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
            throw std::runtime_error(std::string("auto-exposure: shader creation failed: ") + file);
        }
        return shader;
    };

    RefCntAutoPtr<IShader> vs = createShader(SHADER_TYPE_VERTEX, "fullscreen.vsh.hlsl", "meter VS");

    SamplerDesc linearClamp;
    linearClamp.MinFilter = FILTER_TYPE_LINEAR;
    linearClamp.MagFilter = FILTER_TYPE_LINEAR;
    linearClamp.MipFilter = FILTER_TYPE_LINEAR;
    linearClamp.AddressU = TEXTURE_ADDRESS_CLAMP;
    linearClamp.AddressV = TEXTURE_ADDRESS_CLAMP;
    ImmutableSamplerDesc samplers[] = {{SHADER_TYPE_PIXEL, "g_SourceColor", linearClamp}};

    auto makePso = [&](const char* psFile, const char* name, bool withConstants,
                       RefCntAutoPtr<IPipelineState>& pso, RefCntAutoPtr<IShaderResourceBinding>& srb) {
        // The pixel shader is held in a NAMED RefCntAutoPtr. Assigning `createShader(...)` straight
        // into `psoCI.pPS` binds a temporary smart pointer whose destructor runs at the end of that
        // statement, releasing the shader before CreateGraphicsPipelineState ever sees it -- which
        // presents as an access violation executing address 0 during startup, with no stack.
        RefCntAutoPtr<IShader> ps = createShader(SHADER_TYPE_PIXEL, psFile, name);
        GraphicsPipelineStateCreateInfo psoCI;
        psoCI.PSODesc.Name = name;
        psoCI.pVS = vs;
        psoCI.pPS = ps;
        psoCI.GraphicsPipeline.NumRenderTargets = 1;
        psoCI.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RG32_FLOAT;
        psoCI.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN;
        psoCI.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        psoCI.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
        psoCI.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

        // The source texture is DYNAMIC on purpose: CLAUDE.md's Diligent note is that a MUTABLE
        // variable binds exactly once per SRB and a second Set is silently ignored in Release. The
        // scene target is recreated on every resize, so this one is replaced at runtime.
        std::array<ShaderResourceVariableDesc, 2> vars{
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_SourceColor",
                                       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "MeterConstants",
                                       SHADER_RESOURCE_VARIABLE_TYPE_STATIC}};
        psoCI.PSODesc.ResourceLayout.Variables = vars.data();
        psoCI.PSODesc.ResourceLayout.NumVariables = withConstants ? 2u : 1u;
        psoCI.PSODesc.ResourceLayout.ImmutableSamplers = samplers;
        psoCI.PSODesc.ResourceLayout.NumImmutableSamplers = 1;

        rc.device->CreateGraphicsPipelineState(psoCI, &pso);
        if (!pso) {
            throw std::runtime_error(std::string("auto-exposure: PSO creation failed: ") + name);
        }
        if (withConstants) {
            pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "MeterConstants")->Set(meterConstants);
        }
        pso->CreateShaderResourceBinding(&srb, true);
    };

    BufferDesc cbDesc;
    cbDesc.Name = "MeterConstants";
    cbDesc.Size = sizeof(MeterConstantsCpu);
    cbDesc.Usage = USAGE_DYNAMIC;
    cbDesc.BindFlags = BIND_UNIFORM_BUFFER;
    cbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    rc.device->CreateBuffer(cbDesc, nullptr, &meterConstants);
    if (!meterConstants) {
        throw std::runtime_error("auto-exposure: meter constants creation failed");
    }

    makePso("meter.psh.hlsl", "AutoExposure meter PSO", true, meterPso, meterSrb);
    makePso("reduce.psh.hlsl", "AutoExposure reduce PSO", false, reducePso, reduceSrb);
}

bool AutoExposure::Impl::read_back(float& measuredLog2) {
    auto& rc = context->impl();
    if (frameIndex < kReadbackLatency) {
        return false; // the ring has not filled yet
    }
    const std::size_t slot = frameIndex % kReadbackLatency;
    // Only read a slot whose copy the GPU has actually finished. Without the fence this is a race
    // that happens to work on one driver and returns garbage on another.
    if (fences[slot]->GetCompletedValue() < fenceValues[slot]) {
        return false;
    }

    MappedTextureSubresource mapped;
    rc.context->MapTextureSubresource(staging[slot], 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
    if (mapped.pData == nullptr) {
        return false;
    }
    // Diligent hands back the row stride, which is NOT necessarily width * 8 on every backend.
    float logSum = 0.0f;
    float weightSum = 0.0f;
    const auto* base = static_cast<const std::byte*>(mapped.pData);
    for (Uint32 y = 0; y < kReducedSize; ++y) {
        const auto* row = reinterpret_cast<const float*>(base + y * mapped.Stride);
        for (Uint32 x = 0; x < kReducedSize; ++x) {
            logSum += row[x * 2 + 0];
            weightSum += row[x * 2 + 1];
        }
    }
    rc.context->UnmapTextureSubresource(staging[slot], 0, 0);

    if (weightSum <= 1.0e-6f) {
        return false;
    }
    measuredLog2 = logSum / weightSum;
    return true;
}

AutoExposure::AutoExposure(RenderContext& context) : impl_(std::make_unique<Impl>()) {
    impl_->context = &context;
    impl_->create_pipelines();
}

AutoExposure::~AutoExposure() = default;

void AutoExposure::set_settings(const ExposureSettings& settings) noexcept {
    impl_->settings = settings;
}

const ExposureSettings& AutoExposure::settings() const noexcept {
    return impl_->settings;
}

float AutoExposure::exposure() const noexcept {
    return impl_->settings.enabled && impl_->sample.valid ? impl_->sample.exposure : 1.0f;
}

const ExposureSample& AutoExposure::last_sample() const noexcept {
    return impl_->sample;
}

void AutoExposure::measure(float verticalFovRadians, float dtSeconds) {
    if (!impl_->settings.enabled) {
        return;
    }
    auto& rc = impl_->context->impl();
    if (!rc.sceneColor) {
        return; // no HDR target: --no-post, or the very first frame
    }
    impl_->ensure_resources();
    IDeviceContext* ctx = rc.context;

    {
        MapHelper<MeterConstantsCpu> constants(ctx, impl_->meterConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        constants->poolingTangent = pooling_tangent(impl_->settings.pooling_half_angle_deg);
        constants->fovTangent = std::tan(std::max(verticalFovRadians, 0.01f) * 0.5f);
        const SwapChainDesc& scDesc = rc.swapchain->GetDesc();
        constants->aspect =
            scDesc.Height > 0 ? static_cast<float>(scDesc.Width) / static_cast<float>(scDesc.Height) : 1.0f;
        constants->crosshairMetering = impl_->settings.crosshair_metering ? 1.0f : 0.0f;
    }

    auto fullscreen = [&](IPipelineState* pso, IShaderResourceBinding* srb, ITextureView* source,
                          ITexture* target) {
        ITextureView* rtv = target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        ctx->SetRenderTargets(1, &rtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->SetPipelineState(pso);
        srb->GetVariableByName(SHADER_TYPE_PIXEL, "g_SourceColor")->Set(source);
        ctx->CommitShaderResources(srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        ctx->Draw({3, DRAW_FLAG_VERIFY_ALL, 1});
    };

    fullscreen(impl_->meterPso, impl_->meterSrb, rc.sceneColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE),
               impl_->meterTarget);
    fullscreen(impl_->reducePso, impl_->reduceSrb,
               impl_->meterTarget->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE), impl_->reduceTarget);

    // READ BEFORE WRITING. The slot this frame overwrites is the oldest one, which is exactly the
    // slot whose contents are ready -- so the read has to happen before the copy replaces both the
    // pixels and the fence value. Doing it the other way round compares the fence against a value
    // enqueued microseconds earlier, which is never complete, and the readback silently never
    // lands: measured as `exposureValid` false on every capture of a passing run.
    float measured = 0.0f;
    const bool gotSample = impl_->read_back(measured);

    // Queue this frame's 8x8 into the ring, and signal a fence so the read side knows when it is
    // safe. The slot being written is the one that will be read `kReadbackLatency` frames from now.
    const std::size_t writeSlot = impl_->frameIndex % kReadbackLatency;
    CopyTextureAttribs copy;
    copy.pSrcTexture = impl_->reduceTarget;
    copy.pDstTexture = impl_->staging[writeSlot];
    copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    ctx->CopyTexture(copy);
    impl_->fenceValues[writeSlot] = static_cast<Uint64>(impl_->frameIndex) + 1;
    ctx->EnqueueSignal(impl_->fences[writeSlot], impl_->fenceValues[writeSlot]);

    if (impl_->settings.pinned()) {
        // Pinned: still meter (the trace stays honest about what the scene actually is) but hold the
        // adapted state where the caller put it.
        impl_->adapted = impl_->settings.pinned_log2;
        impl_->adaptedSeeded = true;
        impl_->sample.valid = true;
        impl_->sample.measured_log2 = gotSample ? measured : impl_->adapted;
        impl_->sample.brightening = false;
        impl_->sample.adapted_log2 = impl_->adapted;
        impl_->sample.exposure = impl_->settings.key / std::exp2(impl_->adapted);
    } else if (gotSample) {
        if (!impl_->adaptedSeeded) {
            // Seed rather than adapt on the first real sample: otherwise every run opens with a
            // visible ramp from an arbitrary starting exposure, which is a fade-in nobody asked for.
            impl_->adapted =
                std::clamp(measured, impl_->settings.min_log_luminance, impl_->settings.max_log_luminance);
            impl_->adaptedSeeded = true;
        } else {
            impl_->adapted = adapt_log_luminance(impl_->adapted, measured, impl_->settings, dtSeconds);
        }
        impl_->sample.valid = true;
        impl_->sample.measured_log2 = measured;
        impl_->sample.brightening = measured > impl_->adapted;
        impl_->sample.adapted_log2 = impl_->adapted;
        impl_->sample.exposure = impl_->settings.key / std::exp2(impl_->adapted);
    }
    ++impl_->frameIndex;
}

} // namespace render::diligent
