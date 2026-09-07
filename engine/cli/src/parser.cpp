#include "engine/cli/parser.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "engine/cli/response_file.hpp"

namespace engine::cli {

namespace detail {

bool lookup_enum(std::span<const EnumEntry> entries, std::string_view text, int& out) noexcept {
    for (const EnumEntry& entry : entries) {
        if (entry.name == text) {
            out = entry.value;
            return true;
        }
    }
    return false;
}

} // namespace detail

namespace {

// The three diagnostics of goal 202's Check, each naming the option. They are built here rather
// than in the setters so that every one of them words the option the same way -- a setter only
// ever reports Status::BadValue.
[[nodiscard]] std::string unknown_option(std::string_view spelling) {
    return "unknown option \"" + std::string{spelling} + "\"";
}

[[nodiscard]] std::string missing_value(std::string_view spelling, const Option& row) {
    return "option \"" + std::string{spelling} + "\" expects " + std::string{expectation(row.kind)} +
           " but no value followed it";
}

[[nodiscard]] std::string bad_value(std::string_view spelling, const Option& row, std::string_view token) {
    std::string message = "option \"" + std::string{spelling} + "\" expects " +
                          std::string{expectation(row.kind)} + ", got \"" + std::string{token} + "\"";
    if (row.kind == ValueKind::Enum) {
        message += " (accepted: ";
        bool first = true;
        for (const EnumEntry& entry : row.enum_values) {
            if (!first) {
                message += '|';
            }
            message += std::string{entry.name};
            first = false;
        }
        message += ')';
    }
    return message;
}

const Option* find(std::span<const Option> table, std::string_view name) noexcept {
    for (const Option& row : table) {
        if (matches(row, name)) {
            return &row;
        }
    }
    return nullptr;
}

} // namespace

ParseOutcome parse(std::span<const Option> table, void* base, std::span<const std::string> args,
                   std::vector<std::string>* positionals) {
    ParseOutcome outcome;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& raw = args[i];
        const std::string_view arg{raw};
        if (arg == "--help" || arg == "-h" || arg == "-?") {
            outcome.help_requested = true;
            continue;
        }
        if (!arg.starts_with("--")) {
            if (positionals == nullptr) {
                return {.ok = false, .help_requested = false, .message = unknown_option(arg)};
            }
            positionals->emplace_back(raw);
            continue;
        }

        // --name=value is accepted alongside --name value. Both spellings reach the same row; the
        // '=' form is what makes a one-line response-file entry unambiguous.
        std::string_view spelling = arg;
        std::string_view name = arg.substr(2);
        std::string_view inlineValue;
        bool haveInline = false;
        if (const std::size_t eq = name.find('='); eq != std::string_view::npos) {
            inlineValue = name.substr(eq + 1);
            haveInline = true;
            name = name.substr(0, eq);
            spelling = arg.substr(0, eq + 2);
        }

        // --no-x is a property of a Toggle row, not a parser special case: one row, one target,
        // both spellings.
        bool negated = false;
        const Option* row = find(table, name);
        if (row == nullptr && name.starts_with("no-")) {
            const Option* toggle = find(table, name.substr(3));
            if (toggle != nullptr && toggle->kind == ValueKind::Toggle) {
                row = toggle;
                negated = true;
            }
        }
        if (row == nullptr) {
            return {.ok = false, .help_requested = false, .message = unknown_option(spelling)};
        }

        std::string_view token;
        if (row->kind == ValueKind::Flag) {
            token = "true";
        } else if (row->kind == ValueKind::Toggle) {
            token = negated ? "false" : "true";
        } else if (haveInline) {
            token = inlineValue;
        } else if (i + 1 < args.size()) {
            token = args[++i];
        } else {
            return {.ok = false, .help_requested = false, .message = missing_value(spelling, *row)};
        }

        if (row->set(base, token, *row) != Status::Ok) {
            return {.ok = false, .help_requested = false, .message = bad_value(spelling, *row, token)};
        }
    }
    return outcome;
}

ParseOutcome parse_command_line(std::span<const Option> table, void* base, int argc, char** argv,
                                std::vector<std::string>* positionals) {
    std::vector<std::string> raw;
    raw.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0);
    for (int i = 1; i < argc; ++i) {
        raw.emplace_back(argv[i]);
    }
    ExpandResult expanded = expand_response_files(raw);
    if (!expanded.ok) {
        return {.ok = false, .help_requested = false, .message = std::move(expanded.message)};
    }
    return parse(table, base, expanded.args, positionals);
}

} // namespace engine::cli
