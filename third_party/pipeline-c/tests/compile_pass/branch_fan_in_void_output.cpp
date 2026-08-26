#include <pb/pipeline.hpp>

#include <type_traits>
#include <utility>

struct Input {};
struct Done { int completed{}; };

struct Always {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return true; }
};

struct VoidStage {
  using input_type = Input;
  using output_type = void;
  void operator()(Input) const {}
};

using Case = pb::case_<Always>::then<VoidStage>;
using FanInInput = pb::fan_in_output_t<Case>;
struct Join {
  using input_type = FanInInput;
  using output_type = Done;
  Done operator()(const FanInInput& input) const {
    return Done{input.template get<0>().completed() ? 1 : 0};
  }
};

using Pipeline = pb::from<Input>::branch<Case>::fan_in<Join>::to<Done>;
static_assert(pb::valid<Pipeline>);
static_assert(FanInInput::case_count == 1);
using VoidCaseResult = std::remove_reference_t<decltype(std::declval<FanInInput&>().template get<0>())>;
static_assert(std::is_void_v<typename VoidCaseResult::value_type>);

int main() { return 0; }
