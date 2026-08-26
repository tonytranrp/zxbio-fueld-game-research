// ─────────────────────────────────────────────────────────────
// C++20 module-consumer smoke test.
//
// This translation unit deliberately does NOT include any
// <pb/...> header.  It imports the named module `pb.pipeline`
// (declared in include/pb/pipeline.mpp) and exercises the core
// DSL surface — from<>::then<>::to<>, pb::compile, engine.run /
// engine.try_run — proving the module export surface is a faithful
// mirror of the traditional header surface.
//
// Built only when PB_BUILD_MODULE=ON (see root CMakeLists.txt and
// the `modules-ninja` preset).  When the option is OFF this file is
// not compiled at all, so the default header build is unaffected.
// ─────────────────────────────────────────────────────────────

import pb.pipeline;

#include <cassert>
#include <chrono>
#include <memory>
#include <string_view>
#include <type_traits>

namespace {

struct Input {
  int value{};
};
struct Middle {
  int value{};
};
struct Output {
  int value{};
};

struct AddOne {
  using input_type = Input;
  using output_type = Middle;
  static constexpr auto key = pb::fixed_string{"module.add_one"};
  static constexpr auto name = pb::fixed_string{"add_one"};
  Middle operator()(Input input) const { return {input.value + 1}; }
};

struct DoubleValue {
  using input_type = Middle;
  using output_type = Output;
  static constexpr auto key = pb::fixed_string{"module.double"};
  static constexpr auto name = pb::fixed_string{"double"};
  Output operator()(Middle input) const { return {input.value * 2}; }
};

struct SenderFactory {
  using output_type = Middle;
  auto operator()(Input input) const { return pb::sync_just(Middle{input.value + 5}); }
};

using SenderStage = pb::sync_sender_stage<SenderFactory, Input>;
static_assert(pb::Stage<SenderStage>);
static_assert(std::is_same_v<pb::sync_value_sender<Middle>::value_type, Middle>);

// The DSL chain resolves entirely through the module export surface.
using Pipeline = pb::from<Input>::then<AddOne>::then<DoubleValue>::to<Output>;
using PackAliasPipeline = pb::from<Input>::then_all<AddOne, DoubleValue>::done;
using PipeAliasPipeline = pb::from<Input>::pipe<AddOne>::through<DoubleValue>::returns<Output>;
using AsAliasPipeline = pb::from<Input>::then_all<AddOne>::then<DoubleValue>::as<Output>;
static_assert(pb::valid<Pipeline>);
static_assert(std::is_same_v<Pipeline, PackAliasPipeline>);
static_assert(std::is_same_v<Pipeline, PipeAliasPipeline>);
static_assert(std::is_same_v<Pipeline, AsAliasPipeline>);

// Move-only stages must compile through the module surface too.
struct MoveInput {
  std::unique_ptr<int> value{};
};
struct MoveOutput {
  std::unique_ptr<int> value{};
};

struct MoveAddOne {
  using input_type = MoveInput;
  using output_type = MoveOutput;
  MoveOutput operator()(MoveInput input) const {
    return {std::make_unique<int>(*input.value + 1)};
  }
};

using MoveOnlyPipeline = pb::from<MoveInput>::then<MoveAddOne>::to<MoveOutput>;
static_assert(pb::valid<MoveOnlyPipeline>);
static_assert(pb::backend_support_name(pb::backend_support::supported) == std::string_view{"supported"});
static_assert(pb::backend_execution_model_name(pb::backend_execution_model::thread_pool) == std::string_view{"thread_pool"});
static_assert(pb::backend_available("sequential"));
static_assert(pb::backend_available("thread_pool"));
static_assert(!pb::backend_available("unknown"));
static_assert(pb::thread_pool_snapshot{.worker_count = 1}.idle());

}  // namespace

int main() {
  // ── compile + run through the module surface ────────────────
  auto engine = pb::compile<Pipeline>(pb::sequential{});
  using Engine = decltype(engine);
  static_assert(std::is_same_v<typename Engine::pipeline_type, Pipeline>);
  static_assert(std::is_same_v<typename Engine::input_type, Input>);
  static_assert(std::is_same_v<typename Engine::output_type, Output>);
  static_assert(Engine::stage_count == 2);

  // ── compile-time descriptor view (metadata surface) ─────────
  constexpr auto view = pb::descriptor_view<Pipeline>();
  static_assert(decltype(view)::stage_count == 2);
  static_assert(view.stage_records()[0].key == std::string_view{"module.add_one"});
  static_assert(view.stage_records()[1].name == std::string_view{"double"});

  // ── value-level execution ───────────────────────────────────
  auto output = engine.run(Input{20});
  if (output.value != 42) {
    return 1;
  }

  // ── error-aware execution path (pb::result) ─────────────────
  auto safe = engine.try_run(Input{20});
  assert(safe.has_value());
  if (safe.value().value != 42) {
    return 1;
  }

  // ── synchronous sender scaffold through the module surface ───
  auto sender_engine = pb::compile<pb::from<Input>::then<SenderStage>::to<Middle>>(pb::sequential{});
  auto sender_output = sender_engine.run(Input{37});
  if (sender_output.value != 42) {
    return 1;
  }

  // ── move-only pipeline through the module surface ───────────
  auto move_engine = pb::compile<MoveOnlyPipeline>(pb::sequential{});
  auto move_output = move_engine.run(MoveInput{std::make_unique<int>(41)});
  if (*move_output.value != 42) {
    return 1;
  }

  pb::thread_pool pool{1};
  const auto pool_snapshot = pool.snapshot();
  if (pool_snapshot.worker_count != 1 || !pool_snapshot.idle()) {
    return 1;
  }
  if (!pool.wait_idle_for(std::chrono::seconds{1})) {
    return 1;
  }

  return 0;
}
