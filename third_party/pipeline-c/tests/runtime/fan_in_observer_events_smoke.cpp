#include <pb/pipeline.hpp>

#include <cstddef>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace {

void require(bool condition) {
  if (!condition) std::abort();
}

struct Input {
  int mask{};
  int value{};
};
struct Output {
  int value{};
};
struct Done {
  std::string text{};
};

struct IsA {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 0b001) != 0; }
};
struct IsB {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 0b010) != 0; }
};
struct IsFail {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 0b100) != 0; }
};

struct StageA {
  using input_type = Input;
  using output_type = Output;
  Output operator()(Input input) const { return Output{input.value + 10}; }
};
struct StageB {
  using input_type = Input;
  using output_type = Output;
  Output operator()(Input input) const { return Output{input.value + 100}; }
};
struct FailingStage {
  using input_type = Input;
  using output_type = Output;
  pb::runtime::result<Output> operator()(Input) const {
    return pb::runtime::result<Output>{pb::runtime::error{.message = "stage failure"}};
  }
};

using CaseA = pb::case_<IsA>::then<StageA>;
using CaseB = pb::case_<IsB>::then<StageB>;
using FailCase = pb::case_<IsFail>::then<FailingStage>;
using FanInInput = pb::fan_in_output_t<CaseA, CaseB, FailCase>;

struct Join {
  using input_type = FanInInput;
  using output_type = Done;
  Done operator()(const FanInInput& input) const {
    std::string text;
    text += input.template get<0>().completed() ? "A=" + std::to_string(input.template get<0>().get().value) : "A=skip";
    text += ";";
    text += input.template get<1>().completed() ? "B=" + std::to_string(input.template get<1>().get().value) : "B=skip";
    text += ";";
    text += input.template get<2>().failed() ? "F=fail" : "F=ok";
    return Done{std::move(text)};
  }
};
using Pipeline = pb::from<Input>::branch<CaseA, CaseB, FailCase>::fan_in<Join>::to<Done>;
static_assert(pb::valid<Pipeline>);

// Records the new fan-in lifecycle events.  Thread-safe so the same
// observer type works on both the sequential and the thread-pool backend
// (on the thread-pool backend the synchronized_observer already serializes
// calls, but locking here keeps the recorder self-contained and harmless).
struct EventObserver : pb::runtime::observer {
  std::mutex mutex;
  std::size_t started_calls{};
  std::size_t completed_calls{};
  std::size_t started_case_count{};
  std::size_t reported_selected{};
  std::size_t reported_completed{};
  std::size_t reported_failed{};
  std::vector<std::size_t> scheduled_indices;
  std::vector<std::size_t> case_completed_indices;
  std::vector<bool> case_completed_success;
  std::vector<std::string> sequence;  // coarse ordering trace

  void on_fan_in_started(const pb::runtime::stage_id&, std::size_t case_count) override {
    const std::scoped_lock lock{mutex};
    ++started_calls;
    started_case_count = case_count;
    sequence.push_back("started");
  }

  void on_fan_in_case_scheduled(const pb::runtime::stage_id&, std::size_t case_index,
                                const pb::runtime::stage_id&) override {
    const std::scoped_lock lock{mutex};
    scheduled_indices.push_back(case_index);
    sequence.push_back("scheduled:" + std::to_string(case_index));
  }

  void on_fan_in_case_completed(const pb::runtime::stage_id&, std::size_t case_index, const pb::runtime::stage_id&,
                                bool success) override {
    const std::scoped_lock lock{mutex};
    case_completed_indices.push_back(case_index);
    case_completed_success.push_back(success);
    sequence.push_back("case_completed:" + std::to_string(case_index) + (success ? ":ok" : ":fail"));
  }

  void on_fan_in_completed(const pb::runtime::stage_id&, std::size_t selected_count, std::size_t completed_count,
                           std::size_t failed_count) override {
    const std::scoped_lock lock{mutex};
    ++completed_calls;
    reported_selected = selected_count;
    reported_completed = completed_count;
    reported_failed = failed_count;
    sequence.push_back("completed");
  }
};

bool contains_index(const std::vector<std::size_t>& items, std::size_t value) {
  for (auto item : items) {
    if (item == value) return true;
  }
  return false;
}

