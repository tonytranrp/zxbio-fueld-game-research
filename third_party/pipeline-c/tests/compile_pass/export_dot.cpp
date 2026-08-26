#include <pb/pipeline.hpp>

#include <cassert>
#include <string_view>

struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Done { int value{}; };

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  static constexpr auto stage_key() noexcept { return "order.parse"; }
  static constexpr auto name = pb::fixed_string{"parse"};
  Parsed operator()(Raw input) const { return {input.value + 1}; }
};

struct Finish {
  using input_type = Parsed;
  using output_type = Done;
  static constexpr auto stage_name() noexcept { return "finish"; }
  Done operator()(Parsed input) const { return {input.value * 2}; }
};

using Pipeline = pb::from<Raw>::then<Parse>::then<Finish>::to<Done>;

int main() {
  const auto dot = pb::to_dot<Pipeline>("order_pipeline");

  assert(dot.find("digraph order_pipeline") != std::string_view::npos);
  assert(dot.find("  stage_0 [label=\"parse\\n(order.parse)\"]") != std::string_view::npos);
  assert(dot.find("  stage_1 [label=\"finish\"]") != std::string_view::npos);
  assert(dot.find("  stage_0 -> stage_1 [label=\"order.parse -> finish\"]") != std::string_view::npos);
  return 0;
}
