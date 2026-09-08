// The text <-> enum tables for the .scn format. One array per enum, read in both directions, so
// parse and emit cannot drift -- the same discipline world/materials applies to material names.

#include <array>
#include <string>

#include "dev/scenario/assertion.hpp"
#include "dev/scenario/capture.hpp"
#include "dev/scenario/scenario.hpp"

namespace dev::scenario {

namespace {

template <class T>
struct Named {
    std::string_view name;
    T value;
};

constexpr std::array kMetrics{
    Named<Metric>{"frames", Metric::Frames},
    Named<Metric>{"frame_ms_mean", Metric::FrameMsMean},
    Named<Metric>{"frame_ms_median", Metric::FrameMsMedian},
    Named<Metric>{"frame_ms_p95", Metric::FrameMsP95},
    Named<Metric>{"frame_ms_p99", Metric::FrameMsP99},
    Named<Metric>{"frame_ms_max", Metric::FrameMsMax},
    Named<Metric>{"slow_frames", Metric::SlowFrames},
    Named<Metric>{"slow_frames_on_swap", Metric::SlowFramesOnSwap},
    Named<Metric>{"slow_frames_uploading", Metric::SlowFramesUploading},
    Named<Metric>{"slow_frames_building", Metric::SlowFramesBuilding},
    Named<Metric>{"gpu_ms_mean", Metric::GpuMsMean},
    Named<Metric>{"gpu_ms_median", Metric::GpuMsMedian},
    Named<Metric>{"gpu_ms_p95", Metric::GpuMsP95},
    Named<Metric>{"gpu_ms_max", Metric::GpuMsMax},
    Named<Metric>{"gpu_memory_mb", Metric::GpuMemoryMb},
    Named<Metric>{"resident_bricks", Metric::ResidentBricks},
    Named<Metric>{"resident_mb", Metric::ResidentMb},
    Named<Metric>{"contrast_percent", Metric::ContrastPercent},
    Named<Metric>{"moire_ratio", Metric::MoireRatio},
    Named<Metric>{"golden_distance", Metric::GoldenDistance},
    Named<Metric>{"walk_violations", Metric::WalkViolations},
    Named<Metric>{"inside_solid", Metric::InsideSolid},
    Named<Metric>{"stance_changes", Metric::StanceChanges},
    Named<Metric>{"uploads", Metric::Uploads},
};

constexpr std::array kOps{
    Named<Op>{"<", Op::Lt},  Named<Op>{"<=", Op::Le}, Named<Op>{">", Op::Gt},
    Named<Op>{">=", Op::Ge}, Named<Op>{"==", Op::Eq},
};

constexpr std::array kEvents{
    Named<CaptureEvent>{"tree-swapped", CaptureEvent::FirstTreeSwapped},
    Named<CaptureEvent>{"grounded", CaptureEvent::FirstGrounded},
    Named<CaptureEvent>{"slow-frame", CaptureEvent::FirstSlowFrame},
    Named<CaptureEvent>{"world-ready", CaptureEvent::WorldReady},
};

constexpr std::array kBackends{
    Named<BackendSelection>{"vk", BackendSelection::Vulkan},
    Named<BackendSelection>{"d3d12", BackendSelection::D3D12},
    Named<BackendSelection>{"both", BackendSelection::Both},
};

template <class T, std::size_t N>
[[nodiscard]] bool lookup(const std::array<Named<T>, N>& table, std::string_view text, T& out) noexcept {
    for (const Named<T>& entry : table) {
        if (entry.name == text) {
            out = entry.value;
            return true;
        }
    }
    return false;
}

template <class T, std::size_t N>
[[nodiscard]] std::string_view name_of(const std::array<Named<T>, N>& table, T value) noexcept {
    for (const Named<T>& entry : table) {
        if (entry.value == value) {
            return entry.name;
        }
    }
    return "?";
}

[[nodiscard]] std::string trimmed_number(float value) {
    std::string text = std::to_string(value);
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

} // namespace

bool parse_metric(std::string_view text, Metric& out) noexcept {
    return lookup(kMetrics, text, out);
}
std::string_view metric_name(Metric metric) noexcept {
    return name_of(kMetrics, metric);
}

bool parse_op(std::string_view text, Op& out) noexcept {
    return lookup(kOps, text, out);
}
std::string_view op_name(Op op) noexcept {
    return name_of(kOps, op);
}

bool evaluate(Op op, double measured, double threshold) noexcept {
    switch (op) {
    case Op::Lt:
        return measured < threshold;
    case Op::Le:
        return measured <= threshold;
    case Op::Gt:
        return measured > threshold;
    case Op::Ge:
        return measured >= threshold;
    case Op::Eq:
        // A tolerance, not bitwise equality: every metric on the list is a double derived from
        // timings or counts, and `assert walk_violations == 0` must not depend on how 0.0 was
        // arrived at.
        return measured >= threshold - 1e-9 && measured <= threshold + 1e-9;
    }
    return false;
}

bool parse_capture_when(std::string_view text, CapturePoint& out) noexcept {
    CaptureEvent event{};
    if (!lookup(kEvents, text, event)) {
        return false;
    }
    out.when = CaptureWhen::Event;
    out.event = event;
    return true;
}

std::string capture_when_to_string(const CapturePoint& point) {
    switch (point.when) {
    case CaptureWhen::Frame:
        return "frame " + std::to_string(point.frame);
    case CaptureWhen::Seconds:
        return "time " + trimmed_number(point.seconds);
    case CaptureWhen::Event:
        return "event " + std::string{name_of(kEvents, point.event)};
    case CaptureWhen::End:
        break;
    }
    return "end";
}

bool parse_backend(std::string_view text, BackendSelection& out) noexcept {
    return lookup(kBackends, text, out);
}

std::string_view backend_name(BackendSelection backend) noexcept {
    return name_of(kBackends, backend);
}

} // namespace dev::scenario
