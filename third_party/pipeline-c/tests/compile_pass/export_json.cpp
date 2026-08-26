#include <pb/pipeline.hpp>

#include <cassert>
#include <string_view>

struct Raw { int value{}; };
struct Routed { int value{}; };
struct Done { int value{}; };

struct IsEven {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.even"; }
  static constexpr auto stage_name() noexcept { return "is even"; }
  bool operator()(Raw raw) const { return raw.value % 2 == 0; }
};

struct IsOdd {
  using input_type = Raw;
  using output_type = bool;
  static constexpr auto stage_key() noexcept { return "is.odd"; }
  static constexpr auto stage_name() noexcept { return "is odd"; }
  bool operator()(Raw raw) const { return raw.value % 2 != 0; }
};

struct EvenRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route.even"; }
  static constexpr auto stage_name() noexcept { return "route even"; }
  Routed operator()(Raw raw) const { return {raw.value * 2}; }
};

struct OddRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route.odd"; }
  static constexpr auto stage_name() noexcept { return "route odd"; }
  Routed operator()(Raw raw) const { return {raw.value * 3}; }
};

struct Finish {
  using input_type = Routed;
  using output_type = Done;
  static constexpr auto stage_key() noexcept { return "finish"; }
  Done operator()(Routed routed) const { return {routed.value + 1}; }
};

struct LinearRoute {
  using input_type = Raw;
  using output_type = Routed;
  static constexpr auto stage_key() noexcept { return "route.linear"; }
  static constexpr auto stage_name() noexcept { return "route linear"; }
  Routed operator()(Raw raw) const { return {raw.value}; }
};

using EvenCase = pb::case_<IsEven>::then<EvenRoute>;
using OddCase = pb::case_<IsOdd>::then<OddRoute>;
using Pipeline = pb::from<Raw>::branch<EvenCase, OddCase>::join<Finish>::to<Done>;
using LinearPipeline = pb::from<Raw>::then<LinearRoute>::then<Finish>::to<Done>;

int main() {
  const auto graph = pb::to_json<Pipeline>();
  const auto linear_graph = pb::to_json<LinearPipeline>();

  assert(graph.find("\"schema_version\":\"pb.core.graph.v1\"") != std::string_view::npos);
  assert(graph.find("\"topology\":\"branch\"") != std::string_view::npos);
  assert(graph.find("\"stage_count\":2") != std::string_view::npos);
  assert(graph.find("\"edge_count\":1") != std::string_view::npos);
  assert(graph.find("\"kind\":\"branch\"") != std::string_view::npos);
  assert(graph.find("\"branch_cases\":[") != std::string_view::npos);
  assert(graph.find("\"predicate_key\":\"is.even\"") != std::string_view::npos);
  assert(graph.find("\"stage_key\":\"route.even\"") != std::string_view::npos);
  assert(graph.find("\"predicate_key\":\"is.odd\"") != std::string_view::npos);
  assert(graph.find("\"stage_key\":\"route.odd\"") != std::string_view::npos);
  assert(graph.find("\"kind\":\"stage\"") != std::string_view::npos);
  assert(graph.find("\"from_key\":\"branch\"") != std::string_view::npos);
  assert(graph.find("\"to_key\":\"finish\"") != std::string_view::npos);

  assert(linear_graph.find("\"schema_version\":\"pb.core.graph.v1\"") != std::string_view::npos);
  assert(linear_graph.find("\"topology\":\"linear\"") != std::string_view::npos);
  assert(linear_graph.find("\"kind\":\"branch\"") == std::string_view::npos);

  return 0;
}