// Validates the structural invariants that must hold on any backend:
//   started exactly once, completed exactly once,
//   sequence begins with "started" and ends with "completed",
//   every scheduled index has a matching case_completed,
//   counts add up.
void check_invariants(const EventObserver& observer, std::size_t expected_case_count) {
  require(observer.started_calls == 1);
  require(observer.completed_calls == 1);
  require(observer.started_case_count == expected_case_count);

  require(!observer.sequence.empty());
  require(observer.sequence.front() == "started");
  require(observer.sequence.back() == "completed");

  // One case_completed per scheduled case.
  require(observer.scheduled_indices.size() == observer.case_completed_indices.size());
  require(observer.scheduled_indices.size() == observer.reported_selected);

  // Every scheduled case index was eventually completed.
  for (auto index : observer.scheduled_indices) {
    require(contains_index(observer.case_completed_indices, index));
  }

  // For each case, its scheduled event precedes its own case_completed event.
  for (std::size_t i = 0; i < observer.sequence.size(); ++i) {
    const auto& event = observer.sequence[i];
    if (event.rfind("case_completed:", 0) == 0) {
      const std::string scheduled =
          "scheduled:" + event.substr(std::string{"case_completed:"}.size(),
                                      event.find(':', std::string{"case_completed:"}.size()) -
                                          std::string{"case_completed:"}.size());
      bool found_scheduled_before = false;
      for (std::size_t j = 0; j < i; ++j) {
        if (observer.sequence[j] == scheduled) {
          found_scheduled_before = true;
          break;
        }
      }
      require(found_scheduled_before);
    }
  }

  // selected == completed + (selected-but-failed); failed includes those.
  require(observer.reported_completed <= observer.reported_selected);
  require(observer.reported_failed >= (observer.reported_selected - observer.reported_completed));
}

}  // namespace

int main() {
  // Mask 0b111 selects all three cases: A and B succeed, FailCase fails its stage.
  const Input input{0b111, 5};

  // --- Sequential backend ---
  {
    EventObserver observer;
    auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
    engine.set_observer(&observer);
    auto result = engine.run(input);
    require(result.has_value());
    require(result.value().text == "A=15;B=105;F=fail");

    check_invariants(observer, 3);
    require(observer.reported_selected == 3);
    require(observer.reported_completed == 2);
    require(observer.reported_failed == 1);
    require(contains_index(observer.scheduled_indices, 0));
    require(contains_index(observer.scheduled_indices, 1));
    require(contains_index(observer.scheduled_indices, 2));

    // The failing case (index 2) reports success == false.
    bool saw_fail = false;
    for (std::size_t i = 0; i < observer.case_completed_indices.size(); ++i) {
      if (observer.case_completed_indices[i] == 2) {
        require(!observer.case_completed_success[i]);
        saw_fail = true;
      }
    }
    require(saw_fail);

    // Sequential ordering is strict: started, all scheduled/case_completed, completed.
    require(observer.sequence.front() == "started");
    require(observer.sequence.back() == "completed");
  }

  // --- Partial selection: only B matches ---
  {
    EventObserver observer;
    auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
    engine.set_observer(&observer);
    auto result = engine.run(Input{0b010, 7});
    require(result.has_value());
    require(result.value().text == "A=skip;B=107;F=ok");

    check_invariants(observer, 3);
    require(observer.reported_selected == 1);
    require(observer.reported_completed == 1);
    require(observer.reported_failed == 0);
    require(observer.scheduled_indices.size() == 1);
    require(observer.scheduled_indices.front() == 1);
  }

  // --- Thread-pool backend ---
  {
    EventObserver observer;
    auto engine = pb::compile<Pipeline>(pb::runtime::thread_pool_backend{.worker_count = 2});
    engine.set_observer(&observer);
    auto result = engine.run(input);
    require(result.has_value());
    require(result.value().text == "A=15;B=105;F=fail");

    check_invariants(observer, 3);
    require(observer.reported_selected == 3);
    require(observer.reported_completed == 2);
    require(observer.reported_failed == 1);
    require(contains_index(observer.scheduled_indices, 0));
    require(contains_index(observer.scheduled_indices, 1));
    require(contains_index(observer.scheduled_indices, 2));

    bool saw_fail = false;
    for (std::size_t i = 0; i < observer.case_completed_indices.size(); ++i) {
      if (observer.case_completed_indices[i] == 2) {
        require(!observer.case_completed_success[i]);
        saw_fail = true;
      }
    }
    require(saw_fail);
  }

  return 0;
}
