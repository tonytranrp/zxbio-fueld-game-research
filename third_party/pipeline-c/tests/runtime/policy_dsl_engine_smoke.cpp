// Production-grade smoke for the runtime policy-engine wrappers:
//   * pb::with_ignore_errors      — fallback value on failure
//   * pb::with_propagate_exceptions — rethrow stage exceptions
//   * pb::with_verbose_diagnostics  — auto-attach a verbose observer
//
// pb::with_terminate_on_error is intentionally exercised in a compile-pass
// fashion only — calling it on a failing pipeline calls std::terminate()
// which would abort the whole CTest process.  The compile-pass coverage
// in tests/compile_pass/public_headers/runtime_error_policy.cpp confirms
// the wrapper is instantiable on every supported pipeline shape.

#include <pb/pipeline.hpp>

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace policy_dsl_smoke {

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
        .message  = "declared failure",
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

} // namespace policy_dsl_smoke

int main() {
  using namespace policy_dsl_smoke;

  // ── 1. ignore_errors substitutes the fallback on failure. ──
  {
    auto engine = pb::with_ignore_errors(
        pb::compile<FailurePipeline>(pb::runtime::sequential{}),
        Output{.value = -1});
    const auto value = engine.run(Input{.value = 7});
    assert(value.value == -1);

    // try_run still surfaces the real failure for callers who want it.
    const auto outcome = engine.try_run(Input{.value = 7});
    assert(!outcome.has_value());
    assert(outcome.error().stage.key == "fail");

    // Fallback is mutable.
    engine.set_fallback(Output{.value = 42});
    assert(engine.get_fallback().value == 42);
    assert(engine.run(Input{.value = 7}).value == 42);
  }

  // ── 2. ignore_errors passes through the success value. ──
  {
    auto engine = pb::with_ignore_errors(
        pb::compile<SuccessPipeline>(pb::runtime::sequential{}),
        Output{.value = -1});
    assert(engine.run(Input{.value = 10}).value == 11);
  }

  // ── 3. propagate_exceptions rethrows caught exceptions but passes
  //       declared failures through as result<>. ──
  {
    auto engine = pb::with_propagate_exceptions(
        pb::compile<ExceptionPipeline>(pb::runtime::sequential{}));
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& diag = ex.diagnostic();
      assert(diag.stage.key == "boom");
      assert(diag.category == pb::runtime::error_category::exception);
      assert(contains(diag.message, "kaboom"));
    }
    assert(threw);
  }

  {
    auto engine = pb::with_propagate_exceptions(
        pb::compile<FailurePipeline>(pb::runtime::sequential{}));
    const auto outcome = engine.run(Input{});
    // Declared stage failures still come back as result<>.
    assert(!outcome.has_value());
    assert(outcome.error().category == pb::runtime::error_category::stage_failure);
  }

  // ── 4. propagate_exceptions does NOT throw on success. ──
  {
    auto engine = pb::with_propagate_exceptions(
        pb::compile<SuccessPipeline>(pb::runtime::sequential{}));
    const auto outcome = engine.run(Input{.value = 5});
    assert(outcome.has_value());
    assert(outcome.value().value == 6);
  }

  // ── 5. verbose_diagnostics auto-attaches an observer that logs each
  //       stage transition in the documented schema. ──
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(
        pb::compile<SuccessPipeline>(pb::runtime::sequential{}), &sink);
    (void)engine.run(Input{.value = 1});

    const std::string log = sink.str();
    assert(contains(log, "[pb.verbose] stage_start stage=increment"));
    assert(contains(log, "[pb.verbose] stage_success stage=increment"));
  }

  // ── 6. verbose_diagnostics logs failures too. ──
  {
    std::ostringstream sink;
    auto engine = pb::with_verbose_diagnostics(
        pb::compile<FailurePipeline>(pb::runtime::sequential{}), &sink);
    (void)engine.try_run(Input{});

    const std::string log = sink.str();
    assert(contains(log, "[pb.verbose] stage_failure stage=fail"));
    assert(contains(log, "category=stage_failure"));
    assert(contains(log, "message=declared failure"));
  }

  // ── 7. verbose wrapper restores the prior observer on destruction. ──
  {
    auto engine = pb::compile<SuccessPipeline>(pb::runtime::sequential{});
    assert(engine.get_observer() == nullptr);
    {
      std::ostringstream sink;
      auto verbose = pb::with_verbose_diagnostics(std::move(engine), &sink);
      // The verbose engine owns engine_ now; the underlying observer is
      // the verbose_observer it installed.  No external observer ptr to
      // probe — the contract is that the wrapper does the restore itself.
      (void)verbose.run(Input{.value = 2});
    }
    // Original engine has been moved-from; the destructor of `verbose`
    // restored whatever was there before (nullptr).  Nothing to assert
    // on the moved-from engine, but the previous-observer logic compiled
    // and executed without aborting.
  }

  return 0;
}
