#include <pb/core/pipeline_state.hpp>

#include <type_traits>
#include <variant>

namespace {
struct raw {};
struct parsed {};
struct predicate {
  using input_type = raw;
  using output_type = bool;
};
struct parse {
  using input_type = raw;
  using output_type = parsed;
};
struct reviewed {};
struct manual_review {
  using input_type = raw;
  using output_type = reviewed;
};
struct join {
  using input_type = std::variant<parsed, reviewed>;
  using output_type = parsed;
};
struct type_list_join {
  using input_type = pb::core::meta::type_list<parsed, reviewed>;
  using output_type = parsed;
  parsed operator()(parsed) const { return {}; }
  parsed operator()(reviewed) const { return {}; }
};

using branch_case = pb::core::case_<predicate>::then<parse>;
using review_branch_case = pb::core::case_<predicate>::then<manual_review>;
using branch_case_output = pb::core::branch_case_output<branch_case>;
using branch_node = pb::core::branch_node<branch_case, review_branch_case>;
using branch_outputs = pb::core::branch_outputs<branch_case, review_branch_case>;
using reviewed_outputs = pb::core::branch_outputs<review_branch_case>;
using branch_output_validation = pb::core::branch_output_validation<reviewed_outputs, reviewed>;
using branch_unified_output_validation =
    pb::core::branch_unified_output_validation<branch_outputs, std::variant<parsed, reviewed>>;
using join_node = pb::core::join_node<join>;
using join_output = pb::core::join_output<join_node>;
using join_validation = pb::core::join_validation<branch_outputs, join_node>;
using join_builder_validation = pb::core::join_builder_validation<branch_outputs, join>;
using type_list_join_node = pb::core::join_node<type_list_join>;
using type_list_join_validation = pb::core::join_validation<branch_outputs, type_list_join_node>;
using type_list_join_builder_validation =
    pb::core::join_builder_validation<branch_outputs, type_list_join>;
using alias_pipeline = pb::core::from<raw>::then_all<parse>::done;
} // namespace

static_assert(std::is_same_v<branch_case::predicate_type, predicate>);
static_assert(std::is_same_v<branch_case::stage_type, parse>);
static_assert(std::is_same_v<branch_case::input_type, raw>);
static_assert(std::is_same_v<branch_case_output::case_type, branch_case>);
static_assert(std::is_same_v<branch_case_output::output_type, parsed>);
static_assert(branch_node::case_count == 2);
static_assert(std::is_same_v<branch_node::input_type, raw>);
static_assert(std::is_same_v<branch_node::cases, pb::core::meta::type_list<branch_case, review_branch_case>>);
static_assert(std::is_same_v<branch_node::output_types, pb::core::meta::type_list<parsed, reviewed>>);
static_assert(branch_outputs::output_count == 2);
static_assert(std::is_same_v<branch_outputs::input_type, raw>);
static_assert(std::is_same_v<branch_outputs::cases, pb::core::meta::type_list<branch_case, review_branch_case>>);
static_assert(std::is_same_v<branch_outputs::output_types, pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<branch_outputs::output_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<pb::core::branch_raw_output_types_t<branch_case, review_branch_case>,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(
    std::is_same_v<pb::core::branch_unified_output_t<branch_case, review_branch_case>,
                   std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<branch_output_validation::branch_outputs_type, reviewed_outputs>);
static_assert(std::is_same_v<branch_output_validation::input_type, raw>);
static_assert(std::is_same_v<branch_output_validation::output_type, reviewed>);
static_assert(std::is_same_v<branch_output_validation::output_types, pb::core::meta::type_list<reviewed>>);
static_assert(branch_output_validation::output_count == 1);
static_assert(std::is_same_v<branch_unified_output_validation::branch_outputs_type, branch_outputs>);
static_assert(std::is_same_v<branch_unified_output_validation::raw_output_types,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<branch_unified_output_validation::unified_output_type,
                             std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<branch_unified_output_validation::output_type,
                             std::variant<parsed, reviewed>>);
static_assert(branch_unified_output_validation::output_count == 2);
static_assert(std::is_same_v<join_node::stage_type, join>);
static_assert(std::is_same_v<join_node::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<join_node::output_type, parsed>);
static_assert(std::is_same_v<join_output::join_type, join_node>);
static_assert(std::is_same_v<join_output::stage_type, join>);
static_assert(std::is_same_v<join_output::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<join_output::output_type, parsed>);
static_assert(std::is_same_v<join_validation::branch_outputs_type, branch_outputs>);
static_assert(std::is_same_v<join_validation::join_type, join_node>);
static_assert(std::is_same_v<join_validation::raw_output_types, pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<join_validation::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<join_validation::execution_input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<join_validation::output_type, parsed>);
static_assert(join_validation::accepts_unified_output);
static_assert(!join_validation::accepts_raw_type_list);
static_assert(std::is_same_v<join_builder_validation::branch_outputs_type, branch_outputs>);
static_assert(std::is_same_v<join_builder_validation::join_type, join_node>);
static_assert(std::is_same_v<join_builder_validation::stage_type, join>);
static_assert(std::is_same_v<join_builder_validation::raw_output_types,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<join_builder_validation::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<join_builder_validation::execution_input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<join_builder_validation::output_type, parsed>);
static_assert(join_builder_validation::accepts_unified_output);
static_assert(!join_builder_validation::accepts_raw_type_list);
static_assert(std::is_same_v<type_list_join_validation::input_type,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<type_list_join_validation::execution_input_type,
                             std::variant<parsed, reviewed>>);
static_assert(!type_list_join_validation::accepts_unified_output);
static_assert(type_list_join_validation::accepts_raw_type_list);
static_assert(std::is_same_v<type_list_join_builder_validation::input_type,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(type_list_join_builder_validation::accepts_raw_type_list);
static_assert(pb::core::from<raw>::empty);
static_assert(pb::core::from<raw>::stage_count == 0);
static_assert(pb::core::from<raw>::then<parse>::stage_count == 1);
static_assert(std::is_same_v<alias_pipeline, pb::core::from<raw>::then<parse>::to<parsed>>);

int pb_public_header_core_pipeline_state() { return 0; }
