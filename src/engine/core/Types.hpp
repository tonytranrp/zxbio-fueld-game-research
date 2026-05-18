#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <functional>

// ------------------------------------------------------------------------------
// Force-inline hint — portable across MSVC, GCC, Clang.
// Use for small hot-path functions (single-digit lines) where the call overhead
// is measurable relative to the work done.  The compiler still decides; this is
// a strong hint, not a mandate.
// ------------------------------------------------------------------------------
#if defined(_MSC_VER)
  #define BIOFUEL_FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
  #define BIOFUEL_FORCE_INLINE __attribute__((always_inline)) inline
#else
  #define BIOFUEL_FORCE_INLINE inline
#endif

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

// =============================================================================
// Compile-time type-size verification — catch ABI/portability issues early
// =============================================================================
static_assert(sizeof(i8) == 1,  "i8 must be exactly 1 byte");
static_assert(sizeof(i16) == 2, "i16 must be exactly 2 bytes");
static_assert(sizeof(i32) == 4, "i32 must be exactly 4 bytes");
static_assert(sizeof(i64) == 8, "i64 must be exactly 8 bytes");

static_assert(sizeof(u8) == 1,  "u8 must be exactly 1 byte");
static_assert(sizeof(u16) == 2, "u16 must be exactly 2 bytes");
static_assert(sizeof(u32) == 4, "u32 must be exactly 4 bytes");
static_assert(sizeof(u64) == 8, "u64 must be exactly 8 bytes");

static_assert(sizeof(f32) == 4, "f32 must be exactly 4 bytes (IEEE 754 single)");
static_assert(sizeof(f64) == 8, "f64 must be exactly 8 bytes (IEEE 754 double)");

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
