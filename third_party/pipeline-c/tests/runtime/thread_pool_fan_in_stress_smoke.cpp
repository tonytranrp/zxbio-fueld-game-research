/// @file  thread_pool_fan_in_stress_smoke.cpp
/// @brief Stress test for fan-in execution on the thread-pool backend.
///
/// A wide fan-in pipeline (8 independent cases) is run on a multi-worker
/// thread-pool backend across many repeated iterations. Every iteration the
/// test asserts that:
///   • each case produced its correct, case-indexed result,
///   • the aggregate slot ordering is deterministic (slot N == case N) and is
///     NOT affected by the non-deterministic order in which workers finish,
///   • the joined output is byte-for-byte identical run after run.
///
/// This proves thread-safety / absence of data races and stable ordering under
/// load. main() returns 0 on success (assertions abort otherwise).

#include <pb/pipeline.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

void require(bool condition) {
  if (!condition) std::abort();
}

struct Input {
  int seed{};
};

// A single case result carries the case index plus a value derived from the
// shared input, so the join can verify both ordering and correctness.
struct Tagged {
  int index{};
  int value{};
};

struct Aggregated {
  long long checksum{};
  std::string order; // concatenation of slot indices in case order
};

// Always-true predicate shared by every case (the fan-in is "wide", not gated).
struct AlwaysRun {
  using input_type  = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return true; }
};

// Each case is a distinct stage type carrying its own compile-time index. We do
// a touch of contended work (atomic increment + brief overlap) so that, under
// many iterations, races would manifest as wrong values or wrong ordering.
template <int Index>
struct Worker {
  using input_type  = Input;
  using output_type = Tagged;

  static inline std::atomic<long long> invocations{0};

  Tagged operator()(const Input& input) const {
    invocations.fetch_add(1, std::memory_order_relaxed);
    // Encourage genuine overlap across workers without being slow.
    std::this_thread::sleep_for(std::chrono::microseconds{50});
    return Tagged{Index, input.seed * 1000 + Index};
  }
};

template <int Index>
using WorkerCase = typename pb::case_<AlwaysRun>::template then<Worker<Index>>;

using Case0 = WorkerCase<0>;
using Case1 = WorkerCase<1>;
using Case2 = WorkerCase<2>;
using Case3 = WorkerCase<3>;
using Case4 = WorkerCase<4>;
using Case5 = WorkerCase<5>;
using Case6 = WorkerCase<6>;
using Case7 = WorkerCase<7>;

using FanInResults = pb::fan_in_output_t<Case0, Case1, Case2, Case3, Case4, Case5, Case6, Case7>;

struct Join {
  using input_type  = FanInResults;
  using output_type = Aggregated;

  template <std::size_t N>
  static void fold(const FanInResults& results, Aggregated& out) {
    const auto& slot = results.template get<N>();
    require(slot.completed());
    const auto& tagged = slot.get();
    // The slot index MUST equal the case index regardless of completion order.
    require(tagged.index == static_cast<int>(N));
    out.checksum += tagged.value;
    out.order += std::to_string(tagged.index);
  }

  Aggregated operator()(const FanInResults& results) const {
    Aggregated out{};
    fold<0>(results, out);
    fold<1>(results, out);
    fold<2>(results, out);
    fold<3>(results, out);
    fold<4>(results, out);
    fold<5>(results, out);
    fold<6>(results, out);
    fold<7>(results, out);
    return out;
  }
};

using Pipeline = pb::from<Input>
                     ::branch<Case0, Case1, Case2, Case3, Case4, Case5, Case6, Case7>
                     ::fan_in<Join>
                     ::to<Aggregated>;
static_assert(pb::valid<Pipeline>);

long long expected_checksum(int seed) {
  long long sum = 0;
  for (int index = 0; index < 8; ++index) {
    sum += seed * 1000 + index;
  }
  return sum;
}

} // namespace

int main() {
  constexpr int kIterations = 250;

  // A single engine reused across all iterations — exercises the pool's task
  // queue and worker lifecycle repeatedly under load.
  auto engine = pb::compile<Pipeline>(pb::runtime::thread_pool_backend{.worker_count = 4});
  require(engine.worker_count() == 4);

  for (int iteration = 0; iteration < kIterations; ++iteration) {
    const int seed = iteration + 1;
    auto result = engine.run(Input{seed});
    require(result.has_value());

    // Deterministic, stable ordering: slots always appear in case order.
    require(result.value().order == "01234567");
    // Correct aggregate every single iteration (no lost / duplicated cases).
    require(result.value().checksum == expected_checksum(seed));
  }

  // Every case stage ran exactly once per iteration.
  require(Worker<0>::invocations.load() == kIterations);
  require(Worker<7>::invocations.load() == kIterations);

  // Fresh engines in a tight loop also stay correct (construction / teardown
  // of the worker pool under repetition must not corrupt results).
  for (int iteration = 0; iteration < 50; ++iteration) {
    auto fresh = pb::compile<Pipeline>(pb::runtime::thread_pool_backend{.worker_count = 8});
    auto result = fresh.run(Input{iteration + 1});
    require(result.has_value());
    require(result.value().order == "01234567");
    require(result.value().checksum == expected_checksum(iteration + 1));
  }

  return 0;
}
