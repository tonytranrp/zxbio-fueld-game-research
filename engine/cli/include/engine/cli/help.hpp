#pragma once

#include <span>
#include <string>
#include <string_view>

#include "engine/cli/option.hpp"

namespace engine::cli {

// --help, generated from the table (goal 203). Grouped by Option::group in first-appearance order,
// each row printed with every spelling it answers to, its value placeholder, its help text and its
// default. Because this reads the table, an option cannot be added without appearing here --
// which is what the count test in the suite pins down.
[[nodiscard]] std::string render_help(std::string_view program, std::string_view synopsis,
                                      std::span<const Option> table);

} // namespace engine::cli
