#include <pb/pipeline.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Input {
  int mask{};
  int value{};
};

struct Parsed { int value{}; };
struct Reviewed { int value{}; };
struct Done { std::string text{}; };
struct VoidDone { int completed{}; int skipped{}; int failed{}; };

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
struct IsC {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 0b100) != 0; }
};
struct IsFailurePredicate {
  using input_type = Input;
  using output_type = bool;
  pb::runtime::result<bool> operator()(const Input& input) const {
    if ((input.mask & 0b1000) != 0) {
      return pb::runtime::result<bool>{pb::runtime::error{.message = "predicate failure"}};
    }
    return pb::runtime::result<bool>{false};
  }
};
struct IsStageFailure {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 0b1'0000) != 0; }
};
struct IsStageException {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return (input.mask & 0b10'0000) != 0; }
};

struct ParseA {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input input) const { return Parsed{input.value + 10}; }
};
struct Review {
  using input_type = Input;
  using output_type = Reviewed;
  Reviewed operator()(Input input) const { return Reviewed{input.value + 100}; }
};
struct ParseC {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input input) const { return Parsed{input.value + 1000}; }
};
struct FailingStage {
  using input_type = Input;
  using output_type = Parsed;
  pb::runtime::result<Parsed> operator()(Input) const {
    return pb::runtime::result<Parsed>{pb::runtime::error{.message = "stage failure"}};
  }
};
struct ThrowingStage {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input) const { throw std::runtime_error{"stage boom"}; }
};

using CaseA = pb::case_<IsA>::then<ParseA>;
using CaseB = pb::case_<IsB>::then<Review>;
using CaseC = pb::case_<IsC>::then<ParseC>;
using FanInInput = pb::fan_in_output_t<CaseA, CaseB, CaseC>;

struct FanInJoin {
  using input_type = FanInInput;
  using output_type = Done;

  Done operator()(FanInInput input) const {
    std::string text;
    text += input.template get<0>().selected() ? "A=" + std::to_string(input.template get<0>().get().value) : "A=skip";
    text += ";";
    text += input.template get<1>().selected() ? "B=" + std::to_string(input.template get<1>().get().value) : "B=skip";
    text += ";";
    text += input.template get<2>().selected() ? "C=" + std::to_string(input.template get<2>().get().value) : "C=skip";
    return Done{std::move(text)};
  }
};

using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::fan_in<FanInJoin>::to<Done>;
static_assert(pb::valid<Pipeline>);

using FailurePredicateCase = pb::case_<IsFailurePredicate>::then<ParseA>;
using FailureStageCase = pb::case_<IsStageFailure>::then<FailingStage>;
using ExceptionStageCase = pb::case_<IsStageException>::then<ThrowingStage>;
using AggregatingInput = pb::fan_in_output_t<FailurePredicateCase, FailureStageCase, ExceptionStageCase, CaseC>;

struct AggregatingJoin {
  using input_type = AggregatingInput;
  using output_type = Done;

  Done operator()(const AggregatingInput& input) const {
    std::string text;
    text += input.template get<0>().failed() ? "P=failed:" + std::string{input.template get<0>().diagnostic_message()}
                                             : "P=ok";
    text += ";";
    text += input.template get<1>().failed() ? "S=failed:" + std::string{input.template get<1>().diagnostic_message()}
                                             : "S=ok";
    text += ";";
    text += input.template get<2>().failed() ? "E=failed:" + std::string{input.template get<2>().diagnostic_message()}
                                             : "E=ok";
    text += ";";
    text += input.template get<3>().completed() ? "C=" + std::to_string(input.template get<3>().get().value)
                                                : "C=skip";
    return Done{std::move(text)};
  }
};
using AggregatingPipeline = pb::from<Input>::branch<FailurePredicateCase, FailureStageCase, ExceptionStageCase, CaseC>
                                ::fan_in<AggregatingJoin>
                                ::to<Done>;
static_assert(pb::valid<AggregatingPipeline>);

struct CountingStage {
  using input_type = Input;
  using output_type = Parsed;
  int count{};
  Parsed operator()(Input input) { return Parsed{input.value + (++count)}; }
};
struct StatefulJoin {
  using input_type = pb::fan_in_results<pb::fan_in_case_result<0, Parsed>>;
  using output_type = Done;
  Done operator()(input_type input) const { return Done{std::to_string(input.template get<0>().get().value)}; }
};
using StatefulCase = pb::case_<IsA>::then<CountingStage>;
using StatefulPipeline = pb::from<Input>::branch<StatefulCase>::fan_in<StatefulJoin>::to<Done>;

struct VoidStage {
  using input_type = Input;
  using output_type = void;
  static int& calls() {
    static int value = 0;
    return value;
  }
  void operator()(Input) const { ++calls(); }
};
using VoidCase = pb::case_<IsA>::then<VoidStage>;
using VoidFanInInput = pb::fan_in_output_t<VoidCase, CaseB>;
struct VoidJoin {
  using input_type = VoidFanInInput;
  using output_type = VoidDone;
  VoidDone operator()(const VoidFanInInput& input) const {
    return VoidDone{.completed = input.template get<0>().completed() ? 1 : 0,
                    .skipped = input.template get<1>().skipped() ? 1 : 0,
                    .failed = input.template get<0>().failed() || input.template get<1>().failed() ? 1 : 0};
  }
};
using VoidFanInPipeline = pb::from<Input>::branch<VoidCase, CaseB>::fan_in<VoidJoin>::to<VoidDone>;
static_assert(pb::valid<VoidFanInPipeline>);

