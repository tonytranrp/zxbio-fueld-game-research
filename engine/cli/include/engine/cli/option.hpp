#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>

#include "engine/cli/detail/assign.hpp"
#include "engine/cli/value.hpp"

namespace engine::cli {

struct Option;

// The whole of engine/cli's type erasure: one function pointer per row, produced from a member
// pointer at compile time. Not a std::variant of every option type in the program, and not a
// template the parser re-instantiates per option -- `parse()` is compiled exactly once for every
// table in the repo (templates-and-metaprogramming.md §5).
using Setter = Status (*)(void* base, std::string_view token, const Option& self);

// One declared option. A constexpr aggregate: designated initializers make a table read as data.
struct Option {
    std::string_view name;                    // long name, without the leading "--"
    Setter set = nullptr;                     // bind<&Owner::member>()
    ValueKind kind = ValueKind::Flag;         //
    std::string_view help{};                  // one line, shown by --help
    std::string_view default_text{};          // the default, as --help should print it
    std::string_view group = "General";       // --help grouping
    std::string_view alias{};                 // a second accepted long name (--root-log2)
    std::span<const EnumEntry> enum_values{}; // ValueKind::Enum only
};

// bind<&AppOptions::seed>() -- the only way a table names a target. The member pointer carries
// both the owning type (to cast the void* back) and the member type (to pick the conversion), so
// a row cannot name a member of the wrong struct or a type with no conversion: both are compile
// errors at the table's own definition site.
// A PATH of member pointers is accepted for the same reason: half the app's options land in a
// nested settings struct (`options.svo_settings.shadows`), and the alternative -- a flat mirror
// struct copied into the nested one after parsing -- is exactly the hand-mirrored duplication this
// module exists to delete. bind<&AppOptions::svo_settings, &Settings::shadows>().
template <auto... Members>
    requires(sizeof...(Members) > 0 && (std::is_member_object_pointer_v<decltype(Members)> && ...))
[[nodiscard]] consteval Setter bind() noexcept {
    using Root = typename detail::member_ptr_traits<typename detail::first_member<Members...>::type>::klass;
    return [](void* base, std::string_view token, const Option& self) -> Status {
        return detail::assign(detail::walk<Members...>(*static_cast<Root*>(base)), token, self.enum_values);
    };
}

// A table is any contiguous range of rows. Constrained rather than left as a bare `typename T`
// (templates-and-metaprogramming.md §1); every public entry point below takes the erased
// std::span<const Option> instead, so this is only for the compile-time checks.
template <class T>
concept OptionTable = requires(const T& table) {
    { std::span<const Option>{table} };
};

// Two long names reaching two different targets is the one table mistake that produces a silently
// wrong program rather than a diagnostic -- whichever row is found first wins and the other option
// simply does nothing. static_assert this at every table's definition site.
[[nodiscard]] constexpr bool has_unique_names(std::span<const Option> table) noexcept {
    for (std::size_t i = 0; i < table.size(); ++i) {
        if (table[i].name.empty() || table[i].set == nullptr) {
            return false;
        }
        for (std::size_t j = i + 1; j < table.size(); ++j) {
            if (table[i].name == table[j].name ||
                (!table[i].alias.empty() && table[i].alias == table[j].name) ||
                (!table[j].alias.empty() && table[j].alias == table[i].name) ||
                (!table[i].alias.empty() && table[i].alias == table[j].alias)) {
                return false;
            }
        }
    }
    return true;
}

// Does this row answer to this spelling? Aliases are a row property, not a parser special case.
[[nodiscard]] constexpr bool matches(const Option& row, std::string_view name) noexcept {
    return row.name == name || (!row.alias.empty() && row.alias == name);
}

} // namespace engine::cli
