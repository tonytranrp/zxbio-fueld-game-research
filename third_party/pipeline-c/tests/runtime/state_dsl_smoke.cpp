// Smoke test for the stateful storage DSL.
//
// Pins the public surface of pb::with_state<...> and friends:
//   - thread-local state plumbing via pb::current_state<S>()
//   - owned / borrowed / shared / reset_per_run policies
//   - state-context lifetime (push/pop on run, no leakage across runs)
//   - composability with the error-policy wrappers
//   - throwing diagnostic when current_state<T>() is called outside a scope
//   - nested with_state<A>(with_state<B>(engine)) — independent stacks
//
// The schema-version constant `pb.state.v1` is also pinned here.

#include <pb/pipeline.hpp>

#include <array>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace state_dsl_smoke {

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

struct cache_state {
  std::unordered_map<int, std::string> cache{};
};

// A second, independent state so we can test nested with_state<A>(with_state<B>).
struct trace_state {
  std::string trail{};
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

struct lookup_or_fetch {
  using input_type  = int;
  using output_type = std::string;
  static constexpr auto stage_key()  noexcept { return "lookup_or_fetch"; }
  static constexpr auto stage_name() noexcept { return "lookup_or_fetch"; }

  auto operator()(int key) const -> std::string {
    auto& s = pb::current_state<cache_state>();
    if (auto it = s.cache.find(key); it != s.cache.end()) {
      return it->second + "[cached]";
    }
    auto fetched = "value_for_" + std::to_string(key);
    s.cache.emplace(key, fetched);
    return fetched;
  }
};

struct append_trace {
  using input_type  = int;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "append_trace"; }
  static constexpr auto stage_name() noexcept { return "append_trace"; }

  auto operator()(int value) const -> int {
    auto& s = pb::current_state<trace_state>();
    s.trail.append("/" + std::to_string(value));
    return value;
  }
};

// A stage that uses BOTH counter_state and trace_state — must work because
// the two stacks are independent.
struct multi_state_stage {
  using input_type  = int;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "multi_state_stage"; }
  static constexpr auto stage_name() noexcept { return "multi_state_stage"; }

  auto operator()(int value) const -> int {
    auto& counter = pb::current_state<counter_state>();
    auto& trace = pb::current_state<trace_state>();
    ++counter.hits;
    trace.trail.append("|" + std::to_string(value));
    return value;
  }
};

// ----- pipelines -------------------------------------------------------------

using IncrementPipeline = pb::from<int>::then<increment_counter>::to<int>;
using LookupPipeline    = pb::from<int>::then<lookup_or_fetch>::to<std::string>;
using TracePipeline     = pb::from<int>::then<append_trace>::then<increment_counter>::to<int>;
using MultiStatePipeline = pb::from<int>::then<multi_state_stage>::to<int>;

template <class Pipeline>
auto fresh_engine() {
  return pb::compile<Pipeline>(pb::runtime::sequential{});
}

// ────────────────────────────────────────────────────────────────────────────
// 1) Schema-version constant is locked
// ────────────────────────────────────────────────────────────────────────────

void test_schema_version() {
  static_assert(pb::state_dsl_schema_version == std::string_view{"pb.state.v1"},
                "pb::state_dsl_schema_version must be pb.state.v1 — part of the v1 stability promise");
}

// ────────────────────────────────────────────────────────────────────────────
// 2) owned policy: state persists across runs
// ────────────────────────────────────────────────────────────────────────────

void test_owned_state_persists_across_runs() {
  auto stateful = pb::with_state<counter_state>(fresh_engine<IncrementPipeline>());

  pb_test_require(stateful.state().hits   == 0);
  pb_test_require(stateful.state().misses == 0);

  pb_test_require(stateful.run(1)  == 1);
  pb_test_require(stateful.run(2)  == 2);
  pb_test_require(stateful.run(-1) == -1);

  pb_test_require(stateful.state().hits   == 2);
  pb_test_require(stateful.state().misses == 1);
}

// ────────────────────────────────────────────────────────────────────────────
// 3) owned policy: caller-supplied initial state
// ────────────────────────────────────────────────────────────────────────────

void test_owned_state_with_init() {
  counter_state seed{.hits = 100, .misses = 50};
  auto stateful = pb::with_state<counter_state>(fresh_engine<IncrementPipeline>(), seed);

  pb_test_require(stateful.state().hits   == 100);
  pb_test_require(stateful.state().misses == 50);

  pb_test_require(stateful.run(5) == 5);
  pb_test_require(stateful.state().hits == 101);
}

