#include <pb/pipeline.hpp>

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

// =====================================================================
// Test 1: Multi-case branch routing (3 cases)
// =====================================================================

namespace test1 {

struct Input {
  int value{};
};

struct BranchResult {
  int value{};
  std::string origin{};

  bool operator==(const BranchResult&) const = default;
};

struct IsSmall {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-small"; }
  static constexpr auto stage_key() noexcept { return "is-small"; }
  static inline int calls = 0;
  bool operator()(Input input) const {
    ++calls;
    return input.value < 10;
  }
};

struct IsMedium {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-medium"; }
  static constexpr auto stage_key() noexcept { return "is-medium"; }
  static inline int calls = 0;
  bool operator()(Input input) const {
    ++calls;
    return input.value >= 10 && input.value < 100;
  }
};

struct IsLarge {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-large"; }
  static constexpr auto stage_key() noexcept { return "is-large"; }
  static inline int calls = 0;
  bool operator()(Input input) const {
    ++calls;
    return input.value >= 100;
  }
};

struct SmallStage {
  using input_type = Input;
  using output_type = BranchResult;
  static constexpr auto stage_name() noexcept { return "small-stage"; }
  static constexpr auto stage_key() noexcept { return "small-stage"; }
  static inline int calls = 0;
  BranchResult operator()(Input input) const {
    ++calls;
    return BranchResult{input.value * 1, "small"};
  }
};

struct MediumStage {
  using input_type = Input;
  using output_type = BranchResult;
  static constexpr auto stage_name() noexcept { return "medium-stage"; }
  static constexpr auto stage_key() noexcept { return "medium-stage"; }
  static inline int calls = 0;
  BranchResult operator()(Input input) const {
    ++calls;
    return BranchResult{input.value * 10, "medium"};
  }
};

struct LargeStage {
  using input_type = Input;
  using output_type = BranchResult;
  static constexpr auto stage_name() noexcept { return "large-stage"; }
  static constexpr auto stage_key() noexcept { return "large-stage"; }
  static inline int calls = 0;
  BranchResult operator()(Input input) const {
    ++calls;
    return BranchResult{input.value * 100, "large"};
  }
};

using SmallCase = pb::case_<IsSmall>::then<SmallStage>;
using MediumCase = pb::case_<IsMedium>::then<MediumStage>;
using LargeCase = pb::case_<IsLarge>::then<LargeStage>;
using Pipeline = pb::from<Input>::branch<SmallCase, MediumCase, LargeCase>::to<BranchResult>;
static_assert(pb::valid<Pipeline>);

void reset_counters() {
  IsSmall::calls = 0;
  IsMedium::calls = 0;
  IsLarge::calls = 0;
  SmallStage::calls = 0;
  MediumStage::calls = 0;
  LargeStage::calls = 0;
}

void test_multi_case_routing() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  // --- input=5 routes to SmallStage ---
  reset_counters();
  auto small = engine.run(Input{5});
  assert(small.has_value());
  assert((small.value() == BranchResult{5, "small"}));
  assert(SmallStage::calls == 1);
  assert(MediumStage::calls == 0);
  assert(LargeStage::calls == 0);
  // Predicates: IsSmall must have been evaluated (and returned true),
  // IsMedium and IsLarge should not have been evaluated (short-circuit)
  assert(IsSmall::calls == 1);
  assert(IsMedium::calls == 0);
  assert(IsLarge::calls == 0);

  // --- input=50 routes to MediumStage ---
  reset_counters();
  auto medium = engine.run(Input{50});
  assert(medium.has_value());
  assert((medium.value() == BranchResult{500, "medium"}));
  assert(SmallStage::calls == 0);
  assert(MediumStage::calls == 1);
  assert(LargeStage::calls == 0);
  // IsSmall must be checked first (false), then IsMedium (true), IsLarge skipped
  assert(IsSmall::calls == 1);
  assert(IsMedium::calls == 1);
  assert(IsLarge::calls == 0);

