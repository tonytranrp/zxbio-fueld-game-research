#include <pb/pipeline.hpp>

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

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

  static constexpr auto stage_name() noexcept { return "add_one"; }

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

using Pipeline = pb::from<Input>::then<AddOne>::then<MaybeThrowingDouble>::to<Output>;
static_assert(pb::valid<Pipeline>);

struct recording_observer final : pb::runtime::observer {
  std::vector<std::string> events{};

  void on_stage_start(const pb::runtime::stage_id& stage) override {
    events.push_back("start:" + stage.name + "/" + stage.key);
  }

  void on_stage_success(const pb::runtime::stage_id& stage) override {
    events.push_back("success:" + stage.name + "/" + stage.key);
  }

  void on_stage_failure(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    events.push_back("failure:" + stage.name + "/" + stage.key + ":" + error.message);
  }

  void on_stage_exception(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    events.push_back("exception:" + stage.name + "/" + stage.key + ":" + error.message);
  }
};

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  recording_observer observer{};
  engine.set_observer(&observer);

  auto failed = engine.try_run(Input{-2});
  assert(!failed.has_value());
  assert(failed.error().category == pb::runtime::error_category::exception);
  assert(failed.error().stage.key == "math.double");
  assert(failed.error().stage.name == "maybe_throwing_double");
  assert(failed.error().message == "negative middle");
  assert((observer.events == std::vector<std::string>{
              "start:add_one/add_one",
              "success:add_one/add_one",
              "start:maybe_throwing_double/math.double",
              "exception:maybe_throwing_double/math.double:negative middle",
          }));

  return 0;
}
