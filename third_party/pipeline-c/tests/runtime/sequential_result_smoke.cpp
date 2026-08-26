#include <pb/pipeline.hpp>

#include <cassert>
#include <string>

struct Input { int value{}; };
struct Middle { int value{}; };
struct Output { int value{}; };

struct AddOne {
  using input_type = Input;
  using output_type = Middle;
  Middle operator()(Input input) const { return {input.value + 1}; }
};

struct CheckedDouble {
  using input_type = Middle;
  using output_type = Output;
  using error_type = pb::runtime::error;

  pb::runtime::result<Output> operator()(Middle input) const {
    if (input.value < 0) {
      return pb::runtime::error{.stage = {.key = "checked-double", .name = "checked_double"},
                                .category = pb::runtime::error_category::stage_failure,
                                .message = "negative input"};
    }
    return Output{input.value * 2};
  }
};

struct CheckedMayThrow {
  using input_type = Middle;
  using output_type = Output;
  using error_type = pb::runtime::error;

  static constexpr auto stage_name() noexcept { return "checked_may_throw"; }

  pb::runtime::result<Output> operator()(Middle input) const {
    if (input.value == 0) {
      throw std::runtime_error{"checked maybe throw"};
    }
    return Output{input.value * 2};
  }
};

using Pipeline = pb::from<Input>::then<AddOne>::then<CheckedDouble>::to<Output>;
using ThrowingPipeline = pb::from<Input>::then<AddOne>::then<CheckedMayThrow>::to<Output>;
static_assert(pb::valid<Pipeline>);
static_assert(pb::valid<ThrowingPipeline>);

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto ok = engine.run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 42);

  auto failed = engine.run(Input{-2});
  assert(!failed.has_value());
  assert(failed.error().stage.name == "checked_double");
  assert(failed.error().message == "negative input");

  auto throw_engine = pb::compile<ThrowingPipeline>(pb::runtime::sequential{});
  auto throw_failed = throw_engine.run(Input{-1});
  assert(!throw_failed.has_value());
  assert(throw_failed.error().category == pb::runtime::error_category::exception);
  assert(throw_failed.error().stage.key == "checked_may_throw");
  assert(throw_failed.error().stage.name == "checked_may_throw");
  assert(throw_failed.error().message == "checked maybe throw");

  return 0;
}
