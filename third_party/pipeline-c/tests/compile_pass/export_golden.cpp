#include <pb/pipeline.hpp>

#include <string>
#include <string_view>
#include <variant>

namespace linear_json_golden {
struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Done { int value{}; };

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "parse"; }
  static constexpr auto stage_name() noexcept { return "Parse"; }
  Parsed operator()(Raw raw) const { return {raw.value + 1}; }
};

struct Finish {
  using input_type = Parsed;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "finish"; }
  static constexpr auto stage_name() noexcept { return "Finish"; }
  Done operator()(Parsed parsed) const { return {parsed.value + 1}; }
};

using Pipeline = pb::from<Raw>::then<Parse>::then<Finish>::to<Done>;

constexpr auto expected =
    "{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"linear\",\"stage_count\":2,\"edge_count\":1,"
    "\"stages\":[{\"index\":0,\"key\":\"parse\",\"name\":\"Parse\",\"kind\":\"stage\"},"
    "{\"index\":1,\"key\":\"finish\",\"name\":\"Finish\",\"kind\":\"stage\"}],"
    "\"edges\":[{\"index\":0,\"from_stage_index\":0,\"to_stage_index\":1,\"from_key\":\"parse\","
    "\"from_name\":\"Parse\",\"to_key\":\"finish\",\"to_name\":\"Finish\"}]}";

constexpr auto expected_dot = R"DOT(digraph linear_golden {
  rankdir=LR;
  node [shape=box];

  stage_0 [label="Parse\n(parse)"];
  stage_1 [label="Finish\n(finish)"];

  stage_0 -> stage_1 [label="parse -> finish"];
}
)DOT";
} // namespace linear_json_golden

namespace homogeneous_branch_json_golden {
struct Raw { int value{}; };
struct Routed { int value{}; };
struct Done { int value{}; };

struct IsEven {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.even"; }
  static constexpr auto stage_name() noexcept { return "IsEven"; }
  bool operator()(Raw raw) const { return raw.value % 2 == 0; }
};

struct IsOdd {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.odd"; }
  static constexpr auto stage_name() noexcept { return "IsOdd"; }
  bool operator()(Raw raw) const { return raw.value % 2 != 0; }
};

struct EvenRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route.even"; }
  static constexpr auto stage_name() noexcept { return "EvenRoute"; }
  Routed operator()(Raw raw) const { return {raw.value * 2}; }
};

struct OddRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route.odd"; }
  static constexpr auto stage_name() noexcept { return "OddRoute"; }
  Routed operator()(Raw raw) const { return {raw.value * 3}; }
};

struct Finish {
  using input_type = Routed;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "finish"; }
  static constexpr auto stage_name() noexcept { return "Finish"; }
  Done operator()(Routed routed) const { return {routed.value + 1}; }
};

using EvenCase = pb::case_<IsEven>::label<"even-case">::then<EvenRoute>;
using OddCase = pb::case_<IsOdd>::then<OddRoute>;
using Pipeline = pb::from<Raw>::branch<EvenCase, OddCase>::join<Finish>::to<Done>;

constexpr auto expected_json =
    "{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"branch\",\"stage_count\":2,\"edge_count\":1,"
    "\"stages\":[{\"index\":0,\"key\":\"branch\",\"name\":\"branch\",\"kind\":\"branch\",\"branch_cases\":["
    "{\"index\":0,\"case_id\":\"branch.0.case.0\",\"case_key\":\"branch.0.case.0\",\"case_label\":\"even-case\","
    "\"predicate_node_id\":\"branch.0.case.0.predicate\",\"stage_node_id\":\"branch.0.case.0.stage\","
    "\"predicate_key\":\"is.even\",\"predicate_name\":\"IsEven\",\"stage_key\":\"route.even\","
    "\"stage_name\":\"EvenRoute\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}},"
    "{\"index\":1,\"case_id\":\"branch.0.case.1\",\"case_key\":\"branch.0.case.1\",\"case_label\":\"1\","
    "\"predicate_node_id\":\"branch.0.case.1.predicate\",\"stage_node_id\":\"branch.0.case.1.stage\","
    "\"predicate_key\":\"is.odd\",\"predicate_name\":\"IsOdd\",\"stage_key\":\"route.odd\","
    "\"stage_name\":\"OddRoute\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}}]},"
    "{\"index\":1,\"key\":\"finish\",\"name\":\"Finish\",\"kind\":\"stage\"}],"
    "\"edges\":[{\"index\":0,\"from_stage_index\":0,\"to_stage_index\":1,\"from_key\":\"branch\","
    "\"from_name\":\"branch\",\"to_key\":\"finish\",\"to_name\":\"Finish\"}]}";

