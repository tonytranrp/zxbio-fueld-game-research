#pragma once

// The conversions behind engine/cli's type erasure. Not part of the public API -- a table declares
// `bind<&Owner::member>()` and never names anything in here (modular-architecture.md §1's
// detail/ rule: template internals that cannot hide behind a .cpp boundary get a wall instead).

#include <charconv>
#include <concepts>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "engine/cli/value.hpp"

namespace engine::cli {

struct Option;

namespace detail {

template <class>
inline constexpr bool always_false = false;

template <class T>
struct is_optional : std::false_type {};
template <class T>
struct is_optional<std::optional<T>> : std::true_type {
    using value_type = T;
};

// Member-pointer traits: bind<&AppOptions::seed>() has to recover both the owning struct (to cast
// the void* back) and the member type (to pick a conversion).
template <class T>
struct member_ptr_traits;
template <class Class, class Member>
struct member_ptr_traits<Member Class::*> {
    using klass = Class;
    using member = Member;
};

// A bind<> path's ROOT is whatever the first pointer's class is -- that is the type the void* is
// cast back to, and every later hop is checked by the compiler against the type it lands in.
template <auto First, auto...>
struct first_member {
    using type = decltype(First);
};

template <auto Member, auto... Rest, class Base>
[[nodiscard]] constexpr auto& walk(Base& base) noexcept {
    auto& next = base.*Member;
    if constexpr (sizeof...(Rest) == 0) {
        return next;
    } else {
        return walk<Rest...>(next);
    }
}

// std::from_chars everywhere, deliberately: it takes a string_view range (no null terminator to
// manufacture), never allocates, ignores the locale, and -- unlike strtol/atoi, which the three
// hand-rolled parsers this module replaces used -- REJECTS trailing garbage instead of silently
// returning a prefix. "12abc" is a diagnostic here, not 12.
template <class T>
    requires std::integral<T>
[[nodiscard]] inline bool from_text(std::string_view text, T& out) noexcept {
    if (text.empty()) {
        return false;
    }
    // from_chars has no '+' rule for integers; accept it so "--yaw +90" and "--yaw 90" agree.
    const bool plus = text.front() == '+';
    const std::string_view body = plus ? text.substr(1) : text;
    if (body.empty()) {
        return false;
    }
    const auto* first = body.data();
    const auto* last = body.data() + body.size();
    const std::from_chars_result r = std::from_chars(first, last, out);
    return r.ec == std::errc{} && r.ptr == last;
}

template <class T>
    requires std::floating_point<T>
[[nodiscard]] inline bool from_text(std::string_view text, T& out) noexcept {
    if (text.empty()) {
        return false;
    }
    const bool plus = text.front() == '+';
    const std::string_view body = plus ? text.substr(1) : text;
    if (body.empty()) {
        return false;
    }
    const auto* first = body.data();
    const auto* last = body.data() + body.size();
    const std::from_chars_result r = std::from_chars(first, last, out);
    return r.ec == std::errc{} && r.ptr == last;
}

// "1,2,3" -> vec3, "1,2" -> vec2. Split by hand rather than through sscanf: sscanf accepts
// trailing junk and has a %n-vs-locale story nobody should have to think about, and MSVC
// deprecates the plain form.
template <int N>
[[nodiscard]] inline bool parse_floats(std::string_view text, float (&parts)[N]) noexcept {
    std::size_t begin = 0;
    for (int i = 0; i < N; ++i) {
        const std::size_t comma = text.find(',', begin);
        const bool lastPart = i == N - 1;
        if (lastPart != (comma == std::string_view::npos)) {
            return false; // too few separators, or too many
        }
        const std::string_view part = lastPart ? text.substr(begin) : text.substr(begin, comma - begin);
        if (!from_text(part, parts[i])) {
            return false;
        }
        begin = comma + 1;
    }
    return true;
}

[[nodiscard]] inline bool parse_vec3(std::string_view text, glm::vec3& out) noexcept {
    float parts[3]{};
    if (!parse_floats(text, parts)) {
        return false;
    }
    out = glm::vec3{parts[0], parts[1], parts[2]};
    return true;
}

[[nodiscard]] inline bool parse_vec2(std::string_view text, glm::vec2& out) noexcept {
    float parts[2]{};
    if (!parse_floats(text, parts)) {
        return false;
    }
    out = glm::vec2{parts[0], parts[1]};
    return true;
}

// "1280x720". 'X' accepted too -- a shell that upper-cases is not worth a diagnostic.
[[nodiscard]] inline bool parse_size(std::string_view text, Size& out) noexcept {
    const std::size_t x = text.find_first_of("xX");
    if (x == std::string_view::npos) {
        return false;
    }
    Size parsed;
    if (!from_text(text.substr(0, x), parsed.width) || !from_text(text.substr(x + 1), parsed.height)) {
        return false;
    }
    out = parsed;
    return true;
}

[[nodiscard]] inline bool parse_bool(std::string_view text, bool& out) noexcept {
    if (text == "true" || text == "1" || text == "on" || text == "yes") {
        out = true;
        return true;
    }
    if (text == "false" || text == "0" || text == "off" || text == "no") {
        out = false;
        return true;
    }
    return false;
}

[[nodiscard]] bool lookup_enum(std::span<const EnumEntry> entries, std::string_view text, int& out) noexcept;

// One function, one `if constexpr` ladder, rather than an overload set: std::optional<T> has to
// recurse into T, and an overload set makes that ambiguous the moment T is itself convertible.
template <class T>
[[nodiscard]] Status assign(T& dst, std::string_view token, std::span<const EnumEntry> enums) {
    if constexpr (is_optional<T>::value) {
        typename is_optional<T>::value_type inner{};
        const Status status = assign(inner, token, enums);
        if (status == Status::Ok) {
            dst = inner;
        }
        return status;
    } else if constexpr (std::same_as<T, bool>) {
        return parse_bool(token, dst) ? Status::Ok : Status::BadValue;
    } else if constexpr (std::is_enum_v<T>) {
        int value = 0;
        if (!lookup_enum(enums, token, value)) {
            return Status::BadValue;
        }
        dst = static_cast<T>(value);
        return Status::Ok;
    } else if constexpr (std::integral<T> || std::floating_point<T>) {
        // One branch, not two: from_text is overloaded on the constraint, so the integral and
        // floating-point cases have the same BODY and differ only in which overload it resolves
        // to. Written as two branches this was a clang-tidy bugprone-branch-clone error on the
        // Linux CI leg -- and it was right, the duplication carried no information.
        return from_text(token, dst) ? Status::Ok : Status::BadValue;
    } else if constexpr (std::same_as<T, std::string>) {
        dst.assign(token);
        return Status::Ok;
    } else if constexpr (std::same_as<T, glm::vec3>) {
        return parse_vec3(token, dst) ? Status::Ok : Status::BadValue;
    } else if constexpr (std::same_as<T, glm::vec2>) {
        return parse_vec2(token, dst) ? Status::Ok : Status::BadValue;
    } else if constexpr (std::same_as<T, Size>) {
        return parse_size(token, dst) ? Status::Ok : Status::BadValue;
    } else {
        static_assert(always_false<T>, "engine/cli: this member type has no conversion. Add one here (and a "
                                       "ValueKind for it) rather than parsing it at the call site.");
        return Status::BadValue;
    }
}

} // namespace detail
} // namespace engine::cli