  // --- input=500 routes to LargeStage ---
  reset_counters();
  auto large = engine.run(Input{500});
  assert(large.has_value());
  assert((large.value() == BranchResult{50000, "large"}));
  assert(SmallStage::calls == 0);
  assert(MediumStage::calls == 0);
  assert(LargeStage::calls == 1);
  // IsSmall false, IsMedium false, IsLarge true
  assert(IsSmall::calls == 1);
  assert(IsMedium::calls == 1);
  assert(IsLarge::calls == 1);
}

} // namespace test1

// =====================================================================
// Test 2: Branch predicate returning error
// =====================================================================

namespace test2 {

struct Input {
  int value{};
};

struct Routed {
  int value{};
};

struct NegativeCheckPredicate {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "neg-check"; }
  static constexpr auto stage_key() noexcept { return "neg-check"; }

  pb::runtime::result<bool> operator()(Input input) const {
    if (input.value < 0) {
      return pb::runtime::error{.message = "negative value rejected"};
    }
    return true;
  }
};

struct PassThroughStage {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "pass-through"; }
  static constexpr auto stage_key() noexcept { return "pass-through"; }

  Routed operator()(Input input) const { return Routed{input.value}; }
};

using Case = pb::case_<NegativeCheckPredicate>::then<PassThroughStage>;
using Pipeline = pb::from<Input>::branch<Case>::to<Routed>;
static_assert(pb::valid<Pipeline>);

void test_predicate_returning_error() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  // Positive input should succeed
  auto ok = engine.run(Input{42});
  assert(ok.has_value());
  assert(ok.value().value == 42);

  // Negative input should fail via predicate error
  auto err = engine.run(Input{-5});
  assert(!err.has_value());
  assert(err.error().stage.key == "neg-check");
  // Message should be annotated with [branch] prefix by the runtime
  assert((err.error().message.find("[branch]") != std::string::npos));
  assert((err.error().message.find("negative value rejected") != std::string::npos));
}

} // namespace test2

// =====================================================================
// Test 3: Branch predicate throwing std::exception
// =====================================================================

namespace test3 {

struct Input {
  int value{};
};

struct Routed {
  int value{};
};

struct ThrowingPredicate {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "throw-pred"; }
  static constexpr auto stage_key() noexcept { return "throw-pred"; }

  bool operator()(Input input) const {
    if (input.value == 0) {
      throw std::runtime_error("predicate exploded");
    }
    return true;
  }
};

struct QuietStage {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "quiet-stage"; }
  static constexpr auto stage_key() noexcept { return "quiet-stage"; }

  Routed operator()(Input input) const { return Routed{input.value}; }
};

using Case = pb::case_<ThrowingPredicate>::then<QuietStage>;
using Pipeline = pb::from<Input>::branch<Case>::to<Routed>;
static_assert(pb::valid<Pipeline>);

void test_predicate_throwing_exception() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  // Non-triggering input should succeed
  auto ok = engine.run(Input{1});
  assert(ok.has_value());

  // Trigger input should produce exception error
  auto err = engine.run(Input{0});
  assert(!err.has_value());
  assert(err.error().category == pb::runtime::error_category::exception);
  assert(err.error().stage.key == "throw-pred");
  assert((err.error().message.find("[branch]") != std::string::npos));
  assert((err.error().message.find("predicate exploded") != std::string::npos));
}

} // namespace test3

// =====================================================================
// Test 4: Branch stage throwing exception (run path)
// =====================================================================

namespace test4 {

struct Input {
  int value{};
};

struct Routed {
  int value{};
};

struct AlwaysTrue {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "always-true"; }
  static constexpr auto stage_key() noexcept { return "always-true"; }

  bool operator()(Input) const { return true; }
};

struct CrashingStage {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "crash-stage"; }
  static constexpr auto stage_key() noexcept { return "crash-stage"; }

  Routed operator()(Input) const { throw std::runtime_error("stage crashed"); }
};

using Case = pb::case_<AlwaysTrue>::then<CrashingStage>;
using Pipeline = pb::from<Input>::branch<Case>::to<Routed>;
static_assert(pb::valid<Pipeline>);

void test_branch_stage_throwing_exception() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto err = engine.run(Input{1});
  assert(!err.has_value());
  assert(err.error().category == pb::runtime::error_category::exception);
  assert(err.error().stage.key == "crash-stage");
  assert((err.error().message.find("[branch]") != std::string::npos));
  assert((err.error().message.find("stage crashed") != std::string::npos));
}

} // namespace test4