constexpr auto expected_dot = R"DOT(digraph branch_golden {
  rankdir=LR;
  node [shape=box];

  from_input [label="Input"];

  branch_0 [shape=diamond, label="branch"];

  subgraph cluster_case_0_0 {
    label="Case even-case: IsEven";
    pred_0_0 [label="pred: IsEven"];
    case_0_0 [label="EvenRoute"];
    branch_0 -> pred_0_0 [style=dashed, label="test"];
    pred_0_0 -> case_0_0;
  }
  subgraph cluster_case_0_1 {
    label="Case 1: IsOdd";
    pred_0_1 [label="pred: IsOdd"];
    case_0_1 [label="OddRoute"];
    branch_0 -> pred_0_1 [style=dashed, label="test"];
    pred_0_1 -> case_0_1;
  }

  stage_1 [label="Finish\n(finish)"];
  to_output [shape=doublecircle, label="Output"];

  from_input -> branch_0;
  case_0_0 -> stage_1;
  case_0_1 -> stage_1;
  stage_1 -> to_output;
}
)DOT";
} // namespace homogeneous_branch_json_golden

namespace heterogeneous_branch_json_golden {
struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Reviewed { int value{}; };
struct Done { int value{}; };

struct IsParse {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.parse"; }
  static constexpr auto stage_name() noexcept { return "IsParse"; }
  bool operator()(Raw raw) const { return raw.value >= 0; }
};

struct IsReview {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.review"; }
  static constexpr auto stage_name() noexcept { return "IsReview"; }
  bool operator()(Raw raw) const { return raw.value < 0; }
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "route.parse"; }
  static constexpr auto stage_name() noexcept { return "Parse"; }
  Parsed operator()(Raw raw) const { return {raw.value}; }
};

struct Review {
  using input_type = Raw;
  using output_type = Reviewed;
  static constexpr auto stage_key() noexcept { return "route.review"; }
  static constexpr auto stage_name() noexcept { return "Review"; }
  Reviewed operator()(Raw raw) const { return {raw.value}; }
};

struct Join {
  using input_type = std::variant<Parsed, Reviewed>;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "join"; }
  static constexpr auto stage_name() noexcept { return "Join"; }
  Done operator()(std::variant<Parsed, Reviewed>) const { return {}; }
};

using ParseCase = pb::case_<IsParse>::then<Parse>;
using ReviewCase = pb::case_<IsReview>::then<Review>;
using Pipeline = pb::from<Raw>::branch<ParseCase, ReviewCase>::join<Join>::to<Done>;

constexpr auto expected =
    "{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"branch\",\"stage_count\":2,\"edge_count\":1,"
    "\"stages\":[{\"index\":0,\"key\":\"branch\",\"name\":\"branch\",\"kind\":\"branch\",\"branch_cases\":["
    "{\"index\":0,\"case_id\":\"branch.0.case.0\",\"case_key\":\"branch.0.case.0\",\"case_label\":\"0\","
    "\"predicate_node_id\":\"branch.0.case.0.predicate\",\"stage_node_id\":\"branch.0.case.0.stage\","
    "\"predicate_key\":\"is.parse\",\"predicate_name\":\"IsParse\",\"stage_key\":\"route.parse\","
    "\"stage_name\":\"Parse\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}},"
    "{\"index\":1,\"case_id\":\"branch.0.case.1\",\"case_key\":\"branch.0.case.1\",\"case_label\":\"1\","
    "\"predicate_node_id\":\"branch.0.case.1.predicate\",\"stage_node_id\":\"branch.0.case.1.stage\","
    "\"predicate_key\":\"is.review\",\"predicate_name\":\"IsReview\",\"stage_key\":\"route.review\","
    "\"stage_name\":\"Review\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}}]},"
    "{\"index\":1,\"key\":\"join\",\"name\":\"Join\",\"kind\":\"stage\"}],"
    "\"edges\":[{\"index\":0,\"from_stage_index\":0,\"to_stage_index\":1,\"from_key\":\"branch\","
    "\"from_name\":\"branch\",\"to_key\":\"join\",\"to_name\":\"Join\"}]}";
} // namespace heterogeneous_branch_json_golden

