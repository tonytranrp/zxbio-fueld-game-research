#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <functional>

namespace biofuel {

// ------------------------------------------------------------------------------
// Common type aliases
// ------------------------------------------------------------------------------
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using usize = std::size_t;

using f32 = float;
using f64 = double;

// ------------------------------------------------------------------------------
// Transparent hash for heterogeneous string/string_view map lookups.
// Usage: std::unordered_map<std::string, T, TransparentHash, std::equal_to<>>
// ------------------------------------------------------------------------------
struct TransparentHash {
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
};

} // namespace biofuel
