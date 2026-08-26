// Bug-hunt smoke for the production-grade error-policy DSL.
//
// Goal: prove the wrappers compose.  Common stacks the user might want:
//
//   verbose(throwing(engine))     — log all transitions, throw on failure
//   verbose(ignoring(engine, fb)) — log all transitions, fallback on failure
//   verbose(propagating(engine))  — log all transitions, rethrow exceptions
//   verbose(terminating(engine))  — log all transitions, terminate on failure
//
// Each stack must compile and execute correctly.  This file tries each
// composition and pins the expected behavior.  A regression that breaks
// composability (e.g. a wrapper drops set_observer/get_observer/describe)
// will fail this test, not just a deep template instantiation error.

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace composability {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

struct Input  { int value{}; };
struct Output { int value{}; };

struct Increment {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "increment"; }
  static constexpr auto stage_name() noexcept { return "increment"; }
  Output operator()(Input input) const { return {input.value + 1}; }
};

struct FailingStage {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "fail"; }
  static constexpr auto stage_name() noexcept { return "fail"; }
  pb::runtime::result<Output> operator()(Input) const {
    return pb::runtime::result<Output>{pb::runtime::error{
        .category = pb::runtime::error_category::stage_failure,
        .message  = "intentional stage failure",
    }};
  }
};

struct ThrowingStage {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "boom"; }
  static constexpr auto stage_name() noexcept { return "boom"; }
  Output operator()(Input) const { throw std::runtime_error{"kaboom"}; }
};

using SuccessPipeline   = pb::from<Input>::then<Increment>::to<Output>;
using FailurePipeline   = pb::from<Input>::then<FailingStage>::to<Output>;
using ExceptionPipeline = pb::from<Input>::then<ThrowingStage>::to<Output>;

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

template <class Pipeline>
auto fresh_engine() {
  return pb::compile<Pipeline>(pb::runtime::sequential{});
}

// ────────────────────────────────────────────────────────────────────────────
// 1) Direct observer/descriptor forwarding on each wrapper
// ────────────────────────────────────────────────────────────────────────────

void test_observer_forwarding() {
  // throwing_engine exposes set_observer / get_observer directly
  {
    auto engine = pb::with_throw_on_error(fresh_engine<SuccessPipeline>());
    pb_test_require(engine.get_observer() == nullptr);
    pb_test_require(engine.describe().size() == 1);
    pb_test_require(engine.describe()[0].key == "increment");
    pb::runtime::observer dummy{};
    engine.set_observer(&dummy);
    pb_test_require(engine.get_observer() == &dummy);
    engine.set_observer(nullptr);
  }
  // terminating_engine
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<SuccessPipeline>());
    pb_test_require(engine.get_observer() == nullptr);
    pb_test_require(engine.describe()[0].key == "increment");
  }
  // ignoring_engine
  {
    auto engine = pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = -1});
    pb_test_require(engine.get_observer() == nullptr);
    pb_test_require(engine.describe()[0].key == "increment");
  }
  // propagating_engine
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<SuccessPipeline>());
    pb_test_require(engine.get_observer() == nullptr);
    pb_test_require(engine.describe()[0].key == "increment");
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 2) verbose(throwing(engine)): log + throw on failure
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_over_throwing() {
  // Success: returns value, logs both transitions
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_throw_on_error(fresh_engine<SuccessPipeline>()), &sink);
    const auto out = stack.run(Input{.value = 1});
    pb_test_require(out.value == 2);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=increment"));
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=increment"));
  }
  // Failure: throws, log captures the failure event before the throw
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_throw_on_error(fresh_engine<FailurePipeline>()), &sink);
    bool threw = false;
    try {
      (void)stack.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      pb_test_require(ex.diagnostic().stage.key == "fail");
    }
    pb_test_require(threw);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail"));
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 3) verbose(ignoring(engine, fallback)): log + fallback on failure
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_over_ignoring() {
  // Failure: returns fallback, log captures the underlying failure
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1}), &sink);
    const auto out = stack.run(Input{});
    pb_test_require(out.value == -1);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail"));
    pb_test_require(contains(log, "category=stage_failure"));
  }
  // Success: returns value
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = -1}), &sink);
    const auto out = stack.run(Input{.value = 5});
    pb_test_require(out.value == 6);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 4) verbose(propagating(engine)): log + rethrow exceptions
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_over_propagating() {
  // Thrown exception: rethrows as pipeline_exception, log captures
  // the underlying stage_exception event
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_propagate_exceptions(fresh_engine<ExceptionPipeline>()), &sink);
    bool threw = false;
    try {
      (void)stack.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      pb_test_require(ex.diagnostic().category == pb::runtime::error_category::exception);
      pb_test_require(ex.diagnostic().stage.key == "boom");
    }
    pb_test_require(threw);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_exception stage=boom"));
  }
  // Declared failure: propagating returns result, verbose logs it
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_propagate_exceptions(fresh_engine<FailurePipeline>()), &sink);
    const auto outcome = stack.run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == "fail");
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail"));
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 5) verbose(terminating(engine)): success-only — failure would terminate
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_over_terminating() {
  // Success: log + value
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_terminate_on_error(fresh_engine<SuccessPipeline>()), &sink);
    const auto out = stack.run(Input{.value = 3});
    pb_test_require(out.value == 4);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=increment"));
  }
  // try_run on failure path: terminating's try_run returns result<>, verbose
  // observer captures the failure event
  {
    std::ostringstream sink;
    auto stack = pb::with_verbose_diagnostics(
        pb::with_terminate_on_error(fresh_engine<FailurePipeline>()), &sink);
    const auto outcome = stack.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == "fail");
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail"));
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 6) describe/descriptor forwarding through wrapper stacks
// ────────────────────────────────────────────────────────────────────────────

void test_describe_through_stack() {
  // describe() reaches the base engine through every wrapper layer
  {
    auto stack = pb::with_verbose_diagnostics(
        pb::with_throw_on_error(fresh_engine<SuccessPipeline>()), nullptr);
    const auto desc = stack.describe();
    pb_test_require(desc.size() == 1);
    pb_test_require(desc[0].key == "increment");
  }
  // descriptor() also reaches through
  {
    auto stack = pb::with_verbose_diagnostics(
        pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = 0}), nullptr);
    const auto rt_desc = stack.descriptor();
    pb_test_require(rt_desc.stage_count == 1);
  }
}

} // namespace composability

int main() {
  composability::test_observer_forwarding();
  composability::test_verbose_over_throwing();
  composability::test_verbose_over_ignoring();
  composability::test_verbose_over_propagating();
  composability::test_verbose_over_terminating();
  composability::test_describe_through_stack();
  return 0;
}
