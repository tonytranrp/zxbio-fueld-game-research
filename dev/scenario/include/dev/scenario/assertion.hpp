#pragma once

#include <cstdint>
#include <string_view>

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
    GpuMsP95,
    GpuMsMax,
    GpuMemoryMb,     // the renderer's own tracked allocation
    ResidentBricks,  //
    ResidentMb,      //
    ContrastPercent, // frame_verify's LOCAL-CONTRAST metric: is there a scene at all
    GoldenDistance,  // the regression metric of goal 217, worst over the run's captures
    WalkViolations,  // ticks that ended below the ground surface
    Uploads,         // tree uploads completed
};

enum class Op : std::uint8_t { Lt, Le, Gt, Ge, Eq };

struct Assertion {
    Metric metric = Metric::Frames;
    Op op = Op::Ge;
    double value = 0.0;

    [[nodiscard]] friend bool operator==(const Assertion&, const Assertion&) = default;
};

[[nodiscard]] bool parse_metric(std::string_view text, Metric& out) noexcept;
[[nodiscard]] std::string_view metric_name(Metric metric) noexcept;
[[nodiscard]] bool parse_op(std::string_view text, Op& out) noexcept;
[[nodiscard]] std::string_view op_name(Op op) noexcept;
[[nodiscard]] bool evaluate(Op op, double measured, double threshold) noexcept;

} // namespace dev::scenario
