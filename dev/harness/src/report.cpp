// The machine-readable run report (Prompt 002 goal 216). One object per invocation, containing
// everything a later reader needs to know WHAT was run as well as what came out of it -- the git
// SHA, the build preset, the GPU, and every option that differed from its default -- because a
// baseline whose provenance is unknown is not a baseline.

#include "harness_run.hpp"

#include <ctime>
#include <string>

#include "dev/scenario/parser.hpp"
#include "dev/telemetry/json.hpp"

#ifndef VOXEL_GIT_SHA
#define VOXEL_GIT_SHA "unknown"
#endif
#ifndef VOXEL_BUILD_PRESET
#define VOXEL_BUILD_PRESET "unknown"
#endif

namespace dev::harness {

namespace {

void write_percentiles(telemetry::JsonWriter& json, const char* name, const telemetry::Percentiles& p) {
    json.key(name);
    json.begin_object();
    json.field("mean", p.mean);
    json.field("median", p.median);
    json.field("p95", p.p95);
    json.field("p99", p.p99);
    json.field("max", p.max);
    json.field("min", p.min);
    json.end_object();
}

[[nodiscard]] std::string iso_timestamp() {
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
#if defined(_MSC_VER)
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

} // namespace

bool write_report(const std::string& path, const scenario::Scenario& sc, const Options& harnessOptions,
                  const std::vector<RunResult>& runs) {
    telemetry::JsonWriter json;
    json.begin_object();

    // ---- provenance. Everything in this block is a TIMING-INDEPENDENT field: goal 216's Check is
    // that two runs of the same scenario differ only in the timing fields and the timestamp, and
    // `timestamp` is the one key here that is expected to move.
    json.field("schema", 1);
    json.field("scenario", sc.name);
    json.field("description", sc.description);
    json.field("scenario_source", sc.source);
    json.field("git_sha", VOXEL_GIT_SHA);
    json.field("build_preset", VOXEL_BUILD_PRESET);
    json.field("headless", harnessOptions.headless);
    json.field("timestamp", iso_timestamp());
    // The scenario's own options, verbatim: "every option that differed from its default", stated
    // as the list that produced them rather than as a diff computed against a default AppOptions
    // (which would silently omit an option whose scenario value happens to equal its default --
    // and that is exactly the case a reader most wants to see stated).
    json.key("options");
    json.begin_array();
    for (const std::string& option : sc.options) {
        json.value(option);
    }
    json.end_array();
    // The whole scenario, re-emitted. A report you cannot reproduce the run from is a number
    // without an experiment; emit_scenario() round-trips, which the format test pins.
    json.field("scenario_text", scenario::emit_scenario(sc));

    json.key("runs");
    json.begin_array();
    for (const RunResult& run : runs) {
        json.begin_object();
        json.field("backend", run.backend);
        json.field("passed", run.passed && run.exit_code == 0);
        json.field("exit_code", run.exit_code);
        json.field("frames", run.report.frame_count());
        json.field("warmup_frames", run.report.warmup_count());
        json.field("warmup_seconds", run.report.warmup_seconds());
        json.field("contrast_percent", static_cast<double>(run.contrast_percent) * 100.0);
        json.field("moire_ratio", run.moire_ratio);
        json.field("moire_measured", run.moire_measured);
        json.field("walk_violations", run.walk_violations);
        json.field("inside_solid_events", run.inside_solid_events);

        write_percentiles(json, "frame_ms", run.report.frame_ms());
        write_percentiles(json, "gpu_ms", run.report.gpu_ms());
        write_percentiles(json, "gpu_frame_ms", run.report.gpu_frame_ms());
        write_percentiles(json, "gpu_march_ms", run.report.gpu_pass_ms(0));
        write_percentiles(json, "gpu_resolve_ms", run.report.gpu_pass_ms(1));
        write_percentiles(json, "gpu_post_ms", run.report.gpu_pass_ms(2));
        write_percentiles(json, "gpu_overlay_ms", run.report.gpu_pass_ms(3));
        json.field("gpu_pass_coverage", run.report.gpu_pass_coverage());

        const telemetry::SlowFrameCounts slow = run.report.slow_frames();
        json.key("slow_frames");
        json.begin_object();
        json.field("threshold_ms", telemetry::kSlowFrameMs);
        json.field("total", slow.total);
        json.field("on_swap", slow.on_swap);
        json.field("while_uploading", slow.while_uploading);
        json.field("while_building", slow.while_building);
        json.field("other", slow.other);
        json.end_object();

        json.key("histogram");
        json.begin_object();
        const auto bins = run.report.histogram();
        double previous = 0.0;
        for (std::size_t i = 0; i < telemetry::kHistogramEdgesMs.size(); ++i) {
            char label[32];
            std::snprintf(label, sizeof(label), "%.0f-%.0f", previous, telemetry::kHistogramEdgesMs[i]);
            json.field(label, bins[i]);
            previous = telemetry::kHistogramEdgesMs[i];
        }
        char tail[32];
        std::snprintf(tail, sizeof(tail), "%.0f+", previous);
        json.field(tail, bins.back());
        json.end_object();

        json.key("phase_accounting");
        json.begin_object();
        json.field("coverage", run.report.phase_coverage());
        json.field("frames_outside_1pct", run.report.frames_outside_phase_tolerance());
        json.end_object();

        const telemetry::FrameCounters counters = run.report.final_counters();
        json.key("counters");
        json.begin_object();
        json.field("bricks", counters.bricks);
        json.field("internal_nodes", counters.internal_nodes);
        json.field("solid_leaves", counters.solid_leaves);
        json.field("resident_mb", static_cast<double>(counters.resident_bytes) / 1.0e6);
        json.field("peak_gpu_mb", static_cast<double>(run.report.peak_gpu_bytes()) / (1024.0 * 1024.0));
        json.field("uploads", counters.uploads);
        json.end_object();

        json.key("worst_frames");
        json.begin_array();
        for (const telemetry::FrameRecord& record : run.report.worst(5)) {
            json.begin_object();
            json.field("index", record.index);
            json.field("wall_ms", record.wall_ms);
            json.field("frame_start_ms", record.phases.frame_start);
            json.field("upload_ms", record.phases.upload);
            json.field("camera_ms", record.phases.camera);
            json.field("render_ms", record.phases.render);
            json.field("post_ms", record.phases.post);
            json.field("overlay_ms", record.phases.overlay);
            json.field("present_ms", record.phases.present);
            json.field("capture_ms", record.phases.capture);
            json.field("swapped", record.causes.swapped);
            json.field("uploading", record.causes.uploading);
            json.field("building", record.causes.building);
            json.field("refreshed", record.causes.refreshed);
            json.end_object();
        }
        json.end_array();

        json.key("captures");
        json.begin_array();
        for (const CaptureResult& capture : run.captures) {
            json.begin_object();
            json.field("name", capture.name);
            json.field("path", capture.path);
            json.field("written", capture.written);
            json.field("golden", capture.golden_path);
            json.field("golden_checked", capture.golden_checked);
            if (capture.golden_checked) {
                json.field("mean_abs_difference", capture.comparison.mean_abs_difference);
                json.field("changed_pixel_fraction", capture.comparison.changed_pixel_fraction);
                json.field("max_channel_difference", capture.comparison.max_channel_difference);
                json.field("passed", passes(capture.comparison));
                json.field("threshold_mean_abs_difference", kMaxMeanAbsDifference);
                json.field("threshold_changed_pixel_fraction", kMaxChangedPixelFraction);
            } else {
                json.field("note", capture.comparison.note);
            }
            if (!capture.diff_path.empty()) {
                json.field("diff", capture.diff_path);
            }
            json.end_object();
        }
        json.end_array();

        json.key("assertions");
        json.begin_array();
        for (const AssertionResult& assertion : run.assertions) {
            json.begin_object();
            json.field("metric", scenario::metric_name(assertion.assertion.metric));
            json.field("op", scenario::op_name(assertion.assertion.op));
            json.field("threshold", assertion.assertion.value);
            json.field("measured", assertion.measured);
            json.field("passed", assertion.passed);
            json.end_object();
        }
        json.end_array();

        json.end_object();
    }
    json.end_array();
    json.end_object();
    return json.write_to(path);
}

} // namespace dev::harness
