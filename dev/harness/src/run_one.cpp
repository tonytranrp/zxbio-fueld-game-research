// One scenario, on one backend. This file is the whole of the harness's execution logic, and it
// is deliberately small: everything that makes a frame happen lives in app_run.cpp, which
// voxel_app links too. What is here is the translation from a Scenario into an AppOptions plus a
// ScriptedInput plus a set of RunHooks -- and the evaluation afterwards.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "app_options.hpp"
#include "world/generation/field/macro_pipeline.hpp"
#include "app_run.hpp"
#include "dev/scenario/parser.hpp"
#include "dev/scenario/scenario.hpp"
#include "dev/telemetry/frame_report.hpp"
#include "dev/telemetry/json.hpp"
#include "engine/cli/parser.hpp"
#include "engine/core/log.hpp"
#include "harness_options.hpp"
#include "harness_run.hpp"
#include "image_compare.hpp"
#include "moire_metric.hpp"
#include "render/diligent/frame_verify.hpp"
#include "scripted_input.hpp"
#include "world/generation/heightmap_generator.hpp"
#include "world/player/fixed_step.hpp"
#include "world/player/tuning.hpp"

namespace dev::harness {

namespace {

using engine::core::log;
using engine::core::LogLevel;

[[nodiscard]] std::string backend_suffix(render::diligent::Backend backend) {
    return backend == render::diligent::Backend::Vulkan ? "vk" : "d3d12";
}

// The scenario's `option` lines, fed through voxel_app's OWN table. A typo in a .scn therefore
// produces the app's diagnostic, naming the option, rather than a second wording invented here.
[[nodiscard]] bool build_app_options(const scenario::Scenario& sc, const Options& harnessOptions,
                                     render::diligent::Backend backend, std::size_t scriptTicks,
                                     app::AppOptions& out, std::string& error) {
    const engine::cli::ParseOutcome parsed = engine::cli::parse(app::option_table(), &out, sc.options);
    if (!parsed.ok) {
        error = sc.name + ": " + parsed.message;
        return false;
    }
    out.backend = backend;
    // A scenario is a developer context by definition: it may ask for fly or noclip without every
    // .scn having to repeat --dev.
    out.dev = true;
    if (sc.pose) {
        out.start_pos = sc.pose->position;
        out.start_yaw_deg = sc.pose->yaw_deg;
        out.start_pitch_deg = sc.pose->pitch_deg;
    }
    if (sc.ground_pose) {
        // Resolved against the SAME generator the world is built from, and clamped to sea level
        // the same way tools/svo_render's --xz does -- so "0.3 m above the ground" over water means
        // 0.3 m above the water, not 0.3 m above a sea floor 60 m down.
        // WITH THE MACRO FIELD, because that is the world the app builds. Without it this resolved
        // `pose_ground` against the pre-Prompt-006 noise terrain -- a different surface, by tens of
        // metres -- so every ground pose in the library was spawning the body somewhere the ground
        // is not. Found in goal 336 by putting a CPU frame and a GPU frame of the same coordinates
        // side by side; the collision counter had been reporting it as "ticks inside solid" all
        // along.
        const world::generation::HeightmapGenerator heightmap(
            out.seed, out.svo.macro_field
                          ? world::generation::field::bake_playable_field(out.seed, out.svo.field_stages)
                          : nullptr);
        // The MAXIMUM over the body's footprint, not the height at its centre. The body is a
        // 0.6 m box; at (48, 0) the terrain falls ~0.6 m per metre, so the uphill corner of that
        // box sits ~0.19 m above the centre column, and a body spawned at the centre's surface is
        // 0.19 m INSIDE the uphill ground. That was the rest of goal 228's counter: after fixing
        // the analytic-vs-voxelised offset, spawn_stand still reported 181 of 181 ticks inside
        // solid, because a point height cannot place a box.
        float analytic = -std::numeric_limits<float>::infinity();
        constexpr float kHalf = world::player::kDefaultTuning.body_half_width;
        for (int corner = 0; corner < 5; ++corner) {
            const float dx = corner == 4 ? 0.0f : ((corner & 1) != 0 ? kHalf : -kHalf);
            const float dz = corner == 4 ? 0.0f : ((corner & 2) != 0 ? kHalf : -kHalf);
            analytic =
                std::max(analytic, heightmap.height_at(sc.ground_pose->xz.x + dx, sc.ground_pose->xz.y + dz));
        }
        analytic = std::max(analytic, world::player::kSeaLevelWorld);
        // ...and then SNAPPED UP TO THE VOXEL GRID, which is the surface the body actually stands
        // on. The sampler's rule is "a voxel is solid iff its BOTTOM is at or below the column's
        // surface height", so the voxel containing the analytic height is SOLID and its top is
        // above that height. Spawning the feet at the analytic height therefore puts them inside
        // the top solid voxel -- which is exactly what goal 228's new counter reported the moment
        // the analytic backstop stopped hiding it: `spawn_stand`, a scenario in which nothing
        // moves, logged 181 ticks with the body inside solid. Snapping up is the fix, and it is
        // the same arithmetic TerrainCollider::voxel_top_of has always used.
        const float voxelEdge = std::ldexp(1.0f, out.svo.voxel_size_log2);
        // Snapped up, plus TWO voxels of clearance. One is not enough and the reason is the
        // sampler's own rule: a voxel is solid iff its BOTTOM is at or below the column height
        // sampled at THAT VOXEL's min corner -- so no point sample of height_at can predict the
        // voxel top of a neighbouring column, and the measured miss was exactly one voxel
        // (feet 66.3438, uphill corner voxel top 66.3516, edge 0.0078). The second voxel is float
        // margin. The body then settles the remaining centimetre under gravity, which is what
        // walk mode is for -- and is why this and goal 231 landed together.
        const float ground = std::ceil(analytic / voxelEdge) * voxelEdge + 2.0f * voxelEdge;
        out.start_pos = glm::vec3{sc.ground_pose->xz.x, ground + sc.ground_pose->height_above_ground,
                                  sc.ground_pose->xz.y};
        out.start_yaw_deg = sc.ground_pose->yaw_deg;
        out.start_pitch_deg = sc.ground_pose->pitch_deg;
    }
    // The crosshair and the view polish are off under a scripted run for the same reason
    // --verify-frame turns them off: a HUD cross carries local contrast, and head-bob moves the
    // rendered eye off the body the assertions are about. A scenario can turn them back on with
    // its own `option --crosshair` line, because that line is parsed BEFORE this.
    // Goal 337: a scripted run gets a FIXED animation clock, so the wind is at the same phase at
    // the same scripted second on every run. Without it, three runs of one scenario measured canopy
    // motion at 0.122% / 0.255% / 0.192% -- a spread larger than anything a change would make.
    // 1/60 s per frame: the scenario clock's own nominal rate.
    if (out.svo_settings.fixed_anim_step <= 0.0f) {
        out.svo_settings.fixed_anim_step = 1.0f / 60.0f;
    }
    if (!out.crosshair.has_value()) {
        out.crosshair = false;
    }
    // The overlay is OFF for a scripted run unless the scenario asked for it, for the same reason
    // the crosshair is: its fps/ms/brick/VRAM digits change every run, and a golden that contains
    // them is holding still a picture of something that never holds still. A scenario that wants
    // to look at the overlay says `option --overlay`, and that line is parsed BEFORE this.
    if (std::find(sc.options.begin(), sc.options.end(), "--overlay") == sc.options.end()) {
        out.overlay = false;
    }
    // An assertion on contrast_percent is a request for the --verify-frame readback: turning it
    // on here rather than making every .scn say `option --verify-frame` keeps the assertion and
    // the measurement from being two things a scenario author has to remember to pair.
    for (const scenario::Assertion& assertion : sc.assertions) {
        if (assertion.metric == scenario::Metric::ContrastPercent) {
            out.verify_frame = true;
        }
    }
    // The SCRIPT is what ends a scenario run (ScriptedInput::exhausted()), not a frame count. The
    // budget below is only a safety ceiling so a hung scenario cannot hold a CI runner forever,
    // and it is deliberately generous because it also counts LOADING-SCREEN frames -- during which
    // the script does not advance at all, since update_camera_phase only runs once the world
    // exists. A tight `--frames` here is how the first version of this harness reported "0 frames
    // measured" on macro_ground: 240 frames all went to the ~2 s build.
    const auto scripted = static_cast<std::uint32_t>(scriptTicks);
    if (harnessOptions.frame_cap > 0) {
        out.frames = static_cast<std::uint32_t>(harnessOptions.frame_cap);
    } else if (out.frames == 0) {
        out.frames = std::max(6000u, scripted * 8u);
    }
    app::finalize(out);
    return true;
}

} // namespace

int run_one(const scenario::Scenario& sc, const Options& harnessOptions, render::diligent::Backend backend,
            RunResult& out) {
    out.backend = backend_suffix(backend);

    // The script, and the tick rate it is generated at -- the SAME constant world/player's
    // FixedStepper uses, so a `hold forward 3` is three seconds of simulation and not of wall
    // clock.
    // The SAME step constant world/player's FixedStepper defaults to, so a `hold forward 3` is
    // three seconds of SIMULATION rather than of wall clock.
    constexpr auto kStep = static_cast<float>(world::player::FixedStepper::kDefaultStep);
    ScriptedInput input(sc.segments, kStep);
    const std::size_t worstCaseTicks = scenario::MotionDriver(sc.segments, kStep).worst_case_ticks();

    app::AppOptions options;
    std::string error;
    if (!build_app_options(sc, harnessOptions, backend, worstCaseTicks, options, error)) {
        log(LogLevel::Error, "{}", error);
        out.passed = false;
        return EXIT_FAILURE;
    }

    const std::filesystem::path outDir{harnessOptions.out_dir};
    std::error_code ec;
    std::filesystem::create_directories(outDir, ec);
    const std::filesystem::path goldenDir = std::filesystem::path{harnessOptions.golden_root} /
                                            (sc.golden_dir.empty() ? sc.name : sc.golden_dir) / out.backend;

    CaptureSchedule schedule(sc);
    bool worldWasReady = false;
    // The capture point's LOGICAL name (what the .scn called it) kept beside the path the frame
    // was written to. They differ: the file carries a backend prefix so two backends' captures can
    // share an --out-dir, while the golden it is compared against lives in a per-backend directory
    // and is therefore named by the logical name alone. Conflating the two put `vk_shot.png` inside
    // `goldens/<scenario>/vk/`, which reads as a mistake even when it works.
    struct Captured {
        std::string logical;
        std::string stem;
        std::string path;
    };
    std::vector<Captured> captured;

    app::RunHooks hooks;
    hooks.on_frame = [&out](const telemetry::FrameRecord& record) { out.report.add(record); };
    hooks.on_warmup_frame = [&out](double wallMs) { out.report.add_warmup(wallMs); };
    hooks.on_invariants = [&out](std::uint32_t walkViolations, std::uint32_t insideSolid,
                                 std::uint32_t stanceChanges) {
        out.walk_violations = walkViolations;
        out.inside_solid_events = insideSolid;
        out.stance_changes = stanceChanges;
    };
    // The MAXIMUM over the run's capture points: a scenario that moves will have frames looking
    // at sky and frames looking at terrain, and "did this run ever show a world" is the question
    // the metric was built to answer.
    hooks.on_verify = [&out](float fraction) {
        out.contrast_percent = std::max(out.contrast_percent, fraction);
    };
    hooks.capture_name = [&](std::uint32_t frame, float seconds, bool worldReady, bool treeSwapped,
                             bool slowFrame, bool grounded) -> std::string {
        const bool readyThisFrame = worldReady && !worldWasReady;
        worldWasReady = worldWasReady || worldReady;
        std::string name = schedule.due(frame, seconds, treeSwapped, slowFrame, grounded, readyThisFrame);
        // The last frame: the loop is about to stop, either because the script ran out or because
        // the frame budget did. Asking here rather than after the loop is what lets `capture end`
        // use the same back-buffer readback every other capture point does.
        if (name.empty() && (input.exhausted() || frame + 1 >= options.frames)) {
            name = schedule.take_end();
        }
        if (name.empty()) {
            return {};
        }
        const std::string stem = (outDir / (out.backend + "_" + name)).string();
        captured.push_back(Captured{.logical = name, .stem = stem, .path = {}});
        return stem;
    };
    hooks.on_capture = [&captured](const std::string& stem, const std::string& path, bool written) {
        for (Captured& entry : captured) {
            if (entry.stem == stem) {
                entry.path = written ? path : std::string{};
            }
        }
    };

    app::Session session(options, !harnessOptions.headless);
    session.attach_overlay(); // no LiveInput: a scripted run's window receives no keyboard
    const auto started = std::chrono::steady_clock::now();
    const int code = options.renderer == app::RendererKind::Svo
                         ? app::run_svo(session, options, input, hooks)
                         : app::run_mesh(session, options, input, hooks);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    out.exit_code = code;

    // ---- captures, and the golden verdict -------------------------------------------------------
    for (const Captured& entry : captured) {
        const std::string& name = entry.logical;
        const std::string& path = entry.path;
        CaptureResult capture;
        capture.name = name;
        capture.path = path;
        capture.written = !path.empty();
        if (!capture.written) {
            log(LogLevel::Error, "capture \"{}\" was scheduled but no image was written", name);
            out.passed = false;
            out.captures.push_back(std::move(capture));
            continue;
        }
        // Prompt 005 goal 276: the aliasing metric, on EVERY capture and independently of goldens.
        // Deliberately not inside the `--no-golden` branch below -- a run with no reference image
        // can still answer "is this frame aliased", and that is the question this pass is about.
        // WORST over the run's captures, because a scenario that moves will have easy frames and
        // hard ones and the hard one is the one worth gating on.
        {
            const MoireResult moire = moire_ratio(load_png(path));
            capture.moire = moire.ratio;
            capture.moire_measured = moire.valid;
            if (moire.valid) {
                out.moire_ratio = std::max(out.moire_ratio, moire.ratio);
                out.moire_measured = true;
            }
            log(LogLevel::Info, "capture \"{}\": moire ratio {}", name,
                moire.valid ? std::to_string(moire.ratio) : ("n/a (" + moire.note + ")"));
        }

        // A scenario can declare a capture ungoldened, and that beats --accept-golden: the whole
        // point is that no run of this scenario produces a reference for this frame.
        const bool wantsGolden = [&] {
            for (const scenario::CapturePoint& p : sc.captures) {
                if (p.name == name) {
                    return p.golden;
                }
            }
            return true;
        }();
        if (!harnessOptions.no_golden && wantsGolden) {
            const std::filesystem::path golden = goldenDir / (name + ".png");
            capture.golden_path = golden.string();
            const Image actual = load_png(path);
            const Image reference = load_png(golden.string());
            capture.comparison = compare(reference, actual);
            capture.golden_checked = capture.comparison.comparable;
            if (harnessOptions.accept_golden) {
                // A promotion PRINTS the distance it is about to erase. Silently overwriting a
                // golden is how a regression becomes the new baseline.
                if (capture.comparison.comparable) {
                    log(LogLevel::Warn,
                        "promoting golden \"{}\": current distance mean {:.3f}/255, {:.3f}% of pixels "
                        "changed -- this becomes the new reference",
                        name, capture.comparison.mean_abs_difference,
                        capture.comparison.changed_pixel_fraction * 100.0);
                } else {
                    log(LogLevel::Info, "creating golden \"{}\" ({})", name, capture.comparison.note);
                }
                std::filesystem::create_directories(goldenDir, ec);
                if (!save_png(golden.string(), actual)) {
                    log(LogLevel::Error, "could not write golden {}", golden.string());
                    out.passed = false;
                }
            } else if (capture.comparison.comparable && !passes(capture.comparison)) {
                // A failure writes the WHERE, not just the how-much.
                const Image diff = difference_image(reference, actual);
                capture.diff_path = (outDir / (out.backend + "_" + name + "_diff.png")).string();
                (void)save_png(capture.diff_path, diff);
                log(LogLevel::Error,
                    "golden \"{}\": mean {:.3f}/255 (max {:.3f}), {:.3f}% pixels changed (max "
                    "{:.3f}%), worst channel {} -- diff written to {}",
                    name, capture.comparison.mean_abs_difference, kMaxMeanAbsDifference,
                    capture.comparison.changed_pixel_fraction * 100.0, kMaxChangedPixelFraction * 100.0,
                    capture.comparison.max_channel_difference, capture.diff_path);
                out.passed = false;
            } else if (capture.comparison.comparable) {
                log(LogLevel::Info, "golden \"{}\": PASS (mean {:.3f}/255, {:.4f}% pixels changed)", name,
                    capture.comparison.mean_abs_difference,
                    capture.comparison.changed_pixel_fraction * 100.0);
            } else {
                log(LogLevel::Warn, "golden \"{}\": not compared ({})", name, capture.comparison.note);
            }
        }
        out.captures.push_back(std::move(capture));
    }
    for (const std::string& missed : schedule.unfired()) {
        log(LogLevel::Error, "capture point \"{}\" never fired -- its condition did not happen", missed);
        out.passed = false;
    }

    // ---- assertions ------------------------------------------------------------------------------
    const scenario::BackendSelection running = backend == render::diligent::Backend::Vulkan
                                                   ? scenario::BackendSelection::Vulkan
                                                   : scenario::BackendSelection::D3D12;
    for (const scenario::Assertion& assertion : sc.assertions) {
        // Goal 273: an assertion narrowed to the other backend is not this run's business. It is
        // SKIPPED silently rather than passed, so a per-backend budget cannot be mistaken for one
        // that held everywhere.
        if (!scenario::applies_to(assertion, running)) {
            continue;
        }
        AssertionResult result;
        result.assertion = assertion;
        result.measured = measure(assertion.metric, out);
        result.passed = scenario::evaluate(assertion.op, result.measured, assertion.value);
        if (!result.passed) {
            // One line, with the metric, the threshold and the measured value -- goal 211's Check.
            log(LogLevel::Error, "ASSERTION FAILED: {} {} {} (measured {:.4g})",
                scenario::metric_name(assertion.metric), scenario::op_name(assertion.op), assertion.value,
                result.measured);
            out.passed = false;
        } else {
            log(LogLevel::Info, "assert {} {} {}{}: measured {:.4g} PASS",
                scenario::metric_name(assertion.metric), scenario::op_name(assertion.op), assertion.value,
                assertion.backend == scenario::BackendSelection::Both
                    ? std::string{}
                    : " [" + std::string{scenario::backend_name(assertion.backend)} + "]",
                result.measured);
        }
        out.assertions.push_back(result);
    }

    std::fputs(out.report.summary().c_str(), stdout);
    log(LogLevel::Info, "scenario \"{}\" on {}: {} in {:.1f}s", sc.name, out.backend,
        out.passed && code == EXIT_SUCCESS ? "PASS" : "FAIL", seconds);

    return out.passed && code == EXIT_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace dev::harness
