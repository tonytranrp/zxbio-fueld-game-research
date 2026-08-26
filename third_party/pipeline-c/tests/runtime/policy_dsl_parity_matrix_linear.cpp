// Cross-wrapper run()/try_run() parity matrix for the linear topology.
//
// Spec table (one row per wrapper, two columns per method × three case
// columns).  Each subtest enforces the expected behavior of a single
// (wrapper, method, case) cell on a single-stage linear pipeline:
//
//   wrapper             | run() / Success | run() / Decl fail        | run() / Thrown
//   --------------------|-----------------|--------------------------|-------------------------
//   throwing_engine     | value           | throw pipeline_exception | throw pipeline_exception
//                       |                 |   cat=stage_failure      |   cat=exception
//   terminating_engine  | value           | std::terminate (skipped) | std::terminate (skipped)
//   ignoring_engine     | value           | fallback                 | fallback
//   propagating_engine  | result(value)   | result(stage_failure)    | throw pipeline_exception
//                       |                 |                          |   cat=exception
//   verbose_engine      | underlying.run  | underlying.run           | underlying.run
//                       | (logged)        |   (logged)               |   (logged)
//
//   wrapper             | try_run() / Success | try_run() / Decl fail | try_run() / Thrown
//   --------------------|--------------------|----------------------|----------------------
//   throwing_engine     | result(value)      | result(stage_failure) | result(exception)
//   terminating_engine  | result(value)      | result(stage_failure) | result(exception)
//   ignoring_engine     | result(value)      | result(stage_failure) | result(exception)
//   propagating_engine  | result(value)      | result(stage_failure) | result(exception)
//   verbose_engine      | result(value)      | result(stage_failure) | result(exception)
//
// Run-path terminate cells are intentionally not exercised — calling them
// would terminate the test process.  The compile-pass terminate_on_error
// smoke proves the wrapper is instantiable on supported pipeline shapes.

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace parity_linear {

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
// 1) throwing_engine
// ────────────────────────────────────────────────────────────────────────────

void test_throwing_engine() {
  // run() / Success
  {
    auto engine = pb::with_throw_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 10});
    pb_test_require(out.value == 11);
  }
  // try_run() / Success
  {
    auto engine = pb::with_throw_on_error(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.try_run(Input{.value = 10});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 11);
  }
  // run() / Declared failure -> throws pipeline_exception(stage_failure)
  {
    auto engine = pb::with_throw_on_error(fresh_engine<FailurePipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& d = ex.diagnostic();
      pb_test_require(d.category == pb::runtime::error_category::stage_failure);
      pb_test_require(d.stage.key == "fail");
      pb_test_require(d.message == "intentional stage failure");
    }
    pb_test_require(threw);
  }
  // try_run() / Declared failure -> result with stage_failure
  {
    auto engine = pb::with_throw_on_error(fresh_engine<FailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
  }
  // run() / Thrown -> throws pipeline_exception(exception)
  {
    auto engine = pb::with_throw_on_error(fresh_engine<ExceptionPipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& d = ex.diagnostic();
      pb_test_require(d.category == pb::runtime::error_category::exception);
      pb_test_require(d.stage.key == "boom");
      pb_test_require(contains(d.message, "kaboom"));
    }
    pb_test_require(threw);
  }
  // try_run() / Thrown -> result with category=exception
  {
    auto engine = pb::with_throw_on_error(fresh_engine<ExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom");
    pb_test_require(contains(outcome.error().message, "kaboom"));
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 2) terminating_engine — run() failure cells skipped (would terminate)
// ────────────────────────────────────────────────────────────────────────────

void test_terminating_engine() {
  // run() / Success — returns value (no terminate fires)
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 7});
    pb_test_require(out.value == 8);
  }
  // try_run() / Success
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.try_run(Input{.value = 7});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 8);
  }
  // try_run() / Declared failure
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<FailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
  }
  // try_run() / Thrown
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<ExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom");
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 3) ignoring_engine
// ────────────────────────────────────────────────────────────────────────────

void test_ignoring_engine() {
  // run() / Success
  {
    auto engine = pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{.value = 10});
    pb_test_require(out.value == 11);
  }
  // try_run() / Success
  {
    auto engine = pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{.value = 10});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 11);
  }
  // run() / Declared failure -> fallback
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{});
    pb_test_require(out.value == -1);
  }
  // try_run() / Declared failure -> result with real error
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
  }
  // run() / Thrown -> fallback
  {
    auto engine = pb::with_ignore_errors(fresh_engine<ExceptionPipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{});
    pb_test_require(out.value == -1);
  }
  // try_run() / Thrown -> result with exception error
  {
    auto engine = pb::with_ignore_errors(fresh_engine<ExceptionPipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom");
  }
  // Fallback is mutable; updates take effect on subsequent run()s
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1});
    pb_test_require(engine.run(Input{}).value == -1);
    engine.set_fallback(Output{.value = 42});
    pb_test_require(engine.get_fallback().value == 42);
    pb_test_require(engine.run(Input{}).value == 42);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 4) propagating_engine