// ────────────────────────────────────────────────────────────────────────────
// 4) owned policy: reset_state clears
// ────────────────────────────────────────────────────────────────────────────

void test_owned_reset_state() {
  auto stateful = pb::with_state<counter_state>(fresh_engine<IncrementPipeline>());
  pb_test_require(stateful.run(1) == 1);
  pb_test_require(stateful.run(2) == 2);
  pb_test_require(stateful.state().hits == 2);

  stateful.reset_state();
  pb_test_require(stateful.state().hits   == 0);
  pb_test_require(stateful.state().misses == 0);
}

// ────────────────────────────────────────────────────────────────────────────
// 5) owned policy: cache scenario
// ────────────────────────────────────────────────────────────────────────────

void test_owned_cache_scenario() {
  auto stateful = pb::with_state<cache_state>(fresh_engine<LookupPipeline>());

  auto first  = stateful.run(7);     // miss
  auto second = stateful.run(7);     // hit
  auto third  = stateful.run(8);     // miss

  pb_test_require(first  == "value_for_7");
  pb_test_require(second == "value_for_7[cached]");
  pb_test_require(third  == "value_for_8");
  pb_test_require(stateful.state().cache.size() == 2);
}

// ────────────────────────────────────────────────────────────────────────────
// 6) borrowed policy: caller supplies state per call
// ────────────────────────────────────────────────────────────────────────────

void test_borrowed_state() {
  auto stateful = pb::with_borrowed_state<counter_state>(fresh_engine<IncrementPipeline>());

  counter_state per_request{};
  pb_test_require(stateful.run_with_state(1, per_request) == 1);
  pb_test_require(stateful.run_with_state(2, per_request) == 2);
  pb_test_require(per_request.hits == 2);

  counter_state other_request{};
  pb_test_require(stateful.run_with_state(-1, other_request) == -1);
  pb_test_require(other_request.misses == 1);
  pb_test_require(other_request.hits   == 0);  // independent state
  pb_test_require(per_request.hits     == 2);  // not affected
}

// ────────────────────────────────────────────────────────────────────────────
// 7) shared policy: shared_ptr<state>
// ────────────────────────────────────────────────────────────────────────────

void test_shared_state() {
  auto state_ptr = std::make_shared<counter_state>();
  auto stateful  = pb::with_shared_state<counter_state>(fresh_engine<IncrementPipeline>(), state_ptr);

  pb_test_require(stateful.run(1) == 1);
  pb_test_require(stateful.run(2) == 2);

  pb_test_require(state_ptr->hits == 2);  // shared_ptr observers the same state
  pb_test_require(stateful.shared_state().get() == state_ptr.get());

  // Two engines sharing the same state both see updates.
  auto stateful2 = pb::with_shared_state<counter_state>(fresh_engine<IncrementPipeline>(), state_ptr);
  pb_test_require(stateful2.run(3) == 3);
  pb_test_require(state_ptr->hits == 3);
}

// ────────────────────────────────────────────────────────────────────────────
// 8) reset_per_run policy: state cleared before each run
// ────────────────────────────────────────────────────────────────────────────

void test_reset_per_run_state() {
  auto stateful = pb::with_reset_per_run_state<counter_state>(fresh_engine<IncrementPipeline>());

  pb_test_require(stateful.run(1) == 1);
  pb_test_require(stateful.state().hits == 1);

  pb_test_require(stateful.run(2) == 2);
  pb_test_require(stateful.state().hits == 1);  // reset before run, then incremented

  pb_test_require(stateful.run(-1) == -1);
  pb_test_require(stateful.state().hits   == 0);
  pb_test_require(stateful.state().misses == 1);
}

// ────────────────────────────────────────────────────────────────────────────
// 9) multi-state: stages that read two independent states in the same call
// ────────────────────────────────────────────────────────────────────────────

void test_multi_state_pipeline() {
  auto stateful = pb::with_state<counter_state>(
      pb::with_state<trace_state>(fresh_engine<MultiStatePipeline>()));

  pb_test_require(stateful.run(7)  == 7);
  pb_test_require(stateful.run(11) == 11);

  pb_test_require(stateful.state().hits == 2);

  // Reach into the inner wrapper to read the trace_state directly.
  auto& inner = stateful.underlying();
  pb_test_require(inner.state().trail == std::string{"|7|11"});
}

// ────────────────────────────────────────────────────────────────────────────
// 10) current_state<T>() outside any scope throws contract_violation
// ────────────────────────────────────────────────────────────────────────────

