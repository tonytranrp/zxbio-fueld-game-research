#include <pb/pipeline.hpp>

#include <cassert>
#include <sstream>
#include <string>
#include <string_view>

namespace {
[[nodiscard]] bool contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}
}  // namespace

struct Input {
  int value{};
};

struct Output {
  int value{};
};

struct custom_error {
  pb::runtime::error diagnostic{};

  custom_error() = default;
  explicit custom_error(std::string message)
      : diagnostic{.category = pb::runtime::error_category::stage_failure, .message = std::move(message)} {}
  custom_error(pb::runtime::error value) : diagnostic(std::move(value)) {}
};

struct CheckedWithDiagnostic {
  using input_type = Input;
  using output_type = Output;
  using error_type = custom_error;

  static constexpr auto stage_name() noexcept { return "checked_with_diagnostic"; }

  pb::runtime::result<Output, custom_error> operator()(Input input) const {
    if (input.value < 0) {
      return custom_error{"custom failure"};
    }
    return Output{input.value + 1};
  }
};

using Pipeline = pb::from<Input>::then<CheckedWithDiagnostic>::to<Output>;

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto ok = engine.try_run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 21);

  auto failed = engine.try_run(Input{-1});
  assert(!failed.has_value());
  assert(failed.error().category == pb::runtime::error_category::expected_error);
  assert(failed.error().stage.name == "checked_with_diagnostic");
  assert(failed.error().message == "custom failure");

  {
    auto throwing = pb::with_throw_on_error(pb::compile<Pipeline>(pb::runtime::sequential{}));
    bool threw = false;
    try {
      (void)throwing.run(Input{-1});
    } catch (const pb::pipeline_exception& ex) {
      threw = true;
      assert(ex.diagnostic().category == pb::runtime::error_category::expected_error);
      assert(ex.diagnostic().stage.name == "checked_with_diagnostic");
      assert(ex.diagnostic().message == "custom failure");
    }
    assert(threw);
  }

  {
    auto ignoring = pb::with_ignore_errors(pb::compile<Pipeline>(pb::runtime::sequential{}), Output{-7});
    const auto fallback = ignoring.run(Input{-1});
    assert(fallback.value == -7);

    const auto revealed = ignoring.try_run(Input{-1});
    assert(!revealed.has_value());
    assert(revealed.error().category == pb::runtime::error_category::expected_error);
    assert(revealed.error().stage.name == "checked_with_diagnostic");
    assert(revealed.error().message == "custom failure");
  }

  {
    std::ostringstream sink;
    auto verbose = pb::with_verbose_diagnostics(pb::compile<Pipeline>(pb::runtime::sequential{}), &sink);
    const auto outcome = verbose.try_run(Input{-1});
    assert(!outcome.has_value());
    assert(outcome.error().category == pb::runtime::error_category::expected_error);
    const auto log = sink.str();
    assert(contains(log, "stage=checked_with_diagnostic"));
    assert(contains(log, "category=expected_error"));
    assert(contains(log, "message=custom failure"));
  }

  return 0;
}
