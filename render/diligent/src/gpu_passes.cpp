#include "render/diligent/gpu_passes.hpp"

#include <cstddef>

#include "detail/render_context_impl.hpp"
#include "engine/core/log.hpp"

namespace render::diligent {

namespace {

[[nodiscard]] std::size_t index_of(GpuPass pass) noexcept {
    return static_cast<std::size_t>(pass);
}

} // namespace

const char* to_string(GpuPass pass) noexcept {
    switch (pass) {
    case GpuPass::Frame:
        return "frame";
    case GpuPass::March:
        return "march";
    case GpuPass::Resolve:
        return "resolve";
    case GpuPass::Post:
        return "post";
    case GpuPass::Overlay:
        return "overlay";
    case GpuPass::Count:
        break;
    }
    return "?";
}

void init_gpu_passes(RenderContext& context) {
    auto& rc = context.impl();
    // Same feature gate the single march timer already used: device_init.cpp asks for
    // DurationQueries as OPTIONAL, so a device without them leaves every number at zero rather
    // than failing the app.
    if (rc.device->GetDeviceInfo().Features.DurationQueries != Diligent::DEVICE_FEATURE_STATE_ENABLED) {
        engine::core::log(engine::core::LogLevel::Warn,
                          "gpu passes: duration queries unsupported on this device -- per-pass GPU "
                          "times unavailable");
        return;
    }
    for (auto& timer : rc.gpuPasses.timers) {
        // 3 query pairs in flight: one being written, one resolving, one spare. Diligent's own
        // Tutorial18 uses the same depth for the same reason -- reading back in the same frame
        // would mean a GPU stall, which is exactly what an instrumentation path must not cause.
        timer.emplace(rc.device, 3);
    }
    rc.gpuPasses.supported = true;
}

void set_gpu_timers_enabled(RenderContext& context, bool enabled) noexcept {
    context.impl().gpuPasses.enabled = enabled;
}

bool gpu_timers_enabled(const RenderContext& context) noexcept {
    return const_cast<RenderContext&>(context).impl().gpuPasses.enabled;
}

double gpu_pass_ms(const RenderContext& context, GpuPass pass) noexcept {
    if (pass == GpuPass::Count) {
        return 0.0;
    }
    return const_cast<RenderContext&>(context).impl().gpuPasses.lastMs[index_of(pass)];
}

void gpu_passes_end_frame(RenderContext& context) noexcept {
    ++context.impl().gpuPasses.frameCounter;
}

GpuPassScope::GpuPassScope(RenderContext& context, GpuPass pass) noexcept : context_(&context), pass_(pass) {
    auto& timers = context.impl().gpuPasses;
    if (!timers.active() || pass == GpuPass::Count) {
        return;
    }
    auto& timer = timers.timers[index_of(pass)];
    if (!timer) {
        return;
    }
    timer->Begin(context.impl().context);
    timers.open[index_of(pass)] = true;
    active_ = true;
}

GpuPassScope::~GpuPassScope() {
    if (!active_ || context_ == nullptr) {
        return;
    }
    auto& timers = context_->impl().gpuPasses;
    auto& timer = timers.timers[index_of(pass_)];
    double seconds = 0.0;
    // End() returns false while the query is still in flight -- normal for the first frames, and
    // the previous value is what the report should keep rather than a zero.
    if (timer && timer->End(context_->impl().context, seconds)) {
        timers.lastMs[index_of(pass_)] = seconds * 1000.0;
    }
    timers.open[index_of(pass_)] = false;
}

} // namespace render::diligent