namespace type_list_selected_output_json_golden {
struct Input { int route{}; };
struct Parsed { int value{}; };
struct Reviewed { int value{}; };
struct Done { int value{}; };

struct IsFirst {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.first"; }
  static constexpr auto stage_name() noexcept { return "IsFirst"; }
  bool operator()(const Input& input) const { return input.route == 0; }
};

struct IsSecond {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.second"; }
  static constexpr auto stage_name() noexcept { return "IsSecond"; }
  bool operator()(const Input& input) const { return input.route == 1; }
};

struct IsThird {
  using input_type = Input;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.third"; }
  static constexpr auto stage_name() noexcept { return "IsThird"; }
  bool operator()(const Input& input) const { return input.route == 2; }
};

struct ParseA {
  using input_type = Input;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "parse.a"; }
  static constexpr auto stage_name() noexcept { return "ParseA"; }
  Parsed operator()(Input input) const { return Parsed{input.route + 10}; }
};

struct Review {
  using input_type = Input;
  using output_type = Reviewed;
  static constexpr auto stage_key() noexcept { return "review"; }
  static constexpr auto stage_name() noexcept { return "Review"; }
  Reviewed operator()(Input input) const { return Reviewed{input.route + 20}; }
};

struct ParseB {
  using input_type = Input;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "parse.b"; }
  static constexpr auto stage_name() noexcept { return "ParseB"; }
  Parsed operator()(Input input) const { return Parsed{input.route + 30}; }
};

struct TypeListJoin {
  using input_type = pb::meta::type_list<Parsed, Reviewed, Parsed>;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "type.list.join"; }
  static constexpr auto stage_name() noexcept { return "TypeListJoin"; }

  Done operator()(Parsed parsed) const { return Done{parsed.value}; }
  Done operator()(Reviewed reviewed) const { return Done{reviewed.value}; }
};

using CaseA = pb::case_<IsFirst>::then<ParseA>;
using CaseB = pb::case_<IsSecond>::label<"review-case">::then<Review>;
using CaseC = pb::case_<IsThird>::then<ParseB>;
using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::join<TypeListJoin>::to<Done>;

constexpr auto expected =
    "{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"branch\",\"stage_count\":2,\"edge_count\":1,"
    "\"stages\":[{\"index\":0,\"key\":\"branch\",\"name\":\"branch\",\"kind\":\"branch\",\"branch_cases\":["
    "{\"index\":0,\"case_id\":\"branch.0.case.0\",\"case_key\":\"branch.0.case.0\",\"case_label\":\"0\","
    "\"predicate_node_id\":\"branch.0.case.0.predicate\",\"stage_node_id\":\"branch.0.case.0.stage\","
    "\"predicate_key\":\"is.first\",\"predicate_name\":\"IsFirst\",\"stage_key\":\"parse.a\","
    "\"stage_name\":\"ParseA\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}},"
    "{\"index\":1,\"case_id\":\"branch.0.case.1\",\"case_key\":\"branch.0.case.1\",\"case_label\":\"review-case\","
    "\"predicate_node_id\":\"branch.0.case.1.predicate\",\"stage_node_id\":\"branch.0.case.1.stage\","
    "\"predicate_key\":\"is.second\",\"predicate_name\":\"IsSecond\",\"stage_key\":\"review\","
    "\"stage_name\":\"Review\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}},"
    "{\"index\":2,\"case_id\":\"branch.0.case.2\",\"case_key\":\"branch.0.case.2\",\"case_label\":\"2\","
    "\"predicate_node_id\":\"branch.0.case.2.predicate\",\"stage_node_id\":\"branch.0.case.2.stage\","
    "\"predicate_key\":\"is.third\",\"predicate_name\":\"IsThird\",\"stage_key\":\"parse.b\","
    "\"stage_name\":\"ParseB\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}}]},"
    "{\"index\":1,\"key\":\"type.list.join\",\"name\":\"TypeListJoin\",\"kind\":\"stage\"}],"
    "\"edges\":[{\"index\":0,\"from_stage_index\":0,\"to_stage_index\":1,\"from_key\":\"branch\","
    "\"from_name\":\"branch\",\"to_key\":\"type.list.join\",\"to_name\":\"TypeListJoin\"}]}";
} // namespace type_list_selected_output_json_golden

namespace fan_in_json_golden {
struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Reviewed { int value{}; };
struct Done { int value{}; };

struct IsParse {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.parse"; }
  static constexpr auto stage_name() noexcept { return "IsParse"; }
  bool operator()(const Raw&) const { return true; }
};

struct IsReview {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.review"; }
  static constexpr auto stage_name() noexcept { return "IsReview"; }
  bool operator()(const Raw&) const { return true; }
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "fan.parse"; }
  static constexpr auto stage_name() noexcept { return "FanParse"; }
  Parsed operator()(Raw raw) const { return {raw.value}; }
};

