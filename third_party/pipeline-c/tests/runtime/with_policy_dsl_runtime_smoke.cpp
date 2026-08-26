/// @file with_policy_dsl_runtime_smoke.cpp
/// @brief Runtime verification that `::with<pb::policy::errors::...>` on a
///        sequential pipeline actually changes `pb::compile<P>(sequential{})`
///        behaviour: the carried marker selects an engine wrapper.
///
///  - `::with<pb::policy::errors::throwing>`  -> run() throws on stage failure
///  - `::with<pb::policy::errors::ignoring>`  -> run() returns a fallback value
///  - no marker                               -> baseline result-returning engine,
///                                               byte-for-byte unchanged.
///
/// main() returns 0 on success; failures abort / propagate as exceptions.

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace {

void require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

struct Input {
  int value{};
};

struct Output {
  int value{};
};

// Always-succeeding stage.
struct Increment {
  using input_type = Input;
  using output_type = Output;
  static constexpr auto stage_key() noexcept { return "increment"; }
  static constexpr auto stage_name() noexcept { return "increment"; }
  Output operator()(Input input) const { return {input.value + 1}; }
};

// Stage that reports a declared (result<>-based) failure.
struct FailingStage {
  using input_type = Input;
  using output_type = Output;
  static constexpr auto stage_key() noexcept { return "fail"; }
  static constexpr auto stage_name() noexcept { return "fail"; }
  pb::runtime::result<Output> operator()(Input) const {
    return pb::runtime::result<Output>{pb::runtime::error{
        .category = pb::runtime::error_category::stage_failure,
        .message = "declared failure",
    }};
  }
};

// Pipelines built purely through the research-plan DSL syntax.  The error
// policy marker is threaded into the pipeline TYPE by ::with<...>; nothing
// at the call site selects a wrapper explicitly.
using ThrowingFailurePipeline =
    pb::from<Input>::with<pb::policy::errors::throwing>::then<FailingStage>::to<Output>;

using IgnoringFailurePipeline =
    pb::from<Input>::with<pb::policy::errors::ignoring>::then<FailingStage>::to<Output>;

using ThrowingSuccessPipeline =
    pb::from<Input>::with<pb::policy::errors::throwing>::then<Increment>::to<Output>;

using PlainPipeline = pb::from<Input>::then<Increment>::to<Output>;
using PlainFailurePipeline = pb::from<Input>::then<FailingStage>::to<Output>;

// Compile-time confirmation that the marker is detected (and absent for plain).
static_assert(pb::has_error_policy_v<ThrowingFailurePipeline>);
static_assert(pb::has_error_policy_v<IgnoringFailurePipeline>);
static_assert(!pb::has_error_policy_v<PlainPipeline>);
static_assert(!pb::has_error_policy_v<PlainFailurePipeline>);
static_assert(
    std::is_same_v<pb::pipeline_error_policy_t<ThrowingFailurePipeline>, pb::policy::errors::throwing>);
static_assert(
    std::is_same_v<pb::pipeline_error_policy_t<IgnoringFailurePipeline>, pb::policy::errors::ignoring>);
static_assert(std::is_same_v<pb::pipeline_error_policy_t<PlainPipeline>, void>);

} // namespace

int main() {
  // 1. ::with<throwing> -> compile selects the throwing wrapper; a stage
  //    failure surfaces as a thrown pb::pipeline_exception.
  {
    auto engine = pb::compile<ThrowingFailurePipeline>(pb::runtime::sequential{});
    bool threw = false;
    try {
      (void)engine.run(Input{.value = 7});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      require(ex.diagnostic().stage.key == "fail");
    }
    require(threw);

    // try_run remains result-based on the wrapped engine.
    const auto outcome = engine.try_run(Input{.value = 7});
    require(!outcome.has_value());
    require(outcome.error().stage.key == "fail");
  }

  // 2. ::with<throwing> on a succeeding pipeline returns the value normally.
  {
    auto engine = pb::compile<ThrowingSuccessPipeline>(pb::runtime::sequential{});
    const auto value = engine.run(Input{.value = 41});
    require(value.value == 42);
  }

  // 3. ::with<ignoring> -> compile selects the ignoring wrapper; a stage
  //    failure yields a value-initialized fallback instead of an error.
  {
    auto engine = pb::compile<IgnoringFailurePipeline>(pb::runtime::sequential{});
    const auto value = engine.run(Input{.value = 7});
    require(value.value == 0); // value-initialized Output{} fallback

    // try_run still surfaces the real failure for callers who want it.
    const auto outcome = engine.try_run(Input{.value = 7});
    require(!outcome.has_value());
    require(outcome.error().stage.key == "fail");
  }

  // 4. No marker -> baseline result-returning engine, behaviourally identical
  //    to compiling the same stages without any ::with<...>.
  {
    auto plain = pb::compile<PlainPipeline>(pb::runtime::sequential{});
    auto baseline = pb::compile<pb::from<Input>::then<Increment>::to<Output>>(pb::runtime::sequential{});
    static_assert(std::is_same_v<decltype(plain), decltype(baseline)>,
                  "a pipeline without an error-policy marker must compile to the SAME bare engine type");

    // run() returns the raw value (no wrapper), exactly like the baseline.
    const auto a = plain.run(Input{.value = 10});
    const auto b = baseline.run(Input{.value = 10});
    require(a.value == 11);
    require(b.value == 11);

    // A declared failure on the unmarked engine comes back as result<>,
    // never thrown.
    auto failing = pb::compile<PlainFailurePipeline>(pb::runtime::sequential{});
    const auto outcome = failing.run(Input{.value = 1});
    require(!outcome.has_value());
    require(outcome.error().stage.key == "fail");
  }

  return 0;
}
