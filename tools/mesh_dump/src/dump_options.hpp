#pragma once

// mesh_dump's options (Prompt 002 goal 205). The tool has always taken POSITIONAL arguments --
// `mesh_dump [cx cy cz] [seed] [out.obj]`, which is what CLAUDE.md documents -- so that contract is
// preserved exactly and named options are added beside it. The positionals are not parsed here by
// hand: they are fed back through the same table, which is what keeps the repo at exactly one
// argv-indexing site (goal 224).

#include <span>
#include <string>

#include "engine/cli/option.hpp"
#include "engine/cli/parser.hpp"

namespace tools::mesh_dump {

struct Options {
    int cx = 0;
    int cy = 0;
    int cz = 0;
    int seed = 1337;
    std::string out; // empty = chunk_<cx>_<cy>_<cz>.obj
};

[[nodiscard]] engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out);
[[nodiscard]] std::string help_text();

} // namespace tools::mesh_dump
