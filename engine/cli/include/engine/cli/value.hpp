#pragma once

#include <cstdint>
#include <string_view>

namespace engine::cli {

// The shapes the 43 existing flags actually use, and no more (Prompt 002 goal 202). Adding a kind
// is a deliberate act: it means the parser learned a new way to consume tokens, which is exactly
// the drift this module exists to prevent.
enum class ValueKind : std::uint8_t {
    Flag,   // presence only: --autofly. Consumes no token.
    Toggle, // --x / --no-x, one row, one target. Consumes no token.
    Int,    // --seed 1337
    Float,  // --lod-radius 4
    String, // --out frame.png
    Vec2,   // --xz x,z
    Vec3,   // --pos x,y,z
    Size,   // --size WxH
    Enum,   // --mode vk|d3d12, from the row's own enum_values
};

// --size 1280x720. A plain pair rather than glm::uvec2 so a tool with no glm dependency can still
// declare one.
struct Size {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] friend constexpr bool operator==(const Size&, const Size&) noexcept = default;
};

// One accepted spelling of a ValueKind::Enum option. The row owns a span of these, so the mapping
// from text to enumerator lives beside the option rather than in a switch somewhere else.
struct EnumEntry {
    std::string_view name;
    int value = 0;
};

// What a setter reports. Deliberately not an error string: the parser owns the wording so that
// every diagnostic names the option the same way.
enum class Status : std::uint8_t {
    Ok,
    BadValue,
};

[[nodiscard]] constexpr bool consumes_token(ValueKind kind) noexcept {
    return kind != ValueKind::Flag && kind != ValueKind::Toggle;
}

// The placeholder --help prints after the option name.
[[nodiscard]] constexpr std::string_view placeholder(ValueKind kind) noexcept {
    switch (kind) {
    case ValueKind::Int:
        return "N";
    case ValueKind::Float:
        return "F";
    case ValueKind::String:
        return "STR";
    case ValueKind::Vec2:
        return "x,z";
    case ValueKind::Vec3:
        return "x,y,z";
    case ValueKind::Size:
        return "WxH";
    case ValueKind::Enum:
        return "NAME";
    case ValueKind::Flag:
    case ValueKind::Toggle:
        break;
    }
    return {};
}

// What the "unparseable value" diagnostic says the option wanted.
[[nodiscard]] constexpr std::string_view expectation(ValueKind kind) noexcept {
    switch (kind) {
    case ValueKind::Int:
        return "an integer";
    case ValueKind::Float:
        return "a number";
    case ValueKind::String:
        return "a string";
    case ValueKind::Vec2:
        return "x,z";
    case ValueKind::Vec3:
        return "x,y,z";
    case ValueKind::Size:
        return "WxH";
    case ValueKind::Enum:
        return "one of its named values";
    case ValueKind::Flag:
    case ValueKind::Toggle:
        break;
    }
    return "no value";
}

} // namespace engine::cli
