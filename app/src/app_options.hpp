#pragma once

// voxel_app's option table (Prompt 002 goal 204). Lives beside AppOptions rather than in
// engine/cli, because a table names its own program's struct -- engine/cli is the mechanism and
// never gains an #include of a table (engine/cli/README.md).
//
// This header is what makes the port testable at all: the parse chain it replaced was 180 lines
// inside main()'s anonymous namespace, reachable only by launching the exe.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "engine/cli/option.hpp"
#include "engine/cli/parser.hpp"
#include "engine/core/math.hpp"
#include "render/diligent/render_context.hpp"
#include "render/diligent/svo_renderer.hpp"
#include "svo_world.hpp"
#include "world/player/tuning.hpp"
#include "world/streaming/world_bounds.hpp"

namespace app {

// Which world representation + renderer to run (micro-voxel pivot, docs/goals.md Groups W-X).
//   Svo:  sparse-brick octree at sub-centimeter voxels near the camera, GPU ray-marched.
//   Mesh: the greedy-meshed 1 m chunk world (Groups P-V), kept intact as the fallback.
enum class RendererKind { Svo, Mesh };

struct AppOptions {
    render::diligent::Backend backend = render::diligent::Backend::Vulkan;
    RendererKind renderer = RendererKind::Svo;
    std::uint32_t frames = 0; // 0 = run until the window is closed
    // Group S (Voxel Representation Redesign SS3): the mesh world's horizontal Chebyshev half-size,
    // pregenerated once at startup rather than streamed around the camera. Defaults to goal 127's
    // 48-column trial size, not the original 8km ask -- see docs/goals.md goal 132.
    std::int32_t radius = world::streaming::kDefaultWorldBounds.radius_chunks;
    int seed = 1337;
    bool verify_frame = false; // Group B smoke check: read the frame back, fail on an empty one
    bool validation = false;
    bool autofly = false; // Group D smoke check: fly +X automatically once the world has loaded
    bool walk = false;    // start in walk (gravity) mode; with --autofly, also asserts no fall-through
    bool noclip = false;  // Group AA: skip body-vs-world collision (the pre-collision spectator)
    // Prompt 001 Group AD. The step allowance is a SMOOTHING BUDGET on the svo path (7.8 mm voxels
    // make every slope a sub-cm staircase) and a real ledge climb on the mesh path (1 m blocks);
    // unset takes each path's own default.
    std::optional<float> step_height;
    // Prompt 002: these five were `no_*` members before the port, mirrored into a positive form at
    // every use site (`svo_settings.sky = !no_sky`). ValueKind::Toggle answers to BOTH --x and
    // --no-x from one row, so the negation now happens exactly once, in the spelling.
    bool view_polish = true; // head-bob, landing dip, boost FOV kick
    bool post = true;        // whole post chain: false renders straight to the swap chain
    bool bloom = true;       // bloom off, tonemap composite still on
    bool tonemap = true;     // tonemap off (raw clamp), bloom still on
    bool sky = true;         // gradient-sky pass (false: flat clear color)
    // The ImGui debug panel. On for a human, off for a golden: its fps, ms, brick count and VRAM
    // digits change every single run, so leaving it in a reference image bakes run-to-run noise
    // into the thing the comparison is supposed to hold still. voxel_harness defaults it off.
    bool overlay = true;
    // A4's crosshair. Default on, but suppressed under --verify-frame so a HUD cross cannot
    // inflate the local-contrast metric; --crosshair forces it back on for a capture.
    std::optional<bool> crosshair;
    std::size_t upload_budget = 4; // mesh commits per frame; 0 = unlimited (the pre-fix stutter behavior)
    std::uint32_t dump_every = 0;  // goal 7: write a numbered frame dump every N frames (0 = off)
    // Debug camera overrides for the visual-verification workflow (goal 8's multi-angle baseline
    // and every later "view a dump from X" check): unset = the default start pose.
    std::optional<glm::vec3> start_pos;
    std::optional<float> start_yaw_deg;
    std::optional<float> start_pitch_deg;
    // Micro-voxel (svo) options.
    SvoWorldOptions svo;
    render::diligent::SvoRenderer::Settings svo_settings;
    // Three options whose EFFECT is a computation, not an assignment. They are parsed into plain
    // members and applied by finalize() below, so the table stays declarative and the derivation
    // stays in one readable place instead of inside a parser branch.
    int svo_threads = 0;    // 0 = three quarters of the hardware threads (goal 170)
    int svo_upload_mb = 32; // slice size; 8 measured WORSE, see research/lin-look-log.md
    bool wind = true;       // --no-wind zeroes the whole field rather than flagging each reader
    bool no_grain = false;  // removes the grain TERM; --grain sets its amplitude. See the table.
    std::string crash_test; // Debug only; see the table
};

// The derived rules the old parse chain applied inline. Called once after parse(), by every entry
// point (voxel_app, the harness, the tests) -- so "--debug-view implies --no-post" cannot be true
// in one of them and false in another.
void finalize(AppOptions& options) noexcept;

// The table. `extern` rather than a header-inline constexpr array: it is large, every consumer
// only ever needs the span, and one definition means --help and the parser cannot disagree
// (compile-time-performance.md rule 13 -- the explicit-instantiation choke point for a table
// several translation units include).
[[nodiscard]] std::span<const engine::cli::Option> option_table() noexcept;

// argv -> AppOptions, including @response-file expansion and --help. Returns the outcome so the
// caller decides whether to log, print help, or exit.
[[nodiscard]] engine::cli::ParseOutcome parse_app_options(int argc, char** argv, AppOptions& out);

[[nodiscard]] std::string app_help_text();

// Goal 207: the RENDERER-SETTINGS half of the table, built without a window and without the app.
// A headless tool and a windowed app can therefore be handed the same configuration file and end
// up with the same Settings -- which is what makes "the same scenario, headless or windowed" a
// statement about one object rather than two parallel derivations.
[[nodiscard]] std::optional<render::diligent::SvoRenderer::Settings>
settings_from_response_file(const std::string& path);

} // namespace app
