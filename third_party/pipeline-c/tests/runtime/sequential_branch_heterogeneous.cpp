/// @file  sequential_branch_heterogeneous.cpp
/// @brief Runtime tests for heterogeneous branch execution with std::variant join.
///
/// Validates end-to-end execution of pipelines where branch cases produce
/// different output types that are unified via std::variant and a visit-
/// based join stage.

#include <pb/pipeline.hpp>

#include <cassert>
#include <string>
#include <variant>

// =====================================================================
// Test 1: Two-case heterogeneous branch with variant join
// =====================================================================

namespace test1 {

struct Input {
  int value{};
};

struct Parsed {
  std::string text{};

  bool operator==(const Parsed&) const = default;
};

struct Reviewed {
  int score{};

  bool operator==(const Reviewed&) const = default;
};

struct Finalized {
  std::string result{};

  bool operator==(const Finalized&) const = default;
};

// --- Predicates ---

struct IsText {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-text"; }
  static constexpr auto stage_key() noexcept { return "is-text"; }

  bool operator()(Input in) const { return in.value > 0; }
};

struct IsNumeric {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_name() noexcept { return "is-numeric"; }
  static constexpr auto stage_key() noexcept { return "is-numeric"; }

  bool operator()(Input in) const { return in.value <= 0; }
};

// --- Branch route stages (heterogeneous outputs) ---

struct ParseText {
  using input_type = Input;
  using output_type = Parsed;
  static constexpr auto stage_name() noexcept { return "parse-text"; }
  static constexpr auto stage_key() noexcept { return "parse-text"; }

  Parsed operator()(Input in) const {
    return Parsed{"parsed:" + std::to_string(in.value)};
  }
};

struct ReviewNumeric {
  using input_type = Input;
  using output_type = Reviewed;
  static constexpr auto stage_name() noexcept { return "review-numeric"; }
  static constexpr auto stage_key() noexcept { return "review-numeric"; }

  Reviewed operator()(Input in) const {
    return Reviewed{in.value * 10};
  }
};

// --- Join stage consumes std::variant<Parsed, Reviewed> ---

struct FinalizeJoin {
  using input_type = std::variant<Parsed, Reviewed>;
  using output_type = Finalized;
  static constexpr auto stage_name() noexcept { return "finalize-join"; }
  static constexpr auto stage_key() noexcept { return "finalize-join"; }

  Finalized operator()(std::variant<Parsed, Reviewed> var) const {
    return std::visit([](auto&& arg) -> Finalized {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, Parsed>) {
        return Finalized{"finalized-text: " + arg.text};
      } else {
        return Finalized{"finalized-score: " + std::to_string(arg.score)};
      }
    }, var);
  }
};

using TextCase = pb::case_<IsText>::then<ParseText>;
using NumCase = pb::case_<IsNumeric>::then<ReviewNumeric>;
using Pipeline = pb::from<Input>::branch<TextCase, NumCase>::join<FinalizeJoin>::to<Finalized>;

static_assert(pb::valid<Pipeline>);
static_assert(pb::pipeline_size_v<Pipeline> == 2);
static_assert(std::is_same_v<pb::pipeline_input_t<Pipeline>, Input>);
static_assert(std::is_same_v<pb::pipeline_output_t<Pipeline>, Finalized>);

void test_text_path() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto result = engine.run(Input{42});
  assert(result.has_value());
  assert(result.value().result == "finalized-text: parsed:42");
}

void test_numeric_path() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto result = engine.run(Input{-5});
  assert(result.has_value());
  assert(result.value().result == "finalized-score: -50");
}

void test_zero_boundary() {
  // value <= 0 routes to numeric path
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto result = engine.run(Input{0});
  assert(result.has_value());
  assert(result.value().result == "finalized-score: 0");
}

} // namespace test1

// =====================================================================
// Test 2: Three-case heterogeneous branch with variant join
// =====================================================================

namespace test2 {

struct Request {
  std::string type;
  int payload{};
};

struct TextResult {
  std::string output{};

  bool operator==(const TextResult&) const = default;
};

struct NumResult {
  int output{};

  bool operator==(const NumResult&) const = default;
};

struct ErrorResult {
  std::string message{};

