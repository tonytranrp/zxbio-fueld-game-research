#pragma once

#include <cstdint>
#include <string_view>

#include "dev/scenario/backend.hpp"

namespace dev::scenario {

// What a scenario can assert about a run. Every one of these is a number the harness's own report
// carries, so an assertion cannot ask about something that is not measured -- and adding a metric
// means adding it to the report first, which is the intended friction.
enum class Metric : std::uint8_t {
    Frames,           // frames actually rendered
    FrameMsMean,      //
    FrameMsMedian,    //
    FrameMsP95,       // the regression signal. NOT fps: this panel's FIFO_RELAXED caps fps at
    FrameMsP99,       // 155-159, which makes it useless as one (see the research file's §6.8).
    FrameMsMax,       //
    SlowFrames,       // frames over the slow-frame threshold, any cause
    SlowFramesOnSwap, //
    SlowFramesUploading,
    SlowFramesBuilding,
    GpuMsMean, // the timestamped march+resolve range
    GpuMsMedian, // goal 273: the stable one on a SHORT scenario -- see the comment below
    GpuMsP95,
    GpuMsMax,
    GpuMemoryMb,     // the renderer's own tracked allocation
    ResidentBricks,  //
    ResidentMb,      //
    ContrastPercent, // frame_verify's LOCAL-CONTRAST metric: is there a scene at all
    GoldenDistance,  // the regression metric of goal 217, worst over the run's captures
    WalkViolations,  // ticks that ended below the ground surface
    InsideSolid,     // goal 228: ticks that ended with the body inside solid geometry
    StanceChanges,   // goal 236: Grounded/Airborne/Swimming transitions -- waterline flicker
    Uploads,         // tree uploads completed
};

// WHICH GPU STATISTIC TO GATE ON, measured in goal 273 rather than chosen by taste.
//
// A p95 is only a stable statistic once the run is long enough for the 95th percentile to sit in
// the steady state rather than in the hitch tail. Four runs each, d3d12, same machine, same build:
//
//   stress_pose   ~800 frames   gpu p95 = 5.05, 5.09, 5.14, 5.21   ->  3% spread   GATEABLE
//   valley_far    ~320 frames   gpu p95 = 4.45, 4.93, 5.92, 6.46   -> 45% spread   NOT gateable
//
// At 320 frames the p95 is the sixteenth-worst frame, which is one hitch away from anything. The
// MEDIAN of the same short run is stable (3.78-4.03, 7%), which is why `gpu_ms_median` exists.
//
// So: gate a long scenario on `gpu_ms_p95` and a short one on `gpu_ms_median`. Never on fps -- this
// machine's 165 Hz FIFO_RELAXED panel caps it at 155-159 and it cannot express a regression.
enum class Op : std::uint8_t { Lt, Le, Gt, Ge, Eq };

struct Assertion {
    Metric metric = Metric::Frames;
    Op op = Op::Ge;
    double value = 0.0;
    // Goal 273: an optional trailing `vk` or `d3d12` narrows an assertion to one backend, and
    // `Both` (the default, written by omitting it) applies it to whichever backend is running.
    //
    // THIS IS NOT A CONVENIENCE. A performance budget that has to hold on both backends is set by
    // the SLOWER one, and the two differ by 30-35% here -- so a single threshold gives the faster
    // backend that much headroom, and `ctest -L scenario` runs vk ONLY. A vk regression of 30%
    // would have sailed through a gate whose number came from d3d12. Per-backend thresholds are
    // what make the gate actually gate.
    BackendSelection backend = BackendSelection::Both;

    [[nodiscard]] friend bool operator==(const Assertion&, const Assertion&) = default;
};

/// Does `assertion` apply to a run on `running`? `Both` on the assertion means "any backend".
[[nodiscard]] constexpr bool applies_to(const Assertion& assertion, BackendSelection running) noexcept {
    return assertion.backend == BackendSelection::Both || assertion.backend == running;
}

[[nodiscard]] bool parse_metric(std::string_view text, Metric& out) noexcept;
[[nodiscard]] std::string_view metric_name(Metric metric) noexcept;
[[nodiscard]] bool parse_op(std::string_view text, Op& out) noexcept;
[[nodiscard]] std::string_view op_name(Op op) noexcept;
[[nodiscard]] bool evaluate(Op op, double measured, double threshold) noexcept;

} // namespace dev::scenario
