#include <pb/pipeline.hpp>

#include <array>
#include <string_view>
#include <type_traits>

struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Done { int value{}; };

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  struct error_type {};
  static constexpr auto key = pb::fixed_string{"order.parse"};
  static constexpr auto name = pb::fixed_string{"parse"};
  Parsed operator()(Raw raw) const { return {raw.value + 1}; }
};

struct Finish {
  using input_type = Parsed;
  using output_type = Done;
  static constexpr auto stage_name() noexcept { return "finish"; }
  Done operator()(Parsed parsed) const { return {parsed.value * 2}; }
};

using Pipeline = pb::from<Raw>::then<Parse>::then<Finish>::to<Done>;
using EmptyPipeline = pb::from<Raw>::to<Raw>;
using Traits = pb::pipeline_traits<Pipeline>;
using ParseDescriptor = pb::stage_descriptor<0, Parse>;
using ParseToFinishEdge = pb::edge_descriptor<0, Parse, Finish>;
using PipelineDescriptorView = pb::pipeline_descriptor_view<2>;

static_assert(pb::valid<Pipeline>);
static_assert(Traits::stage_count == 2);
static_assert(pb::pipeline_descriptor<Pipeline>::edge_count == 1);
static_assert(Traits::edge_count == 1);
static_assert(pb::pipeline_edge_count_v<Pipeline> == 1);
static_assert(pb::pipeline_edge_count_v<EmptyPipeline> == 0);
static_assert(pb::pipeline_descriptor<EmptyPipeline>::edge_count == 0);
static_assert(pb::pipeline_size_v<Pipeline> == 2);
static_assert(!pb::pipeline_empty_v<Pipeline>);
static_assert(pb::pipeline_empty_v<EmptyPipeline>);
static_assert(!Traits::empty);
static_assert(std::same_as<Traits::input_type, Raw>);
static_assert(std::same_as<Traits::output_type, Done>);
static_assert(std::same_as<pb::pipeline_input_t<Pipeline>, Raw>);
static_assert(std::same_as<pb::pipeline_output_t<Pipeline>, Done>);
static_assert(std::same_as<pb::pipeline_stages_t<Pipeline>, pb::meta::type_list<Parse, Finish>>);
static_assert(std::same_as<pb::pipeline_stages_t<EmptyPipeline>, pb::meta::type_list<>>);
static_assert(pb::pipeline_has_stage_v<Pipeline, Parse>);
static_assert(pb::pipeline_has_stage_v<Pipeline, Finish>);
static_assert(!pb::pipeline_has_stage_v<Pipeline, Raw>);
static_assert(!pb::pipeline_has_stage_v<EmptyPipeline, Raw>);
static_assert(!pb::pipeline_has_stage_v<EmptyPipeline, Parse>);
static_assert(std::same_as<Traits::stage_type<0>, Parse>);
static_assert(std::same_as<pb::pipeline_stage_t<Pipeline, 0>, Parse>);
static_assert(std::same_as<pb::pipeline_stage_descriptor_t<Pipeline, 1>, Traits::stage<1>>);
static_assert(std::same_as<pb::pipeline_edge_descriptor_t<Pipeline, 0>, Traits::edge<0>>);
static_assert(std::same_as<pb::pipeline_edge_from_stage_t<Pipeline, 0>, Parse>);
static_assert(std::same_as<pb::pipeline_edge_to_stage_t<Pipeline, 0>, Finish>);
static_assert(std::same_as<Traits::stage<1>::input_type, Parsed>);
static_assert(ParseDescriptor::index == 0);
static_assert(ParseToFinishEdge::index == 0);
static_assert(ParseToFinishEdge::from_stage_index == 0);
static_assert(ParseToFinishEdge::to_stage_index == 1);
static_assert(std::same_as<ParseToFinishEdge::from_stage_type, Parse>);
static_assert(std::same_as<ParseToFinishEdge::to_stage_type, Finish>);
static_assert(std::same_as<ParseToFinishEdge::from_output_type, Parsed>);
static_assert(std::same_as<ParseToFinishEdge::to_input_type, Parsed>);
static_assert(ParseToFinishEdge::from_key() == std::string_view{"order.parse"});
static_assert(ParseToFinishEdge::from_name() == std::string_view{"parse"});
static_assert(ParseToFinishEdge::to_key() == std::string_view{"finish"});
static_assert(ParseToFinishEdge::to_name() == std::string_view{"finish"});
static_assert(pb::edge_from_key<Pipeline, 0>() == std::string_view{"order.parse"});
static_assert(pb::edge_from_name<Pipeline, 0>() == std::string_view{"parse"});
static_assert(pb::edge_to_key<Pipeline, 0>() == std::string_view{"finish"});
static_assert(pb::edge_to_name<Pipeline, 0>() == std::string_view{"finish"});
static_assert(std::same_as<ParseDescriptor::error_type, Parse::error_type>);
static_assert(std::same_as<pb::pipeline_stage_error_t<Pipeline, 0>, Parse::error_type>);
static_assert(std::same_as<pb::pipeline_stage_error_t<Pipeline, 1>, pb::no_error>);
static_assert(ParseDescriptor::key() == std::string_view{"order.parse"});
static_assert(ParseDescriptor::name() == std::string_view{"parse"});
static_assert(std::same_as<decltype(pb::descriptor_view<Pipeline>()), PipelineDescriptorView>);
static_assert(pb::descriptor_view<Pipeline>().stage_records()[0].index == pb::describe<Pipeline>().stage_records()[0].index);
static_assert(pb::descriptor_view<Pipeline>().stage_records()[0].key == pb::describe<Pipeline>().stage_records()[0].key);
static_assert(pb::descriptor_view<Pipeline>().stage_records()[0].name == pb::describe<Pipeline>().stage_records()[0].name);
static_assert(pb::descriptor_view<Pipeline>().stage_records()[1].index == pb::describe<Pipeline>().stage_records()[1].index);
static_assert(pb::descriptor_view<Pipeline>().stage_records()[1].key == pb::describe<Pipeline>().stage_records()[1].key);
static_assert(pb::descriptor_view<Pipeline>().stage_records()[1].name == pb::describe<Pipeline>().stage_records()[1].name);
static_assert(pb::descriptor_view<Pipeline>().edge_records()[0].index == pb::describe<Pipeline>().edge_records()[0].index);
static_assert(pb::descriptor_view<Pipeline>().edge_records()[0].from_key ==
              pb::describe<Pipeline>().edge_records()[0].from_key);
