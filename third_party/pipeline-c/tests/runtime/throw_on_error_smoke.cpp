// pb::with_throw_on_error production-grade error policy smoke.
//
// Validates that wrapping an engine in pb::with_throw_on_error:
//   * returns the value directly on the success path,
//   * throws pb::pipeline_exception with stage identity on stage failure,
//   * throws pb::pipeline_exception on caught stage exceptions,
//   * preserves try_run() so callers can still opt back into result<>,
//   * lets the caller reach the underlying engine for observer wiring.

#include <pb/pipeline.hpp>

#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>

namespace throw_on_error_smoke {

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

} // namespace throw_on_error_smoke

int main() {
  using namespace throw_on_error_smoke;

  // ── 1. Success path: throwing engine returns the value directly. ──
  {
    auto engine = pb::with_throw_on_error(pb::compile<SuccessPipeline>(pb::runtime::sequential{}));
    const auto output = engine.run(Input{.value = 10});
    assert(output.value == 11);

    // try_run still returns result<> for callers that want both modes.
    const auto retry = engine.try_run(Input{.value = 100});
    assert(retry.has_value());
    assert(retry.value().value == 101);
  }

  // ── 2. Stage failure: throws pb::pipeline_exception with diagnostic. ──
  {
    auto engine = pb::with_throw_on_error(pb::compile<FailurePipeline>(pb::runtime::sequential{}));
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      assert(contains(ex.what(), "fail"));
      assert(contains(ex.what(), "intentional stage failure"));
      const auto& diag = ex.diagnostic();
      assert(diag.stage.key == "fail");
      assert(diag.message == "intentional stage failure");
    }
    assert(threw);
  }

  // ── 3. Stage exception: caught and re-thrown as pipeline_exception. ──
  {
    auto engine = pb::with_throw_on_error(pb::compile<ExceptionPipeline>(pb::runtime::sequential{}));
    bool threw = false;
    try {
      (void)engine.run(Input{});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      const auto& diag = ex.diagnostic();
      assert(diag.stage.key == "boom");
      assert(contains(diag.message, "kaboom"));
      assert(diag.category == pb::runtime::error_category::exception);
    }
    assert(threw);
  }

  // ── 4. underlying() lets callers attach an observer or read state. ──
  {
    auto engine = pb::with_throw_on_error(pb::compile<SuccessPipeline>(pb::runtime::sequential{}));
    pb::runtime::observer* hook = nullptr;
    engine.underlying().set_observer(hook);
    assert(engine.underlying().get_observer() == nullptr);
  }

  return 0;
}
