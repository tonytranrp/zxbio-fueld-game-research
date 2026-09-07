#include "dev/telemetry/frame_report.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace dev::telemetry {

namespace {

// Nearest-rank percentile on an already-sorted vector. Nearest-rank rather than interpolated
// because these are frame times: the p99 of a 900-frame run should BE one of the nine worst
// frames, not a number between two of them that never happened.
[[nodiscard]] double percentile(const std::vector<double>& sorted, double fraction) {
    if (sorted.empty()) {
        return 0.0;
    }
    const double rank = fraction * static_cast<double>(sorted.size());
    auto index = static_cast<std::size_t>(std::ceil(rank));
    if (index == 0) {
        index = 1;
    }
    if (index > sorted.size()) {
        index = sorted.size();
    }
    return sorted[index - 1];
}

[[nodiscard]] Percentiles summarize(std::vector<double> values) {
    Percentiles p;
    if (values.empty()) {
        return p;
    }
    std::sort(values.begin(), values.end());
    double total = 0.0;
    for (const double v : values) {
        total += v;
    }
    p.mean = total / static_cast<double>(values.size());
    p.median = percentile(values, 0.50);
    p.p95 = percentile(values, 0.95);
    p.p99 = percentile(values, 0.99);
    p.min = values.front();
    p.max = values.back();
    return p;
}

} // namespace

void FrameReport::add(const FrameRecord& record) {
    records_.push_back(record);
}

void FrameReport::add_warmup(double wallMs) {
    ++warmup_;
    warmupMs_ += wallMs;
}

Percentiles FrameReport::frame_ms() const {
    std::vector<double> values;
    values.reserve(records_.size());
    for (const FrameRecord& r : records_) {
        values.push_back(r.wall_ms);
    }
    return summarize(std::move(values));
}

Percentiles FrameReport::gpu_ms() const {
    std::vector<double> values;
    values.reserve(records_.size());
    for (const FrameRecord& r : records_) {
        // A zero is "no valid query yet", not "the GPU took no time" -- the Vulkan first-command
        // fault means the first frames have none. Including them would drag every percentile down
        // and make the instrumentation-cost measurement in goal 220 read low.
        if (r.counters.gpu_ms > 0.0) {
            values.push_back(r.counters.gpu_ms);
        }
    }
    return summarize(std::move(values));
}

Percentiles FrameReport::gpu_pass_ms(int index) const {
    std::vector<double> values;
    values.reserve(records_.size());
    for (const FrameRecord& r : records_) {
        if (r.counters.gpu_frame_ms <= 0.0) {
            continue; // no valid query on this frame
        }
        switch (index) {
        case 0:
            values.push_back(r.counters.gpu_march_ms);
            break;
        case 1:
            values.push_back(r.counters.gpu_resolve_ms);
            break;
        case 2:
            values.push_back(r.counters.gpu_post_ms);
            break;
        default:
            values.push_back(r.counters.gpu_overlay_ms);
            break;
        }
    }
    return summarize(std::move(values));
}

Percentiles FrameReport::gpu_frame_ms() const {
    std::vector<double> values;
    values.reserve(records_.size());
    for (const FrameRecord& r : records_) {
        if (r.counters.gpu_frame_ms > 0.0) {
            values.push_back(r.counters.gpu_frame_ms);
        }
    }
    return summarize(std::move(values));
}

double FrameReport::gpu_pass_coverage() const {
    double total = 0.0;
    std::size_t counted = 0;
    for (const FrameRecord& r : records_) {
        if (r.counters.gpu_frame_ms <= 0.0) {
            continue;
        }
        total += r.counters.gpu_pass_sum() / r.counters.gpu_frame_ms;
        ++counted;
    }
    return counted == 0 ? 0.0 : total / static_cast<double>(counted);
}

SlowFrameCounts FrameReport::slow_frames() const {
    SlowFrameCounts counts;
    for (const FrameRecord& r : records_) {
        if (r.wall_ms <= kSlowFrameMs) {
            continue;
        }
        ++counts.total;
        // Same priority order run_svo used, so the numbers are comparable with the previous pass's:
        // a swap subsumes an upload subsumes a build.
        if (r.causes.swapped) {
            ++counts.on_swap;
        } else if (r.causes.uploading) {
            ++counts.while_uploading;
        } else if (r.causes.building) {
            ++counts.while_building;
        } else {
            ++counts.other;
        }
    }
    return counts;
}

std::array<std::uint32_t, kHistogramEdgesMs.size() + 1> FrameReport::histogram() const {
    std::array<std::uint32_t, kHistogramEdgesMs.size() + 1> bins{};
    for (const FrameRecord& r : records_) {
        std::size_t bin = kHistogramEdgesMs.size();
        for (std::size_t i = 0; i < kHistogramEdgesMs.size(); ++i) {
            if (r.wall_ms < kHistogramEdgesMs[i]) {
                bin = i;
                break;
            }
        }
        ++bins[bin];
    }
    return bins;
}

