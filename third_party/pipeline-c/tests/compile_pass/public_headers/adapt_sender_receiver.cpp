#include <pb/adapt/sender_receiver.hpp>

#include <exception>
#include <type_traits>
#include <utility>

namespace {
struct In {
  int value{};
};
struct Out {
  int value{};
};

struct MakeSender {
  using output_type = Out;

  auto operator()(In input) const {
    return pb::sync_just(Out{input.value + 1});
  }
};

using Stage = pb::sync_sender_stage<MakeSender, In>;

static_assert(pb::core::Stage<Stage>);
static_assert(std::is_same_v<Stage::input_type, In>);
static_assert(std::is_same_v<Stage::output_type, Out>);
static_assert(std::is_same_v<Stage::error_type, pb::no_error>);
static_assert(std::is_same_v<pb::sync_value_sender<Out>::value_type, Out>);
static_assert(std::is_base_of_v<std::runtime_error, pb::sender_stopped>);
static_assert(std::is_base_of_v<std::runtime_error, pb::sender_no_value>);
}  // namespace

int pb_public_header_adapt_sender_receiver() {
  Stage stage{};
  auto out = stage(In{41});
  return out.value == 42 ? 0 : 1;
}
