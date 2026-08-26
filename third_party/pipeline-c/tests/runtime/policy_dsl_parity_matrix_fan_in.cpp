// Cross-wrapper run()/try_run() parity matrix for the explicit fan-in
// topology.
//
// Fan-in is the trickiest case because case-stage failures aggregate
// *into* the join input, NOT into the overall pipeline failure.  The
// wrapper sees a successful pipeline as long as the join stage runs to
// completion — even when every case failed.  The wrapper only sees a
// "failure" when the JOIN STAGE itself fails (declared result<> error)
// or throws.
//
// This file pins both behaviors:
//
//   A. Case failures aggregate into the join input.  The wrapper sees
//      pipeline SUCCESS with a join-output that reflects the per-case
//      diagnostics.  All five wrappers' run() / try_run() must return
//      success in this scenario.
//
//   B. Join-stage failure.  Wrapper behavior is identical to the linear
//      matrix: throwing -> throws pipeline_exception, terminating ->
//      try_run() returns result<>, ignoring -> fallback, propagating ->
//      result(stage_failure) on declared / pipeline_exception on thrown,
//      verbose -> passthrough + log lines.

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace parity_fan_in {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

struct Input  { int value{}; };
struct Output { int value{}; };

struct Always {
  using input_type  = Input;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "always"; }
  static constexpr auto stage_name() noexcept { return "always"; }
  bool operator()(const Input&) const { return true; }
};

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
  static constexpr auto stage_key()  noexcept { return "fail-case"; }
  static constexpr auto stage_name() noexcept { return "fail-case"; }
  pb::runtime::result<Output> operator()(Input) const {
    return pb::runtime::result<Output>{pb::runtime::error{
        .category = pb::runtime::error_category::stage_failure,
        .message  = "case failed",
    }};
  }
};

struct ThrowingStage {
  using input_type  = Input;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "boom-case"; }
  static constexpr auto stage_name() noexcept { return "boom-case"; }
  Output operator()(Input) const { throw std::runtime_error{"case boom"}; }
};

using SuccessCase       = pb::case_<Always>::then<Increment>;
using FailureCase       = pb::case_<Always>::then<FailingStage>;
using ExceptionCase     = pb::case_<Always>::then<ThrowingStage>;

// Aggregate-aware join — returns the case output on success, marker -1 on
// failure.  Used when we want case failures to NOT escape as overall
// failure (the fan-in invariant).
struct AggregatingJoin {
  using input_type  = pb::fan_in_results<pb::fan_in_case_result<0, Output>>;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "aggregate-join"; }
  static constexpr auto stage_name() noexcept { return "aggregate-join"; }
  Output operator()(const input_type& input) const {
    const auto& slot = input.template get<0>();
    if (slot.failed())    return Output{.value = -1};
    if (slot.skipped())   return Output{.value = -2};
    if (slot.completed()) return slot.get();
    return Output{.value = -3};
  }
};

// Join that fails declaratively.  Used to drive a "real" pipeline-level
// declared failure that the wrappers must surface.
struct FailingJoin {
  using input_type  = pb::fan_in_results<pb::fan_in_case_result<0, Output>>;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "fail-join"; }
  static constexpr auto stage_name() noexcept { return "fail-join"; }
  pb::runtime::result<Output> operator()(const input_type&) const {
    return pb::runtime::result<Output>{pb::runtime::error{
        .category = pb::runtime::error_category::stage_failure,
        .message  = "intentional join failure",
    }};
  }
};

// Join that throws.  Used to drive a pipeline-level exception path.
struct ThrowingJoin {
  using input_type  = pb::fan_in_results<pb::fan_in_case_result<0, Output>>;
  using output_type = Output;
  static constexpr auto stage_key()  noexcept { return "boom-join"; }
  static constexpr auto stage_name() noexcept { return "boom-join"; }
  Output operator()(const input_type&) const { throw std::runtime_error{"join boom"}; }
};

// Group A: case succeeds, join succeeds -> pipeline success
using SuccessPipeline = pb::from<Input>::branch<SuccessCase>::fan_in<AggregatingJoin>::to<Output>;