  bool operator==(const ErrorResult&) const = default;
};

// --- Predicates ---

struct IsText {
  using input_type = Request;
  using output_type = bool;
  bool operator()(const Request& r) const { return r.type == "text"; }
};

struct IsNumber {
  using input_type = Request;
  using output_type = bool;
  bool operator()(const Request& r) const { return r.type == "number"; }
};

struct IsError {
  using input_type = Request;
  using output_type = bool;
  bool operator()(const Request& r) const { return r.type == "error"; }
};

// --- Branch routes with heterogeneous outputs ---

struct ProcessText {
  using input_type = Request;
  using output_type = TextResult;
  TextResult operator()(const Request& r) const {
    return TextResult{"TEXT:" + std::to_string(r.payload)};
  }
};

struct ProcessNumber {
  using input_type = Request;
  using output_type = NumResult;
  NumResult operator()(const Request& r) const {
    return NumResult{r.payload * 100};
  }
};

struct ProcessError {
  using input_type = Request;
  using output_type = ErrorResult;
  ErrorResult operator()(const Request& r) const {
    return ErrorResult{"ERROR:" + std::to_string(r.payload)};
  }
};

// --- Join stage consuming std::variant<TextResult, NumResult, ErrorResult> ---

struct Unify {
  using input_type = std::variant<TextResult, NumResult, ErrorResult>;
  using output_type = std::string;

  std::string operator()(std::variant<TextResult, NumResult, ErrorResult> var) const {
    return std::visit([](auto&& arg) -> std::string {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, TextResult>) {
        return arg.output;
      } else if constexpr (std::is_same_v<T, NumResult>) {
        return "NUM:" + std::to_string(arg.output);
      } else {
        return arg.message;
      }
    }, var);
  }
};

using TextCase = pb::case_<IsText>::then<ProcessText>;
using NumCase = pb::case_<IsNumber>::then<ProcessNumber>;
using ErrorCase = pb::case_<IsError>::then<ProcessError>;
using Pipeline = pb::from<Request>::branch<TextCase, NumCase, ErrorCase>::join<Unify>::to<std::string>;

static_assert(pb::valid<Pipeline>);
static_assert(pb::pipeline_size_v<Pipeline> == 2);

void test_text_routing() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto result = engine.run(Request{"text", 42});
  assert(result.has_value());
  assert(result.value() == "TEXT:42");
}

void test_number_routing() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto result = engine.run(Request{"number", 7});
  assert(result.has_value());
  assert(result.value() == "NUM:700");
}

void test_error_routing() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto result = engine.run(Request{"error", 500});
  assert(result.has_value());
  assert(result.value() == "ERROR:500");
}

} // namespace test2

// =====================================================================
// Test 3: Heterogeneous branch with duplicate output alternatives
// =====================================================================

namespace test3 {

struct Request {
  int route{};
  int payload{};
};

struct Parsed {
  int value{};
};

struct Reviewed {
  int score{};
};

struct IsFastParse {
  using input_type = Request;
  using output_type = bool;
  bool operator()(const Request& request) const { return request.route == 0; }
};

struct NeedsReview {
  using input_type = Request;
  using output_type = bool;
  bool operator()(const Request& request) const { return request.route == 1; }
};

struct IsFallbackParse {
  using input_type = Request;
  using output_type = bool;
  bool operator()(const Request& request) const { return request.route == 2; }
};

struct FastParse {
  using input_type = Request;
  using output_type = Parsed;
  Parsed operator()(const Request& request) const { return Parsed{request.payload + 10}; }
};

struct Review {
  using input_type = Request;
  using output_type = Reviewed;
  Reviewed operator()(const Request& request) const { return Reviewed{request.payload + 20}; }
};

struct FallbackParse {
  using input_type = Request;
  using output_type = Parsed;
  Parsed operator()(const Request& request) const { return Parsed{request.payload + 30}; }
};

using OutputVariant = std::variant<Parsed, Reviewed, Parsed>;

struct JoinDuplicateOutputs {
  using input_type = OutputVariant;
  using output_type = std::string;

