#include "harness_run.hpp"

#include <algorithm>

namespace dev::harness {

double measure(scenario::Metric metric, const RunResult& run) {
    const telemetry::Percentiles frame = run.report.frame_ms();
    const telemetry::Percentiles gpu = run.report.gpu_ms();
    const telemetry::SlowFrameCounts slow = run.report.slow_frames();
    const telemetry::FrameCounters counters = run.report.final_counters();
    double worstGolden = 0.0;
    for (const CaptureResult& capture : run.captures) {
        if (capture.golden_checked && capture.comparison.comparable) {
            worstGolden = std::max(worstGolden, capture.comparison.mean_abs_difference);
        }
    }
    switch (metric) {
    case scenario::Metric::Frames:
        return static_cast<double>(run.report.frame_count());
    case scenario::Metric::FrameMsMean:
        return frame.mean;
    case scenario::Metric::FrameMsMedian:
        return frame.median;
    case scenario::Metric::FrameMsP95:
        return frame.p95;
    case scenario::Metric::FrameMsP99:
        return frame.p99;
    case scenario::Metric::FrameMsMax:
        return frame.max;
    case scenario::Metric::SlowFrames:
        return slow.total;
    case scenario::Metric::SlowFramesOnSwap:
        return slow.on_swap;
    case scenario::Metric::SlowFramesUploading:
        return slow.while_uploading;
    case scenario::Metric::SlowFramesBuilding:
        return slow.while_building;
    case scenario::Metric::GpuMsMean:
        return gpu.mean;
    case scenario::Metric::GpuMsMedian:
        return gpu.median;
    case scenario::Metric::GpuMsP95:
        return gpu.p95;
    case scenario::Metric::GpuMsMax:
        return gpu.max;
    case scenario::Metric::GpuMemoryMb:
        return static_cast<double>(run.report.peak_gpu_bytes()) / (1024.0 * 1024.0);
    case scenario::Metric::ResidentBricks:
        return static_cast<double>(counters.bricks);
    case scenario::Metric::ResidentMb:
        return static_cast<double>(counters.resident_bytes) / 1.0e6;
    case scenario::Metric::ContrastPercent:
        return static_cast<double>(run.contrast_percent) * 100.0;
    case scenario::Metric::GoldenDistance:
        return worstGolden;
    case scenario::Metric::WalkViolations:
        return run.walk_violations;
    case scenario::Metric::InsideSolid:
        return run.inside_solid_events;
    case scenario::Metric::StanceChanges:
        return run.stance_changes;
    case scenario::Metric::Uploads:
        return static_cast<double>(counters.uploads);
    }
    return 0.0;
}

std::string CaptureSchedule::due(std::uint32_t frame, float seconds, bool treeSwapped, bool slowFrame,
                                 bool grounded, bool worldReadyThisFrame) {
    for (std::size_t i = 0; i < points_.size(); ++i) {
        if (fired_[i]) {
            continue;
        }
        const scenario::CapturePoint& p = points_[i];
        bool hit = false;
        switch (p.when) {
        case scenario::CaptureWhen::Frame:
            hit = frame >= p.frame;
            break;
        case scenario::CaptureWhen::Seconds:
            hit = seconds >= p.seconds;
            break;
        case scenario::CaptureWhen::Event:
            switch (p.event) {
            case scenario::CaptureEvent::FirstTreeSwapped:
                hit = treeSwapped;
                break;
            case scenario::CaptureEvent::FirstGrounded:
                hit = grounded;
                break;
            case scenario::CaptureEvent::FirstSlowFrame:
                hit = slowFrame;
                break;
            case scenario::CaptureEvent::WorldReady:
                hit = worldReadyThisFrame;
                break;
            }
            break;
        case scenario::CaptureWhen::End:
            continue; // there is no "last frame" until it happens; take_end() handles these
        }
        if (hit) {
            fired_[i] = true;
            return p.name;
        }
    }
    return {};
}

std::string CaptureSchedule::take_end() {
    for (std::size_t i = 0; i < points_.size(); ++i) {
        if (!fired_[i] && points_[i].when == scenario::CaptureWhen::End) {
            fired_[i] = true;
            return points_[i].name;
        }
    }
    return {};
}

std::vector<std::string> CaptureSchedule::unfired() const {
    std::vector<std::string> names;
    for (std::size_t i = 0; i < points_.size(); ++i) {
        if (!fired_[i]) {
            names.push_back(points_[i].name);
        }
    }
    return names;
}

} // namespace dev::harness
