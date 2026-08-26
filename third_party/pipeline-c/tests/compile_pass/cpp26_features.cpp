// ─────────────────────────────────────────────────────────────
// Compile‑time verification: C++26 feature‑gate macros
//
// This translation unit verifies that the cpp26_features.hpp
// header defines every expected macro and that each macro
// evaluates to a valid 0/1 integral constant.
// ─────────────────────────────────────────────────────────────

#include <pb/pipeline.hpp>
#include <pb/core/cpp26_features.hpp>

#include <type_traits>

// ─────────────────────────────────────────────────────────────
// 1. Every macro must be defined
// ─────────────────────────────────────────────────────────────

#ifndef PB_HAS_CPP26
#error "PB_HAS_CPP26 is not defined"
#endif

#ifndef PB_HAS_REFLECTION
#error "PB_HAS_REFLECTION is not defined"
#endif

#ifndef PB_HAS_CONTRACTS
#error "PB_HAS_CONTRACTS is not defined"
#endif

#ifndef PB_HAS_PACK_INDEXING
#error "PB_HAS_PACK_INDEXING is not defined"
#endif

#ifndef PB_HAS_STD_EXPECTED
#error "PB_HAS_STD_EXPECTED is not defined"
#endif

#ifndef PB_HAS_DEDUCING_THIS
#error "PB_HAS_DEDUCING_THIS is not defined"
#endif

// ─────────────────────────────────────────────────────────────
// 2. Every macro must be a compile‑time integral constant
// ─────────────────────────────────────────────────────────────

static_assert(std::is_integral_v<decltype(PB_HAS_CPP26)>);
static_assert(std::is_integral_v<decltype(PB_HAS_REFLECTION)>);
static_assert(std::is_integral_v<decltype(PB_HAS_CONTRACTS)>);
static_assert(std::is_integral_v<decltype(PB_HAS_PACK_INDEXING)>);
static_assert(std::is_integral_v<decltype(PB_HAS_STD_EXPECTED)>);
static_assert(std::is_integral_v<decltype(PB_HAS_DEDUCING_THIS)>);

// ─────────────────────────────────────────────────────────────
// 3. Every macro evaluates to 0 or 1
// ─────────────────────────────────────────────────────────────

static_assert(PB_HAS_CPP26       == 0 || PB_HAS_CPP26       == 1);
static_assert(PB_HAS_REFLECTION  == 0 || PB_HAS_REFLECTION  == 1);
static_assert(PB_HAS_CONTRACTS   == 0 || PB_HAS_CONTRACTS   == 1);
static_assert(PB_HAS_PACK_INDEXING == 0 || PB_HAS_PACK_INDEXING == 1);
static_assert(PB_HAS_STD_EXPECTED   == 0 || PB_HAS_STD_EXPECTED   == 1);
static_assert(PB_HAS_DEDUCING_THIS  == 0 || PB_HAS_DEDUCING_THIS  == 1);

// ─────────────────────────────────────────────────────────────
// 4. Consistency: PB_HAS_REFLECTION implies PB_HAS_CPP26
//    (per the header's own logic)
// ─────────────────────────────────────────────────────────────

static_assert(!PB_HAS_REFLECTION || PB_HAS_CPP26,
              "PB_HAS_REFLECTION cannot be 1 when PB_HAS_CPP26 is 0");

static_assert(!PB_HAS_CONTRACTS || PB_HAS_CPP26,
              "PB_HAS_CONTRACTS cannot be 1 when PB_HAS_CPP26 is 0");

// ─────────────────────────────────────────────────────────────
// 5. Pack indexing and deducing this are independent of C++26
//    (they can be 1 on a C++23 compiler that supports them)
//    — no constraint to verify beyond the 0/1 check above.
// ─────────────────────────────────────────────────────────────

int main() { return 0; }
