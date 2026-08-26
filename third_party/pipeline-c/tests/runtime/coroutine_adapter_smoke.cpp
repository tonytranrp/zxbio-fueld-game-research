#include <pb/pipeline.hpp>

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

struct Input {
  int value{};
};

struct Parsed {
  int value{};
};

struct Doubled {
  int value{};
};

struct Output {
  int value{};
};

struct MoveOnlyOutput {
  std::unique_ptr<int> value;
};

// A coroutine stage that transforms Input -> Parsed via co_return.
struct ParseCoroutine {
  pb::coro::sync_task<Parsed> operator()(Input in) const {
    co_return Parsed{in.value + 1};
  }
};

// A second coroutine stage, chained after the first, that also co_awaits an
// inner eager sync_task to demonstrate synchronous awaiting composes.
struct EmitCoroutine {
  pb::coro::sync_task<Doubled> operator()(Parsed parsed) const {
    co_return Doubled{parsed.value * 2};
  }
};

struct ThrowCoroutine {
  pb::coro::sync_task<Parsed> operator()(Input) const {
    throw std::runtime_error{"coroutine boom"};
    co_return Parsed{};
  }
};

struct MoveOnlyCoroutine {
  pb::coro::sync_task<MoveOnlyOutput> operator()(Input in) const {
    co_return MoveOnlyOutput{std::make_unique<int>(in.value + 20)};
  }
};

// A normal (non-coroutine) functor stage interleaved between coroutine stages.
struct Offset {
  using input_type = Doubled;
  using output_type = Output;
  static constexpr auto stage_name() noexcept { return "offset"; }
  Output operator()(Doubled d) const { return Output{d.value + 5}; }
};

using ParseStage = pb::coroutine_stage<ParseCoroutine, Input>;
using EmitStage = pb::adapt_coroutine<EmitCoroutine, Parsed>;
using ThrowStage = pb::coroutine_stage<ThrowCoroutine, Input>;
using MoveOnlyStage = pb::coroutine_stage<MoveOnlyCoroutine, Input>;

// Verify the adapter satisfies the Stage concept and exposes the deduced traits.
static_assert(pb::core::Stage<ParseStage>);
static_assert(pb::core::Stage<EmitStage>);
static_assert(std::is_same_v<ParseStage::input_type, Input>);
static_assert(std::is_same_v<ParseStage::output_type, Parsed>);
static_assert(std::is_same_v<EmitStage::input_type, Parsed>);
static_assert(std::is_same_v<EmitStage::output_type, Doubled>);
static_assert(std::is_same_v<MoveOnlyStage::output_type, MoveOnlyOutput>);

// Pipeline mixing two coroutine stages with a normal stage.
using Pipeline = pb::from<Input>::then<ParseStage>::then<EmitStage>::then<Offset>::to<Output>;
static_assert(pb::valid<Pipeline>);
} // namespace

int main() {
  // Direct invocation of the adapter pulls the result out synchronously.
  ParseStage parse_stage{};
  auto parsed = parse_stage(Input{4});
  pb_test_require(parsed.value == 5);

  // sync_task on its own runs eagerly and yields its value via .get().
  ParseCoroutine raw_coroutine{};
  auto task = raw_coroutine(Input{10});
  pb_test_require(task.done());
  pb_test_require(task.get().value == 11);
  try {
    (void)task.get();
    std::abort();
  } catch (const std::logic_error& error) {
    pb_test_require(std::string_view{error.what()} == "pb::coro::sync_task: result already consumed");
  }

  // sync_task propagates coroutine-body exceptions through direct get().
  auto throwing_task = ThrowCoroutine{}(Input{1});
  pb_test_require(throwing_task.done());
  try {
    (void)throwing_task.get();
    std::abort();
  } catch (const std::runtime_error& error) {
    pb_test_require(std::string_view{error.what()} == "coroutine boom");
  }

  // Move-only coroutine outputs are extracted by value through the adapter.
  {
    auto move_only = MoveOnlyStage{}(Input{2});
    pb_test_require(move_only.value != nullptr);
    pb_test_require(*move_only.value == 22);
  }

  // Two coroutine stages chained together plus a normal stage, on the
  // sequential engine.
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  // run() on an all-value pipeline returns the raw Output directly.
  auto raw = engine.run(Input{4});
  // (4 + 1) -> 5, doubled -> 10, +5 -> 15
  pb_test_require(raw.value == 15);

  // try_run() always wraps the output in a result for uniform error handling.
  auto result = engine.try_run(Input{4});
  pb_test_require(result.has_value());
  pb_test_require(result.value().value == 15);

  auto result2 = engine.try_run(Input{0});
  pb_test_require(result2.has_value());
  // (0 + 1) -> 1, doubled -> 2, +5 -> 7
  pb_test_require(result2.value().value == 7);

  // coroutine_stage exceptions flow through try_run as normal runtime exception diagnostics.
  using ThrowPipeline = pb::from<Input>::then<ThrowStage>::to<Parsed>;
  auto throwing_engine = pb::compile<ThrowPipeline>(pb::runtime::sequential{});
  auto failed = throwing_engine.try_run(Input{3});
  pb_test_require(!failed.has_value());
  pb_test_require(failed.error().category == pb::runtime::error_category::exception);
  pb_test_require(failed.error().stage.name == "coroutine_stage");
  pb_test_require(failed.error().message == "coroutine boom");

  return 0;
}
