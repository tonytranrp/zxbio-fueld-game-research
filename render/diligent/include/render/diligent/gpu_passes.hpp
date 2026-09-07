#pragma once

// Per-pass GPU timing (Prompt 002 goal 220).
//
// Before this there was ONE timestamp pair, around march+resolve, inside SvoRenderer. That number
// is what settled "the lag is not the rendering" last pass -- but it cannot say WHICH pass costs
// what, which is the question Prompt 004 opens with.
//
// Mechanism: `DurationQueryHelper` per range, i.e. a TOP_OF_PIPE/BOTTOM_OF_PIPE timestamp pair
// resolved a frame or two later. Per research/gpu-voxel-streaming-and-profiling-research.md §5(b)
// that is the only counter mechanism reliably available unprivileged on this machine, and it is
// safe to leave permanently enabled provided the count stays low -- each write is documented as
// "an execution dependency similar to a barrier on all commands that were submitted before it".
// Five ranges is low. Fifty would not be.
//
// TWO THINGS THIS DELIBERATELY DOES NOT DO:
//
//  * There is no "present" GPU range. Present is a queue operation, not a bracketed workload, and
//    §5(b) is explicit that timestamps from different queues cannot be compared. A pair around
//    Present would measure the CPU-side submit, which the frame report already carries as the
//    `present` PHASE. Asking for a fifth GPU range there would have produced a number that looks
//    like an answer and is not one.
//  * The first two frames are skipped, always. CLAUDE.md records a real crash inside the NVIDIA
//    driver when a timestamp is the app's very first Vulkan command; the existing march timer
//    already skips two frames and this preserves that for every range.

#include <cstdint>

namespace render::diligent {

class RenderContext;

enum class GpuPass : std::uint8_t {
    Frame,   // the whole frame's GPU work: the sum of the four below must land inside it
    March,   // the fullscreen ray march
    Resolve, // the TAA temporal resolve
    Post,    // bloom + tonemap composite
    Overlay, // ImGui
    Count,
};

[[nodiscard]] const char* to_string(GpuPass pass) noexcept;

// The four ranges whose sum is checked against GpuPass::Frame.
inline constexpr GpuPass kSummedPasses[] = {GpuPass::March, GpuPass::Resolve, GpuPass::Post,
                                            GpuPass::Overlay};

// Scoped begin/end. A pass that is not entered on a frame simply reports its previous value, which
// is why last_ms() is paired with measured_this_frame().
class GpuPassScope {
public:
    GpuPassScope(RenderContext& context, GpuPass pass) noexcept;
    ~GpuPassScope();

    GpuPassScope(const GpuPassScope&) = delete;
    GpuPassScope& operator=(const GpuPassScope&) = delete;
    GpuPassScope(GpuPassScope&&) = delete;
    GpuPassScope& operator=(GpuPassScope&&) = delete;

private:
    RenderContext* context_ = nullptr;
    GpuPass pass_ = GpuPass::Count;
    bool active_ = false;
};

// Creates the query pools. Called once from RenderContext's construction path.
void init_gpu_passes(RenderContext& context);

// ON by default, and the cost is MEASURED rather than assumed (goal 220's own Check; the numbers
// are in research/dev-harness-log.md). --no-gpu-timers turns them off, which is what makes that
// A/B possible at all.
void set_gpu_timers_enabled(RenderContext& context, bool enabled) noexcept;
[[nodiscard]] bool gpu_timers_enabled(const RenderContext& context) noexcept;

// Milliseconds, from the most recent frame this pass was measured on. Zero before the first valid
// resolve.
[[nodiscard]] double gpu_pass_ms(const RenderContext& context, GpuPass pass) noexcept;

// Advances the frame counter that implements the two-frame skip. Called once per frame by the
// frame loop, right after present.
void gpu_passes_end_frame(RenderContext& context) noexcept;

} // namespace render::diligent
