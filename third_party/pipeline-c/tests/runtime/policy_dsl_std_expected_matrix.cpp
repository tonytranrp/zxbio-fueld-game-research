// std::expected matrix coverage for the policy-DSL wrappers.
//
// The pb::runtime::expected_like concept normalizes anything with the
// std::expected shape (value_type, error_type, has_value(), value(),
// error()) into pb::runtime::result<>.  std::expected itself is the
// canonical instance.  This test pins that:
//
//   * stages returning std::expected<Output, MyError> are valid stages
//   * each wrapper (throwing/terminating/ignoring/propagating/verbose)
//     observes the normalized error_category::expected_error and the
//     original error's message
//
// Gated on PB_HAS_STD_EXPECTED so the test compiles to a no-op main on
// toolchains without <expected>.  PB_HAS_STD_EXPECTED follows
// __cpp_lib_expected >= 202202L per include/pb/core/cpp26_features.hpp.

#include <pb/pipeline.hpp>

#if PB_HAS_STD_EXPECTED

#include <cstdlib>
#include <expected>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace policy_std_expected {

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

struct Input  { int value{}; };
struct Output { int value{}; };

struct ExpectedSuccess {
  using input_type  = Input;
  using output_type = Output;
  using error_type  = std::string;
  static constexpr auto stage_key()  noexcept { return "expected-add"; }
  static constexpr auto stage_name() noexcept { return "expected-add"; }
  std::expected<Output, std::string> operator()(Input input) const {
    return Output{input.value + 1};
  }
};

struct ExpectedFailure {
  using input_type  = Input;
  using output_type = Output;
  using error_type  = std::string;
  static constexpr auto stage_key()  noexcept { return "expected-fail"; }
  static constexpr auto stage_name() noexcept { return "expected-fail"; }
  std::expected<Output, std::string> operator()(Input) const {
    return std::unexpected<std::string>{"expected-shape failure"};
  }
};

struct ExpectedThrowing {
  using input_type  = Input;
  using output_type = Output;
  using error_type  = std::string;
  static constexpr auto stage_key()  noexcept { return "expected-boom"; }
  static constexpr auto stage_name() noexcept { return "expected-boom"; }
  std::expected<Output, std::string> operator()(Input) const {
    throw std::runtime_error{"expected-shape exception"};
  }
};

using SuccessPipeline   = pb::from<Input>::then<ExpectedSuccess>::to<Output>;
using FailurePipeline   = pb::from<Input>::then<ExpectedFailure>::to<Output>;
using ExceptionPipeline = pb::from<Input>::then<ExpectedThrowing>::to<Output>;

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

template <class Pipeline>
auto fresh_engine() {
  return pb::compile<Pipeline>(pb::runtime::sequential{});
}

// ────────────────────────────────────────────────────────────────────────────
// 1) throwing_engine over std::expected pipelines
// ────────────────────────────────────────────────────────────────────────────

void test_throwing() {
  // Success
  {
    auto engine = pb::with_throw_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 9});
    pb_test_require(out.value == 10);
  }
  // std::expected error -> pipeline_exception(expected_error)
  {
    auto engine = pb::with_throw_on_error(fresh_engine<FailurePipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& d = ex.diagnostic();
      pb_test_require(d.category == pb::runtime::error_category::expected_error);
      pb_test_require(d.stage.key == "expected-fail");
      pb_test_require(contains(d.message, "expected-shape failure"));
    }
    pb_test_require(threw);
  }
  // Thrown -> pipeline_exception(exception)
  {
    auto engine = pb::with_throw_on_error(fresh_engine<ExceptionPipeline>());
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      pb_test_require(ex.diagnostic().category == pb::runtime::error_category::exception);
      pb_test_require(ex.diagnostic().stage.key == "expected-boom");
    }
    pb_test_require(threw);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 2) terminating_engine — try_run() only (run() failure would terminate)
// ────────────────────────────────────────────────────────────────────────────

void test_terminating() {
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<SuccessPipeline>());
    const auto out = engine.run(Input{.value = 3});
    pb_test_require(out.value == 4);
  }
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<FailurePipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::expected_error);
    pb_test_require(outcome.error().stage.key == "expected-fail");
  }
  {
    auto engine = pb::with_terminate_on_error(fresh_engine<ExceptionPipeline>());
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::exception);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 3) ignoring_engine over std::expected
// ────────────────────────────────────────────────────────────────────────────

