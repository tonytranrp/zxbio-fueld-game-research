#pragma once

// ─────────────────────────────────────────────────────────────
// C++26 (and beyond) feature‑detection macros
//
// These macros allow the library to opt into future language
// facilities when available while remaining compatible with
// C++20 baseline compilers.
// ─────────────────────────────────────────────────────────────

// Every PB_HAS_* gate respects a user pre-definition.  This lets
// downstream code force-disable a feature for testing fallback
// behaviour, or force-enable it on compilers that ship the feature
// without advertising its standardised feature-test macro yet.
// Pre-defining a gate to 1 on a compiler that cannot actually parse
// the feature is the caller's responsibility — the library still
// requires the underlying compiler support to use the feature.

// ── C++26 core language ─────────────────────────────────────

#if !defined(PB_HAS_CPP26)
  #if __cplusplus >= 202400L
    #define PB_HAS_CPP26 1
  #else
    #define PB_HAS_CPP26 0
  #endif
#endif

// ── static reflection (P2996 / P1240) ───────────────────────

#if !defined(PB_HAS_REFLECTION)
  #if PB_HAS_CPP26 && defined(__cpp_reflection)
    #define PB_HAS_REFLECTION 1
  #else
    #define PB_HAS_REFLECTION 0
  #endif
#endif

// ── contracts (P2900) ───────────────────────────────────────

#if !defined(PB_HAS_CONTRACTS)
  #if PB_HAS_CPP26 && defined(__cpp_contracts)
    #define PB_HAS_CONTRACTS 1
  #else
    #define PB_HAS_CONTRACTS 0
  #endif
#endif

// ── pack indexing (P2662) ────────────────────────────────────
// This feature may also land in C++23 with sufficient compiler
// support, so the guard is independent of the PB_HAS_CPP26 gate.

#if !defined(PB_HAS_PACK_INDEXING)
  #if defined(__cpp_pack_indexing)
    #define PB_HAS_PACK_INDEXING 1
  #else
    #define PB_HAS_PACK_INDEXING 0
  #endif
#endif

// ── std::expected (C++23) ────────────────────────────────────
// While not C++26, this is the primary "modern result type" and
// is worth a dedicated feature gate for conditional compilation.

#if !defined(PB_HAS_STD_EXPECTED)
  #if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
    #define PB_HAS_STD_EXPECTED 1
  #else
    #define PB_HAS_STD_EXPECTED 0
  #endif
#endif

// ── deducing this (C++23, P0847) ─────────────────────────────

#if !defined(PB_HAS_DEDUCING_THIS)
  #if defined(__cpp_explicit_this_parameter)
    #define PB_HAS_DEDUCING_THIS 1
  #else
    #define PB_HAS_DEDUCING_THIS 0
  #endif
#endif

// ── Compile‑time validation bundle ───────────────────────────
// These static_asserts fire at library inclusion time to
// surface misconfigured feature‑detection scenarios.

static_assert((PB_HAS_CPP26 == 0) || (PB_HAS_CPP26 == 1),
              "PB_HAS_CPP26 must be 0 or 1");

static_assert((PB_HAS_REFLECTION == 0) || (PB_HAS_REFLECTION == 1),
              "PB_HAS_REFLECTION must be 0 or 1");

static_assert((PB_HAS_CONTRACTS == 0) || (PB_HAS_CONTRACTS == 1),
              "PB_HAS_CONTRACTS must be 0 or 1");

static_assert((PB_HAS_PACK_INDEXING == 0) || (PB_HAS_PACK_INDEXING == 1),
              "PB_HAS_PACK_INDEXING must be 0 or 1");

static_assert((PB_HAS_STD_EXPECTED == 0) || (PB_HAS_STD_EXPECTED == 1),
              "PB_HAS_STD_EXPECTED must be 0 or 1");

static_assert((PB_HAS_DEDUCING_THIS == 0) || (PB_HAS_DEDUCING_THIS == 1),
              "PB_HAS_DEDUCING_THIS must be 0 or 1");

// PB_HAS_CPP26 implies the compiler claims C++26 or later.
// The other feature gates are independently gated on the
// corresponding feature‑test macros and may be 0 even when
// PB_HAS_CPP26 is 1 (e.g. a C++26 compiler that hasn't shipped
// reflection yet).

// ── Typed feature constants (production-grade API) ───────────
// Mirror every PB_HAS_* macro as a strongly-typed `constexpr bool`
// inside the `pb::features` namespace.  User code should prefer the
// `pb::features::*` constants over the raw `PB_HAS_*` macros because:
//
//   - they participate in constant evaluation and SFINAE without
//     macro-expansion surprises;
//   - they can be passed as non-type template arguments, used in
//     concepts, and inspected by `pb_cli features`;
//   - they keep `#if` ladders out of user code — `if constexpr` plus
//     `pb::features::has_*` is the supported way to branch on a gate.
//
// The constants are `inline constexpr` so they obey the one-definition
// rule across translation units even when the gates differ per build.

namespace pb::features {

inline constexpr bool has_cpp26          = PB_HAS_CPP26 != 0;
inline constexpr bool has_reflection     = PB_HAS_REFLECTION != 0;
inline constexpr bool has_contracts      = PB_HAS_CONTRACTS != 0;
inline constexpr bool has_pack_indexing  = PB_HAS_PACK_INDEXING != 0;
inline constexpr bool has_std_expected   = PB_HAS_STD_EXPECTED != 0;
inline constexpr bool has_deducing_this  = PB_HAS_DEDUCING_THIS != 0;

/// Implication invariants — these mirror the static_asserts above so user
/// code can use them in static_asserts of its own without depending on the
/// raw macros.
inline constexpr bool reflection_implies_cpp26 = (!has_reflection) || has_cpp26;
inline constexpr bool contracts_implies_cpp26  = (!has_contracts)  || has_cpp26;

static_assert(reflection_implies_cpp26,
              "pb::features::has_reflection must imply pb::features::has_cpp26");
static_assert(contracts_implies_cpp26,
              "pb::features::has_contracts must imply pb::features::has_cpp26");

} // namespace pb::features