struct MoveOnlyInput {
  explicit MoveOnlyInput(int mask_value, int payload_value)
      : mask(mask_value), payload(std::make_unique<int>(payload_value)) {}
  MoveOnlyInput(MoveOnlyInput&&) noexcept = default;
  MoveOnlyInput& operator=(MoveOnlyInput&&) noexcept = default;
  MoveOnlyInput(const MoveOnlyInput&) = delete;
  MoveOnlyInput& operator=(const MoveOnlyInput&) = delete;

  int mask{};
  std::unique_ptr<int> payload;
};
struct MoveOnlyOutput { int value{}; };
struct MoveOnlyIsA {
  using input_type = MoveOnlyInput;
  using output_type = bool;
  bool operator()(const MoveOnlyInput& input) const { return (input.mask & 1) != 0; }
};
struct MoveOnlyIsB {
  using input_type = MoveOnlyInput;
  using output_type = bool;
  bool operator()(const MoveOnlyInput& input) const { return (input.mask & 2) != 0; }
};
struct BorrowA {
  using input_type = MoveOnlyInput;
  using output_type = MoveOnlyOutput;
  MoveOnlyOutput operator()(const MoveOnlyInput& input) const { return MoveOnlyOutput{*input.payload + 10}; }
};
struct BorrowB {
  using input_type = MoveOnlyInput;
  using output_type = MoveOnlyOutput;
  MoveOnlyOutput operator()(const MoveOnlyInput& input) const { return MoveOnlyOutput{*input.payload + 100}; }
};
using BorrowCaseA = pb::case_<MoveOnlyIsA>::then<BorrowA>;
using BorrowCaseB = pb::case_<MoveOnlyIsB>::then<BorrowB>;
using BorrowInput = pb::fan_in_output_t<BorrowCaseA, BorrowCaseB>;
struct BorrowJoin {
  using input_type = BorrowInput;
  using output_type = Done;
  Done operator()(const BorrowInput& input) const {
    return Done{std::to_string(input.template get<0>().get().value) + "," +
                std::to_string(input.template get<1>().get().value)};
  }
};
using BorrowPipeline = pb::from<MoveOnlyInput>::branch<BorrowCaseA, BorrowCaseB>::fan_in<BorrowJoin>::to<Done>;
static_assert(pb::valid<BorrowPipeline>);

struct RecordingObserver : pb::runtime::observer {
  std::vector<std::string> events;
  void on_case_selected(const pb::runtime::stage_id&, std::size_t case_index,
                        const pb::runtime::stage_id&) override {
    events.push_back("selected:" + std::to_string(case_index));
  }
  void on_case_skipped(const pb::runtime::stage_id&, std::size_t case_index,
                       const pb::runtime::stage_id&) override {
    events.push_back("skipped:" + std::to_string(case_index));
  }
  void on_case_failed(const pb::runtime::stage_id&, std::size_t case_index,
                      const pb::runtime::stage_id&, const pb::runtime::error& error) override {
    events.push_back("failed:" + std::to_string(case_index) + ":" + error.message);
  }
};

bool contains(const std::vector<std::string>& events, const std::string& event) {
  for (const auto& item : events) {
    if (item == event) return true;
  }
  return false;
}

template <class Result>
bool expect_done(const Result& actual, const std::string& expected) {
  return actual.has_value() && actual.value().text == expected;
}

} // namespace

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto zero = engine.run(Input{0, 5});
  if (!expect_done(zero, "A=skip;B=skip;C=skip")) return 1;

  auto one = engine.run(Input{0b010, 7});
  if (!expect_done(one, "A=skip;B=107;C=skip")) return 1;

  RecordingObserver observer;
  engine.set_observer(&observer);
  auto many = engine.run(Input{0b101, 3});
  if (!expect_done(many, "A=13;B=skip;C=1003")) return 1;
  if (!contains(observer.events, "selected:0")) return 1;
  if (!contains(observer.events, "skipped:1")) return 1;
  if (!contains(observer.events, "selected:2")) return 1;

  auto stateful = pb::compile<StatefulPipeline>(pb::runtime::stateful_sequential{});
  auto first = stateful.run(Input{0b001, 20});
  auto second = stateful.run(Input{0b001, 20});
  if (!expect_done(first, "21")) return 1;
  if (!expect_done(second, "22")) return 1;

  RecordingObserver failure_observer;
  auto aggregating = pb::compile<AggregatingPipeline>(pb::runtime::sequential{});
  aggregating.set_observer(&failure_observer);
  auto failures = aggregating.run(Input{0b111100, 2});
  if (!expect_done(failures, "P=failed:predicate failure;S=failed:stage failure;E=failed:stage boom;C=1002")) {
    return 1;
  }
  if (!contains(failure_observer.events, "failed:0:predicate failure")) return 1;
  if (!contains(failure_observer.events, "failed:1:stage failure")) return 1;
  if (!contains(failure_observer.events, "failed:2:stage boom")) return 1;
  if (!contains(failure_observer.events, "selected:3")) return 1;

  auto void_engine = pb::compile<VoidFanInPipeline>(pb::runtime::sequential{});
  VoidStage::calls() = 0;
  auto void_result = void_engine.run(Input{0b001, 9});
  if (!void_result.has_value()) return 1;
  if (void_result.value().completed != 1 || void_result.value().skipped != 1 || void_result.value().failed != 0) {
    return 1;
  }
  if (VoidStage::calls() != 1) return 1;

  auto borrowed = pb::compile<BorrowPipeline>(pb::runtime::sequential{});
  auto borrowed_result = borrowed.run(MoveOnlyInput{3, 5});
  if (!expect_done(borrowed_result, "15,105")) return 1;

  return 0;
}