void test_current_state_outside_scope_throws() {
  bool caught{false};
  try {
    (void)pb::current_state<counter_state>();
  } catch (const pb::pipeline_exception& exception) {
    caught = true;
    pb_test_require(exception.diagnostic().category ==
                    pb::runtime::error_category::contract_violation);
    std::string_view what = exception.what();
    pb_test_require(what.find("pb::current_state") != std::string_view::npos);
  }
  pb_test_require(caught);
}

// ────────────────────────────────────────────────────────────────────────────
// 11) try_current_state<T>() returns nullptr outside any scope
// ────────────────────────────────────────────────────────────────────────────

void test_try_current_state_outside_scope() {
  pb_test_require(pb::try_current_state<counter_state>() == nullptr);
}


// ────────────────────────────────────────────────────────────────────────────
// 12) same-type nested frames restore the previous active frame
// ────────────────────────────────────────────────────────────────────────────

void test_state_context_restores_same_type_nested_frames() {
  counter_state outer{.hits = 10, .misses = 0};
  counter_state inner{.hits = 20, .misses = 0};

  pb_test_require(pb::try_current_state<counter_state>() == nullptr);
  {
    pb::state_context<counter_state> outer_frame{outer};
    pb_test_require(pb::try_current_state<counter_state>() == &outer);
    {
      pb::state_context<counter_state> inner_frame{inner};
      pb_test_require(pb::try_current_state<counter_state>() == &inner);
      pb_test_require(&pb::current_state<counter_state>() == &inner);
    }
    pb_test_require(pb::try_current_state<counter_state>() == &outer);
    pb_test_require(&pb::current_state<counter_state>() == &outer);
  }
  pb_test_require(pb::try_current_state<counter_state>() == nullptr);

  // Defensive lifetime hardening: if callers allocate frames dynamically and
  // destroy an older frame before a newer same-type frame, the older destructor
  // must remove its own frame instead of blindly popping the active one.
  auto outer_frame = std::make_unique<pb::state_context<counter_state>>(outer);
  auto inner_frame = std::make_unique<pb::state_context<counter_state>>(inner);
  pb_test_require(pb::try_current_state<counter_state>() == &inner);

  outer_frame.reset();
  pb_test_require(pb::try_current_state<counter_state>() == &inner);

  inner_frame.reset();
  pb_test_require(pb::try_current_state<counter_state>() == nullptr);
}

// ────────────────────────────────────────────────────────────────────────────
// 13) Composability: with_state under with_throw_on_error
// ────────────────────────────────────────────────────────────────────────────

void test_composability_with_throw_on_error() {
  // Wrap with throwing wrapper first, then state — verifies forwarding.
  auto stateful = pb::with_state<counter_state>(
      pb::with_throw_on_error(fresh_engine<IncrementPipeline>()));

  pb_test_require(stateful.run(1) == 1);
  pb_test_require(stateful.run(2) == 2);
  pb_test_require(stateful.state().hits == 2);

  // Reverse stack also works: with_throw_on_error wraps a stateful engine.
  auto throwing_stateful = pb::with_throw_on_error(
      pb::with_state<counter_state>(fresh_engine<IncrementPipeline>()));

  pb_test_require(throwing_stateful.run(1) == 1);
  pb_test_require(throwing_stateful.underlying().state().hits == 1);
}

// ────────────────────────────────────────────────────────────────────────────
// 14) state_context push/pop balanced even when stage throws
// ────────────────────────────────────────────────────────────────────────────

void test_stateful_batch_runs_keep_state_context() {
  const std::array inputs{1, 2, -1};

  auto stateful = pb::with_state<counter_state>(
      pb::with_throw_on_error(fresh_engine<IncrementPipeline>()));
  auto results = stateful.try_run_each(inputs);
  pb_test_require(results.size() == inputs.size());
  pb_test_require(results[0].has_value());
  pb_test_require(results[1].has_value());
  pb_test_require(results[2].has_value());
  pb_test_require(stateful.state().hits == 2);
  pb_test_require(stateful.state().misses == 1);

  auto range_results = stateful.try_run_range(inputs.begin() + 1, inputs.end());
  pb_test_require(range_results.size() == 2);
  pb_test_require(range_results[0].has_value());
  pb_test_require(range_results[1].has_value());
  pb_test_require(stateful.state().hits == 3);
  pb_test_require(stateful.state().misses == 2);

  auto throwing_stateful = pb::with_throw_on_error(
      pb::with_state<counter_state>(fresh_engine<IncrementPipeline>()));
  auto forwarded_results = throwing_stateful.try_run_each(inputs);
  pb_test_require(forwarded_results.size() == inputs.size());
  pb_test_require(forwarded_results[0].has_value());
  pb_test_require(forwarded_results[1].has_value());
  pb_test_require(forwarded_results[2].has_value());
  pb_test_require(throwing_stateful.underlying().state().hits == 2);
  pb_test_require(throwing_stateful.underlying().state().misses == 1);

  auto borrowed = pb::with_borrowed_state<counter_state>(fresh_engine<IncrementPipeline>());
  counter_state per_request{};
  auto borrowed_results = borrowed.try_run_each_with_state(inputs, per_request);
  pb_test_require(borrowed_results.size() == inputs.size());
  pb_test_require(per_request.hits == 2);
  pb_test_require(per_request.misses == 1);
}

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

