#include <pb/pipeline.hpp>

#include <array>
#include <cassert>
#include <stdexcept>
#include <string_view>
#include <type_traits>

struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Done { int value{}; };
struct ParseError {};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  using error_type = ParseError;
  static constexpr auto stage_key() noexcept { return "order.parse"; }
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
using Traits = pb::pipeline_traits<Pipeline>;

static_assert(pb::Stage<Parse>);
static_assert(pb::Connectable<Raw, Parse>);
static_assert(pb::connectable_v<Raw, Parse>);
static_assert(!pb::connectable_v<Parsed, Parse>);
static_assert(pb::AdjacentStages<Parse, Finish>);
static_assert(pb::adjacent_stages_v<Parse, Finish>);
static_assert(!pb::adjacent_stages_v<Finish, Parse>);
static_assert(pb::RunnableStage<Parse, Raw>);
static_assert(pb::runnable_stage_v<Parse, Raw>);
static_assert(!pb::runnable_stage_v<Finish, Raw>);
static_assert(pb::ValidPipeline<Pipeline>);
static_assert(pb::valid<Pipeline>);
static_assert(pb::is_pipeline_v<Pipeline>);
static_assert(std::same_as<pb::stage_info<Parse>::input_type, Raw>);
static_assert(std::same_as<pb::stage_info_t<Parse>, pb::stage_info<Parse>>);
static_assert(std::same_as<pb::stage_input_t<Parse>, Raw>);
static_assert(std::same_as<pb::stage_output_t<Parse>, Parsed>);
static_assert(std::same_as<pb::stage_error_t<Parse>, ParseError>);
static_assert(pb::stage_key<Parse>() == std::string_view{"order.parse"});
static_assert(pb::stage_name<Parse>() == std::string_view{"parse"});
static_assert(pb::stage_key<Pipeline, 0>() == std::string_view{"order.parse"});
static_assert(pb::stage_name<Pipeline, 0>() == std::string_view{"parse"});
static_assert(pb::stage_key<Pipeline, 1>() == std::string_view{"finish"});
static_assert(pb::stage_name<Pipeline, 1>() == std::string_view{"finish"});
static_assert(pb::stage_traits<Parse>::name() == std::string_view{"parse"});
static_assert(pb::stage_traits<Parse>::key() == std::string_view{"order.parse"});
static_assert(Traits::stage_count == 2);
static_assert(pb::pipeline_size_v<Pipeline> == 2);
static_assert(pb::pipeline_edge_count_v<Pipeline> == 1);
static_assert(pb::pipeline_has_stage_v<Pipeline, Parse>);
static_assert(!pb::pipeline_has_stage_v<Pipeline, int>);
static_assert(!pb::pipeline_empty_v<Pipeline>);
static_assert(std::same_as<pb::pipeline_input_t<Pipeline>, Raw>);
static_assert(std::same_as<pb::pipeline_output_t<Pipeline>, Done>);
static_assert(std::same_as<pb::pipeline<Raw, Done, pb::meta::type_list<Parse, Finish>>, Pipeline>);
static_assert(std::same_as<pb::pipeline_state<pb::meta::type_list<>, Raw, Raw>, pb::from<Raw>>);
static_assert(std::same_as<pb::pipeline_stages_t<Pipeline>, pb::meta::type_list<Parse, Finish>>);
static_assert(std::same_as<pb::pipeline_stage_t<Pipeline, 0>, Parse>);
static_assert(std::same_as<pb::pipeline_stage_descriptor_t<Pipeline, 1>, Traits::stage<1>>);
static_assert(std::same_as<pb::pipeline_edge_descriptor_t<Pipeline, 0>, Traits::edge<0>>);
static_assert(std::same_as<pb::pipeline_edge_from_stage_t<Pipeline, 0>, Parse>);
static_assert(std::same_as<pb::pipeline_edge_to_stage_t<Pipeline, 0>, Finish>);
static_assert(std::same_as<pb::pipeline_stage_error_t<Pipeline, 0>, ParseError>);
static_assert(std::same_as<pb::pipeline_stage_error_t<Pipeline, 1>, pb::no_error>);
static_assert(std::same_as<pb::stage_descriptor<0, Parse>::input_type, Raw>);
static_assert(std::same_as<pb::stage_descriptor<0, Parse>::error_type, ParseError>);
static_assert(std::same_as<pb::edge_descriptor<0, Parse, Finish>::from_output_type, Parsed>);
static_assert(std::same_as<pb::edge_descriptor<0, Parse, Finish>::to_input_type, Parsed>);
static_assert(pb::stage_descriptor<0, Parse>::key() == std::string_view{"order.parse"});
static_assert(pb::edge_descriptor<0, Parse, Finish>::from_key() == std::string_view{"order.parse"});
static_assert(pb::edge_descriptor<0, Parse, Finish>::to_key() == std::string_view{"finish"});
static_assert(pb::pipeline_descriptor<Pipeline>::stage_count == 2);
static_assert(pb::pipeline_descriptor<Pipeline>::edge_count == 1);
static_assert(pb::edge_from_key<Pipeline, 0>() == std::string_view{"order.parse"});
static_assert(pb::edge_from_name<Pipeline, 0>() == std::string_view{"parse"});
static_assert(pb::edge_to_key<Pipeline, 0>() == std::string_view{"finish"});
static_assert(pb::edge_to_name<Pipeline, 0>() == std::string_view{"finish"});
static_assert(pb::describe<Pipeline>().stage_key<0>() == std::string_view{"order.parse"});
static_assert(pb::describe<Pipeline>().stage_name<0>() == std::string_view{"parse"});
static_assert(pb::describe<Pipeline>().stage_name<1>() == std::string_view{"finish"});
static_assert(pb::describe<Pipeline>().edge_records()[0].from_stage_index == 0);
static_assert(pb::describe<Pipeline>().edge_records()[0].to_stage_index == 1);
static_assert(pb::describe<Pipeline>().edge_records()[0].from_key == std::string_view{"order.parse"});
static_assert(pb::describe<Pipeline>().edge_records()[0].to_key == std::string_view{"finish"});
static_assert(std::is_same_v<pb::error_category, pb::runtime::error_category>);
static_assert(std::is_same_v<pb::error, pb::runtime::error>);
static_assert(std::is_same_v<pb::result<int>, pb::runtime::result<int>>);
static_assert(pb::is_result_v<pb::result<int>>);
static_assert(pb::expected_like<pb::result<int>>);
static_assert(std::is_same_v<pb::sync_value_sender<Done>::value_type, Done>);
static_assert(std::is_base_of_v<std::runtime_error, pb::sender_stopped>);
static_assert(std::is_base_of_v<std::runtime_error, pb::sender_no_value>);

int main() {
  assert(pb::has_stage(pb::error{.stage = {.key = "parse", .name = "parse"}}));
  assert(pb::has_message(pb::error{.message = "parse failed"}));

  constexpr auto desc = pb::describe<Pipeline>();
  constexpr auto keys = desc.stage_keys();
  constexpr auto names = desc.stage_names();
  constexpr auto records = desc.stage_records();
  constexpr auto edges = desc.edge_records();
  static_assert(keys[0] == std::string_view{"order.parse"});
  static_assert(records[0].index == 0);
  static_assert(records[0].key == std::string_view{"order.parse"});
  static_assert(records[1].name == std::string_view{"finish"});
  static_assert(edges[0].from_name == std::string_view{"parse"});
  static_assert(edges[0].to_name == std::string_view{"finish"});
  return names == std::array<std::string_view, 2>{"parse", "finish"} ? 0 : 1;
}
