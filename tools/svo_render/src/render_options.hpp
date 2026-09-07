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
    bool shadows = true;
    bool ao = true;
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