void test_ignoring() {
  {
    auto engine = pb::with_ignore_errors(fresh_engine<SuccessPipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{.value = 10});
    pb_test_require(out.value == 11);
  }
  // std::expected failure -> fallback
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1});
    const auto out = engine.run(Input{});
    pb_test_require(out.value == -1);
  }
  // try_run reveals the real error
  {
    auto engine = pb::with_ignore_errors(fresh_engine<FailurePipeline>(), Output{.value = -1});
    const auto outcome = engine.try_run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::expected_error);
    pb_test_require(contains(outcome.error().message, "expected-shape failure"));
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 4) propagating_engine over std::expected
// ────────────────────────────────────────────────────────────────────────────

void test_propagating() {
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<SuccessPipeline>());
    const auto outcome = engine.run(Input{.value = 5});
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 6);
  }
  // Declared std::expected failure -> result(expected_error), NO throw
  // (propagating only converts exceptions, not declared failures)
  {
    auto engine = pb::with_propagate_exceptions(fresh_engine<FailurePipeline>());
    const auto outcome = engine.run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::expected_error);
    pb_test_require(outcome.error().stage.key == "expected-fail");
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
    }
    pb_test_require(threw);
  }
}

// ────────────────────────────────────────────────────────────────────────────
// 5) verbose_engine over std::expected — logs the expected_error category
// ────────────────────────────────────────────────────────────────────────────

void test_verbose() {
  std::ostringstream sink;
  auto engine = pb::with_verbose_diagnostics(fresh_engine<FailurePipeline>(), &sink);
  const auto outcome = engine.try_run(Input{});
  pb_test_require(!outcome.has_value());
  pb_test_require(outcome.error().category == pb::runtime::error_category::expected_error);
  const auto log = sink.str();
  pb_test_require(contains(log, "[pb.verbose] stage_failure stage=expected-fail"));
  pb_test_require(contains(log, "category=expected_error"));
  pb_test_require(contains(log, "message=expected-shape failure"));
}

// ────────────────────────────────────────────────────────────────────────────
// 6) verbose(propagating_engine) over std::expected — declared expected
//    failures remain result<> diagnostics, thrown failures rethrow.
// ────────────────────────────────────────────────────────────────────────────

void test_verbose_over_propagating() {
  // Declared std::expected failure: propagating returns result<>, verbose logs
  // the normalized expected_error boundary without converting it to a throw.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(
        pb::with_propagate_exceptions(fresh_engine<FailurePipeline>()), &sink);
    const auto outcome = engine.run(Input{});
    pb_test_require(!outcome.has_value());
    pb_test_require(outcome.error().category == pb::runtime::error_category::expected_error);
    pb_test_require(outcome.error().stage.key == "expected-fail");
    pb_test_require(contains(outcome.error().message, "expected-shape failure"));
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_failure stage=expected-fail"));
    pb_test_require(contains(log, "category=expected_error"));
    pb_test_require(contains(log, "message=expected-shape failure"));
  }
  // Thrown failure: propagating rethrows as pipeline_exception, while verbose
  // still records the underlying stage_exception event before the boundary.
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(
        pb::with_propagate_exceptions(fresh_engine<ExceptionPipeline>()), &sink);
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      pb_test_require(ex.diagnostic().category == pb::runtime::error_category::exception);
      pb_test_require(ex.diagnostic().stage.key == "expected-boom");
    }
    pb_test_require(threw);
    const auto log = sink.str();
    pb_test_require(contains(log, "[pb.verbose] stage_exception stage=expected-boom"));
    pb_test_require(contains(log, "category=exception"));
  }
}

} // namespace policy_std_expected

int main() {
  policy_std_expected::test_throwing();
  policy_std_expected::test_terminating();
  policy_std_expected::test_ignoring();
  policy_std_expected::test_propagating();
  policy_std_expected::test_verbose();
  policy_std_expected::test_verbose_over_propagating();
  return 0;
}

#else  // PB_HAS_STD_EXPECTED == 0

// On toolchains without <expected> the test is a no-op success.  The C++26
// feature gate (cpp26_features_fallback test) confirms the gate itself
// behaves correctly under those conditions.
int main() { return 0; }

#endif // PB_HAS_STD_EXPECTED
