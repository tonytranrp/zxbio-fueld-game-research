#include "engine/cli/help.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace engine::cli {

namespace {

// Every spelling a row answers to, in the order --help should read them: the long name, its alias,
// and -- for a Toggle -- the --no- form, which is not a separate row and must not look like one.
[[nodiscard]] std::string spellings_of(const Option& row) {
    std::string text = "--" + std::string{row.name};
    if (!row.alias.empty()) {
        text += ", --" + std::string{row.alias};
    }
    if (row.kind == ValueKind::Toggle) {
        text += ", --no-" + std::string{row.name};
    }
    if (const std::string_view place = placeholder(row.kind); !place.empty()) {
        text += ' ';
        text += place;
    }
    if (row.kind == ValueKind::Enum) {
        text += " {";
        bool first = true;
        for (const EnumEntry& entry : row.enum_values) {
            if (!first) {
                text += '|';
            }
            text += std::string{entry.name};
            first = false;
        }
        text += '}';
    }
    return text;
}

} // namespace

std::string render_help(std::string_view program, std::string_view synopsis, std::span<const Option> table) {
    std::string out;
    out += "usage: ";
    out += program;
    out += " [options]\n";
    if (!synopsis.empty()) {
        out += '\n';
        out += synopsis;
        out += '\n';
    }

    // Column width from the widest spelling, so the help of a program with a --smooth-pixels does
    // not wrap differently from one without it.
    std::size_t width = 0;
    for (const Option& row : table) {
        width = std::max(width, spellings_of(row).size());
    }
    width = std::min<std::size_t>(width, 40);

    // Groups in first-appearance order: the table's own order is the author's intent, and sorting
    // it alphabetically would scatter --no-bloom away from --no-tonemap.
    std::vector<std::string_view> groups;
    for (const Option& row : table) {
        if (std::find(groups.begin(), groups.end(), row.group) == groups.end()) {
            groups.push_back(row.group);
        }
    }

    for (const std::string_view group : groups) {
        out += '\n';
        out += group;
        out += ":\n";
        for (const Option& row : table) {
            if (row.group != group) {
                continue;
            }
            const std::string spellings = spellings_of(row);
            out += "  ";
            out += spellings;
            out.append(spellings.size() < width ? width - spellings.size() : 1, ' ');
            out += "  ";
            out += row.help;
            if (!row.default_text.empty()) {
                out += " (default ";
                out += row.default_text;
                out += ')';
            }
            out += '\n';
        }
    }
    out += "\n  --help, -h";
    out.append(width > 10 ? width - 10 : 1, ' ');
    out += "  print this and exit\n";
    out += "  @FILE";
    out.append(width > 5 ? width - 5 : 1, ' ');
    out += "  read options from FILE, one per line, # comments\n";
    return out;
}

} // namespace engine::cli
