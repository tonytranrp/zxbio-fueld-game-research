#pragma once

// tree_dump's options (Prompt 002 goal 205). Same shape as mesh_dump's: the documented positional
// contract `tree_dump [species] [seed] [out.obj]` is preserved and fed back through the table, so
// the species spelling is validated by the row that owns it rather than by a second `if` ladder.

#include <span>
#include <string>

#include "engine/core/math.hpp"
#include "engine/cli/option.hpp"
#include "engine/cli/parser.hpp"
#include "engine/cli/value.hpp"
#include "world/generation/tree_skeleton.hpp"

namespace tools::tree_dump {

struct Options {
    world::generation::TreeSpecies species = world::generation::TreeSpecies::RoundBroadleaf;
    int seed = 1337;
    std::string out; // empty = tree_<species>_<seed>.obj

    // Prompt 007 goal 335's "so the motion is seen and not only measured". A skeleton .obj is a
    // still; sway is a thing that happens over time, and a number in a test log is not a picture of
    // it. `--sway-frames N` steps the sway model and writes N orthographic side views spanning one
    // fundamental period, with the rest pose drawn behind the posed one so the displacement is
    // visible in every single frame rather than only in a flipbook.
    int sway_frames = 0;
    float wind_speed = 4.0f;              // m/s, the default breeze
    float sway_periods = 1.0f;            // how many fundamental periods the sequence spans
    float sway_settle_seconds = 20.0f;    // driven from rest before the first frame is written
    bool branch_reaction = true;          // --no-branch-reaction is the tuned-mass-damper A/B
    engine::cli::Size sway_size{640, 720};
    std::string sway_png; // empty = <species>_sway_<seed>

    // `--near x,z` lists the real world's tree placements around a world point, using the same
    // generator the sampler calls. Added for goal 336: "capture a tree at 2 m" needs a tree's
    // coordinates, and guessing at them wastes more time than the twelve lines this costs.
    glm::vec2 near_xz{0.0f};
    bool have_near = false;
    float near_radius = 40.0f;
};

[[nodiscard]] engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out);
[[nodiscard]] std::string help_text();
[[nodiscard]] std::string_view species_name(world::generation::TreeSpecies species) noexcept;

} // namespace tools::tree_dump
