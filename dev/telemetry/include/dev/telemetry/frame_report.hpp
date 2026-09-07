#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "dev/telemetry/frame_record.hpp"

namespace dev::telemetry {

// The threshold a frame is "slow" above. 20 ms, unchanged from run_svo's kSlowFrameMs, so the
// slow-frame counts in this pass's reports are comparable with every number in
// research/lin-look-log.md.
inline constexpr double kSlowFrameMs = 20.0;

struct SlowFrameCounts {
    std::uint32_t total = 0;
    std::uint32_t on_swap = 0;
    std::uint32_t while_uploading = 0;
    std::uint32_t while_building = 0;
    std::uint32_t other = 0;
};

struct Percentiles {
    double mean = 0.0;
    double median = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
    double max = 0.0;
    double min = 0.0;
};

// A log2-ish histogram over frame time, in the bands a human actually asks about.
inline constexpr std::array<double, 8> kHistogramEdgesMs{4.0, 6.0, 8.0, 11.0, 16.0, 20.0, 33.0, 50.0};

class FrameReport {
public:
    void add(const FrameRecord& record);

    // Frames whose telemetry is not comparable with the rest -- the loading screen, and the first
    // two frames on Vulkan, where a timestamp query would fault inside the NVIDIA driver
    // (CLAUDE.md; the existing SvoRenderer timer already skips them). Counted, not silently
    // dropped, because "how long was the world not there" is itself a number worth having.
    void add_warmup(double wallMs);

    [[nodiscard]] std::size_t frame_count() const noexcept { return records_.size(); }
    [[nodiscard]] std::size_t warmup_count() const noexcept { return warmup_; }
    [[nodiscard]] double warmup_seconds() const noexcept { return warmupMs_ / 1000.0; }
    [[nodiscard]] std::span<const FrameRecord> records() const noexcept { return records_; }

    [[nodiscard]] Percentiles frame_ms() const;
    [[nodiscard]] Percentiles gpu_ms() const;
    // Goal 220: the four named ranges, and how much of the whole-frame range they account for.
    [[nodiscard]] Percentiles gpu_pass_ms(int index) const; // 0=march 1=resolve 2=post 3=overlay 4=beam
    [[nodiscard]] Percentiles gpu_frame_ms() const;
    [[nodiscard]] double gpu_pass_coverage() const;
    [[nodiscard]] SlowFrameCounts slow_frames() const;
    [[nodiscard]] std::array<std::uint32_t, kHistogramEdgesMs.size() + 1> histogram() const;
    // The worst N frames by wall time, worst first.
    [[nodiscard]] std::vector<FrameRecord> worst(std::size_t count) const;

    // Goal 215's own check: the six phases must add up to the wall time. Returns the mean ratio
    // of phase sum to wall time across every recorded frame, and the count of frames whose ratio
    // falls outside +-1%. A number well under 1.0 means a phase is unaccounted for -- a finding,
    // not a tolerance to widen.
    [[nodiscard]] double phase_coverage() const;
    [[nodiscard]] std::size_t frames_outside_phase_tolerance(double tolerance = 0.01) const;

    // The last frame's counters, which is what "how much was resident" means for a run.
    [[nodiscard]] FrameCounters final_counters() const;
    [[nodiscard]] std::size_t peak_gpu_bytes() const;

    // The human-readable end-of-run block: count, the percentiles, the histogram, slow frames by
    // cause, and the worst five with their full breakdown.
    [[nodiscard]] std::string summary() const;

private:
    std::vector<FrameRecord> records_;
    std::size_t warmup_ = 0;
    double warmupMs_ = 0.0;
};

} // namespace dev::telemetry