// =====================================================================
// Test 5: Branch observer events (on_case_selected / on_case_skipped)
// =====================================================================

namespace test5 {

struct Input {
  int value{};
};

struct BranchResult {
  int value{};
};

struct PredA {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "pred-a"; }
  static constexpr auto stage_key() noexcept { return "pred-a"; }
  bool operator()(Input input) const { return input.value < 10; }
};

struct PredB {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "pred-b"; }
  static constexpr auto stage_key() noexcept { return "pred-b"; }
  bool operator()(Input input) const { return input.value >= 10 && input.value < 100; }
};

struct PredC {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "pred-c"; }
  static constexpr auto stage_key() noexcept { return "pred-c"; }
  bool operator()(Input input) const { return input.value >= 100; }
};

struct StageA {
  using input_type = Input;
  using output_type = BranchResult;
  static constexpr auto stage_name() noexcept { return "stage-a"; }
  static constexpr auto stage_key() noexcept { return "stage-a"; }
  BranchResult operator()(Input input) const { return BranchResult{input.value * 1}; }
};

struct StageB {
  using input_type = Input;
  using output_type = BranchResult;
  static constexpr auto stage_name() noexcept { return "stage-b"; }
  static constexpr auto stage_key() noexcept { return "stage-b"; }
  BranchResult operator()(Input input) const { return BranchResult{input.value * 10}; }
};

struct StageC {
  using input_type = Input;
  using output_type = BranchResult;
  static constexpr auto stage_name() noexcept { return "stage-c"; }
  static constexpr auto stage_key() noexcept { return "stage-c"; }
  BranchResult operator()(Input input) const { return BranchResult{input.value * 100}; }
};

using CaseA = pb::case_<PredA>::then<StageA>;
using CaseB = pb::case_<PredB>::then<StageB>;
using CaseC = pb::case_<PredC>::then<StageC>;
using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::to<BranchResult>;
static_assert(pb::valid<Pipeline>);

struct RecordingObserver : pb::runtime::observer {
  std::vector<std::string> events{};

  void on_stage_start(const pb::runtime::stage_id& stage) override {
    events.push_back("start:" + stage.key);
  }
  void on_stage_success(const pb::runtime::stage_id& stage) override {
    events.push_back("success:" + stage.key);
  }
  void on_stage_failure(const pb::runtime::stage_id& stage, const pb::runtime::error&) override {
    events.push_back("failure:" + stage.key);
  }
  void on_stage_exception(const pb::runtime::stage_id& stage, const pb::runtime::error&) override {
    events.push_back("exception:" + stage.key);
  }
  void on_case_selected(const pb::runtime::stage_id& branch_id, std::size_t case_index,
                        const pb::runtime::stage_id& predicate_id) override {
    events.push_back("case_selected:" + branch_id.key + ":" + std::to_string(case_index) + ":" +
                     predicate_id.key);
  }
  void on_case_skipped(const pb::runtime::stage_id& branch_id, std::size_t case_index,
                       const pb::runtime::stage_id& predicate_id) override {
    events.push_back("case_skipped:" + branch_id.key + ":" + std::to_string(case_index) + ":" +
                     predicate_id.key);
  }
};

[[nodiscard]] bool contains(const std::vector<std::string>& values, const std::string& expected) {
  for (const auto& v : values) {
    if (v == expected) return true;
  }
  return false;
}