// Group B: case fails / throws, join succeeds -> pipeline still success
using CaseFailurePipeline   = pb::from<Input>::branch<FailureCase>::fan_in<AggregatingJoin>::to<Output>;
using CaseExceptionPipeline = pb::from<Input>::branch<ExceptionCase>::fan_in<AggregatingJoin>::to<Output>;

// Group C: case succeeds, join fails / throws -> pipeline failure
using JoinFailurePipeline   = pb::from<Input>::branch<SuccessCase>::fan_in<FailingJoin>::to<Output>;
using JoinExceptionPipeline = pb::from<Input>::branch<SuccessCase>::fan_in<ThrowingJoin>::to<Output>;

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

template <class Pipeline>
auto fresh_engine() {
  return pb::compile<Pipeline>(pb::runtime::sequential{});
}

// ────────────────────────────────────────────────────────────────────────────
// Fan-in invariant: case failures DO NOT escape as pipeline failure.
//
// All five wrappers must see SUCCESS on case-failure pipelines because
// the join still ran to completion and produced a value.  This is the
// fan-in-specific behavior that any policy DSL change must preserve.
// ────────────────────────────────────────────────────────────────────────────

void test_case_failures_aggregate_to_success() {
  // throwing_engine sees success — does not throw on case failure
  {
    auto engine = pb::with_throw_on_error(fresh_engine<CaseFailurePipeline>());
    const auto out = engine.run(Input{.value = 0});
    pb_test_require(out.value == -1);  // aggregating join marker
  }
  {
    auto engine = pb::with_throw_on_error(fresh_engine<CaseExceptionPipeline>());
    const auto out = engine.run(Input{.value = 0});
    pb_test_require(out.value == -1);
  }
  // try_run() likewise returns engaged result<>
  {
    auto engine = pb::with_throw_on_error(fresh_engine<CaseFailurePipeline>());
    const auto outcome = engine.try_run(Input{.value = 0});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == -1);
  }
  // terminating_engine does not terminate when case fails
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<CaseFailurePipeline>());
    const auto out = engine.run(Input{.value = 0});
    pb_test_require(out.value == -1);
  }
  // ignoring_engine sees success, does NOT substitute fallback
  {
    auto engine = pb::with_ignore_errors(fresh_engine<CaseFailurePipeline>(), Output{.value = 999});
    const auto out = engine.run(Input{.value = 0});
    pb_test_require(out.value == -1);  // join's marker, NOT the fallback
  }
  // propagating_engine: pipeline is successful, returns result(value)
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<CaseFailurePipeline>());
    const auto outcome = engine.run(Input{.value = 0});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == -1);
  }
  // Same for case exception aggregation
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<CaseExceptionPipeline>());
    const auto outcome = engine.run(Input{.value = 0});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == -1);
  }
  // verbose_engine: aggregating success, but case_failed observer event fires
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<CaseFailurePipeline>(), &sink);
    const auto outcome = engine.try_run(Input{.value = 0});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == -1);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] case_failed"));
    pb_test_require(contains(log, "stage=fail-case"));
    pb_test_require(contains(log, "category=stage_failure"));
    // Join still ran successfully.
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=aggregate-join"));
  }
  // verbose_engine: case exceptions also aggregate into join input, preserving
  // policy-level success while recording the case_failed exception diagnostic.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<CaseExceptionPipeline>(), &sink);
    const auto outcome = engine.try_run(Input{.value = 0});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == -1);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] case_failed"));
    pb_test_require(contains(log, "stage=boom-case"));
    pb_test_require(contains(log, "category=exception"));
    pb_test_require(contains(log, "message=case boom"));
    // Join still ran successfully after the case exception was captured.
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=aggregate-join"));
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 1) throwing_engine on fan-in with JOIN failure / exception
// ────────────────────────────────────────────────────────────────────────────

