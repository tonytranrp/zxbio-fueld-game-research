#include <pb/pipeline.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct Input {
  int mask{};
  int value{};
};

struct Parsed { int value{}; };

// Predicates select a case when the corresponding mask bit is set.
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

// CaseA succeeds when selected; CaseB fails via an explicit error result;
// CaseC fails via a thrown exception.
struct OkStage {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input input) const { return Parsed{input.value + 1}; }
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

using CaseA = pb::case_<IsA>::then<OkStage>;
using CaseB = pb::case_<IsB>::then<FailingStage>;
using CaseC = pb::case_<IsC>::then<ThrowingStage>;
using FanInInput = pb::fan_in_output_t<CaseA, CaseB, CaseC>;

struct Done { std::string text{}; };

// Join collects the run's failures into a structured envelope.
struct EnvelopeJoin {
  using input_type = FanInInput;
  using output_type = Done;
  Done operator()(const FanInInput& input) const {
    auto envelope = pb::collect_fan_in_errors(input);
    return Done{envelope.to_string()};
  }
};

using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::fan_in<EnvelopeJoin>::to<Done>;
static_assert(pb::valid<Pipeline>);

[[noreturn]] void fail(const std::string& message) {
  throw std::runtime_error{message};
}

void require(bool condition, const std::string& message) {
  if (!condition) {
    fail(message);
  }
}

} // namespace

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  // --- 0 failures: only CaseA selected (succeeds), B and C skipped. ---
  {
    auto run = engine.run(Input{0b001, 10});
    require(run.has_value(), "0-failure run should succeed");
    const std::string& text = run.value().text;
    require(text == "pb.fan_in.errors.v1\nfailures=0",
            "empty envelope must render exactly the schema + failures=0 lines, got: " + text);
  }

  // --- 1 failure: CaseB selected and fails (A and C skipped). ---
  {
    auto run = engine.run(Input{0b010, 10});
    require(run.has_value(), "1-failure run should produce a Done value");
    const std::string& text = run.value().text;

    require(text.starts_with("pb.fan_in.errors.v1\nfailures=1\n"),
            "1-failure envelope header mismatch: " + text);
    require(text.find("case[1] stage=") != std::string::npos,
            "1-failure envelope must reference case index 1: " + text);
    require(text.find("stage failure") != std::string::npos,
            "1-failure envelope must carry the per-case diagnostic: " + text);
  }

  // --- several failures: CaseB and CaseC both selected and fail. ---
  {
    auto run = engine.run(Input{0b110, 10});
    require(run.has_value(), "multi-failure run should produce a Done value");
    const std::string& text = run.value().text;

    require(text.starts_with("pb.fan_in.errors.v1\nfailures=2\n"),
            "multi-failure envelope header mismatch: " + text);

    // Declaration order is preserved: case[1] before case[2].
    auto pos1 = text.find("case[1] stage=");
    auto pos2 = text.find("case[2] stage=");
    require(pos1 != std::string::npos, "multi-failure envelope missing case[1]: " + text);
    require(pos2 != std::string::npos, "multi-failure envelope missing case[2]: " + text);
    require(pos1 < pos2, "envelope must preserve declaration order (case[1] before case[2]): " + text);

    // Stage identity is synthesized from the case index.
    require(text.find("fan_in.case.1") != std::string::npos,
            "stage identity for case 1 missing: " + text);
    require(text.find("fan_in.case.2") != std::string::npos,
            "stage identity for case 2 missing: " + text);

    // Both per-case diagnostics surface.
    require(text.find("stage failure") != std::string::npos, "missing CaseB diagnostic: " + text);
    require(text.find("stage boom") != std::string::npos, "missing CaseC diagnostic: " + text);
  }

  // --- direct envelope API surface checks against a hand-built aggregate. ---
  {
    using Aggregate =
        pb::fan_in_results<pb::fan_in_case_result<0, Parsed>, pb::fan_in_case_result<1, Parsed>,
                           pb::fan_in_case_result<2, Parsed>>;
    Aggregate aggregate;
    aggregate.get<0>().mark_completed(Parsed{1});             // success -> no error
    aggregate.get<1>().mark_failed("first");                  // failure
    aggregate.get<2>().mark_failed("second");                 // failure

    auto envelope = pb::collect_fan_in_errors(aggregate);
    require(envelope.has_failures(), "hand-built aggregate should report failures");
    require(!envelope.empty(), "envelope should not be empty");
    require(envelope.size() == 2, "envelope should hold exactly the two failed cases");

    require(envelope[0].case_index == 1, "first record should be case index 1");
    require(envelope[1].case_index == 2, "second record should be case index 2");
    require(envelope[0].stage.key == "fan_in.case.1", "stage key for case 1 mismatch");
    require(envelope[1].stage.name == "case 2", "stage name for case 2 mismatch");
    require(envelope[0].diagnostic.message == "first", "case 1 diagnostic message mismatch");
    require(envelope[1].diagnostic.message == "second", "case 2 diagnostic message mismatch");
    require(envelope[0].diagnostic.category == pb::runtime::error_category::stage_failure,
            "lifted diagnostic should use stage_failure category");

    // Iteration visits records in order.
    std::size_t seen = 0;
    std::size_t last_index = 0;
    for (const auto& record : envelope) {
      if (seen == 0) {
        last_index = record.case_index;
      } else {
        require(record.case_index > last_index, "iteration must preserve ascending declaration order");
        last_index = record.case_index;
      }
      ++seen;
    }
    require(seen == 2, "iteration should visit both records");

    require(envelope.to_string() == envelope.format(), "to_string() must alias format()");
    require(envelope.to_string() ==
                std::string{"pb.fan_in.errors.v1\nfailures=2\n"} +
                    "case[1] stage=case 1 (fan_in.case.1) :: stage_failure at case 1 (fan_in.case.1): first\n" +
                    "case[2] stage=case 2 (fan_in.case.2) :: stage_failure at case 2 (fan_in.case.2): second",
            "rendered schema mismatch: " + envelope.to_string());
  }

  // --- skipped cases are ignored and failed empty diagnostics retain their exact shape. ---
  {
    using Aggregate = pb::fan_in_results<pb::fan_in_case_result<0, Parsed>,
                                         pb::fan_in_case_result<1, Parsed>>;
    Aggregate aggregate;
    require(aggregate.get<0>().skipped(), "default case 0 state should be skipped");
    require(!aggregate.get<0>().selected(), "default skipped case must not be selected");
    require(!aggregate.get<0>().completed(), "default skipped case must not be completed");
    require(!aggregate.get<0>().failed(), "default skipped case must not be failed");
    require(!aggregate.get<0>().has_value(), "default skipped case must not hold a value");
    require(aggregate.get<0>().diagnostic_message().empty(),
            "default skipped case diagnostic must be empty");

    aggregate.get<1>().mark_completed(Parsed{11});
    require(aggregate.get<1>().has_value(), "completed case should hold a value before failure");
    aggregate.get<1>().mark_failed("");
    require(aggregate.get<1>().failed(), "failed case should report failed state");
    require(!aggregate.get<1>().selected(), "failed case must not report selected/completed state");
    require(!aggregate.get<1>().completed(), "failed case must not report completed state");
    require(!aggregate.get<1>().has_value(), "mark_failed must clear any previous value");
    require(aggregate.get<1>().diagnostic_message().empty(),
            "empty failure diagnostic should remain an empty string_view");

    auto envelope = pb::collect_fan_in_errors(aggregate);
    require(envelope.has_failures(), "one failed case should produce a non-empty envelope");
    require(envelope.size() == 1, "skipped cases must not produce envelope records");
    require(envelope[0].case_index == 1, "only the failed case index should be captured");
    require(envelope[0].stage.key == "fan_in.case.1", "failed case fallback stage key mismatch");
    require(envelope[0].stage.name == "case 1", "failed case fallback stage name mismatch");
    require(envelope[0].diagnostic.stage.key == envelope[0].stage.key,
            "diagnostic stage key should mirror the envelope stage");
    require(envelope[0].diagnostic.stage.name == envelope[0].stage.name,
            "diagnostic stage name should mirror the envelope stage");
    require(envelope[0].diagnostic.category == pb::runtime::error_category::stage_failure,
            "failed branch diagnostics should lift to stage_failure category");
    require(envelope[0].diagnostic.message.empty(),
            "empty failed branch diagnostic should remain empty in the envelope");
    require(envelope.to_string() ==
                std::string{"pb.fan_in.errors.v1\nfailures=1\n"} +
                    "case[1] stage=case 1 (fan_in.case.1) :: stage_failure at case 1 (fan_in.case.1)",
            "empty failed branch envelope render mismatch: " + envelope.to_string());
  }

  // --- an all-success aggregate yields an empty envelope. ---
  {
    using Aggregate = pb::fan_in_results<pb::fan_in_case_result<0, Parsed>>;
    Aggregate aggregate;
    aggregate.get<0>().mark_completed(Parsed{7});
    auto envelope = pb::collect_fan_in_errors(aggregate);
    require(!envelope.has_failures(), "all-success aggregate must have no failures");
    require(envelope.empty(), "all-success envelope must be empty");
    require(envelope.size() == 0, "all-success envelope size must be 0");
    require(envelope.to_string() == "pb.fan_in.errors.v1\nfailures=0",
            "empty envelope render mismatch: " + envelope.to_string());
  }

  return 0;
}
