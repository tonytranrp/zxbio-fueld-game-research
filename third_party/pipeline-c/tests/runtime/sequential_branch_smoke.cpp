#include <pb/pipeline.hpp>

#include <string>
#include <vector>

struct Raw {
  int value{};
};

struct Routed {
  int value{};
};

struct IsEven {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-even"; }
  static constexpr auto stage_key() noexcept { return "is-even"; }
  bool operator()(Raw raw) const { return raw.value % 2 == 0; }
};

struct IsOdd {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-odd"; }
  static constexpr auto stage_key() noexcept { return "is-odd"; }
  bool operator()(Raw raw) const { return raw.value % 2 != 0; }
};

struct Always {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "always"; }
  static constexpr auto stage_key() noexcept { return "always"; }
  bool operator()(Raw) const { return true; }
};

struct Never {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "never"; }
  static constexpr auto stage_key() noexcept { return "never"; }
  bool operator()(Raw) const { return false; }
};

struct EvenRoute {
  using input_type = Raw;
  using output_type = Routed;
  static inline int calls = 0;
  static constexpr auto stage_name() noexcept { return "even-route"; }
  static constexpr auto stage_key() noexcept { return "even-route"; }
  Routed operator()(Raw raw) const {
    ++calls;
    return Routed{raw.value * 10};
  }
};

struct OddRoute {
  using input_type = Raw;
  using output_type = Routed;
  static inline int calls = 0;
  static constexpr auto stage_name() noexcept { return "odd-route"; }
  static constexpr auto stage_key() noexcept { return "odd-route"; }
  Routed operator()(Raw raw) const {
    ++calls;
    return Routed{raw.value * 100};
  }
};

struct FailingRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "fail-route"; }
  static constexpr auto stage_key() noexcept { return "fail-route"; }
  pb::runtime::result<Routed> operator()(Raw) const {
    return pb::runtime::result<Routed>{pb::runtime::error{.message = "route failed"}};
  }
};

struct Finish {
  using input_type = Routed;
  using output_type = int;
  static constexpr auto stage_name() noexcept { return "finish"; }
  static constexpr auto stage_key() noexcept { return "finish"; }
  int operator()(Routed routed) const { return routed.value + 1; }
};

struct RecordingObserver : pb::runtime::observer {
  std::vector<std::string> events{};

  void on_stage_start(const pb::runtime::stage_id& stage) override { events.push_back("start:" + stage.key); }
  void on_stage_success(const pb::runtime::stage_id& stage) override { events.push_back("success:" + stage.key); }
  void on_stage_failure(const pb::runtime::stage_id& stage, const pb::runtime::error& error) override {
    events.push_back("failure:" + stage.key + ":" + error.stage.key + ":" + error.message);
  }
};

[[nodiscard]] auto contains(const std::vector<std::string>& values, const std::string& expected) -> bool {
  for (const auto& value : values) {
    if (value == expected) {
      return true;
    }
  }
  return false;
}

using EvenCase = pb::case_<IsEven>::then<EvenRoute>;
using OddCase = pb::case_<IsOdd>::then<OddRoute>;
using Pipeline = pb::from<Raw>::branch<EvenCase, OddCase>::to<Routed>;
using JoinPipeline = pb::from<Raw>::branch<EvenCase, OddCase>::join<Finish>::to<int>;
static_assert(pb::valid<Pipeline>);
static_assert(pb::valid<JoinPipeline>);
static_assert(pb::pipeline_size_v<Pipeline> == 1);
static_assert(pb::pipeline_size_v<JoinPipeline> == 2);
static_assert(std::is_same_v<pb::pipeline_output_t<Pipeline>, Routed>);
static_assert(std::is_same_v<pb::pipeline_output_t<JoinPipeline>, int>);

using FailingCase = pb::case_<Always>::then<FailingRoute>;
using FailingPipeline = pb::from<Raw>::branch<FailingCase>::to<Routed>;

using NoMatchCase = pb::case_<Never>::then<OddRoute>;
using NoMatchPipeline = pb::from<Raw>::branch<NoMatchCase>::to<Routed>;

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  RecordingObserver observer{};
  engine.set_observer(&observer);

  EvenRoute::calls = 0;
  OddRoute::calls = 0;
  auto even = engine.run(Raw{4});
  if (!even.has_value() || even.value().value != 40 || EvenRoute::calls != 1 || OddRoute::calls != 0) {
    return 1;
  }
  if (!contains(observer.events, "start:branch") || !contains(observer.events, "start:is-even") ||
      !contains(observer.events, "success:is-even") || !contains(observer.events, "start:even-route") ||
      !contains(observer.events, "success:even-route") || !contains(observer.events, "success:branch")) {
    return 2;
  }

  auto odd = engine.run(Raw{3});
  if (!odd.has_value() || odd.value().value != 300 || EvenRoute::calls != 1 || OddRoute::calls != 1) {
    return 3;
  }

  auto join_engine = pb::compile<JoinPipeline>(pb::runtime::sequential{});
  auto joined = join_engine.run(Raw{2});
  if (!joined.has_value() || joined.value() != 21) {
    return 4;
  }

  auto failing_engine = pb::compile<FailingPipeline>(pb::runtime::sequential{});
  RecordingObserver failing_observer{};
  failing_engine.set_observer(&failing_observer);
  auto failed = failing_engine.run(Raw{5});
  if (failed.has_value() || failed.error().stage.key != "fail-route" || failed.error().message != "[branch] route failed") {
    return 5;
  }
  if (!contains(failing_observer.events, "failure:fail-route:fail-route:route failed") ||
      !contains(failing_observer.events, "failure:branch:fail-route:[branch] route failed")) {
    return 6;
  }

  auto no_match_engine = pb::compile<NoMatchPipeline>(pb::runtime::sequential{});
  auto no_match = no_match_engine.run(Raw{7});
  if (no_match.has_value() || no_match.error().stage.key != "branch" ||
      no_match.error().category != pb::runtime::error_category::contract_violation) {
    return 7;
  }

  return 0;
}
