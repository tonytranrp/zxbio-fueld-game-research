#include <pb/pipeline.hpp>

#include <type_traits>
#include <variant>

namespace {

struct Input { int route{}; };
struct Parsed { int value{}; };
struct Reviewed { int value{}; };
struct Done { int value{}; };

struct IsFirst {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return input.route == 0; }
};

struct IsSecond {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return input.route == 1; }
};

struct IsThird {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input& input) const { return input.route == 2; }
};

struct ParseA {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input input) const { return Parsed{input.route + 10}; }
};

struct Review {
  using input_type = Input;
  using output_type = Reviewed;
  Reviewed operator()(Input input) const { return Reviewed{input.route + 20}; }
};

struct ParseB {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input input) const { return Parsed{input.route + 30}; }
};

struct TypeListJoin {
  using input_type = pb::meta::type_list<Parsed, Reviewed, Parsed>;
  using output_type = Done;

  Done operator()(Parsed parsed) const { return Done{parsed.value}; }
  Done operator()(Reviewed reviewed) const { return Done{reviewed.value}; }
};

using CaseA = pb::case_<IsFirst>::then<ParseA>;
using CaseB = pb::case_<IsSecond>::then<Review>;
using CaseC = pb::case_<IsThird>::then<ParseB>;
using Outputs = pb::branch_outputs<CaseA, CaseB, CaseC>;
using Join = pb::join_node<TypeListJoin>;
using JoinValidation = pb::join_validation<Outputs, Join>;
using JoinBuilderValidation = pb::join_builder_validation<Outputs, TypeListJoin>;
using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::join<TypeListJoin>::to<Done>;

static_assert(pb::valid<Pipeline>);
static_assert(std::is_same_v<Outputs::output_types, pb::meta::type_list<Parsed, Reviewed, Parsed>>);
static_assert(std::is_same_v<Outputs::output_type, std::variant<Parsed, Reviewed, Parsed>>);
static_assert(std::is_same_v<JoinValidation::input_type, pb::meta::type_list<Parsed, Reviewed, Parsed>>);
static_assert(std::is_same_v<JoinValidation::execution_input_type, std::variant<Parsed, Reviewed, Parsed>>);
static_assert(JoinValidation::accepts_raw_type_list);
static_assert(!JoinValidation::accepts_unified_output);
static_assert(JoinBuilderValidation::accepts_raw_type_list);
static_assert(std::is_same_v<pb::pipeline_output_t<Pipeline>, Done>);

} // namespace

int main() { return 0; }
