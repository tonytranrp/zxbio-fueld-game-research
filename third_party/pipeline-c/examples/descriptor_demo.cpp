/// @file  descriptor_demo.cpp
/// @brief Demonstrates the pb::runtime descriptor schema for both linear and
///        branch pipelines.
///
/// This example shows how to introspect pipeline structure at compile-time
/// using the versioned descriptor schema (pb.descriptor.v1). Descriptors
/// expose every stage, edge, and (for branch pipelines) every branch case
/// with predicate and stage metadata.
///
/// Build & run:
///   cmake --build build --config Release --target pb_descriptor_demo
///   ctest --test-dir build -C Release -R descriptor_demo

#include <pb/pipeline.hpp>

#include <cassert>
#include <iostream>
#include <string_view>

// ===========================================================================
// Linear pipeline — simple two-stage chain
// ===========================================================================

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

  static constexpr auto stage_name() noexcept { return "parse"; }
  static constexpr auto stage_key() noexcept { return "order.parse"; }

  Parsed operator()(Input input) const { return Parsed{input.value + 1}; }
};

struct Finish {
  using input_type = Parsed;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "finish"; }
  static constexpr auto stage_key() noexcept { return "order.finish"; }

  Output operator()(Parsed parsed) const { return Output{parsed.value * 2}; }
};

using LinearPipeline = pb::from<Input>::then<Parse>::then<Finish>::to<Output>;
static_assert(pb::valid<LinearPipeline>);

// ===========================================================================
// Branch pipeline — document routing with two cases
// ===========================================================================

struct Document {
  int id{};
};

struct ProcessedDoc {
  int id{};
  int result{};
};

struct IsInvoice {
  using input_type = Document;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-invoice"; }
  static constexpr auto stage_key() noexcept { return "is-invoice"; }

  bool operator()(const Document& doc) const { return doc.id == 1; }
};

struct IsReport {
  using input_type = Document;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-report"; }
  static constexpr auto stage_key() noexcept { return "is-report"; }

  bool operator()(const Document& doc) const { return doc.id == 2; }
};

struct ProcessInvoice {
  using input_type = Document;
  using output_type = ProcessedDoc;

  static constexpr auto stage_name() noexcept { return "process-invoice"; }
  static constexpr auto stage_key() noexcept { return "process-invoice"; }

  ProcessedDoc operator()(const Document& doc) const { return ProcessedDoc{doc.id, doc.id * 10}; }
};

struct ProcessReport {
  using input_type = Document;
  using output_type = ProcessedDoc;

  static constexpr auto stage_name() noexcept { return "process-report"; }
  static constexpr auto stage_key() noexcept { return "process-report"; }

  ProcessedDoc operator()(const Document& doc) const { return ProcessedDoc{doc.id, doc.id * 20}; }
};

struct Finalize {
  using input_type = ProcessedDoc;
  using output_type = int;

  static constexpr auto stage_name() noexcept { return "finalize"; }
  static constexpr auto stage_key() noexcept { return "finalize"; }

  int operator()(const ProcessedDoc& doc) const { return doc.result; }
};

using InvoiceCase = pb::case_<IsInvoice>::then<ProcessInvoice>;
using ReportCase = pb::case_<IsReport>::then<ProcessReport>;

using BranchPipeline = pb::from<Document>::branch<InvoiceCase, ReportCase>::join<Finalize>::to<int>;
static_assert(pb::valid<BranchPipeline>);

// ===========================================================================
// Helper: print a descriptor to stdout
// ===========================================================================

template <class Descriptor>
void print_descriptor(std::string_view title, const Descriptor& desc) {
  std::cout << "=== " << title << " ===\n";
  std::cout << "  schema_version : " << desc.schema_version << '\n';
  std::cout << "  topology       : "
            << (desc.topology == pb::descriptor_topology::linear ? "linear" : "branch") << '\n';
  std::cout << "  stage_count    : " << desc.stage_count << '\n';
  std::cout << "  edge_count     : " << desc.edge_count << '\n';
  std::cout << "  case_count     : " << desc.case_count << '\n';
  std::cout << "  empty          : " << (desc.empty ? "true" : "false") << '\n';

  std::cout << "\n  Stages:\n";
  for (const auto& stage : desc.stage_records()) {
    std::cout << "    [" << stage.index << "] " << stage.key << " (\"" << stage.name << "\")\n";
    std::cout << "        topology       : "
              << (stage.topology == pb::descriptor_topology::linear ? "linear" : "branch") << '\n';
    std::cout << "        input_type     : " << stage.input_type_name << '\n';
    std::cout << "        output_type    : " << stage.output_type_name << '\n';
  }

  std::cout << "\n  Edges:\n";
  for (const auto& edge : desc.edge_records()) {
    std::cout << "    [" << edge.index << "] " << edge.from_key << " → " << edge.to_key;
    if (!edge.label.empty()) {
      std::cout << " (" << edge.label << ')';
    }
    std::cout << '\n';
  }

  if constexpr (Descriptor::case_count > 0) {
    std::cout << "\n  Branch cases:\n";
    for (const auto& bc : desc.branch_case_records()) {
      std::cout << "    [" << bc.case_index << "] predicate: " << bc.predicate_key
                << " → stage: " << bc.stage_key << '\n';
      std::cout << "        predicate_name : " << bc.predicate_name << '\n';
      std::cout << "        stage_name     : " << bc.stage_name << '\n';
    }
  }

  std::cout << '\n';
}

// ===========================================================================
// main
// ===========================================================================

int main() {
  // --- Linear descriptor ---
  constexpr auto linear_desc = pb::make_descriptor<LinearPipeline>();
  static_assert(linear_desc.topology == pb::descriptor_topology::linear);
  static_assert(linear_desc.stage_count == 2);
  static_assert(linear_desc.edge_count == 1);
  static_assert(linear_desc.case_count == 0);
  print_descriptor("Linear Pipeline Descriptor (make_descriptor)", linear_desc);

  // --- Branch descriptor (make_branch_descriptor) ---
  constexpr auto branch_desc = pb::make_branch_descriptor<BranchPipeline>();
  static_assert(branch_desc.topology == pb::descriptor_topology::branch);
  static_assert(branch_desc.stage_count == 2);
  static_assert(branch_desc.case_count == 2);
  print_descriptor("Branch Pipeline Descriptor (make_branch_descriptor)", branch_desc);

  // --- Runtime engine descriptor ---
  auto engine = pb::compile<LinearPipeline>(pb::runtime::sequential{});
  const auto rt_desc = engine.descriptor();
  assert(rt_desc.schema_version == pb::runtime::descriptor_schema_version);
  print_descriptor("Runtime Engine Descriptor (linear)", rt_desc);

  auto branch_engine = pb::compile<BranchPipeline>(pb::runtime::sequential{});
  const auto rt_branch_desc = branch_engine.descriptor();
  print_descriptor("Runtime Engine Descriptor (branch)", rt_branch_desc);

  // --- type_name utility ---
  std::cout << "=== type_name utility ===\n";
  std::cout << "  type_name<int>           = " << pb::type_name<int>() << '\n';
  std::cout << "  type_name<Document>      = " << pb::type_name<Document>() << '\n';
  std::cout << "  type_name<ProcessedDoc>  = " << pb::type_name<ProcessedDoc>() << '\n';

  std::cout << "\nAll descriptor demos completed.\n";
  return 0;
}
