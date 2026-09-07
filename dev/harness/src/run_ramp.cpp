// Goal 219: the voxel-throughput yardstick.
//
// "How many voxels can this GPU actually march at 150 fps" answered with a number instead of an
// opinion. Sweeps one option across a declared ladder, holding the pose fixed, and tabulates what
// is resident against what it costs. The result is what Prompt 003 (and 004) are judged against.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/core/log.hpp"
#include "harness_run.hpp"
#include "image_compare.hpp"

namespace dev::harness {

namespace {

using engine::core::log;
using engine::core::LogLevel;

// "lod-radius:1,2,4,8,16" -> ("lod-radius", {"1","2","4","8","16"})
[[nodiscard]] bool split_ramp(const std::string& text, std::string& option,
                              std::vector<std::string>& values) {
    const std::size_t colon = text.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size()) {
        return false;
    }
    option = text.substr(0, colon);
    std::size_t begin = colon + 1;
    while (begin <= text.size()) {
        const std::size_t comma = text.find(',', begin);
        const std::string value =
            comma == std::string::npos ? text.substr(begin) : text.substr(begin, comma - begin);
        if (value.empty()) {
            return false;
        }
        values.push_back(value);
        if (comma == std::string::npos) {
            break;
        }
        begin = comma + 1;
    }
    return !values.empty();
}

// The `steps` debug view writes the primary ray's traversal iteration count divided by 256 into
// the frame. Averaging it back out is the only way to get the marcher's OWN step count without
// adding a counter to the shader -- and the shader is out of scope for this prompt.
[[nodiscard]] double mean_steps_from(const std::string& path) {
    const Image image = load_png(path);
    if (image.empty()) {
        return 0.0;
    }
    std::uint64_t total = 0;
    const std::size_t pixels = static_cast<std::size_t>(image.width) * image.height;
    for (std::size_t i = 0; i < pixels; ++i) {
        total += image.rgb[i * 3]; // the view is a greyscale ramp; any channel does
    }
    const double mean255 = static_cast<double>(total) / static_cast<double>(pixels);
    return mean255 / 255.0 * 256.0;
}

} // namespace

int run_ramp(const scenario::Scenario& sc, const Options& harnessOptions, render::diligent::Backend backend,
             std::vector<RampRung>& out) {
    std::string option;
    std::vector<std::string> values;
    if (!split_ramp(harnessOptions.ramp, option, values)) {
        log(LogLevel::Error, "--ramp expects NAME:v1,v2,... (got \"{}\")", harnessOptions.ramp);
        return EXIT_FAILURE;
    }

    for (const std::string& value : values) {
        // A rung is a REAL run of a real scenario with one extra option, not a special measurement
        // path -- so a number in this table is the same kind of number the rest of the report
        // carries, and a rung can be reproduced by hand.
        scenario::Scenario rung = sc;
        rung.options.push_back("--" + option);
        rung.options.push_back(value);
        // No golden comparison across a ramp: every rung renders a deliberately different image.
        Options rungOptions = harnessOptions;
        rungOptions.no_golden = true;
        rungOptions.accept_golden = false;
        rungOptions.ramp.clear();

        RunResult run;
        (void)run_one(rung, rungOptions, backend, run);

        RampRung row;
        row.value = value;
        const telemetry::FrameCounters counters = run.report.final_counters();
        row.bricks = counters.bricks;
        row.resident_mb = static_cast<double>(counters.resident_bytes) / 1.0e6;
        row.node_words = counters.internal_nodes;
        row.gpu_ms_median = run.report.gpu_ms().median;
        row.gpu_ms_p95 = run.report.gpu_ms().p95;
        row.frame_ms_median = run.report.frame_ms().median;
        row.fps_from_frame_ms = row.frame_ms_median > 0.0 ? 1000.0 / row.frame_ms_median : 0.0;

        // Second pass for the step count. --debug-view steps implies --no-post, so this run's
        // frame times are NOT comparable with the first pass's and are deliberately discarded.
        scenario::Scenario stepsRung = rung;
        stepsRung.options.push_back("--debug-view");
        stepsRung.options.push_back("steps");
        stepsRung.captures.clear();
        stepsRung.captures.push_back(
            scenario::CapturePoint{.when = scenario::CaptureWhen::End, .name = "ramp_steps_" + value});
        stepsRung.assertions.clear();
        RunResult stepsRun;
        (void)run_one(stepsRung, rungOptions, backend, stepsRun);
        if (!stepsRun.captures.empty() && stepsRun.captures.front().written) {
            row.mean_primary_steps = mean_steps_from(stepsRun.captures.front().path);
        }

        out.push_back(row);
    }

    // The table.
    std::printf("\nthroughput ramp: --%s across %zu rungs, %s, pose held fixed\n", option.c_str(),
                values.size(), backend == render::diligent::Backend::Vulkan ? "Vulkan" : "D3D12");
    std::printf("%10s %12s %12s %14s %12s %12s %12s %8s\n", option.c_str(), "bricks", "resident MB",
                "internal nodes", "mean steps", "gpu ms p50", "gpu ms p95", "fps");
    for (const RampRung& row : out) {
        std::printf("%10s %12zu %12.1f %14zu %12.1f %12.2f %12.2f %8.1f\n", row.value.c_str(), row.bricks,
                    row.resident_mb, row.node_words, row.mean_primary_steps, row.gpu_ms_median,
                    row.gpu_ms_p95, row.fps_from_frame_ms);
    }

    // Monotonicity is a FINDING, not an assertion: goal 219 says that if resident detail and GPU
    // cost are not monotone together, something else is the bottleneck -- and that is worth saying
    // out loud rather than failing a run over.
    bool monotone = true;
    for (std::size_t i = 1; i < out.size(); ++i) {
        const bool moreDetail = out[i].bricks > out[i - 1].bricks;
        const bool moreCost = out[i].gpu_ms_median > out[i - 1].gpu_ms_median;
        if (moreDetail != moreCost) {
            monotone = false;
            std::printf("  NOT MONOTONE between %s and %s: bricks %zu -> %zu but gpu %0.2f -> %0.2f ms\n",
                        out[i - 1].value.c_str(), out[i].value.c_str(), out[i - 1].bricks, out[i].bricks,
                        out[i - 1].gpu_ms_median, out[i].gpu_ms_median);
        }
    }
    std::printf("  resident detail and GPU cost move together: %s\n", monotone ? "yes" : "NO -- see above");

    // The two rungs a budget is actually set from.
    const char* below150 = "never";
    const char* below60 = "never";
    for (const RampRung& row : out) {
        if (row.fps_from_frame_ms < 150.0 && std::string{below150} == "never") {
            below150 = row.value.c_str();
        }
        if (row.fps_from_frame_ms < 60.0 && std::string{below60} == "never") {
            below60 = row.value.c_str();
        }
    }
    std::printf("  first rung under 150 fps: %s\n  first rung under 60 fps: %s\n", below150, below60);
    return EXIT_SUCCESS;
}

} // namespace dev::harness
