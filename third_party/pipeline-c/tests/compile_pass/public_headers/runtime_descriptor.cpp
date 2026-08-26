#include <pb/pipeline.hpp>

#include <string_view>
#include <type_traits>

struct Input {
  int value{};
};

struct Parsed {
  int value{};
};

struct Output {
  int value{};
};

struct Parse {
  using input_type = Input;
  using output_type = Parsed;

  static constexpr auto key = pb::fixed_string{"runtime.parse"};
  static constexpr auto name = pb::fixed_string{"parse"};
};

struct Finish {
  using input_type = Parsed;
  using output_type = Output;

  static constexpr auto key = pb::fixed_string{"runtime.finish"};
  static constexpr auto name = pb::fixed_string{"finish"};
};

struct InvoiceCase {
  using input_type = Input;
  using output_type = Parsed;

  static constexpr auto key = pb::fixed_string{"is.invoice"};
  static constexpr auto name = pb::fixed_string{"IsInvoice"};
};

struct ReportCase {
  using input_type = Input;
  using output_type = Parsed;

  static constexpr auto key = pb::fixed_string{"is.report"};
  static constexpr auto name = pb::fixed_string{"IsReport"};
};

struct IsInvoice {
  using input_type = Input;
  using output_type = bool;

  static constexpr auto key = pb::fixed_string{"is.invoice"};
  static constexpr auto name = pb::fixed_string{"IsInvoice"};

  bool operator()(const Input& input) const { return input.value == 0; }
};

struct IsReport {
  using input_type = Input;
  using output_type = bool;

  static constexpr auto key = pb::fixed_string{"is.report"};
  static constexpr auto name = pb::fixed_string{"IsReport"};

  bool operator()(const Input& input) const { return input.value != 0; }
};

using Pipeline = pb::from<Input>::then<Parse>::then<Finish>::to<Output>;
using BranchPipeline = pb::from<Input>::branch<pb::case_<IsInvoice>::label<"invoice-case">::then<InvoiceCase>,
                                               pb::case_<IsReport>::then<ReportCase>>::to<Parsed>;
using EmptyPipeline = pb::from<Input>::to<Input>;

static_assert(pb::valid<Pipeline>);
static_assert(pb::descriptor_schema_version == std::string_view{"pb.descriptor.v1"});
static_assert(pb::descriptor_topology::linear == pb::runtime::descriptor_topology::linear);

constexpr auto descriptor = pb::make_descriptor<Pipeline>();
static_assert(decltype(descriptor)::stage_count == 2);
static_assert(decltype(descriptor)::edge_count == 1);
static_assert(!decltype(descriptor)::empty);
static_assert(decltype(descriptor)::schema_version == std::string_view{"pb.descriptor.v1"});
static_assert(decltype(descriptor)::topology == pb::descriptor_topology::linear);
static_assert(descriptor.stage_records()[0].index == 0);
static_assert(descriptor.stage_records()[0].key == std::string_view{"runtime.parse"});
static_assert(descriptor.stage_records()[0].name == std::string_view{"parse"});
static_assert(descriptor.stage_records()[1].key == std::string_view{"runtime.finish"});
static_assert(descriptor.stage_records()[1].name == std::string_view{"finish"});
static_assert(descriptor.edge_records()[0].from_key == std::string_view{"runtime.parse"});
static_assert(descriptor.edge_records()[0].from_name == std::string_view{"parse"});
static_assert(descriptor.edge_records()[0].to_key == std::string_view{"runtime.finish"});
static_assert(descriptor.edge_records()[0].to_name == std::string_view{"finish"});
static_assert(std::same_as<decltype(descriptor), const pb::runtime::descriptor_view<2>>);

constexpr auto empty_descriptor = pb::make_descriptor<EmptyPipeline>();
static_assert(decltype(empty_descriptor)::stage_count == 0);
static_assert(decltype(empty_descriptor)::edge_count == 0);
static_assert(decltype(empty_descriptor)::empty);
static_assert(empty_descriptor.stage_records().empty());
static_assert(empty_descriptor.edge_records().empty());

constexpr auto branch_descriptor = pb::make_branch_descriptor<BranchPipeline>();
static_assert(branch_descriptor.stage_count == 1);
static_assert(branch_descriptor.edge_count == 0);
static_assert(branch_descriptor.case_count == 2);
static_assert(!branch_descriptor.empty);
static_assert(branch_descriptor.stage_records()[0].key == std::string_view{"branch"});
static_assert(branch_descriptor.stage_records()[0].name == std::string_view{"branch"});
static_assert(branch_descriptor.branch_case_records()[0].case_index == 0);
static_assert(branch_descriptor.branch_case_records()[0].branch_stage_index == 0);
static_assert(branch_descriptor.branch_case_records()[0].case_id == std::string_view{"branch.0.case.0"});
static_assert(branch_descriptor.branch_case_records()[0].case_key == std::string_view{"branch.0.case.0"});
static_assert(branch_descriptor.branch_case_records()[0].case_label == std::string_view{"invoice-case"});
static_assert(branch_descriptor.branch_case_records()[0].predicate_node_id == std::string_view{"branch.0.case.0.predicate"});
static_assert(branch_descriptor.branch_case_records()[0].stage_node_id == std::string_view{"branch.0.case.0.stage"});
static_assert(branch_descriptor.branch_case_records()[0].predicate_key == std::string_view{"is.invoice"});
static_assert(branch_descriptor.branch_case_records()[0].predicate_name == std::string_view{"IsInvoice"});
static_assert(branch_descriptor.branch_case_records()[0].stage_key == std::string_view{"is.invoice"});
static_assert(branch_descriptor.branch_case_records()[0].stage_name == std::string_view{"IsInvoice"});
static_assert(branch_descriptor.branch_case_records()[1].case_index == 1);
static_assert(branch_descriptor.branch_case_records()[1].branch_stage_index == 0);
static_assert(branch_descriptor.branch_case_records()[1].case_id == std::string_view{"branch.0.case.1"});
static_assert(branch_descriptor.branch_case_records()[1].case_key == std::string_view{"branch.0.case.1"});
static_assert(branch_descriptor.branch_case_records()[1].case_label.empty());
static_assert(branch_descriptor.branch_case_records()[1].predicate_node_id == std::string_view{"branch.0.case.1.predicate"});
static_assert(branch_descriptor.branch_case_records()[1].stage_node_id == std::string_view{"branch.0.case.1.stage"});
static_assert(branch_descriptor.branch_case_records()[1].predicate_key == std::string_view{"is.report"});
static_assert(branch_descriptor.branch_case_records()[1].predicate_name == std::string_view{"IsReport"});
static_assert(branch_descriptor.branch_case_records()[1].stage_key == std::string_view{"is.report"});
static_assert(branch_descriptor.branch_case_records()[1].stage_name == std::string_view{"IsReport"});

int pb_public_header_runtime_descriptor() { return 0; }
