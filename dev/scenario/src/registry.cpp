#include "dev/scenario/registry.hpp"

#include <algorithm>
#include <filesystem>

#include "dev/scenario/parser.hpp"

namespace dev::scenario {

Registry& Registry::instance() {
    // Function-local static: the built-ins register during static initialization, and a namespace-
    // scope Registry could be constructed after the first AutoRegister that uses it (the classic
    // static-initialization-order failure modular-architecture.md §4 warns about).
    static Registry registry;
    return registry;
}

void Registry::add(Entry entry) {
    const auto existing =
        std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.name == entry.name; });
    if (existing != entries_.end()) {
        // A .scn shadowing a built-in of the same name is how you iterate on a built-in without
        // rebuilding; the file wins and --list-scenarios shows where each came from.
        *existing = std::move(entry);
        return;
    }
    entries_.push_back(std::move(entry));
}

int Registry::add_directory(const std::string& directory, std::string& error) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        error = "scenario directory \"" + directory + "\" does not exist";
        return -1;
    }
    int added = 0;
    std::vector<std::filesystem::path> files;
    for (const std::filesystem::directory_entry& item : std::filesystem::directory_iterator(directory, ec)) {
        if (item.is_regular_file() && item.path().extension() == ".scn") {
            files.push_back(item.path());
        }
    }
    // Sorted, so --list-scenarios is stable across machines and filesystems.
    std::sort(files.begin(), files.end());
    for (const std::filesystem::path& path : files) {
        const std::string text = path.string();
        ParseResult parsed = load_scenario(text);
        if (!parsed.ok) {
            error = parsed.message;
            return -1;
        }
        Entry entry;
        entry.name = parsed.scenario.name;
        entry.description = parsed.scenario.description;
        entry.origin = Origin::File;
        entry.source = text;
        entry.build = [text]() {
            ParseResult again = load_scenario(text);
            return again.ok ? std::move(again.scenario) : Scenario{};
        };
        add(std::move(entry));
        ++added;
    }
    return added;
}

const Entry* Registry::find(std::string_view name) const noexcept {
    for (const Entry& entry : entries_) {
        if (entry.name == name) {
            return &entry;
        }
    }
    return nullptr;
}

} // namespace dev::scenario
