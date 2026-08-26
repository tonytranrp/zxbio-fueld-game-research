#include <pb/pipeline.hpp>

#include <type_traits>

namespace {

struct Input { int route{}; int value{}; };
struct Parsed { int value{}; };
struct Reviewed { int value{}; };
struct Done { int value{}; };

struct IsA {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return true; }
};
struct IsB {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return true; }
};
struct IsC {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return true; }
};

struct ParseA {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input input) const { return Parsed{input.value + 1}; }
};
struct Review {
  using input_type = Input;
  using output_type = Reviewed;
  Reviewed operator()(Input input) const { return Reviewed{input.value + 2}; }
};
struct ParseC {
  using input_type = Input;
  using output_type = Parsed;
  Parsed operator()(Input input) const { return Parsed{input.value + 3}; }
};

using CaseA = pb::case_<IsA>::then<ParseA>;
using CaseB = pb::case_<IsB>::then<Review>;
using CaseC = pb::case_<IsC>::then<ParseC>;
using FanInInput = pb::fan_in_results<pb::fan_in_case_result<0, Parsed>,
                                       pb::fan_in_case_result<1, Reviewed>,
                                       pb::fan_in_case_result<2, Parsed>>;

struct FanInJoin {
  using input_type = FanInInput;
  using output_type = Done;
  Done operator()(FanInInput input) const {
    int total = 0;
    if (input.template get<0>().has_value()) total += input.template get<0>().get().value;
    if (input.template get<1>().has_value()) total += input.template get<1>().get().value;
    if (input.template get<2>().has_value()) total += input.template get<2>().get().value;
    return Done{total};
  }
};

using ExpectedFanIn = pb::fan_in_output_t<CaseA, CaseB, CaseC>;
using Pipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::fan_in<FanInJoin>::to<Done>;
using AliasPipeline = pb::from<Input>::branch<CaseA, CaseB, CaseC>::join_all<FanInJoin>::to<Done>;

static_assert(std::same_as<ExpectedFanIn, FanInInput>);
static_assert(FanInInput::case_count == 3);
static_assert(std::same_as<decltype(std::declval<FanInInput&>().template get<0>()),
                           pb::fan_in_case_result<0, Parsed>&>);
static_assert(std::same_as<decltype(std::declval<FanInInput&>().template get<2>()),
                           pb::fan_in_case_result<2, Parsed>&>);
static_assert(pb::valid<Pipeline>);
static_assert(pb::valid<AliasPipeline>);
static_assert(std::same_as<pb::pipeline_output_t<Pipeline>, Done>);
static_assert(std::same_as<pb::pipeline_output_t<AliasPipeline>, Done>);

} // namespace

int main() { return 0; }