// ────────────────────────────────────────────────────────────────────────────

void test_propagating_engine() {
  // run() / Success -> result(value)
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.run(Input{.value = 5});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 6);
  }
  // try_run() / Success
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.try_run(Input{.value = 5});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 6);
  }
  // run() / Declared failure -> result(stage_failure), no throw
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<FailurePipeline>());
    const auto outcome = engine.run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
  }
  // try_run() / Declared failure -> same result, no throw
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<FailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
  }
  // run() / Thrown -> pipeline_exception (the propagation policy)
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<ExceptionPipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& d = ex.diagnostic();
      pb_test_require(d.category == pb::runtime::error_category::exception);
      pb_test_require(d.stage.key == "boom");
      pb_test_require(contains(d.message, "kaboom"));
    }
    pb_test_require(threw);
  }
  // try_run() / Thrown -> result with exception, NOT throw (escape hatch)
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<ExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom");
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 5) verbose_engine — observer-passthrough wrapper
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_engine() {
  // try_run() / Success — value preserved AND log lines present
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<SuccessPipeline>(), &sink);
    const auto outcome = engine.try_run(Input{.value = 1});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 2);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=increment"));
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=increment"));
  }
  // try_run() / Declared failure — error preserved AND log lines present
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<FailurePipeline>(), &sink);
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=fail"));
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail"));
    pb_test_require(contains(log, "category=stage_failure"));
    pb_test_require(contains(log, "message=intentional stage failure"));
  }
  // try_run() / Thrown — captured as exception AND log lines present
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<ExceptionPipeline>(), &sink);
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom");
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=boom"));
    pb_test_require(contains(log, "[pb.verbose] stage_exception stage=boom"));
    pb_test_require(contains(log, "category=exception"));
  }
  // run() / Success — underlying engine run() returns plain Output for
  // this pipeline shape (no stage returns result<>), so verbose preserves
  // the raw value AND logs both transitions.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<SuccessPipeline>(), &sink);
    const auto out = engine.run(Input{.value = 3});
    pb_test_require(out.value == 4);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=increment"));
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=increment"));
  }
  // run() / Declared failure — underlying engine.run() on the
  // FailurePipeline returns result<> because the stage uses result<>.
  // Verbose preserves that and logs the failure event.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<FailurePipeline>(), &sink);
    const auto outcome = engine.run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail"));
  }
  // run() / Thrown — underlying engine.run() on the ExceptionPipeline
  // propagates the exception raw because no stage returns result<>.
  // Verbose preserves that propagation.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<ExceptionPipeline>(), &sink);
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const std::exception& ex) {
      threw = true;
      pb_test_require(contains(ex.what(), "kaboom"));
    }
    pb_test_require(threw);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_start stage=boom"));
    pb_test_require(contains(log, "[pb.verbose] stage_exception stage=boom"));
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 6) Descriptor / observer / error stage-identity consistency
// ────────────────────────────────────────────────────────────────────────────

// The wrappers must surface the same stage_id that engine.describe() reports
// for the failing stage.  This regression catches future refactors that drop
// or rename stage identity at the wrapper boundary.
void test_stage_identity_consistency() {
  // throwing on declared failure: pipeline_exception.diagnostic().stage matches describe()[0]
  {
    auto engine = pb::with_throw_on_error(fresh_engine<FailurePipeline>());
    const auto describe = engine.underlying().describe();
    pb_test_require(describe.size() == 1);
    pb_test_require(describe[0].key == "fail");
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      pb_test_require(ex.diagnostic().stage.key == describe[0].key);
      pb_test_require(ex.diagnostic().stage.name == describe[0].name);
    }
  }
  // ignoring on declared failure: try_run error.stage matches describe()[0]
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = 0});
    const auto describe = engine.underlying().describe();
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == describe[0].key);
    pb_test_require(outcome.error().stage.name == describe[0].name);
  }
  // propagating on thrown exception: pipeline_exception stage matches describe()[0]
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<ExceptionPipeline>());
    const auto describe = engine.underlying().describe();
    pb_test_require(describe.size() == 1);
    pb_test_require(describe[0].key == "boom");
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      pb_test_require(ex.diagnostic().stage.key == describe[0].key);
      pb_test_require(ex.diagnostic().stage.name == describe[0].name);
    }
  }
}

} // namespace parity_linear

int main() {
  parity_linear::test_throwing_engine();
  parity_linear::test_terminating_engine();
  parity_linear::test_ignoring_engine();
  parity_linear::test_propagating_engine();
  parity_linear::test_verbose_engine();
  parity_linear::test_stage_identity_consistency();
  return 0;
}