void test_branch_observer_events() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  RecordingObserver observer{};
  engine.set_observer(&observer);

  // Input matching case 1 (index 1, medium: 10 <= 50 < 100)
  auto result = engine.run(Input{50});
  assert(result.has_value());

  // Expected event chain:
  // start:branch
  //   start:pred-a  ->  success:pred-a  (returns false)
  //   case_skipped:branch:0:pred-a
  //   start:pred-b  ->  success:pred-b  (returns true)
  //   case_selected:branch:1:pred-b
  //   start:stage-b  ->  success:stage-b
  // success:branch

  assert(contains(observer.events, "start:branch"));
  assert(contains(observer.events, "start:pred-a"));
  assert(contains(observer.events, "success:pred-a"));
  assert(contains(observer.events, "case_skipped:branch:0:pred-a"));

  assert(contains(observer.events, "start:pred-b"));
  assert(contains(observer.events, "success:pred-b"));
  assert(contains(observer.events, "case_selected:branch:1:pred-b"));

  assert(contains(observer.events, "start:stage-b"));
  assert(contains(observer.events, "success:stage-b"));
  assert(contains(observer.events, "success:branch"));

  // pred-c should NOT have been evaluated (short-circuit after match)
  assert(!contains(observer.events, "start:pred-c"));
  assert(!contains(observer.events, "case_skipped:branch:2:pred-c"));
}

} // namespace test5

// =====================================================================
// Test 6: try_run with branch→join pipeline
// =====================================================================

namespace test6 {

struct Input {
  int value{};
};

struct Decision {
  int code{};
};

struct AlwaysGo {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "always-go"; }
  static constexpr auto stage_key() noexcept { return "always-go"; }
  bool operator()(Input) const { return true; }
};

struct GoodStage {
  using input_type = Input;
  using output_type = Decision;
  static constexpr auto stage_name() noexcept { return "good-stage"; }
  static constexpr auto stage_key() noexcept { return "good-stage"; }
  Decision operator()(Input input) const { return Decision{input.value * 2}; }
};

struct FailingStage {
  using input_type = Input;
  using output_type = Decision;
  static constexpr auto stage_name() noexcept { return "fail-stage"; }
  static constexpr auto stage_key() noexcept { return "fail-stage"; }
  pb::runtime::result<Decision> operator()(Input) const {
    return pb::runtime::error{.message = "stage failed intentionally"};
  }
};

struct ThrowingStage {
  using input_type = Input;
  using output_type = Decision;
  static constexpr auto stage_name() noexcept { return "throw-stage"; }
  static constexpr auto stage_key() noexcept { return "throw-stage"; }
  Decision operator()(Input) const { throw std::runtime_error("stage exploded"); }
};

struct JoinStage {
  using input_type = Decision;
  using output_type = int;
  static constexpr auto stage_name() noexcept { return "join-stage"; }
  static constexpr auto stage_key() noexcept { return "join-stage"; }
  int operator()(Decision d) const { return d.code + 1; }
};

using GoodCase = pb::case_<AlwaysGo>::then<GoodStage>;
using FailingCase = pb::case_<AlwaysGo>::then<FailingStage>;
using ThrowingCase = pb::case_<AlwaysGo>::then<ThrowingStage>;

using GoodPipeline = pb::from<Input>::branch<GoodCase>::join<JoinStage>::to<int>;
using FailingPipeline = pb::from<Input>::branch<FailingCase>::join<JoinStage>::to<int>;
using ThrowingPipeline = pb::from<Input>::branch<ThrowingCase>::join<JoinStage>::to<int>;
static_assert(pb::valid<GoodPipeline>);
static_assert(pb::valid<FailingPipeline>);
static_assert(pb::valid<ThrowingPipeline>);

void test_try_run_branch_join() {
  // --- try_run success ---
  {
    auto engine = pb::compile<GoodPipeline>(pb::runtime::sequential{});
    auto result = engine.try_run(Input{21});
    assert(result.has_value());
    assert(result.value() == 43);
  }

  // --- try_run with failing branch stage ---
  {
    auto engine = pb::compile<FailingPipeline>(pb::runtime::sequential{});
    auto result = engine.try_run(Input{1});
    assert(!result.has_value());
    assert(result.error().stage.key == "fail-stage");
    assert(result.error().message.find("[branch]") != std::string::npos);
    assert(result.error().message.find("stage failed intentionally") != std::string::npos);
  }

  // --- try_run with throwing branch stage ---
  {
    auto engine = pb::compile<ThrowingPipeline>(pb::runtime::sequential{});
    auto result = engine.try_run(Input{1});
    assert(!result.has_value());
    assert(result.error().category == pb::runtime::error_category::exception);
    assert(result.error().stage.key == "throw-stage");
    assert(result.error().message.find("[branch]") != std::string::npos);
    assert(result.error().message.find("stage exploded") != std::string::npos);
  }
}

} // namespace test6

