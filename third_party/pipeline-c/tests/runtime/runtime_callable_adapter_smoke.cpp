#include <pb/pipeline.hpp>

#include <cstdlib>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}
}  // namespace

struct In {
  int value{};
};

struct Out {
  int value{};
};

// A real C-style free function, adapted via pb::c_function_stage.
Out c_style_increment(In input) {
  return Out{input.value + 100};
}

int main() {
  // ------------------------------------------------------------------
  // 1. Stateful lambda via pb::bind_callable on the stateful engine path.
  //
  // The lambda captures a multiplier and a running counter (by shared_ptr
  // so the test can observe the persisted state).  The runtime_callable
  // stage is bound for each run via pb::run_bound; because the SAME bound
  // stage (holding the captured counter) is reused, the running counter
  // persists across runs — proving stateful runtime-bound behaviour on the
  // stateful_sequential engine path.
  // ------------------------------------------------------------------
  auto call_count = std::make_shared<int>(0);
  // Held by shared_ptr so it is a genuine runtime capture (not a compile-time
  // constant the compiler can fold away).
  auto multiplier = std::make_shared<int>(3);

  auto stateful_stage = pb::bind_callable<In, Out>(
      [multiplier, call_count](In input) -> Out {
        ++*call_count;
        return Out{input.value * *multiplier + *call_count};
      });

  using StatefulStage = decltype(stateful_stage);
  static_assert(pb::Stage<StatefulStage>);
  static_assert(pb::core::Stage<StatefulStage>);
  static_assert(std::is_same_v<StatefulStage::input_type, In>);
  static_assert(std::is_same_v<StatefulStage::output_type, Out>);

  using StatefulPipeline = pb::from<In>::then<StatefulStage>::to<Out>;
  static_assert(pb::valid<StatefulPipeline>);

  // Stateful storage policy: engine stores the (value-initialised) stage and
  // reuses it; pb::run_bound publishes our bound callable for each run.
  auto engine = pb::compile<StatefulPipeline>(pb::runtime::stateful_sequential{});

  // run #1: value 10 -> 10*3 + 1 = 31
  auto r1 = pb::run_bound(engine, In{10}, stateful_stage);
  pb_test_require(r1.value == 31);
  pb_test_require(*call_count == 1);

  // run #2: value 10 -> 10*3 + 2 = 32  (running counter persisted)
  auto r2 = pb::run_bound(engine, In{10}, stateful_stage);
  pb_test_require(r2.value == 32);
  pb_test_require(*call_count == 2);

  // run #3: value 5 -> 5*3 + 3 = 18
  auto r3 = pb::run_bound(engine, In{5}, stateful_stage);
  pb_test_require(r3.value == 18);
  pb_test_require(*call_count == 3);

  // try_run variant returns result<Out> and shares the same persisted state.
  auto r4 = pb::try_run_bound(engine, In{1}, stateful_stage);
  pb_test_require(r4.has_value());
  pb_test_require(r4.value().value == 1 * (*multiplier) + 4);  // 3 + 4 = 7
  pb_test_require(*call_count == 4);

  // Without a scoped binding, the engine's value-initialised
  // runtime_callable is unbound. That must take the documented exception path,
  // not dereference a null callable.
  auto unbound = engine.try_run(In{2});
  pb_test_require(!unbound.has_value());
  pb_test_require(unbound.error().category == pb::runtime::error_category::exception);
  pb_test_require(unbound.error().stage.key == "runtime_callable");
  pb_test_require(unbound.error().stage.name == "runtime_callable");
  pb_test_require(*call_count == 4);

  // ------------------------------------------------------------------
  // 2. std::function bound at runtime via pb::bind_callable.  Also exercise
  //    direct invocation of the owned stage (no engine, no binding scope —
  //    falls back to the stage's own owned callable).
  // ------------------------------------------------------------------
  std::function<Out(In)> fn = [](In input) { return Out{input.value - 1}; };
  auto fn_stage = pb::bind_callable<In, Out>(std::move(fn));
  using FnStage = decltype(fn_stage);
  static_assert(pb::Stage<FnStage>);

  // Direct invocation uses the stage's own owned callable.
  auto direct = fn_stage(In{42});
  pb_test_require(direct.value == 41);

  using FnPipeline = pb::from<In>::then<FnStage>::to<Out>;
  static_assert(pb::valid<FnPipeline>);
  auto fn_engine = pb::compile<FnPipeline>(pb::runtime::stateful_sequential{});
  auto fr = pb::run_bound(fn_engine, In{7}, fn_stage);
  pb_test_require(fr.value == 6);

  // ------------------------------------------------------------------
  // 3. C-style free function via pb::c_function_stage (non-type template
  //    parameter).  Carries no runtime state, so the default
  //    pb::runtime::sequential{} policy works too.
  // ------------------------------------------------------------------
  using CStage = pb::c_function_stage<In, Out, &c_style_increment>;
  static_assert(pb::Stage<CStage>);
  static_assert(std::is_same_v<CStage::input_type, In>);
  static_assert(std::is_same_v<CStage::output_type, Out>);

  // Directly invoke the C-function stage too.
  pb_test_require(CStage{}(In{0}).value == 100);

  using CPipeline = pb::from<In>::then<CStage>::to<Out>;
  static_assert(pb::valid<CPipeline>);

  auto c_engine = pb::compile<CPipeline>(pb::runtime::sequential{});
  auto cr = c_engine.run(In{1});
  pb_test_require(cr.value == 101);

  // Also valid on the stateful path (default-constructible, stateless).
  auto c_stateful_engine = pb::compile<CPipeline>(pb::runtime::stateful_sequential{});
  auto csr = c_stateful_engine.run(In{2});
  pb_test_require(csr.value == 102);

  // ------------------------------------------------------------------
  // 4. Compose a runtime_callable stage WITH a c_function_stage in one
  //    pipeline on the stateful path.  The two runtime_callable stages use
  //    distinct <In,Out> specialisations, so their bindings don't collide.
  // ------------------------------------------------------------------
  auto add_offset = pb::bind_callable<Out, In>(
      [](Out o) { return In{o.value + 1000}; });
  using ComposedPipeline =
      pb::from<In>::then<CStage>::then<decltype(add_offset)>::to<In>;
  static_assert(pb::valid<ComposedPipeline>);
  auto composed_engine =
      pb::compile<ComposedPipeline>(pb::runtime::stateful_sequential{});
  // In{3} -> c_style_increment -> Out{103} -> add_offset -> In{1103}
  auto composed_guard = pb::with_runtime_callable_binding(add_offset);
  auto composed = composed_engine.run(In{3});
  pb_test_require(composed.value == 1103);

  return 0;
}
