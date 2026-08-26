// Smoke test for the thread-local stateful storage policy.
//
// Pins the public surface of pb::with_thread_local_state<...>:
//   - single-thread correctness: state visible via pb::current_state<S>(),
//     persists across runs on the same thread (owned-per-thread semantics)
//   - seeded variant: each thread's first state is copy-constructed from the seed
//   - multi-threaded isolation: several std::threads each invoke the SAME
//     wrapped engine concurrently and each thread keeps its own isolated State
//     (no cross-thread interference / no data race on the state value)
//   - thread_local_state() accessor returns the calling thread's instance
//   - exception safety: the state-context frame is balanced even on stage failure
//
// This is the storage policy required before parallel backends can safely
// carry stateful stages (pb.state.v1 / thread-local backend storage).

#include <pb/pipeline.hpp>

#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace state_thread_local_smoke {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

// ----- shared state types ----------------------------------------------------

struct counter_state {
  int hits{0};
  int misses{0};
};

// ----- stateful stages -------------------------------------------------------

struct increment_counter {
  using input_type  = int;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "increment_counter"; }
  static constexpr auto stage_name() noexcept { return "increment_counter"; }

  auto operator()(int value) const -> int {
    auto& s = pb::current_state<counter_state>();
    if (value > 0) {
      ++s.hits;
    } else {
      ++s.misses;
    }
    return value;
  }
};

struct throwing_stateful_stage {
  using input_type  = int;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "throwing_stateful_stage"; }
  static constexpr auto stage_name() noexcept { return "throwing_stateful_stage"; }

  auto operator()(int) const -> int {
    auto& s = pb::current_state<counter_state>();
    ++s.hits;
    throw std::runtime_error{"intentional"};
  }
};

// ----- pipelines -------------------------------------------------------------

using IncrementPipeline = pb::from<int>::then<increment_counter>::to<int>;
using ThrowingPipeline  = pb::from<int>::then<throwing_stateful_stage>::to<int>;

template <class Pipeline>
auto fresh_engine() {
  return pb::compile<Pipeline>(pb::runtime::sequential{});
}

// ────────────────────────────────────────────────────────────────────────────
// 1) single-thread correctness: state visible and persists across runs
// ────────────────────────────────────────────────────────────────────────────

void test_single_thread_state_visible_and_persists() {
  auto stateful = pb::with_thread_local_state<counter_state>(fresh_engine<IncrementPipeline>());

  pb_test_require(stateful.run(1)  == 1);
  pb_test_require(stateful.run(2)  == 2);
  pb_test_require(stateful.run(-1) == -1);

  // Owned-per-thread: state persists across runs on this thread.
  auto& s = stateful.thread_local_state();
  pb_test_require(s.hits   == 2);
  pb_test_require(s.misses == 1);
}

// ────────────────────────────────────────────────────────────────────────────
// 2) seeded variant: this thread's first state is copy-constructed from seed
// ────────────────────────────────────────────────────────────────────────────

void test_seeded_initial_state() {
  counter_state seed{.hits = 100, .misses = 50};
  auto stateful =
      pb::with_thread_local_state<counter_state>(fresh_engine<IncrementPipeline>(), seed);

  pb_test_require(stateful.run(5) == 5);
  auto& s = stateful.thread_local_state();
  pb_test_require(s.hits   == 101);
  pb_test_require(s.misses == 50);
}

// ────────────────────────────────────────────────────────────────────────────
// 3) multi-threaded isolation: one engine, many threads, isolated state
// ────────────────────────────────────────────────────────────────────────────

void test_multithreaded_isolation() {
  // A single shared wrapped engine, invoked concurrently from many threads.
  // Each thread runs a distinct number of positive inputs; if the state were
  // shared, the per-thread hit counts would bleed into one another.
  auto stateful = pb::with_thread_local_state<counter_state>(fresh_engine<IncrementPipeline>());

  constexpr int thread_count   = 8;
  constexpr int base_run_count = 50;  // thread i performs (base + i) runs

  std::atomic<int> ready{0};
  std::atomic<bool> go{false};
  std::vector<int> observed(thread_count, -1);
  std::vector<std::thread> workers;
  workers.reserve(thread_count);

  for (int i = 0; i < thread_count; ++i) {
    workers.emplace_back([&, i] {
      const int runs = base_run_count + i;
      // Simple spin barrier so all threads hammer the engine simultaneously,
      // maximizing the chance of catching a data race if state were shared.
      ready.fetch_add(1, std::memory_order_acq_rel);
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      for (int r = 0; r < runs; ++r) {
        (void)stateful.run(1);
      }
      // Each thread reads ITS OWN thread-local state instance.
      observed[i] = stateful.thread_local_state().hits;
    });
  }

  while (ready.load(std::memory_order_acquire) < thread_count) {
    std::this_thread::yield();
  }
  go.store(true, std::memory_order_release);

  for (auto& w : workers) {
    w.join();
  }

  // Every thread must observe exactly its own run count — no interference.
  for (int i = 0; i < thread_count; ++i) {
    pb_test_require(observed[i] == base_run_count + i);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 4) exception safety: state-context frame balanced even when stage throws
// ────────────────────────────────────────────────────────────────────────────

void test_state_context_balanced_on_exception() {
  {
    auto stateful = pb::with_thread_local_state<counter_state>(fresh_engine<ThrowingPipeline>());

    auto outcome = stateful.try_run(5);
    pb_test_require(!outcome.has_value());
    pb_test_require(stateful.thread_local_state().hits == 1);  // stage ran once
  }
  // The RAII frame must have popped, so no state of this type stays active.
  pb_test_require(pb::try_current_state<counter_state>() == nullptr);
}

// ────────────────────────────────────────────────────────────────────────────
// 5) forwarding surface mirrors the other wrappers
// ────────────────────────────────────────────────────────────────────────────

void test_forwarding_surface() {
  auto stateful = pb::with_thread_local_state<counter_state>(fresh_engine<IncrementPipeline>());

  pb_test_require(stateful.get_observer() == nullptr);
  stateful.set_observer(nullptr);  // no-op, just verifying the method exists
  (void)stateful.describe();
  (void)stateful.descriptor();
  auto& engine = stateful.underlying();
  (void)engine.try_run(1);
}

void test_state_thread_local_smoke() {
  test_single_thread_state_visible_and_persists();
  test_seeded_initial_state();
  test_multithreaded_isolation();
  test_state_context_balanced_on_exception();
  test_forwarding_surface();
}

} // namespace state_thread_local_smoke

int main() {
  state_thread_local_smoke::test_state_thread_local_smoke();
  return 0;
}
