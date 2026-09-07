#pragma once

#include <span>
#include <string>
#include <vector>

namespace engine::cli {

// @file expansion (goal 206). A line is one argument -- an option and its value may share a line
// ("--seed 1337") or not; blank lines and '#' comments are dropped. This is the mechanism a .scn
// scenario carries its engine settings on, so it exists before dev/scenario does.
//
// Nesting is deliberately rejected rather than supported: a config that can include a config is a
// dependency graph, and the moment it is one somebody wants cycle detection, relative-path rules
// and an ordering guarantee. `include` in the scenario format is where that complexity is paid
// for, once, with those rules written down.
struct ExpandResult {
    bool ok = true;
    std::string message{};
    std::vector<std::string> args{};
};

[[nodiscard]] ExpandResult expand_response_files(std::span<const std::string> args);

} // namespace engine::cli