struct Review {
  using input_type = Raw;
  using output_type = Reviewed;
  static constexpr auto stage_key() noexcept { return "fan.review"; }
  static constexpr auto stage_name() noexcept { return "FanReview"; }
  Reviewed operator()(Raw raw) const { return {raw.value}; }
};

using ParseCase = pb::case_<IsParse>::label<"parse-case">::then<Parse>;
using ReviewCase = pb::case_<IsReview>::then<Review>;
using JoinInput = pb::fan_in_output_t<ParseCase, ReviewCase>;

struct FanInJoin {
  using input_type = JoinInput;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "fan.join"; }
  static constexpr auto stage_name() noexcept { return "FanInJoin"; }
  Done operator()(const JoinInput&) const { return {}; }
};

using Pipeline = pb::from<Raw>::branch<ParseCase, ReviewCase>::fan_in<FanInJoin>::to<Done>;

constexpr auto expected_json =
    "{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"fan_in\",\"stage_count\":2,\"edge_count\":1,"
    "\"stages\":[{\"index\":0,\"key\":\"fan_in\",\"name\":\"fan_in\",\"kind\":\"fan_in\",\"branch_cases\":["
    "{\"index\":0,\"case_id\":\"branch.0.case.0\",\"case_key\":\"branch.0.case.0\",\"case_label\":\"parse-case\","
    "\"predicate_node_id\":\"branch.0.case.0.predicate\",\"stage_node_id\":\"branch.0.case.0.stage\","
    "\"predicate_key\":\"is.parse\",\"predicate_name\":\"IsParse\",\"stage_key\":\"fan.parse\","
    "\"stage_name\":\"FanParse\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}},"
    "{\"index\":1,\"case_id\":\"branch.0.case.1\",\"case_key\":\"branch.0.case.1\",\"case_label\":\"1\","
    "\"predicate_node_id\":\"branch.0.case.1.predicate\",\"stage_node_id\":\"branch.0.case.1.stage\","
    "\"predicate_key\":\"is.review\",\"predicate_name\":\"IsReview\",\"stage_key\":\"fan.review\","
    "\"stage_name\":\"FanReview\",\"predicate_edge\":{\"from\":\"branch\",\"to\":\"predicate\",\"style\":\"dashed\",\"label\":\"test\"},"
    "\"stage_edge\":{\"from\":\"predicate\",\"to\":\"case_stage\"}}]},"
    "{\"index\":1,\"key\":\"fan.join\",\"name\":\"FanInJoin\",\"kind\":\"stage\"}],"
    "\"edges\":[{\"index\":0,\"from_stage_index\":0,\"to_stage_index\":1,\"from_key\":\"fan_in\","
    "\"from_name\":\"fan_in\",\"to_key\":\"fan.join\",\"to_name\":\"FanInJoin\"}]}";

constexpr auto expected_dot = R"DOT(digraph fan_in_golden {
  rankdir=LR;
  node [shape=box];

  from_input [label="Input"];

  branch_0 [shape=diamond, label="fan_in"];

  subgraph cluster_case_0_0 {
    label="Case parse-case: IsParse";
    pred_0_0 [label="pred: IsParse"];
    case_0_0 [label="FanParse"];
    branch_0 -> pred_0_0 [style=dashed, label="test"];
    pred_0_0 -> case_0_0;
  }
  subgraph cluster_case_0_1 {
    label="Case 1: IsReview";
    pred_0_1 [label="pred: IsReview"];
    case_0_1 [label="FanReview"];
    branch_0 -> pred_0_1 [style=dashed, label="test"];
    pred_0_1 -> case_0_1;
  }

  stage_1 [label="FanInJoin\n(fan.join)"];
  to_output [shape=doublecircle, label="Output"];

  from_input -> branch_0;
  case_0_0 -> stage_1;
  case_0_1 -> stage_1;
  stage_1 -> to_output;
}
)DOT";
} // namespace fan_in_json_golden

namespace dot_escaping {
struct Raw { int value{}; };
struct Routed { int value{}; };
struct Done { int value{}; };

struct WeirdPredicate {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "pred key/with spaces"; }
  static constexpr auto stage_name() noexcept { return "Pred \"quoted\"\\slash\nline"; }
  bool operator()(Raw) const { return true; }
};

struct WeirdRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route key/with spaces"; }
  static constexpr auto stage_name() noexcept { return "Route \"quoted\"\\slash\nline"; }
  Routed operator()(Raw raw) const { return {raw.value}; }
};