  std::string operator()(OutputVariant output) const {
    switch (output.index()) {
    case 0:
      return "fast-parse:" + std::to_string(std::get<0>(output).value);
    case 1:
      return "review:" + std::to_string(std::get<1>(output).score);
    case 2:
      return "fallback-parse:" + std::to_string(std::get<2>(output).value);
    default:
      return "unknown";
    }
  }
};

using FastCase = pb::case_<IsFastParse>::then<FastParse>;
using ReviewCase = pb::case_<NeedsReview>::then<Review>;
using FallbackCase = pb::case_<IsFallbackParse>::then<FallbackParse>;
using Outputs = pb::branch_outputs<FastCase, ReviewCase, FallbackCase>;
using Pipeline = pb::from<Request>::branch<FastCase, ReviewCase, FallbackCase>::join<JoinDuplicateOutputs>::to<std::string>;

static_assert(std::is_same_v<Outputs::output_types, pb::meta::type_list<Parsed, Reviewed, Parsed>>);
static_assert(std::is_same_v<Outputs::output_type, OutputVariant>);
static_assert(std::is_same_v<pb::branch_unified_output_t<FastCase, ReviewCase, FallbackCase>, OutputVariant>);
static_assert(pb::valid<Pipeline>);

void test_duplicate_first_parsed_route() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto result = engine.run(Request{0, 1});
  assert(result.has_value());
  assert(result.value() == "fast-parse:11");
}

void test_duplicate_review_route() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto result = engine.run(Request{1, 2});
  assert(result.has_value());
  assert(result.value() == "review:22");
}

void test_duplicate_second_parsed_route() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto result = engine.run(Request{2, 3});
  assert(result.has_value());
  assert(result.value() == "fallback-parse:33");
}

} // namespace test3

// =====================================================================
// Test 4: Heterogeneous branch with stateful sequential policy
// =====================================================================

namespace test4 {

struct Input {
  int value{};
};

struct StringOut {
  std::string text{};

  bool operator==(const StringOut&) const = default;
};

struct IntOut {
  int number{};

  bool operator==(const IntOut&) const = default;
};

struct IsPositive {
  using input_type = Input;
  using output_type = bool;
  bool operator()(Input in) const { return in.value > 0; }
};

struct IsNonPositive {
  using input_type = Input;
  using output_type = bool;
  bool operator()(Input in) const { return in.value <= 0; }
};

struct MakeString {
  using input_type = Input;
  using output_type = StringOut;

  int counter = 0;

  StringOut operator()(Input in) {
    ++counter;
    return StringOut{"str:" + std::to_string(in.value) + ":" + std::to_string(counter)};
  }
};

struct MakeInt {
  using input_type = Input;
  using output_type = IntOut;

  int counter = 0;

  IntOut operator()(Input in) {
    ++counter;
    return IntOut{in.value * (10 + counter)};
  }
};

struct JoinVariant {
  using input_type = std::variant<StringOut, IntOut>;
  using output_type = std::string;

  std::string operator()(std::variant<StringOut, IntOut> var) const {
    return std::visit([](auto&& arg) -> std::string {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, StringOut>) {
        return arg.text;
      } else {
        return std::to_string(arg.number);
      }
    }, var);
  }
};

using PosCase = pb::case_<IsPositive>::then<MakeString>;
using NonPosCase = pb::case_<IsNonPositive>::then<MakeInt>;
using Pipeline = pb::from<Input>::branch<PosCase, NonPosCase>::join<JoinVariant>::to<std::string>;

static_assert(pb::valid<Pipeline>);

void test_stateful_heterogeneous_branch() {
  auto engine = pb::compile<Pipeline>(pb::runtime::stateful_sequential{});

  // First run: positive → MakeString (counter=1)
  auto r1 = engine.run(Input{5});
  assert(r1.has_value());
  assert(r1.value() == "str:5:1");

  // Second run: non-positive → MakeInt (counter=1)
  auto r2 = engine.run(Input{-3});
  assert(r2.has_value());
  assert(r2.value() == "-33");  // -3 * (10 + 1) = -33

  // Third run: positive again → MakeString (counter=2)
  auto r3 = engine.run(Input{5});
  assert(r3.has_value());
  assert(r3.value() == "str:5:2");

  // Fourth run: non-positive again → MakeInt (counter=2)
  auto r4 = engine.run(Input{-3});
  assert(r4.has_value());
  assert(r4.value() == "-36");  // -3 * (10 + 2) = -36
}

} // namespace test4

// =====================================================================
// main
// =====================================================================

int main() {
  test1::test_text_path();
  test1::test_numeric_path();
  test1::test_zero_boundary();
  test2::test_text_routing();
  test2::test_number_routing();
  test2::test_error_routing();
  test3::test_duplicate_first_parsed_route();
  test3::test_duplicate_review_route();
  test3::test_duplicate_second_parsed_route();
  test4::test_stateful_heterogeneous_branch();
  return 0;
}
