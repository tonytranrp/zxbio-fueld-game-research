#include <pb/pipeline.hpp>

#include <string_view>
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
  using input_type = pb::meta::type_list<parsed, reviewed>;
  using output_type = parsed;
  parsed operator()(parsed) const { return {}; }
  parsed operator()(reviewed) const { return {}; }
};

using public_branch_case = pb::case_<predicate>::then<parse>;
using public_labeled_branch_case = pb::case_<predicate>::label<"parse-case">::then<parse>;
using public_review_branch_case = pb::case_<predicate>::then<manual_review>;
using public_branch_case_output = pb::branch_case_output<public_branch_case>;
using public_branch_node = pb::branch_node<public_branch_case, public_review_branch_case>;
using public_branch_outputs = pb::branch_outputs<public_branch_case, public_review_branch_case>;
using public_review_branch_outputs = pb::branch_outputs<public_review_branch_case>;
using public_branch_output_validation = pb::branch_output_validation<public_review_branch_outputs, reviewed>;
using public_branch_unified_output_validation =
    pb::branch_unified_output_validation<public_branch_outputs, std::variant<parsed, reviewed>>;
using public_join_node = pb::join_node<join>;
using public_join_output = pb::join_output<public_join_node>;
using public_join_validation = pb::join_validation<public_branch_outputs, public_join_node>;
using public_join_builder_validation = pb::join_builder_validation<public_branch_outputs, join>;
using public_type_list_join_node = pb::join_node<type_list_join>;
using public_type_list_join_validation =
    pb::join_validation<public_branch_outputs, public_type_list_join_node>;
using public_type_list_join_builder_validation =
    pb::join_builder_validation<public_branch_outputs, type_list_join>;
} // namespace

static_assert(std::is_same_v<public_branch_case::predicate_type, predicate>);
static_assert(std::is_same_v<public_branch_case::stage_type, parse>);
static_assert(std::is_same_v<public_branch_case::input_type, raw>);
static_assert(public_branch_case::case_label().empty());
static_assert(std::is_same_v<public_labeled_branch_case::predicate_type, predicate>);
static_assert(std::is_same_v<public_labeled_branch_case::stage_type, parse>);
static_assert(public_labeled_branch_case::case_label() == std::string_view{"parse-case"});
static_assert(std::is_same_v<public_branch_case_output::case_type, public_branch_case>);
static_assert(std::is_same_v<public_branch_case_output::output_type, parsed>);
static_assert(public_branch_node::case_count == 2);
static_assert(std::is_same_v<public_branch_node::input_type, raw>);
static_assert(public_branch_outputs::output_count == 2);
static_assert(std::is_same_v<public_branch_outputs::input_type, raw>);
static_assert(std::is_same_v<public_branch_outputs::output_types,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<public_branch_outputs::output_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<pb::branch_raw_output_types_t<public_branch_case, public_review_branch_case>,
                             pb::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<pb::branch_unified_output_t<public_branch_case, public_review_branch_case>,
                             std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_branch_output_validation::branch_outputs_type, public_review_branch_outputs>);
static_assert(std::is_same_v<public_branch_output_validation::input_type, raw>);
static_assert(std::is_same_v<public_branch_output_validation::output_type, reviewed>);
static_assert(public_branch_output_validation::output_count == 1);
static_assert(std::is_same_v<public_branch_unified_output_validation::branch_outputs_type,
                             public_branch_outputs>);
static_assert(std::is_same_v<public_branch_unified_output_validation::raw_output_types,
                             pb::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<public_branch_unified_output_validation::unified_output_type,
                             std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_branch_unified_output_validation::output_type,
                             std::variant<parsed, reviewed>>);
static_assert(public_branch_unified_output_validation::output_count == 2);
static_assert(std::is_same_v<public_join_node::stage_type, join>);
static_assert(std::is_same_v<public_join_node::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_node::output_type, parsed>);
static_assert(std::is_same_v<public_join_output::join_type, public_join_node>);
static_assert(std::is_same_v<public_join_output::stage_type, join>);
static_assert(std::is_same_v<public_join_output::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_output::output_type, parsed>);
static_assert(std::is_same_v<public_join_validation::branch_outputs_type, public_branch_outputs>);
static_assert(std::is_same_v<public_join_validation::join_type, public_join_node>);
static_assert(std::is_same_v<public_join_validation::raw_output_types,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_validation::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_validation::execution_input_type,
                             std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_validation::output_type, parsed>);
static_assert(public_join_validation::accepts_unified_output);
static_assert(!public_join_validation::accepts_raw_type_list);
static_assert(std::is_same_v<public_join_builder_validation::branch_outputs_type, public_branch_outputs>);
static_assert(std::is_same_v<public_join_builder_validation::join_type, public_join_node>);
static_assert(std::is_same_v<public_join_builder_validation::stage_type, join>);
static_assert(std::is_same_v<public_join_builder_validation::raw_output_types,
                             pb::core::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_builder_validation::input_type, std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_builder_validation::execution_input_type,
                             std::variant<parsed, reviewed>>);
static_assert(std::is_same_v<public_join_builder_validation::output_type, parsed>);
static_assert(public_join_builder_validation::accepts_unified_output);
static_assert(!public_join_builder_validation::accepts_raw_type_list);
static_assert(std::is_same_v<public_type_list_join_validation::input_type,
                             pb::meta::type_list<parsed, reviewed>>);
static_assert(std::is_same_v<public_type_list_join_validation::execution_input_type,
                             std::variant<parsed, reviewed>>);
static_assert(!public_type_list_join_validation::accepts_unified_output);
static_assert(public_type_list_join_validation::accepts_raw_type_list);
static_assert(std::is_same_v<public_type_list_join_builder_validation::input_type,
                             pb::meta::type_list<parsed, reviewed>>);
static_assert(public_type_list_join_builder_validation::accepts_raw_type_list);

int pb_public_header_pipeline() { return 0; }