struct WeirdFinish {
  using input_type = Routed;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "finish key/with spaces"; }
  static constexpr auto stage_name() noexcept { return "Finish \"quoted\"\\slash\nline"; }
  Done operator()(Routed routed) const { return {routed.value}; }
};

using WeirdCase = pb::case_<WeirdPredicate>::then<WeirdRoute>;
using Pipeline = pb::from<Raw>::branch<WeirdCase>::join<WeirdFinish>::to<Done>;
} // namespace dot_escaping

namespace json_escaping {
struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Done { int value{}; };

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "parse\"key\\slash\b\f\x01"; }
  static constexpr auto stage_name() noexcept { return "Parse\"Name\\slash\n\r\t\x1f"; }
  Parsed operator()(Raw raw) const { return {raw.value}; }
};

struct Finish {
  using input_type = Parsed;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "finish"; }
  static constexpr auto stage_name() noexcept { return "Finish"; }
  Done operator()(Parsed parsed) const { return {parsed.value}; }
};

using Pipeline = pb::from<Raw>::then<Parse>::then<Finish>::to<Done>;

constexpr auto expected_json =
    "{\"schema_version\":\"pb.core.graph.v1\",\"topology\":\"linear\",\"stage_count\":2,\"edge_count\":1,"
    "\"stages\":[{\"index\":0,\"key\":\"parse\\\"key\\\\slash\\b\\f\\u0001\","
    "\"name\":\"Parse\\\"Name\\\\slash\\n\\r\\t\\u001f\",\"kind\":\"stage\"},"
    "{\"index\":1,\"key\":\"finish\",\"name\":\"Finish\",\"kind\":\"stage\"}],"
    "\"edges\":[{\"index\":0,\"from_stage_index\":0,\"to_stage_index\":1,"
    "\"from_key\":\"parse\\\"key\\\\slash\\b\\f\\u0001\","
    "\"from_name\":\"Parse\\\"Name\\\\slash\\n\\r\\t\\u001f\","
    "\"to_key\":\"finish\",\"to_name\":\"Finish\"}]}";
} // namespace json_escaping

namespace {
[[nodiscard]] auto expect_equal(std::string_view actual, std::string_view expected) -> bool {
  return actual == expected;
}

[[nodiscard]] auto expect_contains(std::string_view actual, std::string_view expected) -> bool {
  return actual.find(expected) != std::string_view::npos;
}
} // namespace

int main() {
  if (!expect_equal(pb::to_json<linear_json_golden::Pipeline>(), linear_json_golden::expected)) return 1;
  if (!expect_equal(pb::to_dot<linear_json_golden::Pipeline>("linear_golden"),
                    linear_json_golden::expected_dot)) return 10;
  if (!expect_equal(pb::to_json<homogeneous_branch_json_golden::Pipeline>(),
                    homogeneous_branch_json_golden::expected_json)) return 2;
  if (!expect_equal(pb::to_json<heterogeneous_branch_json_golden::Pipeline>(),
                    heterogeneous_branch_json_golden::expected)) return 3;
  if (!expect_equal(pb::to_json<type_list_selected_output_json_golden::Pipeline>(),
                    type_list_selected_output_json_golden::expected)) return 12;
  if (!expect_equal(pb::to_json<fan_in_json_golden::Pipeline>(), fan_in_json_golden::expected_json)) return 13;
  if (!expect_equal(pb::to_dot<homogeneous_branch_json_golden::Pipeline>("branch_golden"),
                    homogeneous_branch_json_golden::expected_dot)) return 4;
  if (!expect_equal(pb::to_dot<fan_in_json_golden::Pipeline>("fan_in_golden"),
                    fan_in_json_golden::expected_dot)) return 14;

  const auto escaped_dot = pb::to_dot<dot_escaping::Pipeline>("escaping graph");
  if (!expect_contains(escaped_dot, "digraph escaping_graph")) return 5;
  if (!expect_contains(escaped_dot, "label=\"Case 0: Pred \\\"quoted\\\"\\\\slash\\nline\"")) return 6;
  if (!expect_contains(escaped_dot, "pred_0_0 [label=\"pred: Pred \\\"quoted\\\"\\\\slash\\nline\"]")) return 7;
  if (!expect_contains(escaped_dot, "case_0_0 [label=\"Route \\\"quoted\\\"\\\\slash\\nline\"]")) return 8;
  if (!expect_contains(escaped_dot,
                       "stage_1 [label=\"Finish \\\"quoted\\\"\\\\slash\\nline\\n(finish key/with spaces)\"]")) return 9;

  if (!expect_equal(pb::to_json<json_escaping::Pipeline>(), json_escaping::expected_json)) return 11;

  return 0;
}
