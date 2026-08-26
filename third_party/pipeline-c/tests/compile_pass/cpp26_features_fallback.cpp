// Production-grade C++26 feature-gate fallback test.
//
// Forces every PB_HAS_* gate to 0 *before* including the gate header to
// prove that the C++20 fallback path compiles and is internally consistent
// even when a future compiler stops advertising one of the feature-test
// macros.  This is the exact code path that runs on the supported GCC,
// Clang, and MSVC baselines today.

#define PB_HAS_CPP26           0
#define PB_HAS_REFLECTION      0
#define PB_HAS_CONTRACTS       0
#define PB_HAS_PACK_INDEXING   0
#define PB_HAS_STD_EXPECTED    0
#define PB_HAS_DEDUCING_THIS   0

#include <pb/core/cpp26_features.hpp>

// Every typed feature constant must compile and evaluate to false in the
// fallback configuration.  This catches regressions where a future header
// change accidentally tied a typed constant to something *other* than the
// macro it documents.
static_assert(pb::features::has_cpp26          == false);
static_assert(pb::features::has_reflection     == false);
static_assert(pb::features::has_contracts      == false);
static_assert(pb::features::has_pack_indexing  == false);
static_assert(pb::features::has_std_expected   == false);
static_assert(pb::features::has_deducing_this  == false);

// Implication invariants must continue to hold in the fallback config.
static_assert(pb::features::reflection_implies_cpp26);
static_assert(pb::features::contracts_implies_cpp26);

// `if constexpr` on a feature constant must compile both branches in the
// fallback configuration — that's the whole point of the typed gate API.
constexpr int probe() {
  if constexpr (pb::features::has_cpp26) {
    return 1;
  } else {
    return 0;
  }
}

static_assert(probe() == 0);

int main() { return 0; }
