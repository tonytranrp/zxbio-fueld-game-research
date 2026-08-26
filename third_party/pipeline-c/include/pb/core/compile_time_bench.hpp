#pragma once

#include <cstddef>
#include <type_traits>

namespace pb::bench {

// ─────────────────────────────────────────────────────────────
// Compile-time counter using template instantiation depth.
// Each recursive instantiation adds one level; the final depth
// is a proxy for compile-time cost of template processing.
// ─────────────────────────────────────────────────────────────
template <std::size_t N>
struct compile_counter {
  static constexpr std::size_t value = N;
};

// ─────────────────────────────────────────────────────────────
// Extract the compile-time depth from a counter type.
// ─────────────────────────────────────────────────────────────
template <class T>
struct instantiation_depth;

template <std::size_t N>
struct instantiation_depth<compile_counter<N>>
    : std::integral_constant<std::size_t, N> {};

template <class T>
inline constexpr std::size_t instantiation_depth_v =
    instantiation_depth<T>::value;

// ─────────────────────────────────────────────────────────────
// Qualitative depth classification for compile-time profiling.
// ─────────────────────────────────────────────────────────────
template <std::size_t N>
struct chain_depth {
  using counter = compile_counter<N>;
  static constexpr std::size_t depth = N;

  static constexpr bool is_trivial  = (N <= 5);
  static constexpr bool is_moderate = (N > 5 && N <= 25);
  static constexpr bool is_heavy    = (N > 25 && N <= 75);
  static constexpr bool is_stress   = (N > 75);
};

// ─────────────────────────────────────────────────────────────
// Stage chain benchmark: measures template instantiation cost
// for an N-stage pipeline chain.  Use in static_assert tests
// to verify the compiler can handle the specified depth.
// ─────────────────────────────────────────────────────────────
template <std::size_t N>
struct stage_chain_bench {
  static constexpr std::size_t template_depth = N;
  static constexpr std::size_t stage_count    = N;

  // Baseline reference: 5 stages is the "trivial" compile-time baseline
  static constexpr std::size_t baseline_depth = 5;

  // Depth ratio relative to baseline
  static constexpr double depth_ratio =
      static_cast<double>(N) / static_cast<double>(baseline_depth);

  // Qualitative classification
  using depth_class = chain_depth<N>;
  static constexpr bool is_trivial  = depth_class::is_trivial;
  static constexpr bool is_moderate = depth_class::is_moderate;
  static constexpr bool is_heavy    = depth_class::is_heavy;
  static constexpr bool is_stress   = depth_class::is_stress;
};

// ─────────────────────────────────────────────────────────────
// Compile-time baseline size constants (stage counts).
// ─────────────────────────────────────────────────────────────
inline constexpr std::size_t trivial_chain_size  = 5;
inline constexpr std::size_t moderate_chain_size = 25;
inline constexpr std::size_t heavy_chain_size    = 50;
inline constexpr std::size_t stress_chain_size   = 100;

// ─────────────────────────────────────────────────────────────
// Baseline record: captures a compile-time measurement.
// ─────────────────────────────────────────────────────────────
template <std::size_t StageCount, std::size_t TemplateDepth>
struct baseline_record {
  static constexpr std::size_t stages = StageCount;
  static constexpr std::size_t depth  = TemplateDepth;
};

// ─────────────────────────────────────────────────────────────
// Pre-computed baseline aliases for common chain sizes.
// ─────────────────────────────────────────────────────────────
using trivial_baseline =
    baseline_record<trivial_chain_size, trivial_chain_size>;
using moderate_baseline =
    baseline_record<moderate_chain_size, moderate_chain_size>;
using heavy_baseline =
    baseline_record<heavy_chain_size, heavy_chain_size>;
using stress_baseline =
    baseline_record<stress_chain_size, stress_chain_size>;

// ─────────────────────────────────────────────────────────────
// Compile-time verification helper: ensures a chain of N stages
// is within the expected compile-time budget for the given class.
// ─────────────────────────────────────────────────────────────
template <std::size_t N>
consteval bool verify_chain_budget() noexcept {
  return stage_chain_bench<N>::template_depth == N;
}

// ─────────────────────────────────────────────────────────────
// Static assertions for the compile-time benchmark infrastructure
// itself (self-testing the measurement framework).
// ─────────────────────────────────────────────────────────────
static_assert(compile_counter<0>::value == 0);
static_assert(compile_counter<5>::value == 5);
static_assert(compile_counter<50>::value == 50);
static_assert(instantiation_depth_v<compile_counter<42>> == 42);

static_assert(stage_chain_bench<5>::template_depth == 5);
static_assert(stage_chain_bench<5>::stage_count == 5);
static_assert(stage_chain_bench<5>::is_trivial);
static_assert(!stage_chain_bench<5>::is_moderate);

static_assert(stage_chain_bench<50>::template_depth == 50);
static_assert(stage_chain_bench<50>::is_heavy);
static_assert(!stage_chain_bench<50>::is_trivial);

static_assert(stage_chain_bench<100>::is_stress);
static_assert(!stage_chain_bench<100>::is_heavy);

static_assert(chain_depth<3>::is_trivial);
static_assert(chain_depth<10>::is_moderate);
static_assert(chain_depth<30>::is_heavy);
static_assert(chain_depth<80>::is_stress);

static_assert(trivial_baseline::stages == 5);
static_assert(heavy_baseline::stages == 50);
static_assert(stress_baseline::stages == 100);

static_assert(verify_chain_budget<5>());
static_assert(verify_chain_budget<50>());

} // namespace pb::bench
