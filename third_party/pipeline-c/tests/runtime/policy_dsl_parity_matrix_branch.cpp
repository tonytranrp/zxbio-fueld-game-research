// Cross-wrapper run()/try_run() parity matrix for the selected-output
// branch topology.
//
// Same matrix as the linear lane, but the failing/throwing stage lives
// inside a pb::case_<Predicate>::then<Stage>.  The runtime annotates the
// branch-level error.message with a "[<branch_name>] " prefix; the
// stage_id on the error remains the case stage's identity.  These tests
// pin both behaviors at every wrapper boundary.

#include <pb/pipeline.hpp>

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace parity_branch {

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

using SuccessCase   = pb::case_<Always>::then<Increment>;
using FailureCase   = pb::case_<Always>::then<FailingStage>;
using ExceptionCase = pb::case_<Always>::then<ThrowingStage>;

using SuccessPipeline   = pb::from<Input>::branch<SuccessCase>::to<Output>;
using FailurePipeline   = pb::from<Input>::branch<FailureCase>::to<Output>;
using ExceptionPipeline = pb::from<Input>::branch<ExceptionCase>::to<Output>;

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

template <class Pipeline>
auto fresh_engine() {
  return pb::compile<Pipeline>(pb::runtime::sequential{});
}

// ────────────────────────────────────────────────────────────────────────────
// 1) throwing_engine on branch
// ────────────────────────────────────────────────────────────────────────────

void test_throwing_engine() {
  // Success
  {
    auto engine = pb::with_throw_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 10});
    pb_test_require(out.value == 11);
  }
  // try_run() Success
  {
    auto engine = pb::with_throw_on_error(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.try_run(Input{.value = 10});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 11);
  }
  // Declared case-stage failure -> throws, with "[branch] " annotated message
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
      pb_test_require(contains(d.message, "[branch]"));
      pb_test_require(contains(d.message, "intentional stage failure"));
    }
    pb_test_require(threw);
  }
  // try_run() declared failure -> result with annotated message
  {
    auto engine = pb::with_throw_on_error(fresh_engine<FailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
    pb_test_require(contains(outcome.error().message, "[branch]"));
  }
  // Case-stage thrown -> pipeline_exception(exception)
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
  // try_run() thrown -> result with exception
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
// 2) terminating_engine — run() failures unobservable, try_run() exercised
// ────────────────────────────────────────────────────────────────────────────

void test_terminating_engine() {
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 7});
    pb_test_require(out.value == 8);
  }
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.try_run(Input{.value = 7});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 8);
  }
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<FailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
    pb_test_require(contains(outcome.error().message, "[branch]"));
  }
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<ExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    pb_test_require(outcome.error().stage.key == "boom");
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 3) ignoring_engine on branch
// ────────────────────────────────────────────────────────────────────────────

void test_ignoring_engine() {
  {
    auto engine = pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{.value = 10});
    pb_test_require(out.value == 11);
  }
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{});
    pb_test_require(out.value == -1);
  }
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == "fail");
    pb_test_require(contains(outcome.error().message, "[branch]"));
  }
  {
    auto engine = pb::with_ignore_errors(fresh_engine<ExceptionPipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{});
    pb_test_require(out.value == -1);
  }
  {
    auto engine = pb::with_ignore_errors(fresh_engine<ExceptionPipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 4) propagating_engine on branch
// ────────────────────────────────────────────────────────────────────────────

void test_propagating_engine() {
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.run(Input{.value = 5});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 6);
  }
  // Declared failure -> result, no throw
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<FailurePipeline>());
    const auto outcome = engine.run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::stage_failure);
    pb_test_require(outcome.error().stage.key == "fail");
  }
  // Thrown -> pipeline_exception
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<ExceptionPipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      pb_test_require(ex.diagnostic().category == pb::runtime::error_category::exception);
      pb_test_require(ex.diagnostic().stage.key == "boom");
    }
    pb_test_require(threw);
  }
  // try_run() / Thrown -> result, no throw (escape hatch)
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<ExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 5) verbose_engine on branch — case events fire alongside stage events
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_engine() {
  // Success path: case_selected event and case stage events should appear
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<SuccessPipeline>(), &sink);
    const auto outcome = engine.try_run(Input{.value = 1});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 2);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] case_selected"));
    pb_test_require(contains(log, "predicate=always"));
    pb_test_require(contains(log, "[pb.verbose] stage_success stage=increment"));
  }
  // Failure path: case_selected then stage_failure on the case stage
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<FailurePipeline>(), &sink);
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().stage.key == "fail");
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] case_selected"));
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=fail"));
    pb_test_require(contains(log, "category=stage_failure"));
  }
  // Exception path: case stage exception event
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(fresh_engine<ExceptionPipeline>(), &sink);
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_exception stage=boom"));
    pb_test_require(contains(log, "category=exception"));
  }
}

} // namespace parity_branch

int main() {
  parity_branch::test_throwing_engine();
  parity_branch::test_terminating_engine();
  parity_branch::test_ignoring_engine();
  parity_branch::test_propagating_engine();
  parity_branch::test_verbose_engine();
  return 0;
}
