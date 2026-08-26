/// @file  thread_pool_fan_in.cpp
/// @brief Runtime micro-benchmark: sequential vs thread-pool fan-in.
///
/// Times a wide fan-in pipeline (several independent cases, each doing a fixed
/// chunk of CPU-bound work) executed first on the sequential backend, then on
/// the thread_pool_backend, over N iterations. Prints both wall-clock timings
/// and the resulting speedup so the parallel backend's benefit is visible.
///
/// This file is compiled only when PB_BUILD_BENCHMARKS is enabled (see
/// bench/CMakeLists.txt). It returns 0 on success so it can also serve as a
/// smoke check under a benchmark preset.

#include <pb/pipeline.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

struct Order {
  std::uint64_t seed{};
};

struct Score {
  std::uint64_t value{};
};

struct Total {
  std::uint64_t value{};
};

// Deterministic CPU-bound mixing so the work cannot be optimized away and each
// case has a meaningful, parallelizable cost.
std::uint64_t mix(std::uint64_t state, int rounds) {
  for (int i = 0; i < rounds; ++i) {
    state ^= state >> 33;
    state *= 0xff51afd7ed558ccdULL;
    state ^= state >> 33;
    state *= 0xc4ceb9fe1a85ec53ULL;
    state ^= state >> 33;
  }
  return state;
}

struct AlwaysRun {
  using input_type  = Order;
  using output_type = bool;
  bool operator()(const Order&) const { return true; }
};

template <int Index>
struct Heavy {
  using input_type  = Order;
  using output_type = Score;
  Score operator()(const Order& order) const {
    return Score{mix(order.seed + Index, 4000)};
  }
};

template <int Index>
using HeavyCase = typename pb::case_<AlwaysRun>::template then<Heavy<Index>>;

using C0 = HeavyCase<0>;
using C1 = HeavyCase<1>;
using C2 = HeavyCase<2>;
using C3 = HeavyCase<3>;
using C4 = HeavyCase<4>;
using C5 = HeavyCase<5>;
using C6 = HeavyCase<6>;
using C7 = HeavyCase<7>;

using FanInResults = pb::fan_in_output_t<C0, C1, C2, C3, C4, C5, C6, C7>;

struct Join {
  using input_type  = FanInResults;
  using output_type = Total;

  template <std::size_t N>
  static std::uint64_t slot(const FanInResults& r) {
    return r.template get<N>().completed() ? r.template get<N>().get().value : 0;
  }

  Total operator()(const FanInResults& r) const {
    return Total{slot<0>(r) ^ slot<1>(r) ^ slot<2>(r) ^ slot<3>(r) ^
                 slot<4>(r) ^ slot<5>(r) ^ slot<6>(r) ^ slot<7>(r)};
  }
};

using Pipeline = pb::from<Order>
                     ::branch<C0, C1, C2, C3, C4, C5, C6, C7>
                     ::fan_in<Join>
                     ::to<Total>;

template <class Engine>
std::pair<double, std::uint64_t> time_runs(Engine& engine, int iterations) {
  std::uint64_t sink = 0;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    auto result = engine.run(Order{static_cast<std::uint64_t>(i) + 1});
    sink ^= result.value().value;
  }
  const auto end = std::chrono::steady_clock::now();
  const double ms = std::chrono::duration<double, std::milli>(end - start).count();
  return {ms, sink};
}

} // namespace

int main() {
  constexpr int kIterations = 2000;

  auto sequential_engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto pool_engine       = pb::compile<Pipeline>(pb::runtime::thread_pool_backend{.worker_count = 8});

  // Warm up both engines (touch the pool's worker threads, prime caches).
  (void)time_runs(sequential_engine, 50);
  (void)time_runs(pool_engine, 50);

  const auto [seq_ms, seq_sink]   = time_runs(sequential_engine, kIterations);
  const auto [pool_ms, pool_sink] = time_runs(pool_engine, kIterations);

  std::cout << "=== Thread-Pool Fan-In Benchmark ===\n";
  std::cout << "iterations      : " << kIterations << "\n";
  std::cout << "fan-in cases    : 8\n";
  std::cout << "pool workers    : " << pool_engine.worker_count() << "\n";
  std::cout << "sequential (ms) : " << seq_ms << "\n";
  std::cout << "thread-pool(ms) : " << pool_ms << "\n";
  if (pool_ms > 0.0) {
    std::cout << "speedup         : " << (seq_ms / pool_ms) << "x\n";
  }

  // Correctness guard: both backends must compute the identical aggregate, so
  // the benchmark cannot silently measure broken work.
  if (seq_sink != pool_sink) {
    std::cerr << "MISMATCH: sequential and thread-pool produced different results\n";
    return 1;
  }
  return 0;
}
