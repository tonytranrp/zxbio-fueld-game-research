#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "dev/scenario/scenario.hpp"

namespace dev::scenario {

// The .scn format. Line-oriented, one directive per line, '#' comments:
//
//   name <identifier>
//   describe <free text>
//   option <--name> [value]        -- fed through voxel_app's own table by the harness
//   pose <x,y,z> <yaw> <pitch>
//   hold <keys|none> <seconds>     -- keys are '+'-joined: forward+boost
//   look <yaw> <pitch> <seconds>
//   goto <x,y,z> <radius> [timeout]
//   wait <seconds>
//   capture <frame N|time T|event NAME|end> <capture-name>
//   assert <metric> <op> <value>
//   backend <vk|d3d12|both>
//   golden <dir>
//   include <other.scn>
//
// Deliberately NOT JSON, and deliberately not a scripting language. JSON because a scenario is
// read and edited by a human at 2am and a quoted, comma-terminated option list is worse for that
// than a bare line; a scripting language because the moment a scenario can compute, two scenarios
// stop being comparable and the format stops being a description of what was run.
struct ParseResult {
    bool ok = true;
    std::string message{}; // "<file>:<line>: <what was expected>"
    Scenario scenario{};
};

// `path` is used for diagnostics and to resolve `include` relative to the including file.
[[nodiscard]] ParseResult parse_scenario(std::string_view text, std::string_view path);
[[nodiscard]] ParseResult load_scenario(const std::string& path);

// The other direction, for the round-trip test and for a report that wants to record exactly what
// it ran. `include` is not re-emitted: by the time a scenario is a Scenario, its includes have
// been resolved, and writing them back would produce a file that means something different.
[[nodiscard]] std::string emit_scenario(const Scenario& scenario);

} // namespace dev::scenario
