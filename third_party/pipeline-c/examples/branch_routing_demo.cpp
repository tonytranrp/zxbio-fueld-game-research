/// @file  branch_routing_demo.cpp
/// @brief User-facing demonstration of the Pipeline-c++ branch/join DSL.
///
/// This example models a document-routing pipeline that:
///   1. Receives a raw Document
///   2. Classifies it via branch predicates (IsInvoice / IsReport / IsMemo)
///   3. Dispatches to the matching processing stage (ProcessInvoice / ProcessReport / ProcessMemo)
///   4. Joins all branches through a Finalize stage that produces an int result
///
/// Every type relationship is verified at compile-time — miswiring a predicate
/// input_type or a branch output_type is a compiler error, not a runtime surprise.
///
/// Build & run:
///   cmake --build build --config Release --target pb_branch_routing_demo
///   ctest --test-dir build -C Release -R branch_routing_demo

#include <pb/pipeline.hpp>

#include <iostream>
#include <string>
#include <utility>

// ---------------------------------------------------------------------------
// Domain types
// ---------------------------------------------------------------------------

/// Incoming document with a type tag and free-form content.
struct Document {
  std::string type;
  std::string content;
};

/// Result of processing a document inside a branch route.
struct ProcessedDoc {
  std::string result;
};

// ---------------------------------------------------------------------------
// Predicate stages — determine which branch case to select.
//
// Every predicate must expose:
//   • input_type  = Document   (all predicates see the same input)
//   • output_type = bool       (the runtime expects a boolean decision)
// ---------------------------------------------------------------------------

struct IsInvoice {
  using input_type  = Document;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-invoice"; }
  static constexpr auto stage_key()  noexcept { return "is-invoice"; }

  bool operator()(const Document& doc) const { return doc.type == "invoice"; }
};

struct IsReport {
  using input_type  = Document;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-report"; }
  static constexpr auto stage_key()  noexcept { return "is-report"; }

  bool operator()(const Document& doc) const { return doc.type == "report"; }
};

struct IsMemo {
  using input_type  = Document;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-memo"; }
  static constexpr auto stage_key()  noexcept { return "is-memo"; }

  bool operator()(const Document& doc) const { return doc.type == "memo"; }
};

// ---------------------------------------------------------------------------
// Branch route stages — each processes a Document into a ProcessedDoc.
//
// Every route stage must have the same input_type as the predicate
// (enforced by branch_case at compile-time).
// ---------------------------------------------------------------------------

struct ProcessInvoice {
  using input_type  = Document;
  using output_type = ProcessedDoc;

  static constexpr auto stage_name() noexcept { return "process-invoice"; }
  static constexpr auto stage_key()  noexcept { return "process-invoice"; }

  ProcessedDoc operator()(const Document& doc) const {
    return ProcessedDoc{"[INVOICE] " + doc.content + " — paid"};
  }
};

struct ProcessReport {
  using input_type  = Document;
  using output_type = ProcessedDoc;

  static constexpr auto stage_name() noexcept { return "process-report"; }
  static constexpr auto stage_key()  noexcept { return "process-report"; }

  ProcessedDoc operator()(const Document& doc) const {
    return ProcessedDoc{"[REPORT] " + doc.content + " — archived"};
  }
};

struct ProcessMemo {
  using input_type  = Document;
  using output_type = ProcessedDoc;

  static constexpr auto stage_name() noexcept { return "process-memo"; }
  static constexpr auto stage_key()  noexcept { return "process-memo"; }

  ProcessedDoc operator()(const Document& doc) const {
    return ProcessedDoc{"[MEMO] " + doc.content + " — noted"};
  }
};

// ---------------------------------------------------------------------------
// Join stage — receives the unified ProcessedDoc from whichever branch
// was selected and produces the final pipeline output.
//
// The join stage's input_type must match every branch case's output_type
// (enforced by join_builder_validation at compile-time).
// ---------------------------------------------------------------------------

