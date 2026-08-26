#include <pb/pipeline.hpp>

#include <cassert>
#if __has_include(<expected>)
#include <expected>
#endif
#include <stdexcept>
#include <string>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

struct Input {
  int value{};
};

struct Middle {
  int value{};
};

struct Output {
  int value{};
};

struct StdExpectedAddOne {
  using input_type = Input;
  using output_type = Middle;

  static constexpr auto stage_key() noexcept { return "std.expected.add_one"; }
  static constexpr auto stage_name() noexcept { return "std_expected_add_one"; }

  std::expected<Middle, std::string> operator()(Input input) const {
    if (input.value == -1) {
      return std::unexpected{std::string{"std expected rejected input"}};
    }
    return Middle{input.value + 1};
  }
};

struct ThrowingAfterExpected {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_key() noexcept { return "std.expected.throw_after"; }
  static constexpr auto stage_name() noexcept { return "throw_after_std_expected"; }

  Output operator()(Middle input) const {
    if (input.value < 0) {
      throw std::runtime_error{"throw after std expected"};
    }
    return Output{input.value * 2};
  }
};

using Pipeline = pb::from<Input>::then<StdExpectedAddOne>::then<ThrowingAfterExpected>::to<Output>;
static_assert(pb::valid<Pipeline>);
static_assert(pb::runtime::expected_like<std::expected<Middle, std::string>>);
static_assert(pb::runtime::expected_like<std::expected<void, std::string>>);

#else

int main() { return 0; }

#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

int main() {
  auto void_ok = pb::runtime::to_result(std::expected<void, std::string>{});
  assert(void_ok.has_value());
  assert(!void_ok.has_error());

  auto void_failed = pb::runtime::to_result(std::expected<void, std::string>{
      std::unexpected{std::string{"std expected void rejected"}}});
  assert(!void_failed.has_value());
  assert(void_failed.error().category == pb::runtime::error_category::expected_error);
  assert(void_failed.error().message == "std expected void rejected");

  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto ok = engine.run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 42);

  auto expected_failed = engine.run(Input{-1});
  assert(!expected_failed.has_value());
  assert(expected_failed.error().category == pb::runtime::error_category::expected_error);
  assert(expected_failed.error().stage.key == "std.expected.add_one");
  assert(expected_failed.error().stage.name == "std_expected_add_one");
  assert(expected_failed.error().message == "std expected rejected input");

  auto run_exception = engine.run(Input{-2});
  assert(!run_exception.has_value());
  assert(run_exception.error().category == pb::runtime::error_category::exception);
  assert(run_exception.error().stage.key == "std.expected.throw_after");
  assert(run_exception.error().stage.name == "throw_after_std_expected");
  assert(run_exception.error().message == "throw after std expected");

  auto try_run_exception = engine.try_run(Input{-2});
  assert(!try_run_exception.has_value());
  assert(try_run_exception.error().category == pb::runtime::error_category::exception);
  assert(try_run_exception.error().stage.key == "std.expected.throw_after");
  assert(try_run_exception.error().stage.name == "throw_after_std_expected");
  assert(try_run_exception.error().message == "throw after std expected");

  return 0;
}

#endif