static_assert(pb::descriptor_view<Pipeline>().edge_records()[0].to_key ==
              pb::describe<Pipeline>().edge_records()[0].to_key);
static_assert(pb::describe<Pipeline>().stage_count == 2);
static_assert(pb::describe<Pipeline>().edge_count == 1);
static_assert(pb::describe<Pipeline>().stage_key<0>() == std::string_view{"order.parse"});
static_assert(pb::describe<Pipeline>().stage_key<1>() == std::string_view{"finish"});
static_assert(pb::describe<Pipeline>().stage_name<0>() == std::string_view{"parse"});
static_assert(pb::describe<Pipeline>().stage_name<1>() == std::string_view{"finish"});
static_assert(pb::describe<EmptyPipeline>().edge_records().empty());
static_assert(pb::descriptor_view<EmptyPipeline>().edge_records().empty());

int main() {
  constexpr auto desc = pb::describe<Pipeline>();
  constexpr auto keys = desc.stage_keys();
  constexpr auto names = desc.stage_names();
  constexpr auto records = desc.stage_records();
  constexpr auto edges = desc.edge_records();
  static_assert(keys.size() == 2);
  static_assert(keys[0] == std::string_view{"order.parse"});
  static_assert(keys[1] == std::string_view{"finish"});
  static_assert(names.size() == 2);
  static_assert(names[0] == std::string_view{"parse"});
  static_assert(names[1] == std::string_view{"finish"});
  static_assert(records.size() == 2);
  static_assert(records[0].index == 0);
  static_assert(records[0].key == std::string_view{"order.parse"});
  static_assert(records[0].name == std::string_view{"parse"});
  static_assert(records[1].index == 1);
  static_assert(records[1].key == std::string_view{"finish"});
  static_assert(records[1].name == std::string_view{"finish"});
  static_assert(edges.size() == 1);
  static_assert(edges[0].index == 0);
  static_assert(edges[0].from_stage_index == 0);
  static_assert(edges[0].to_stage_index == 1);
  static_assert(edges[0].from_key == std::string_view{"order.parse"});
  static_assert(edges[0].from_name == std::string_view{"parse"});
  static_assert(edges[0].to_key == std::string_view{"finish"});
  static_assert(edges[0].to_name == std::string_view{"finish"});
  return names == std::array<std::string_view, 2>{"parse", "finish"} ? 0 : 1;
}
