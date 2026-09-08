#pragma once

// svo_render's option table (Prompt 002 goal 205). Same rule as app/src/app_options.hpp: a table
// lives with the struct it fills. What this file also does is END THE DRIFT -- --root-log2 and
// --verify are now aliases of the app's --region-log2 and --verify-frame rather than second names
// for the same concept in a second parser, and every documented command line for either program
// keeps working.

#include <optional>
#include <span>
#include <string>
#include <thread>

#include "engine/cli/option.hpp"
#include "engine/cli/parser.hpp"
#include "engine/core/math.hpp"

namespace tools::svo_render {

struct Options {
    int seed = 1337;
    int voxel_log2 = -7; // 7.8 mm
    int root_log2 = 9;   // 512 m
    float lod_radius = 4.0f;
    // Default pose: hovering above the ~65 m summit near the origin, looking down the -Z valley
    // toward the sea -- chosen by looking at real renders (a ground-level pose here stares into a
    // slope half a meter away), not guessed.
    //
    // `pos` and `xz` were a vec3 plus two `_set` booleans before the port. They are optionals now,
    // which is the same information with no way to disagree with itself: --xz gives x and z and
    // derives the eye height from the terrain, --pos gives all three.
    std::optional<glm::vec3> pos;
    std::optional<glm::vec2> xz;
    float yaw_deg = 0.0f;
    float pitch_deg = -22.0f;
    engine::cli::Size size{1280, 720};
    float fov_deg = 70.0f;
    bool trees = true;
    // Goal 336: the macro terrain pipeline, ON by default because the app has it on by default and
    // a CPU REFERENCE that renders a different world from the thing it is a reference for is worse
    // than no reference. `--no-macro-field` is the A/B (and what a pure-noise determinism check
    // wants).
    bool macro_field = true;
    // Goal 336: metres within which trees voxelize from their grown skeleton. 0 = implicit shapes,
    // which is what the CI smoke test and every determinism check use.
    float skeleton_radius = 0.0f;
    float grass_radius = 0.0f;
    float grass_plants_per_tuft = 20.0f;
    bool shadows = true;
    bool ao = true;
    // Prompt 005 goal 277: the albedo mottle is two octaves of world-XZ value noise with NO
    // distance fade, which the prompt names as the strongest moire suspect. It had no toggle,
    // so it could not be bisected -- and every OTHER term could. This is that toggle.
    bool mottle = true;
    // Goal 277: shade every hit with ONE albedo regardless of its material. Isolates the cost of
    // sampling a DISCRETE shading attribute (Laine & Karras' named artefact) from everything
    // else, which no existing flag could do -- every other toggle removes a term applied AFTER
    // the material is chosen, and all six of them measured at zero.
    bool flat_albedo = false;
    // Goal 278: blend the albedo toward the smoothing ancestor's representative material as the
    // hit cube approaches pixel size -- the same rule, and the same weight, the NORMAL has used
    // since Group Z. Material IDs cannot be averaged; their albedos can.
    bool filter_albedo = true;
    bool grain = true;
    bool no_grain = false; // removes the grain TERM; --grain sets its amplitude. See the table.
    bool verify = false;
    bool lod_march = true;
    // Group Z knobs (the app's defaults; see SvoRenderer::Settings).
    float smooth_pixels = 6.0f;
    float grain_amplitude = 0.10f;
    float ao_radius_px = 32.0f;
    float shadow_lod = 4.0f;
    float ao_lod = 8.0f;
    // --lod-center: build the tree's LOD around a point OTHER than the camera -- how the app looks
    // after the camera has moved away from the last build center (goal 164's repro).
    std::optional<glm::vec3> lod_center;
    // --view: one shading term instead of the shaded color (mirrors --debug-view in the app).
    std::string view;
    std::string out = "svo_render.png";
    int threads = 0; // 0 = hardware_concurrency, clamped to >= 1 by threads_or_default()
    // Prompt 004 goal 266: measure the ORACLE BEAM at this tile size -- the ceiling on any coarse
    // start-t pre-pass, since the seed used is the tile minimum of the TRUE hit distances and no
    // conservative bound can be looser than that. 0 = off.
    int beam_tile = 0;

    [[nodiscard]] glm::vec3 default_pos() const noexcept { return glm::vec3{12.0f, 82.0f, 24.0f}; }
    [[nodiscard]] unsigned threads_or_default() const noexcept {
        return threads > 0 ? static_cast<unsigned>(threads)
                           : std::max(1u, std::thread::hardware_concurrency());
    }
};

[[nodiscard]] std::span<const engine::cli::Option> option_table() noexcept;
[[nodiscard]] engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out);
[[nodiscard]] std::string help_text();

} // namespace tools::svo_render
