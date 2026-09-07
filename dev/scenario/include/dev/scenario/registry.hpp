#pragma once

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "dev/scenario/scenario.hpp"

namespace dev::scenario {

// Where a scenario came from. `--list-scenarios` prints it, because "why is this scenario not the
// one I edited" is otherwise a five-minute question.
enum class Origin : std::uint8_t { BuiltIn, File };

struct Entry {
    std::string name;
    std::string description;
    Origin origin = Origin::File;
    std::string source;              // the .scn path, or the built-in's TU name
    std::function<Scenario()> build; // built-ins compute; files just re-parse
};

// The self-registering factory of modular-architecture.md §4: a built-in scenario is ONE
// translation unit that registers itself, with no central list to keep in sync. Registration runs
// during static initialization, so nothing may depend on the order between two built-ins.
class Registry {
public:
    [[nodiscard]] static Registry& instance();

    void add(Entry entry);
    // Reads every *.scn under `directory` (non-recursive) and registers it. Returns how many were
    // added, or -1 with `error` set if the directory could not be read.
    int add_directory(const std::string& directory, std::string& error);

    [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }
    [[nodiscard]] const Entry* find(std::string_view name) const noexcept;

private:
    std::vector<Entry> entries_;
};

// The one line a built-in scenario's TU writes. Anonymous-namespace static object -> its
// constructor runs before main().
struct AutoRegister {
    explicit AutoRegister(Entry entry) { Registry::instance().add(std::move(entry)); }
};

} // namespace dev::scenario