struct Finalize {
  using input_type  = ProcessedDoc;
  using output_type = int;

  static constexpr auto stage_name() noexcept { return "finalize"; }
  static constexpr auto stage_key()  noexcept { return "finalize"; }

  int operator()(const ProcessedDoc& doc) const {
    std::cout << "  Finalize: " << doc.result << '\n';
    return static_cast<int>(doc.result.size());
  }
};

// ---------------------------------------------------------------------------
// Pipeline definition — the DSL reads left-to-right:
//
///   pb::from<Input>
///     ::branch<CaseA, CaseB, CaseC>   // classify & dispatch
///     ::join<JoinStage>                // unify branch outputs
///     ::to<Output>                     // final sink type
///
/// Compile-time checks (all verified by static_assert inside the library):
///   • Every predicate has input_type == Document, output_type convertible to bool
///   • Every branch stage has input_type == Document
///   • Every branch stage has output_type == ProcessedDoc (all branches unify)
///   • Join stage input_type == ProcessedDoc, output_type == int
///   • Pipeline sink (int) matches join stage output_type
// ---------------------------------------------------------------------------

using InvoiceCase = pb::case_<IsInvoice>::then<ProcessInvoice>;
using ReportCase  = pb::case_<IsReport>::then<ProcessReport>;
using MemoCase    = pb::case_<IsMemo>::then<ProcessMemo>;

using DocPipeline = pb::from<Document>
                        ::branch<InvoiceCase, ReportCase, MemoCase>
                        ::join<Finalize>
                        ::to<int>;

// Verify the pipeline is well-formed at compile-time (catches errors before
// any code runs).
static_assert(pb::valid<DocPipeline>, "DocPipeline failed validation");
static_assert(pb::pipeline_size_v<DocPipeline> == 2,
              "DocPipeline: expected 2 runtime stages (1 branch node + 1 join)");
static_assert(std::is_same_v<pb::pipeline_input_t<DocPipeline>,  Document>,
              "DocPipeline input must be Document");
static_assert(std::is_same_v<pb::pipeline_output_t<DocPipeline>, int>,
              "DocPipeline output must be int");

// ---------------------------------------------------------------------------
// main — compile the pipeline into a sequential engine and run examples.
// ---------------------------------------------------------------------------

int main() {
  // Compile the pipeline into a zero-overhead sequential engine.
  // All type checks have already passed; the engine does no RTTI or
  // virtual dispatch — the compiler can inline every stage.
  auto engine = pb::compile<DocPipeline>(pb::runtime::sequential{});

  // Helper lambda to run a single document and print the result.
  auto run_doc = [&](Document doc) -> int {
    auto result = engine.run(std::move(doc));
    if (!result.has_value()) {
      std::cerr << "  ERROR [" << result.error().stage.key
                << "]: " << result.error().message << '\n';
      return 1;
    }
    std::cout << "  Result length: " << result.value() << "\n\n";
    return 0;
  };

  std::cout << "=== Document Routing Pipeline ===\n\n";

  // Invoice: IsInvoice predicate matches → ProcessInvoice route → Finalize
  std::cout << "[1] Routing an invoice...\n";
  if (run_doc(Document{"invoice", "ACME Corp order #5"})) return 1;

  // Report: IsReport predicate matches → ProcessReport route → Finalize
  std::cout << "[2] Routing a report...\n";
  if (run_doc(Document{"report", "Q4 earnings summary"})) return 2;

  // Memo: IsMemo predicate matches → ProcessMemo route → Finalize
  std::cout << "[3] Routing a memo...\n";
  if (run_doc(Document{"memo", "Team standup notes"})) return 3;

  // Unknown type: no predicate matches → contract violation error
  std::cout << "[4] Routing an unknown document type...\n";
  if (run_doc(Document{"unknown", "mystery content"})) {
    std::cout << "  (Expected: no predicate matched 'unknown')\n\n";
  }

  std::cout << "All examples completed.\n";
  return 0;
}
