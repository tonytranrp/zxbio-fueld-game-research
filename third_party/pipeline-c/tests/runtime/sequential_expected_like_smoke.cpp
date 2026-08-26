#include <pb/pipeline.hpp>

#include <cassert>
#include <string>
#include <utility>

struct Input {
  int value{};
};

struct Middle {
  int value{};
};

struct Output {
  int value{};
};

template <class T, class E>
struct external_expected {
  using value_type = T;
  using error_type = E;

  bool ok{};
  T value_{};
  E error_{};

  [[nodiscard]] bool has_value() const { return ok; }
  [[nodiscard]] const T& value() const& { return value_; }
  [[nodiscard]] T&& value() && { return std::move(value_); }
  [[nodiscard]] const E& error() const& { return error_; }
  [[nodiscard]] E&& error() && { return std::move(error_); }
};

struct CheckedAddOne {
  using input_type = Input;
  static constexpr auto stage_name() noexcept { return "checked_add_one"; }
  using output_type = Middle;
  using error_type = std::string;

  external_expected<Middle, std::string> operator()(Input input) const {
    if (input.value < 0) {
      return {.ok = false, .error_ = "negative input"};
    }
    return {.ok = true, .value_ = {input.value + 1}};
  }
};

struct DoubleValue {
  using input_type = Middle;
  using output_type = Output;

  Output operator()(Middle input) const { return {input.value * 2}; }
};

using Pipeline = pb::from<Input>::then<CheckedAddOne>::then<DoubleValue>::to<Output>;
static_assert(pb::valid<Pipeline>);

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto ok = engine.run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 42);

  auto failed = engine.run(Input{-1});
  assert(!failed.has_value());
  assert(failed.error().category == pb::runtime::error_category::expected_error);
  assert(failed.error().stage.name == "checked_add_one");
  assert(failed.error().message == "negative input");

  return 0;
}
