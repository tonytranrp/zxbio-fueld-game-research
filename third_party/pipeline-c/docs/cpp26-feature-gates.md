# C++26 feature gates

Pipeline-c++ targets a stable C++20 baseline and treats every C++23/C++26
language facility as **optional**. Each optional feature lives behind a
two-layer gate: a `PB_HAS_*` preprocessor macro (for `#if` ladders inside
library headers) and a typed `pb::features::*` `constexpr bool` constant
(for `if constexpr` and concept gating in user code).

## Why two layers?

- `PB_HAS_*` macros are necessary because some features can only be probed
  with `#if defined(__cpp_*)` — you cannot enter the relevant syntax block
  on a compiler that doesn't parse the feature.
- `pb::features::*` constants are the supported API for user code. They
  work inside `if constexpr`, concepts, and non-type template parameters
  with none of the macro-expansion footguns.

Always prefer `pb::features::has_cpp26` over `PB_HAS_CPP26` in user code.

## The current gate matrix

| Feature                          | Macro                  | Constant                            |
| -------------------------------- | ---------------------- | ----------------------------------- |
| C++26 core language              | `PB_HAS_CPP26`         | `pb::features::has_cpp26`           |
| Static reflection (P2996/P1240)  | `PB_HAS_REFLECTION`    | `pb::features::has_reflection`      |
| Contracts (P2900)                | `PB_HAS_CONTRACTS`     | `pb::features::has_contracts`       |
| Pack indexing (P2662)            | `PB_HAS_PACK_INDEXING` | `pb::features::has_pack_indexing`   |
| `std::expected` (C++23)          | `PB_HAS_STD_EXPECTED`  | `pb::features::has_std_expected`    |
| Deducing this (C++23, P0847)     | `PB_HAS_DEDUCING_THIS` | `pb::features::has_deducing_this`   |

Every gate evaluates to `0`/`false` on the C++20 baseline. The
implication invariants are also exposed:

- `pb::features::reflection_implies_cpp26`
- `pb::features::contracts_implies_cpp26`

These hold even on hypothetical compilers that ship one feature without
the other — the gate header refuses to claim a sub-feature is available
unless the corresponding language version is also detected.

## Production-grade conformance

Two tests pin the gate semantics:

- `tests/compile_pass/cpp26_features.cpp` — verifies every macro is
  defined and every constant agrees with its macro on whatever compiler
  the build actually runs against.
- `tests/compile_pass/cpp26_features_fallback.cpp` — pre-defines every
  `PB_HAS_*` macro to `0` *before* including the gate header to prove
  that the C++20 fallback path compiles cleanly and that the typed
  constants flip to `false`. This is the exact code path that runs on
  the supported GCC, Clang, and MSVC C++20 baselines.

`pb_cli features` exposes the runtime values for diagnostics and CI logs.

## Adding a new gate

1. Add the `PB_HAS_NEW_FEATURE` macro to `include/pb/core/cpp26_features.hpp`.
2. Mirror it as `pb::features::has_new_feature` immediately below.
3. Add a 0/1 `static_assert` for the new macro inside the same header.
4. Extend `tests/compile_pass/cpp26_features.cpp` with the new macro.
5. Extend `tests/compile_pass/cpp26_features_fallback.cpp` with a `#define
   PB_HAS_NEW_FEATURE 0` line and an assertion that the constant is
   `false`.
6. Update this table.

The fallback test is the production-grade gate net: if a future change to
the header accidentally couples a typed constant to something other than
the macro it documents, the fallback test fails loudly.
