#include <pb/pipeline.hpp>

#include <cassert>
#include <stdexcept>
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

struct AddOne {
  using input_type = Input;
  using output_type = Middle;

  Middle operator()(Input input) const { return {input.value + 1}; }
};

struct MaybeThrowingDouble {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_key() noexcept { return "math.double"; }
  static constexpr auto stage_name() noexcept { return "maybe_throwing_double"; }

  Output operator()(Middle input) const {
    if (input.value < 0) {
      throw std::runtime_error{"negative middle"};
    }
    return {input.value * 2};
  }
};

struct UnknownThrowingDouble {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "unknown_throwing_double"; }

  Output operator()(Middle) const { throw 7; }
};

struct CheckedDouble {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "checked_double"; }

  pb::runtime::result<Output> operator()(Middle input) const {
    if (input.value == 0) {
      return pb::runtime::error{.category = pb::runtime::error_category::stage_failure,
                                .message = "zero middle"};
    }
    return Output{input.value * 2};
  }
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

struct ExternallyCheckedDouble {
  using input_type = Middle;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "externally_checked_double"; }

  external_expected<Output, std::string> operator()(Middle input) const {
    if (input.value == 0) {
      return {.ok = false, .error_ = "external zero middle"};
    }
    return {.ok = true, .value_ = {input.value * 2}};
  }
};

using ThrowingPipeline = pb::from<Input>::then<AddOne>::then<MaybeThrowingDouble>::to<Output>;
using UnknownThrowingPipeline = pb::from<Input>::then<AddOne>::then<UnknownThrowingDouble>::to<Output>;
using ResultPipeline = pb::from<Input>::then<AddOne>::then<CheckedDouble>::to<Output>;
using ExternalExpectedPipeline = pb::from<Input>::then<AddOne>::then<ExternallyCheckedDouble>::to<Output>;
static_assert(pb::valid<ThrowingPipeline>);
static_assert(pb::valid<UnknownThrowingPipeline>);
static_assert(pb::valid<ResultPipeline>);
static_assert(pb::valid<ExternalExpectedPipeline>);

int main() {
  auto throwing_engine = pb::compile<ThrowingPipeline>(pb::runtime::sequential{});

  auto ok = throwing_engine.try_run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 42);

  auto exception = throwing_engine.try_run(Input{-2});
  assert(!exception.has_value());
  assert(exception.error().category == pb::runtime::error_category::exception);
  assert(exception.error().stage.key == "math.double");
  assert(exception.error().stage.name == "maybe_throwing_double");
  assert(pb::runtime::describe(exception.error().stage) == "maybe_throwing_double (math.double)");
  assert(exception.error().message == "negative middle");
  assert(pb::runtime::describe(exception.error()) == "exception at maybe_throwing_double (math.double): negative middle");
  auto exception_record = pb::runtime::to_record(exception.error());
  assert(exception_record.stage_key == "math.double");
  assert(exception_record.stage_name == "maybe_throwing_double");
  assert(exception_record.category == "exception");
  assert(exception_record.message == "negative middle");

  auto unknown_throwing_engine = pb::compile<UnknownThrowingPipeline>(pb::runtime::sequential{});
  auto unknown_exception = unknown_throwing_engine.try_run(Input{1});
  assert(!unknown_exception.has_value());
  assert(unknown_exception.error().category == pb::runtime::error_category::exception);
  assert(unknown_exception.error().stage.key == "unknown_throwing_double");
  assert(unknown_exception.error().stage.name == "unknown_throwing_double");
  assert(pb::runtime::describe(unknown_exception.error().stage) == "unknown_throwing_double");
  assert(unknown_exception.error().message == "stage threw an unknown exception");
  assert(pb::runtime::describe(unknown_exception.error()) ==
         "exception at unknown_throwing_double: stage threw an unknown exception");

  auto result_engine = pb::compile<ResultPipeline>(pb::runtime::sequential{});
  auto failed = result_engine.try_run(Input{-1});
  assert(!failed.has_value());
  assert(failed.error().category == pb::runtime::error_category::stage_failure);
  assert(failed.error().stage.key == "checked_double");
  assert(failed.error().stage.name == "checked_double");
  assert(failed.error().message == "zero middle");
  assert(pb::runtime::describe(failed.error()) == "stage_failure at checked_double: zero middle");
  auto failed_record = pb::runtime::to_record(failed.error());
  assert(failed_record.stage_key == "checked_double");
  assert(failed_record.stage_name == "checked_double");
  assert(failed_record.category == "stage_failure");
  assert(failed_record.message == "zero middle");

  auto external_expected_engine = pb::compile<ExternalExpectedPipeline>(pb::runtime::sequential{});
  auto external_failed = external_expected_engine.try_run(Input{-1});
  assert(!external_failed.has_value());
  assert(external_failed.error().category == pb::runtime::error_category::expected_error);
  assert(external_failed.error().stage.key == "externally_checked_double");
  assert(external_failed.error().stage.name == "externally_checked_double");
  assert(external_failed.error().message == "external zero middle");
  assert(pb::runtime::describe(external_failed.error()) ==
         "expected_error at externally_checked_double: external zero middle");
  auto external_record = pb::runtime::to_record(external_failed.error());
  assert(external_record.stage_key == "externally_checked_double");
  assert(external_record.stage_name == "externally_checked_double");
  assert(external_record.category == "expected_error");
  assert(external_record.message == "external zero middle");

  return 0;
}
