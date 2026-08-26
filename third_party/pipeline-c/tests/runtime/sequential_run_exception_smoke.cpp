#include <pb/pipeline.hpp>

#include <cassert>
#include <stdexcept>
#include <string_view>

struct Input {
  int value{};
};

struct Middle {
  int value{};
};

struct Output {
  int value{};
};

struct CheckedDouble {
  using input_type = Input;
  using output_type = Middle;
  using error_type = std::string;

  static constexpr auto stage_name() noexcept { return "checked_double"; }

  pb::runtime::result<Middle, pb::runtime::error> operator()(Input input) const {
    if (input.value < 0) {
      throw std::runtime_error{"negative input"};
    }
    return Middle{input.value};
  }
};

struct UnknownThrowingFinal {
  using input_type = Middle;
  using output_type = Output;
  using error_type = std::string;

  static constexpr auto stage_name() noexcept { return "unknown_throwing_final"; }

  pb::runtime::result<Output, pb::runtime::error> operator()(Middle input) const {
    if (input.value == 0) {
      throw 7;
    }
    return Output{input.value * 2};
  }
};

using ThrowingPipeline = pb::from<Input>::then<CheckedDouble>::then<UnknownThrowingFinal>::to<Output>;

int main() {
  auto engine = pb::compile<ThrowingPipeline>(pb::runtime::sequential{});

  auto ok = engine.run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 40);

  auto from_negative = engine.run(Input{-2});
  assert(!from_negative.has_value());
  assert(from_negative.error().category == pb::runtime::error_category::exception);
  assert(from_negative.error().stage.name == "checked_double");
  assert(from_negative.error().message == "negative input");

  auto unknown_exception = engine.run(Input{0});
  assert(!unknown_exception.has_value());
  assert(unknown_exception.error().category == pb::runtime::error_category::exception);
  assert(unknown_exception.error().stage.name == "unknown_throwing_final");
  assert(unknown_exception.error().stage.key == "unknown_throwing_final");
  assert(unknown_exception.error().message == "stage threw an unknown exception");
  assert(unknown_exception.error().message != std::string_view{});

  return 0;
}