void test_throwing_engine_join_path() {
  // Success: pipeline runs end-to-end
  {
    auto engine = pb::with_throw_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 10});
    pb_test_require(out.value == 11);
  }
  // Declared join failure -> pipeline_exception(stage_failure)
  {
    auto engine = pb::with_throw_on_error(fresh_engine<JoinFailurePipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& d = ex.diagnostic();
      pb_test_require(d.category == pb::runtime::error_category::stage_failure);
      pb_test_require(d.stage.key == "fail-join");
      pb_test_require(contains(d.message, "intentional join failure"));
    }
    pb_test_require(threw);
  }
  // try_run() declared join failure
  {
    auto engine = pb::with_throw_on_error(fresh_engine<JoinFailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail-join");
  }
  // Thrown join exception -> pipeline_exception(exception)
  {
    auto engine = pb::with_throw_on_error(fresh_engine<JoinExceptionPipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& d = ex.diagnostic();
      pb_test_require(d.category == pb::runtime::error_category::exception);
      pb_test_require(d.stage.key == "boom-join");
      pb_test_require(contains(d.message, "join boom"));
    }
    pb_test_require(threw);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 2) terminating_engine — try_run() only (run() would terminate)
// ────────────────────────────────────────────────────────────────────────────

void test_terminating_engine_join_path() {
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 7});
    pb_test_require(out.value == 8);
  }
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<JoinFailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == "fail-join");
  }
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<JoinExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom-join");
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 3) ignoring_engine on fan-in
// ────────────────────────────────────────────────────────────────────────────

void test_ignoring_engine_join_path() {
  {
    auto engine = pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{.value = 10});
    pb_test_require(out.value == 11);
  }
  // Join failure -> fallback
  {
    auto engine = pb::with_ignore_errors(fresh_engine<JoinFailurePipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{});
    pb_test_require(out.value == -1);
  }
  // Join failure -> try_run() reveals real error
  {
    auto engine = pb::with_ignore_errors(fresh_engine<JoinFailurePipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == "fail-join");
  }
  // Join exception -> fallback
  {
    auto engine = pb::with_ignore_errors(fresh_engine<JoinExceptionPipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{});
    pb_test_require(out.value == -1);
  }
  // Join exception -> try_run() reveals exception
  {
    auto engine = pb::with_ignore_errors(fresh_engine<JoinExceptionPipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom-join");
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 4) propagating_engine on fan-in
// ────────────────────────────────────────────────────────────────────────────

void test_propagating_engine_join_path() {
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.run(Input{.value = 5});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 6);
  }
  // Declared join failure -> result, no throw
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<JoinFailurePipeline>());
    const auto outcome = engine.run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail-join");
  }
  // Thrown join -> pipeline_exception
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<JoinExceptionPipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      pb_test_require(ex.diagnostic().category == pb::runtime::error_category::exception);
      pb_test_require(ex.diagnostic().stage.key == "boom-join");
    }
    pb_test_require(threw);
  }
  // try_run() join exception -> result, no throw (escape hatch)
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<JoinExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 5) verbose_engine on fan-in — case_failed + stage_failure events fire
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_engine_join_path() {
  // Success path
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<SuccessPipeline>(), &sink);
    const auto outcome = engine.try_run(Input{.value = 1});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 2);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] case_selected"));
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=increment"));
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=aggregate-join"));
  }
  // Join failure path
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<JoinFailurePipeline>(), &sink);
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == "fail-join");
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail-join"));
    pb_test_require(contains(log, "category=stage_failure"));
  }
  // Join exception path
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<JoinExceptionPipeline>(), &sink);
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_exception stage=boom-join"));
    pb_test_require(contains(log, "category=exception"));
  }
}

} // namespace parity_fan_in

int main() {
  parity_fan_in::test_case_failures_aggregate_to_success();
  parity_fan_in::test_throwing_engine_join_path();
  parity_fan_in::test_terminating_engine_join_path();
  parity_fan_in::test_ignoring_engine_join_path();
  parity_fan_in::test_propagating_engine_join_path();
  parity_fan_in::test_verbose_engine_join_path();
  return 0;
}
