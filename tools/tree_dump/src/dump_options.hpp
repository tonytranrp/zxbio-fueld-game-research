#pragma once

// tree_dump's options (Prompt 002 goal 205). Same shape as mesh_dump's: the documented positional
// contract `tree_dump [species] [seed] [out.obj]` is preserved and fed back through the table, so
// the species spelling is validated by the row that owns it rather than by a second `if` ladder.

#include <span>
#include <string>

#include "engine/cli/option.hpp"
#include "engine/cli/parser.hpp"
#include "world/generation/tree_skeleton.hpp"

namespace tools::tree_dump {

struct Options {
    world::generation::TreeSpecies species = world::generation::TreeSpecies::RoundBroadleaf;
    int seed = 1337;
    std::string out; // empty = tree_<species>_<seed>.obj
};

[[nodiscard]] engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out);
[[nodiscard]] std::string help_text();
[[nodiscard]] std::string_view species_name(world::generation::TreeSpecies species) noexcept;

} // namespace tools::tree_dump