std::vector<FrameRecord> FrameReport::worst(std::size_t count) const {
    std::vector<FrameRecord> sorted = records_;
    const std::size_t n = std::min(count, sorted.size());
    std::partial_sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(n), sorted.end(),
                      [](const FrameRecord& a, const FrameRecord& b) { return a.wall_ms > b.wall_ms; });
    sorted.resize(n);
    return sorted;
}

double FrameReport::phase_coverage() const {
    if (records_.empty()) {
        return 0.0;
    }
    double total = 0.0;
    std::size_t counted = 0;
    for (const FrameRecord& r : records_) {
        if (r.wall_ms <= 0.0) {
            continue;
        }
        total += r.phases.sum() / r.wall_ms;
        ++counted;
    }
    return counted == 0 ? 0.0 : total / static_cast<double>(counted);
}

std::size_t FrameReport::frames_outside_phase_tolerance(double tolerance) const {
    std::size_t outside = 0;
    for (const FrameRecord& r : records_) {
        if (r.wall_ms <= 0.0) {
            continue;
        }
        const double ratio = r.phases.sum() / r.wall_ms;
        if (ratio < 1.0 - tolerance || ratio > 1.0 + tolerance) {
            ++outside;
        }
    }
    return outside;
}

FrameCounters FrameReport::final_counters() const {
    return records_.empty() ? FrameCounters{} : records_.back().counters;
}

std::size_t FrameReport::peak_gpu_bytes() const {
    std::size_t peak = 0;
    for (const FrameRecord& r : records_) {
        peak = std::max(peak, r.counters.gpu_bytes);
    }
    return peak;
}

std::string FrameReport::summary() const {
    std::string out;
    char line[512];
    const Percentiles frame = frame_ms();
    const Percentiles gpu = gpu_ms();
    const SlowFrameCounts slow = slow_frames();

    std::snprintf(line, sizeof(line), "frames: %zu measured (+%zu warm-up over %.1f s)\n", records_.size(),
                  warmup_, warmup_seconds());
    out += line;
    std::snprintf(line, sizeof(line),
                  "frame ms: mean %.2f  median %.2f  p95 %.2f  p99 %.2f  max %.2f  min %.2f\n", frame.mean,
                  frame.median, frame.p95, frame.p99, frame.max, frame.min);
    out += line;
    std::snprintf(line, sizeof(line), "gpu ms  : mean %.2f  median %.2f  p95 %.2f  p99 %.2f  max %.2f\n",
                  gpu.mean, gpu.median, gpu.p95, gpu.p99, gpu.max);
    out += line;

    out += "histogram (ms):";
    const auto bins = histogram();
    double previous = 0.0;
    for (std::size_t i = 0; i < kHistogramEdgesMs.size(); ++i) {
        std::snprintf(line, sizeof(line), " [%.0f,%.0f)=%u", previous, kHistogramEdgesMs[i], bins[i]);
        out += line;
        previous = kHistogramEdgesMs[i];
    }
    std::snprintf(line, sizeof(line), " [%.0f,inf)=%u\n", previous, bins.back());
    out += line;

    std::snprintf(line, sizeof(line),
                  "slow frames (> %.0f ms): %u of %zu -- %u on a tree swap, %u while uploading a "
                  "slice, %u while only building, %u other\n",
                  kSlowFrameMs, slow.total, records_.size(), slow.on_swap, slow.while_uploading,
                  slow.while_building, slow.other);
    out += line;

    std::snprintf(line, sizeof(line), "phase coverage: %.1f%% of wall time, %zu frames outside +-1%%\n",
                  phase_coverage() * 100.0, frames_outside_phase_tolerance());
    out += line;

    // Goal 220: the four named ranges and how much of the whole-frame range they account for.
    std::snprintf(line, sizeof(line),
                  "gpu passes (median ms): march %.2f  resolve %.2f  post %.2f  overlay %.2f  |  whole "
                  "frame %.2f  |  sum accounts for %.1f%%\n",
                  gpu_pass_ms(0).median, gpu_pass_ms(1).median, gpu_pass_ms(2).median, gpu_pass_ms(3).median,
                  gpu_frame_ms().median, gpu_pass_coverage() * 100.0);
    out += line;

    out += "worst five frames:\n";
    for (const FrameRecord& r : worst(5)) {
        std::snprintf(line, sizeof(line),
                      "  frame %u: %.1f ms = start %.1f + upload %.1f + camera %.1f + render %.1f + "
                      "post %.1f + overlay %.1f + present %.1f + capture %.1f%s%s%s%s\n",
                      r.index, r.wall_ms, r.phases.frame_start, r.phases.upload, r.phases.camera,
                      r.phases.render, r.phases.post, r.phases.overlay, r.phases.present, r.phases.capture,
                      r.causes.swapped ? " [tree swapped]" : "", r.causes.uploading ? " [uploading]" : "",
                      r.causes.building ? " [building]" : "", r.causes.refreshed ? " [cache refreshed]" : "");
        out += line;
    }
    return out;
}

} // namespace dev::telemetry
