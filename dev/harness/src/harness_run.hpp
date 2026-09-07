#pragma once

// The types shared between the harness's entry point and its runner. One place, so the console
// line, the JSON report and the exit code cannot disagree about whether a run passed.

#include <cstdint>
#include <string>
#include <vector>

#include "dev/scenario/assertion.hpp"
#include "dev/scenario/capture.hpp"
#include "dev/scenario/scenario.hpp"
#include "dev/telemetry/frame_report.hpp"
#include "harness_options.hpp"
#include "image_compare.hpp"
#include "render/diligent/render_context.hpp"

namespace dev::harness {

struct AssertionResult {
    scenario::Assertion assertion;
    double measured = 0.0;
    bool passed = false;
};

struct CaptureResult {
    std::string name;
    std::string path;
    bool written = false;
    Comparison comparison;
    bool golden_checked = false;
    std::string golden_path;
    std::string diff_path;
};

struct RunResult {
    std::string backend;
    int exit_code = 0;
    telemetry::FrameReport report;
    std::vector<CaptureResult> captures;
    std::vector<AssertionResult> assertions;
    float contrast_percent = 0.0f;
    std::uint32_t walk_violations = 0;
    std::uint32_t inside_solid_events = 0;
    bool passed = true;
};

// Every metric a scenario can assert, read out of the finished run. Adding a Metric enumerator
// without adding it here is a compile error (the switch is exhaustive and warnings are errors),
// which is the intended friction: an assertion must not be able to ask about something the report
// does not carry.
[[nodiscard]] double measure(scenario::Metric metric, const RunResult& run);

// Resolves a scenario's capture points against the frame the loop is on. Stateful: each point
// fires at most once, which is what "the FIRST tree swap" means.
class CaptureSchedule {
public:
    explicit CaptureSchedule(const scenario::Scenario& s)
        : points_(s.captures), fired_(s.captures.size(), false) {}

    [[nodiscard]] std::string due(std::uint32_t frame, float seconds, bool treeSwapped, bool slowFrame,
                                  bool grounded, bool worldReadyThisFrame);
    // The `capture end` points, taken when the loop is known to be on its last frame.
    [[nodiscard]] std::string take_end();
    // Points that never fired: a scenario asking for an event that did not happen SAYS so rather
    // than quietly producing one fewer image than it promised.
    [[nodiscard]] std::vector<std::string> unfired() const;

private:
    std::vector<scenario::CapturePoint> points_;
    std::vector<bool> fired_;
};

// One scenario, one backend. Returns EXIT_SUCCESS only if every assertion and every golden passed.
int run_one(const scenario::Scenario& sc, const Options& harnessOptions, render::diligent::Backend backend,
            RunResult& out);

// Goal 219. Re-runs `sc` once per value of the option named in `harnessOptions.ramp`, holding the
// pose fixed, and prints the table. `mean_primary_steps` comes from a second pass per rung with
// --debug-view steps, whose shader writes steps/256 into the frame -- so the number is the
// marcher's own step count, read back, not an estimate.
struct RampRung {
    std::string value;
    bool measured = false; // false = the rung produced no frames (a failed build, not a slow one)
    std::size_t bricks = 0;
    double resident_mb = 0.0;
    std::size_t node_words = 0;
    double mean_primary_steps = 0.0;
    double gpu_ms_median = 0.0;
    double gpu_ms_p95 = 0.0;
    double frame_ms_median = 0.0;
    double fps_from_frame_ms = 0.0;
};

int run_ramp(const scenario::Scenario& sc, const Options& harnessOptions,
             render::diligent::Backend backend, std::vector<RampRung>& out);

// The machine-readable report of goal 216.
[[nodiscard]] bool write_report(const std::string& path, const scenario::Scenario& sc,
                                const Options& harnessOptions, const std::vector<RunResult>& runs);

} // namespace dev::harness