// =====================================================================
// Test 7: Stateful sequential with branch
// =====================================================================

namespace test7 {

struct Input {
  int value{};
};

struct Routed {
  int value{};
};

struct IsEven {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-even"; }
  static constexpr auto stage_key() noexcept { return "is-even"; }
  bool operator()(Input input) const { return input.value % 2 == 0; }
};

struct IsOdd {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-odd"; }
  static constexpr auto stage_key() noexcept { return "is-odd"; }
  bool operator()(Input input) const { return input.value % 2 != 0; }
};

struct EvenRoute {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "even-route"; }
  static constexpr auto stage_key() noexcept { return "even-route"; }

  int counter = 0;

  Routed operator()(Input input) {
    ++counter;
    return Routed{input.value * (10 + counter)};
  }
};

struct OddRoute {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "odd-route"; }
  static constexpr auto stage_key() noexcept { return "odd-route"; }

  int counter = 0;

  Routed operator()(Input input) {
    ++counter;
    return Routed{input.value * (100 + counter)};
  }
};

using EvenCase = pb::case_<IsEven>::then<EvenRoute>;
using OddCase = pb::case_<IsOdd>::then<OddRoute>;
using Pipeline = pb::from<Input>::branch<EvenCase, OddCase>::to<Routed>;
static_assert(pb::valid<Pipeline>);

void test_stateful_sequential_branch() {
  auto engine = pb::compile<Pipeline>(pb::runtime::stateful_sequential{});

  // First run: even input
  auto first = engine.run(Input{2});
  assert(first.has_value());
  // counter starts at 0, increment to 1: 2 * (10 + 1) = 22
  assert(first.value().value == 22);

  // Second run: odd input
  auto second = engine.run(Input{3});
  assert(second.has_value());
  // counter starts at 0, increment to 1: 3 * (100 + 1) = 303
  assert(second.value().value == 303);

  // Third run: even input again — counter should be at 2 now
  auto third = engine.run(Input{2});
  assert(third.has_value());
  // counter was 1 after first run, increment to 2: 2 * (10 + 2) = 24
  assert(third.value().value == 24);

  // Fourth run: odd input again
  auto fourth = engine.run(Input{3});
  assert(fourth.has_value());
  // counter was 1 after second run, increment to 2: 3 * (100 + 2) = 306
  assert(fourth.value().value == 306);
}

} // namespace test7

// =====================================================================
// Test 8: Multiple runs on same engine
// =====================================================================

namespace test8 {

struct Input {
  int value{};
};

struct Routed {
  int value{};
  std::string tag{};
};

struct IsSmall {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-small"; }
  static constexpr auto stage_key() noexcept { return "is-small"; }
  bool operator()(Input input) const { return input.value < 10; }
};

struct IsBig {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-big"; }
  static constexpr auto stage_key() noexcept { return "is-big"; }
  bool operator()(Input input) const { return input.value >= 10; }
};

struct SmallRoute {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "small-route"; }
  static constexpr auto stage_key() noexcept { return "small-route"; }
  Routed operator()(Input input) const { return Routed{input.value, "small"}; }
};

struct BigRoute {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "big-route"; }
  static constexpr auto stage_key() noexcept { return "big-route"; }
  Routed operator()(Input input) const { return Routed{input.value * 10, "big"}; }
};

using SmallCase = pb::case_<IsSmall>::then<SmallRoute>;
using BigCase = pb::case_<IsBig>::then<BigRoute>;
using Pipeline = pb::from<Input>::branch<SmallCase, BigCase>::to<Routed>;
static_assert(pb::valid<Pipeline>);

struct RunObserver : pb::runtime::observer {
  std::vector<std::string> events{};

