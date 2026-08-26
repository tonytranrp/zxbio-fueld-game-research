#include <pb/pipeline.hpp>

#include <cassert>
#include <string>

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

using Pipeline = pb::from<Input>::then<AddOne>::then<CheckedDouble>::to<Output>;
static_assert(pb::valid<Pipeline>);

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto run_ok = engine.run(Input{20});
  assert(run_ok.has_value());
  assert(run_ok.value().value == 42);

  auto run_failed = engine.run(Input{-1});
  assert(!run_failed.has_value());
  assert(run_failed.error().category == pb::runtime::error_category::stage_failure);
  assert(run_failed.error().stage.key == "checked_double");
  assert(run_failed.error().stage.name == "checked_double");
  assert(run_failed.error().message == "zero middle");

  auto ok = engine.try_run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 42);

  auto failed = engine.try_run(Input{-1});
  assert(!failed.has_value());
  assert(failed.error().category == pb::runtime::error_category::stage_failure);
  assert(failed.error().stage.key == "checked_double");
  assert(failed.error().stage.name == "checked_double");
  assert(failed.error().message == "zero middle");
  const auto failed_json = pb::runtime::to_json(failed.error());
  assert(failed_json.find("\"stage\":{\"key\":\"checked_double\",\"name\":\"checked_double\"}") !=
         std::string::npos);
  assert(failed_json.find("\"category\":\"stage_failure\"") != std::string::npos);
  assert(failed_json.find("\"message\":\"zero middle\"") != std::string::npos);

  return 0;
}
