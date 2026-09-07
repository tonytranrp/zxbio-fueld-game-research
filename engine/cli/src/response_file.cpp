#include "engine/cli/response_file.hpp"

#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace engine::cli {

namespace {

[[nodiscard]] std::string_view trim(std::string_view text) noexcept {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const std::size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

// One line becomes one or more arguments, split on whitespace so "--seed 1337" and two separate
// lines mean the same thing. No quoting: nothing in this project's option surface takes a value
// with a space in it, and a quoting rule nobody needs is a rule that will be wrong later.
void split_into(std::string_view line, std::vector<std::string>& out) {
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        const std::size_t begin = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
            ++i;
        }
        if (i > begin) {
            out.emplace_back(line.substr(begin, i - begin));
        }
    }
}

} // namespace

ExpandResult expand_response_files(std::span<const std::string> args) {
    ExpandResult result;
    result.args.reserve(args.size());
    for (const std::string& arg : args) {
        if (!arg.starts_with('@')) {
            result.args.push_back(arg);
            continue;
        }
        const std::string path = arg.substr(1);
        std::ifstream file(path);
        if (!file) {
            return {.ok = false, .message = "response file \"" + path + "\" could not be opened", .args = {}};
        }
        std::string line;
        int lineNumber = 0;
        while (std::getline(file, line)) {
            ++lineNumber;
            std::string_view text = trim(line);
            if (const std::size_t hash = text.find('#'); hash != std::string_view::npos) {
                text = trim(text.substr(0, hash));
            }
            if (text.empty()) {
                continue;
            }
            if (text.front() == '@') {
                return {.ok = false,
                        .message = path + ":" + std::to_string(lineNumber) +
                                   ": nested response files are not supported (found \"" + std::string{text} +
                                   "\")",
                        .args = {}};
            }
            split_into(text, result.args);
        }
    }
    return result;
}

} // namespace engine::cli
