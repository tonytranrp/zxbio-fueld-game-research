#pragma once

// voxel_harness's own option table. The SCENARIO's options are voxel_app's table (a .scn's
// `option --no-taa` is fed through app::option_table()); these are the runner's own.

#include <span>
#include <string>

#include "engine/cli/option.hpp"
#include "engine/cli/parser.hpp"

namespace dev::harness {

struct Options {
    std::string scenario;
    std::string backend_filter; // "", "vk", "d3d12", or "vk,d3d12"
    std::string scenario_dir = "dev/scenarios";
    std::string golden_root = "dev/goldens";
    std::string out_dir = "."; // captures, diffs and the report land here
    std::string report;        // --report <path.json>; empty = no JSON
    bool list = false;
    bool headless = false;
    bool accept_golden = false;
    bool no_golden = false; // run the scenario but skip the golden comparison entirely
    int frame_cap = 0; // hard ceiling regardless of the script's length; 0 = the script decides
    // Goal 219's yardstick: "--ramp lod-radius:1,2,4,8,16" re-runs the scenario once per value,
    // holding the pose fixed, and prints a table of resident detail against GPU cost. The option
    // named is any voxel_app option; the values are appended to the scenario's own option list, so
    // a rung is a real run of a real scenario rather than a special measurement path.
    std::string ramp;
};

[[nodiscard]] std::span<const engine::cli::Option> option_table() noexcept;
[[nodiscard]] engine::cli::ParseOutcome parse_options(int argc, char** argv, Options& out);
[[nodiscard]] std::string help_text();

} // namespace dev::harness
