#include <pb/pipeline.hpp>
#include <cassert>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

struct Input { int value{}; };
struct Output { int result{}; };

struct PredA {
  using input_type = Input;
  using output_type = bool;
  bool operator()(Input in) const { return in.value < 50; }
};

struct PredB {
  using input_type = Input;
  using output_type = bool;
  bool operator()(Input in) const { return in.value >= 50; }
};

struct StageA {
  using input_type = Input;
  using output_type = Output;
  Output operator()(Input in) const { return Output{in.value * 2}; }
};

struct StageB {
  using input_type = Input;
  using output_type = Output;
  Output operator()(Input in) const { return Output{in.value * 3}; }
};

struct RecordingObserver : pb::runtime::observer {
  std::vector<std::string> events;

  void on_stage_start(const pb::runtime::stage_id& stage) override {
    events.push_back("start:" + stage.key);
  }
  void on_stage_success(const pb::runtime::stage_id& stage) override {
    events.push_back("success:" + stage.key);
  }
  void on_stage_failure(const pb::runtime::stage_id& stage, const pb::runtime::error& err) override {
    events.push_back("failure:" + stage.key + ":" + err.stage.key);
  }
  void on_case_selected(const pb::runtime::stage_id& branch_id, std::size_t case_index,
                        const pb::runtime::stage_id& predicate_id) override {
    events.push_back("case_selected:" + branch_id.key + ":" + std::to_string(case_index) + ":" + predicate_id.key);
  }
  void on_case_skipped(const pb::runtime::stage_id& branch_id, std::size_t case_index,
                       const pb::runtime::stage_id& predicate_id) override {
    events.push_back("case_skipped:" + branch_id.key + ":" + std::to_string(case_index) + ":" + predicate_id.key);
  }
};

[[nodiscard]] bool contains(const std::vector<std::string>& values, const std::string& expected) {
  for (const auto& v : values) if (v == expected) return true;
  return false;
}

using CaseA = pb::case_<PredA>::then<StageA>;
using CaseB = pb::case_<PredB>::then<StageB>;
using Pipeline = pb::from<Input>::branch<CaseA, CaseB>::to<Output>;

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  RecordingObserver observer;
  engine.set_observer(&observer);

  // All predicates and branch stages are unnamed — stage_key() / stage_name()
  // are not defined. The runtime should generate distinct branch-child
  // fallback identities so predicate/stage observer events do not collide even
  // though they share the parent branch node's pipeline StageIndex.
  auto result = engine.run(Input{25});
  assert(result.has_value());
  assert(result.value().result == 50);

  auto second = engine.run(Input{75});
  assert(second.has_value());
  assert(second.value().result == 225);

  // The branch node always has key="branch"
  assert(contains(observer.events, "start:branch"));
  assert(contains(observer.events, "success:branch"));

  assert(contains(observer.events, "start:branch.case.0.predicate"));
  assert(contains(observer.events, "success:branch.case.0.predicate"));
  assert(contains(observer.events, "start:branch.case.0.stage"));
  assert(contains(observer.events, "success:branch.case.0.stage"));

  assert(contains(observer.events, "start:branch.case.1.predicate"));
  assert(contains(observer.events, "success:branch.case.1.predicate"));
  assert(contains(observer.events, "start:branch.case.1.stage"));
  assert(contains(observer.events, "success:branch.case.1.stage"));

  assert(contains(observer.events, "case_selected:branch:0:branch.case.0.predicate"));
  assert(contains(observer.events, "case_skipped:branch:0:branch.case.0.predicate"));
  assert(contains(observer.events, "case_selected:branch:1:branch.case.1.predicate"));

  std::vector<std::string> branch_child_keys;
  for (const auto& event : observer.events) {
    constexpr auto start_prefix = std::string_view{"start:"};
    constexpr auto success_prefix = std::string_view{"success:"};
    if (event.rfind(start_prefix, 0) == 0) {
      branch_child_keys.push_back(event.substr(start_prefix.size()));
    } else if (event.rfind(success_prefix, 0) == 0) {
      branch_child_keys.push_back(event.substr(success_prefix.size()));
    }
  }
  assert(std::find(branch_child_keys.begin(), branch_child_keys.end(), "0") == branch_child_keys.end());
  assert(std::find(branch_child_keys.begin(), branch_child_keys.end(), "branch.case.0.predicate") !=
         branch_child_keys.end());
  assert(std::find(branch_child_keys.begin(), branch_child_keys.end(), "branch.case.1.stage") !=
         branch_child_keys.end());

  return 0;
}
