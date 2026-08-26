#include <pb/pipeline.hpp>
#include <pb/tooling/cli_registry.hpp>

#include <cstdlib>
#include <string>
#include <string_view>

namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

bool contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
}

struct In {
  int value{};
};

struct Mid {
  int value{};
};

struct Out {
  int value{};
};

struct Parse {
  using input_type = In;
  using output_type = Mid;
  static constexpr auto stage_key() noexcept { return "tooling.parse"; }
  static constexpr auto stage_name() noexcept { return "tooling parse"; }
  Mid operator()(In input) const { return Mid{input.value + 1}; }
};

struct Finish {
  using input_type = Mid;
  using output_type = Out;
  static constexpr auto stage_key() noexcept { return "tooling.finish"; }
  static constexpr auto stage_name() noexcept { return "tooling finish"; }
  Out operator()(Mid mid) const { return Out{mid.value * 2}; }
};

struct FinishOnly {
  using input_type = In;
  using output_type = Out;
  static constexpr auto stage_key() noexcept { return "tooling.only"; }
  static constexpr auto stage_name() noexcept { return "tooling only"; }
  Out operator()(In input) const { return Out{input.value}; }
};

using SmokePipeline = pb::from<In>::then<Parse>::then<Finish>::to<Out>;
using ExplicitGraphPipeline = pb::from<In>::then<FinishOnly>::to<Out>;
static_assert(pb::valid<SmokePipeline>);
static_assert(pb::valid<ExplicitGraphPipeline>);
} // namespace

int main() {
  pb::tooling::pipeline_registry registry;
  pb_test_require(registry.size() == 0);
  pb_test_require(!registry.contains("tooling-smoke"));
  pb_test_require(registry.find("tooling-smoke") == nullptr);
  pb_test_require(registry.render("tooling-smoke", "dot").empty());

  registry.add<SmokePipeline>("tooling-smoke", "linear", "Tooling registry smoke.");
  registry.add<ExplicitGraphPipeline>("custom-name", "linear", "Explicit graph id.",
                                      "custom_graph_id");
  registry.add<ExplicitGraphPipeline>("tooling-smoke", "duplicate",
                                      "Duplicate names stay insertion-ordered.",
                                      "duplicate_graph_id");

  pb_test_require(registry.size() == 3);
  pb_test_require(registry.contains("tooling-smoke"));
  pb_test_require(registry.contains("custom-name"));
  pb_test_require(!registry.contains("missing"));

  const auto& entries = registry.entries();
  pb_test_require(entries.size() == 3);
  pb_test_require(entries[0].name == "tooling-smoke");
  pb_test_require(entries[0].topology == "linear");
  pb_test_require(entries[0].description == "Tooling registry smoke.");
  pb_test_require(entries[0].graph_name == "tooling_smoke");
  pb_test_require(entries[1].name == "custom-name");
  pb_test_require(entries[1].graph_name == "custom_graph_id");
  pb_test_require(entries[2].name == "tooling-smoke");
  pb_test_require(entries[2].topology == "duplicate");
  pb_test_require(entries[2].description == "Duplicate names stay insertion-ordered.");
  pb_test_require(entries[2].graph_name == "duplicate_graph_id");

  const auto* found = registry.find("tooling-smoke");
  pb_test_require(found != nullptr);
  pb_test_require(found == &entries[0]);
  pb_test_require(found->name == "tooling-smoke");

  const std::string dot = registry.render("tooling-smoke", "dot");
  pb_test_require(contains(dot, "digraph tooling_smoke"));
  pb_test_require(contains(dot, "tooling.parse"));
  pb_test_require(contains(dot, "tooling.finish"));

  const std::string direct_dot = found->to_dot("direct_graph_id");
  pb_test_require(contains(direct_dot, "digraph direct_graph_id"));
  pb_test_require(contains(direct_dot, "tooling.parse"));

  const std::string duplicate_dot = entries[2].to_dot("duplicate_direct_graph");
  pb_test_require(contains(duplicate_dot, "digraph duplicate_direct_graph"));
  pb_test_require(contains(duplicate_dot, "tooling.only"));

  const std::string explicit_dot = registry.render("custom-name", "dot");
  pb_test_require(contains(explicit_dot, "digraph custom_graph_id"));

  const std::string json = registry.render("tooling-smoke", "json");
  pb_test_require(contains(json, "\"schema_version\":\"pb.core.graph.v1\""));
  pb_test_require(contains(json, "\"topology\":\"linear\""));
  pb_test_require(contains(found->to_json(), "\"stage_count\":2"));

  const std::string text = registry.render("tooling-smoke", "text");
  pb_test_require(contains(text, "# pb.core.graph.v1"));
  pb_test_require(contains(text, "topology=linear"));
  pb_test_require(contains(found->to_text(), "[1] tooling.finish"));

  pb_test_require(registry.render("missing", "dot").empty());
  pb_test_require(registry.render("tooling-smoke", "xml").empty());

  return 0;
}
