#include <pb/adapt/runtime_callable.hpp>

#include <functional>
#include <type_traits>

namespace {
struct In {
  int value{};
};
struct Out {
  int value{};
};

Out c_free_fn(In input) { return Out{input.value + 1}; }

using CStage = pb::c_function_stage<In, Out, &c_free_fn>;

static_assert(pb::core::Stage<CStage>);
static_assert(std::is_same_v<CStage::input_type, In>);
static_assert(std::is_same_v<CStage::output_type, Out>);
static_assert(std::is_same_v<pb::runtime_callable<In, Out>::input_type, In>);
static_assert(std::is_same_v<pb::runtime_callable<In, Out>::output_type, Out>);
static_assert(std::is_default_constructible_v<pb::runtime_callable<In, Out>>);
}  // namespace

int pb_public_header_adapt_runtime_callable() {
  auto stage = pb::bind_callable<In, Out>([](In in) { return Out{in.value * 2}; });
  static_assert(pb::core::Stage<decltype(stage)>);
  (void)pb::with_runtime_callable_binding(stage);
  return 0;
}
