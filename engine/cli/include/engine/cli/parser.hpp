#pragma once

#include <span>
#include <string>
#include <vector>

#include "engine/cli/option.hpp"

namespace engine::cli {

// What a parse produced. The diagnostic is returned rather than logged: the caller decides how to
// show it, and a test can assert on the exact wording (goal 202's Check names all three).
struct ParseOutcome {
    bool ok = true;
    bool help_requested = false;
    std::string message{};

    explicit operator bool() const noexcept { return ok; }
};

// Drive `args` (argv[1..], already response-file-expanded) through `table` into `base`.
//
// `base` must point at an object of the type every row's member pointer belongs to. That is the
// one thing this interface cannot check -- bind<>() erases the type on purpose so the parser
// compiles once -- so keep the table beside the struct it fills, which is the rule in README.md.
//
// Non-option arguments are appended to `positionals` when it is non-null (tools/mesh_dump takes
// three), and are an error when it is null.
[[nodiscard]] ParseOutcome parse(std::span<const Option> table, void* base, std::span<const std::string> args,
                                 std::vector<std::string>* positionals = nullptr);

// Convenience for a main(): copies argv[1..] into strings, expands @response-files, then parses.
[[nodiscard]] ParseOutcome parse_command_line(std::span<const Option> table, void* base, int argc,
                                              char** argv, std::vector<std::string>* positionals = nullptr);

} // namespace engine::cli
