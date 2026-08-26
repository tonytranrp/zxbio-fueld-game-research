#include <pb/pipeline.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>


namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
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

struct CheckedStage {
  using input_type = Input;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "checked_stage"; }
  static constexpr auto stage_key() noexcept { return "runtime.checked"; }

  pb::runtime::result<Output, custom_error> operator()(Input input) const {
    if (input.value == 0) {
      return custom_error{"custom failure"};
    }
    if (input.value < 0) {
      throw std::runtime_error{"custom exception"};
    }
    return Output{input.value + 1};
  }
};

using Pipeline = pb::from<Input>::then<CheckedStage>::to<Output>;
static_assert(pb::valid<Pipeline>);

struct recording_observer final : pb::runtime::observer {
  std::vector<std::string> events{};
  pb::runtime::stage_id last_stage{};
  pb::runtime::error last_error{};

  void on_stage_failure(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    last_stage = stage;
    last_error = error;
    events.push_back("failure:" + stage.name + "/" + stage.key + ":" + pb::runtime::describe(error));
  }

  void on_stage_exception(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    last_stage = stage;
    last_error = error;
    events.push_back("exception:" + stage.name + "/" + stage.key + ":" + pb::runtime::describe(error));
  }
};

void assert_record_matches_stage(const pb::runtime::error_record& record, const pb::runtime::stage_id& stage) {
  pb_test_require(record.stage_key == stage.key);
  pb_test_require(record.stage_name == stage.name);
}

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  const auto descriptor = engine.describe();
  static_assert(decltype(engine)::stage_count == 1);
  pb_test_require(descriptor.size() == 1);
  pb_test_require(descriptor[0].key == "runtime.checked");
  pb_test_require(descriptor[0].name == "checked_stage");

  recording_observer observer{};
  engine.set_observer(&observer);

  auto failed = engine.run(Input{0});
  pb_test_require(!failed.has_value());
  pb_test_require(failed.error().diagnostic.message == "custom failure");
  assert_record_matches_stage(pb::runtime::to_record(failed.error().diagnostic), descriptor[0]);
  pb_test_require(observer.events.size() == 1);
  pb_test_require(observer.events[0].find("failure:") == 0);
  pb_test_require(observer.events[0].find("stage_failure at") != std::string::npos);
  pb_test_require(observer.events[0].find("custom failure") != std::string::npos);
  pb_test_require(observer.last_stage.key == descriptor[0].key);
  pb_test_require(observer.last_stage.name == descriptor[0].name);
  assert_record_matches_stage(pb::runtime::to_record(observer.last_error), descriptor[0]);

  observer.events.clear();
  auto thrown = engine.run(Input{-1});
  pb_test_require(!thrown.has_value());
  pb_test_require(thrown.error().diagnostic.category == pb::runtime::error_category::exception);
  assert_record_matches_stage(pb::runtime::to_record(thrown.error().diagnostic), descriptor[0]);
  pb_test_require(thrown.error().diagnostic.message == "custom exception");
  pb_test_require(observer.events.size() == 1);
  pb_test_require(observer.events[0].find("exception:") == 0);
  pb_test_require(observer.events[0].find("exception at") != std::string::npos);
  pb_test_require(observer.events[0].find("custom exception") != std::string::npos);
  pb_test_require(observer.last_stage.key == descriptor[0].key);
  pb_test_require(observer.last_stage.name == descriptor[0].name);
  assert_record_matches_stage(pb::runtime::to_record(observer.last_error), descriptor[0]);

  return 0;
}