  void on_stage_start(const pb::runtime::stage_id& stage) override {
    events.push_back("start:" + stage.key);
  }
  void on_stage_success(const pb::runtime::stage_id& stage) override {
    events.push_back("success:" + stage.key);
  }
  void on_case_selected(const pb::runtime::stage_id& branch_id, std::size_t case_index,
                        const pb::runtime::stage_id& predicate_id) override {
    events.push_back("case_selected:" + branch_id.key + ":" + std::to_string(case_index) + ":" +
                     predicate_id.key);
  }
  void on_case_skipped(const pb::runtime::stage_id& branch_id, std::size_t case_index,
                       const pb::runtime::stage_id& predicate_id) override {
    events.push_back("case_skipped:" + branch_id.key + ":" + std::to_string(case_index) + ":" +
                     predicate_id.key);
  }

  void reset() { events.clear(); }
};

[[nodiscard]] bool contains(const std::vector<std::string>& values, const std::string& expected) {
  for (const auto& v : values) {
    if (v == expected) return true;
  }
  return false;
}

void test_multiple_runs_same_engine() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  RunObserver observer{};
  engine.set_observer(&observer);

  // Run 1: small input
  observer.reset();
  auto r1 = engine.run(Input{3});
  assert(r1.has_value());
  assert(r1.value().value == 3);
  assert(r1.value().tag == "small");
  assert(contains(observer.events, "case_skipped:branch:0:is-small") == false);
  assert(contains(observer.events, "case_selected:branch:0:is-small"));

  // Run 2: big input
  observer.reset();
  auto r2 = engine.run(Input{42});
  assert(r2.has_value());
  assert(r2.value().value == 420);
  assert(r2.value().tag == "big");
  assert(contains(observer.events, "case_skipped:branch:0:is-small"));
  assert(contains(observer.events, "case_selected:branch:1:is-big"));

  // Run 3: small input again — verify fresh events
  observer.reset();
  auto r3 = engine.run(Input{7});
  assert(r3.has_value());
  assert(r3.value().value == 7);
  assert(r3.value().tag == "small");
  // No events from previous runs should contaminate
  assert(observer.events.size() > 0);
  // These should NOT appear because run 2 was big:
  assert(!contains(observer.events, "case_skipped:branch:1:is-big"));
  assert(!contains(observer.events, "case_selected:branch:1:is-big"));
  // Run 3 matched small (case 0):
  assert(contains(observer.events, "case_selected:branch:0:is-small"));
}

} // namespace test8

// =====================================================================
// Test 9: Stateful branch predicates observe const input
// =====================================================================

namespace test9 {

struct Input {
  int value{};
};

struct Routed {
  int value{};
};

struct OverloadedPredicate {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "overloaded-predicate"; }
  static constexpr auto stage_key() noexcept { return "overloaded-predicate"; }

  static inline int const_calls = 0;
  static inline int mutable_calls = 0;

  bool operator()(const Input& input) const {
    ++const_calls;
    return input.value == 42;
  }

  bool operator()(Input&) const {
    ++mutable_calls;
    return false;
  }
};

struct Route {
  using input_type = Input;
  using output_type = Routed;
  static constexpr auto stage_name() noexcept { return "route"; }
  static constexpr auto stage_key() noexcept { return "route"; }

  Routed operator()(Input input) const { return Routed{input.value + 1}; }
};

using Case = pb::case_<OverloadedPredicate>::then<Route>;
using Pipeline = pb::from<Input>::branch<Case>::to<Routed>;
static_assert(pb::valid<Pipeline>);

void test_stateful_predicate_uses_const_input_overload() {
  OverloadedPredicate::const_calls = 0;
  OverloadedPredicate::mutable_calls = 0;

  auto engine = pb::compile<Pipeline>(pb::runtime::stateful_sequential{});
  auto result = engine.run(Input{42});

  assert(result.has_value());
  assert(result.value().value == 43);
  assert(OverloadedPredicate::const_calls == 1);
  assert(OverloadedPredicate::mutable_calls == 0);
}

} // namespace test9

// =====================================================================
// main
// =====================================================================

int main() {
  test1::test_multi_case_routing();
  test2::test_predicate_returning_error();
  test3::test_predicate_throwing_exception();
  test4::test_branch_stage_throwing_exception();
  test5::test_branch_observer_events();
  test6::test_try_run_branch_join();
  test7::test_stateful_sequential_branch();
  test8::test_multiple_runs_same_engine();
  test9::test_stateful_predicate_uses_const_input_overload();
  return 0;
}