using ThrowingStatefulPipeline = pb::from<int>::then<throwing_stateful_stage>::to<int>;

void test_state_context_balanced_on_exception() {
  {
    auto stateful = pb::with_state<counter_state>(fresh_engine<ThrowingStatefulPipeline>());

    auto outcome = stateful.try_run(5);
    pb_test_require(!outcome.has_value());
    pb_test_require(stateful.state().hits == 1);  // stage ran once
  }
  // After the engine is destroyed and the context-frame's destructor ran,
  // the thread-local stack must be empty again — otherwise a later call to
  // current_state<counter_state>() would silently succeed on a dangling
  // pointer.
  pb_test_require(pb::try_current_state<counter_state>() == nullptr);
}

void test_try_run_with_state_clears_frame_on_error() {
  auto borrowed = pb::with_borrowed_state<counter_state>(fresh_engine<ThrowingStatefulPipeline>());

  counter_state per_request{};
  auto borrowed_outcome = borrowed.try_run_with_state(5, per_request);
  pb_test_require(!borrowed_outcome.has_value());
  pb_test_require(per_request.hits == 1);
  pb_test_require(pb::try_current_state<counter_state>() == nullptr);

  auto owned = pb::with_state<counter_state>(
      fresh_engine<ThrowingStatefulPipeline>(), counter_state{.hits = 40, .misses = 2});
  counter_state shadow{};
  auto owned_outcome = owned.try_run_with_state(7, shadow);
  pb_test_require(!owned_outcome.has_value());
  pb_test_require(shadow.hits == 1);
  // A caller-supplied state shadows, but does not mutate, the owned storage.
  pb_test_require(owned.state().hits == 40);
  pb_test_require(owned.state().misses == 2);
  pb_test_require(pb::try_current_state<counter_state>() == nullptr);
}

// ────────────────────────────────────────────────────────────────────────────
// 15) borrowed: shadowing owned state via run_with_state
// ────────────────────────────────────────────────────────────────────────────

void test_owned_with_run_with_state_shadows() {
  auto stateful = pb::with_state<counter_state>(fresh_engine<IncrementPipeline>(),
                                                counter_state{.hits = 99, .misses = 99});

  counter_state per_call{};
  pb_test_require(stateful.run_with_state(1, per_call) == 1);

  pb_test_require(per_call.hits == 1);            // active during the call
  pb_test_require(stateful.state().hits == 99);   // owned state untouched
  pb_test_require(stateful.state().misses == 99);
}

// ────────────────────────────────────────────────────────────────────────────
// 16) Forwarding surface mirrors the other wrappers
// ────────────────────────────────────────────────────────────────────────────

void test_forwarding_surface() {
  auto stateful = pb::with_state<counter_state>(fresh_engine<IncrementPipeline>());

  pb_test_require(stateful.get_observer() == nullptr);
  stateful.set_observer(nullptr);  // no-op, just verifying the method exists
  (void)stateful.describe();
  (void)stateful.descriptor();

  // underlying() chain works
  auto& engine = stateful.underlying();
  (void)engine.try_run(1);
}

void test_state_dsl_smoke() {
  test_schema_version();
  test_owned_state_persists_across_runs();
  test_owned_state_with_init();
  test_owned_reset_state();
  test_owned_cache_scenario();
  test_borrowed_state();
  test_shared_state();
  test_reset_per_run_state();
  test_multi_state_pipeline();
  test_current_state_outside_scope_throws();
  test_try_current_state_outside_scope();
  test_state_context_restores_same_type_nested_frames();
  test_composability_with_throw_on_error();
  test_stateful_batch_runs_keep_state_context();
  test_state_context_balanced_on_exception();
  test_try_run_with_state_clears_frame_on_error();
  test_owned_with_run_with_state_shadows();
  test_forwarding_surface();
}

} // namespace state_dsl_smoke

int main() {
  state_dsl_smoke::test_state_dsl_smoke();
  return 0;
}
