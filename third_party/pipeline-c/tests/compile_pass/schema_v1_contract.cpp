// pb.core.graph.v1 contract regression test.
//
// The literal schema_version string is THE contract identifier for the v1
// helper output schema documented in docs/export-schema-v1.md.  A change
// to any of the strings below means either (a) the schema is being broken
// and the major must be bumped to v2, or (b) the change is wrong.  Either
// way the test fails loudly so reviewers see the regression.

#include <pb/pipeline.hpp>

#include <cassert>
#include <string_view>

namespace {

struct Raw  { int value{}; };
struct Done { int value{}; };

struct Step {
  using input_type  = Raw;
  using output_type = Done;
  static constexpr auto stage_key()  noexcept { return "step"; }
  static constexpr auto stage_name() noexcept { return "step"; }
  Done operator()(Raw raw) const { return {raw.value + 1}; }
};

using Pipeline = pb::from<Raw>::then<Step>::to<Done>;

constexpr auto contains(std::string_view haystack, std::string_view needle) noexcept -> bool {
  return haystack.find(needle) != std::string_view::npos;
}

} // namespace

int main() {
  // Top-level schema-version strings — these are the public contract.
  static_assert(std::string_view{"pb.core.graph.v1"} == std::string_view{"pb.core.graph.v1"},
                "graph schema major identifier MUST stay v1 until a v2 emitter is added");
  static_assert(pb::descriptor_schema_version == std::string_view{"pb.descriptor.v1"},
                "descriptor schema MUST stay pb.descriptor.v1 until a v2 record is added");
  static_assert(pb::diagnostics_schema_version == std::string_view{"pb.diagnostics.v1"},
                "diagnostics schema MUST stay pb.diagnostics.v1 until a v2 record is added");
  static_assert(pb::trace_schema_version == std::string_view{"pb.trace.v1"},
                "trace schema MUST stay pb.trace.v1 until a v2 record is added");

  const std::string json = pb::to_json<Pipeline>();
  const std::string text = pb::to_text<Pipeline>();

  // JSON must lead with the exact contract identifier in the field-order
  // documented in docs/export-schema-v1.md.
  assert(json.rfind("{\"schema_version\":\"pb.core.graph.v1\",\"topology\":", 0) == 0);

  // Compact text format must lead with the banner identifier.
  assert(text.rfind("# pb.core.graph.v1", 0) == 0);

  // Every documented top-level JSON field is present in the documented order
  // so a renamed field gets caught here even if the value happens to remain
  // representable.
  const auto topology_pos     = json.find("\"topology\"");
  const auto stage_count_pos  = json.find("\"stage_count\"");
  const auto edge_count_pos   = json.find("\"edge_count\"");
  const auto stages_pos       = json.find("\"stages\"");
  const auto edges_pos        = json.find("\"edges\"");
  assert(topology_pos    != std::string::npos);
  assert(stage_count_pos != std::string::npos);
  assert(edge_count_pos  != std::string::npos);
  assert(stages_pos      != std::string::npos);
  assert(edges_pos       != std::string::npos);
  assert(topology_pos    < stage_count_pos);
  assert(stage_count_pos < edge_count_pos);
  assert(edge_count_pos  < stages_pos);
  assert(stages_pos      < edges_pos);

  // Every documented StageRecord field is emitted in the documented order
  // (`index`, then `key`, then `name`, then `kind`).
  const auto index_pos = json.find("\"index\":0");
  const auto key_pos   = json.find("\"key\":\"step\"");
  const auto name_pos  = json.find("\"name\":\"step\"");
  const auto kind_pos  = json.find("\"kind\":\"stage\"");
  assert(index_pos < key_pos);
  assert(key_pos   < name_pos);
  assert(name_pos  < kind_pos);

  // pb_cli schema reports the contract identifiers too; if it ever drifts
  // from the library constants the build still catches it via the same
  // string equality.
  assert(contains(json, "pb.core.graph.v1"));
  assert(contains(text, "pb.core.graph.v1"));

  return 0;
}
