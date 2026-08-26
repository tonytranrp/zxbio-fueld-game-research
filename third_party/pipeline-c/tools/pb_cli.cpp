// ─────────────────────────────────────────────────────────────
// Pipeline‑c++ CLI — pb_cli
//
// Lightweight command‑line frontend for Pipeline‑c++ introspection,
// graph export, and feature inspection.
//
// Usage:
//   pb_cli                                          show banner & usage
//   pb_cli version                                  print version info
//   pb_cli features                                 print feature‑gate matrix
//   pb_cli list                                     list built‑in example pipelines
//   pb_cli describe <name> --format=<dot|json>      emit graph for a built‑in pipeline
//                  [--out=<path>]                   (default: stdout)
//   pb_cli export [--dot|--json|--text]             show export-API docs
// ─────────────────────────────────────────────────────────────
//
// EXTENSION POINT — registering MORE pipelines
// --------------------------------------------
// `list` / `describe` are driven by a pb::tooling::pipeline_registry, not a
// hard-coded switch (see include/pb/tooling/cli_registry.hpp).  To expose your
// own pipeline in a fork of this CLI:
//
//   1. Define a validated pipeline type in pb_cli_examples (or your own TU):
//          using MyPipeline = pb::from<In>::then<...>::to<Out>;
//   2. Register it with a single line in build_registry():
//          registry.add<MyPipeline>("my-name", "my-topology",
//                                    "One-line description.");
//
// Everything else — `pb_cli list`, `pb_cli describe my-name --format=…`, the
// unknown-name error path — picks it up automatically.  A compiled CLI cannot
// compile arbitrary user source at runtime, so this single-line registration is
// the realistic closure for "accept user-defined pipelines".
// ─────────────────────────────────────────────────────────────

#include <pb/pipeline.hpp>
#include <pb/core/cpp26_features.hpp>
#include <pb/tooling/cli_registry.hpp>

#include <variant>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

// ─────────────────────────────────────────────────────────────
// version constants
// ─────────────────────────────────────────────────────────────

static constexpr std::string_view kVersion    = "0.1.0";
static constexpr std::string_view kStdVersion = "C++20";
static constexpr std::string_view kLibType    = "header-only core + compiled runtime";

// ─────────────────────────────────────────────────────────────
// Built‑in example pipelines
//
// These are kept inside the CLI translation unit so the type-level
// pipelines remain known at compile time.  Each example covers a
// distinct topology so the user can see how the export helpers render
// linear, branch, and fan-in graphs.
// ─────────────────────────────────────────────────────────────

namespace pb_cli_examples {

// Shared domain types kept tiny — the CLI only needs the type graph,
// not the runtime payloads.
struct RawText  { std::string value; };
struct OrderDraft     { std::string id; };
struct ValidatedOrder { std::string id; };
struct Receipt        { std::string id; };
struct Order  { int amount{}; };
struct Decision { int amount{}; };

// ── Linear pipeline: parse → validate → persist ────────────────────
struct ParseOrder {
  using input_type  = RawText;
  using output_type = OrderDraft;
  static constexpr auto stage_key()  noexcept { return "parse_order"; }
  static constexpr auto stage_name() noexcept { return "parse order"; }
  OrderDraft operator()(RawText input) const { return OrderDraft{std::move(input.value)}; }
};
struct ValidateOrder {
  using input_type  = OrderDraft;
  using output_type = ValidatedOrder;
  static constexpr auto stage_key()  noexcept { return "validate_order"; }
  static constexpr auto stage_name() noexcept { return "validate order"; }
  ValidatedOrder operator()(OrderDraft draft) const { return ValidatedOrder{std::move(draft.id)}; }
};
struct PersistOrder {
  using input_type  = ValidatedOrder;
  using output_type = Receipt;
  static constexpr auto stage_key()  noexcept { return "persist_order"; }
  static constexpr auto stage_name() noexcept { return "persist order"; }
  Receipt operator()(ValidatedOrder order) const { return Receipt{std::move(order.id)}; }
};

using OrderLinearPipeline = pb::from<RawText>
    ::then<ParseOrder>
    ::then<ValidateOrder>
    ::then<PersistOrder>
    ::to<Receipt>;

static_assert(pb::valid<OrderLinearPipeline>);

// ── Branch pipeline: route by amount, then join ──────────────────────
struct IsHighRisk {
  using input_type  = Order;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is.high_risk"; }
  static constexpr auto stage_name() noexcept { return "is high risk"; }
  bool operator()(const Order& order) const { return order.amount > 1000; }
};
struct IsNormal {
  using input_type  = Order;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "is.normal"; }
  static constexpr auto stage_name() noexcept { return "is normal"; }
  bool operator()(const Order& order) const { return order.amount <= 1000; }
};
struct ManualReview {
  using input_type  = Order;
  using output_type = Decision;
  static constexpr auto stage_key()  noexcept { return "review.manual"; }
  static constexpr auto stage_name() noexcept { return "manual review"; }
  Decision operator()(Order order) const { return Decision{order.amount}; }
};
struct AutoApprove {
  using input_type  = Order;
  using output_type = Decision;
  static constexpr auto stage_key()  noexcept { return "review.auto"; }
  static constexpr auto stage_name() noexcept { return "auto approve"; }
  Decision operator()(Order order) const { return Decision{order.amount}; }
};
struct FinalizeDecision {
  using input_type  = Decision;
  using output_type = Receipt;
  static constexpr auto stage_key()  noexcept { return "finalize"; }
  static constexpr auto stage_name() noexcept { return "finalize"; }
  Receipt operator()(Decision decision) const {
    return Receipt{"order-" + std::to_string(decision.amount)};
  }
};

using HighRiskCase = pb::case_<IsHighRisk>::then<ManualReview>;
using NormalCase   = pb::case_<IsNormal>::then<AutoApprove>;
using OrderBranchPipeline = pb::from<Order>
    ::branch<HighRiskCase, NormalCase>
    ::join<FinalizeDecision>
    ::to<Receipt>;

static_assert(pb::valid<OrderBranchPipeline>);

// ── Fan-in pipeline: every passing case runs, results merged ─────────
struct AmountAboveMin {
  using input_type  = Order;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.above_min"; }
  static constexpr auto stage_name() noexcept { return "amount above min"; }
  bool operator()(const Order& order) const { return order.amount > 0; }
};
struct AmountBelowMax {
  using input_type  = Order;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.below_max"; }
  static constexpr auto stage_name() noexcept { return "amount below max"; }
  bool operator()(const Order& order) const { return order.amount < 10'000; }
};
struct AmountIsRound {
  using input_type  = Order;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.is_round"; }
  static constexpr auto stage_name() noexcept { return "amount is round"; }
  bool operator()(const Order& order) const { return order.amount % 100 == 0; }
};
struct RecordSummary {
  using input_type  = Order;
  using output_type = Decision;
  static constexpr auto stage_key()  noexcept { return "record.summary"; }
  static constexpr auto stage_name() noexcept { return "record summary"; }
  Decision operator()(Order order) const { return Decision{order.amount}; }
};

using AboveMinCase = pb::case_<AmountAboveMin>::then<RecordSummary>;
using BelowMaxCase = pb::case_<AmountBelowMax>::then<RecordSummary>;
using RoundCase    = pb::case_<AmountIsRound>::then<RecordSummary>;

using FanInAggregate = pb::fan_in_output_t<AboveMinCase, BelowMaxCase, RoundCase>;

struct FanInFinalize {
  using input_type  = FanInAggregate;
  using output_type = Receipt;
  static constexpr auto stage_key()  noexcept { return "finalize.fan_in"; }
  static constexpr auto stage_name() noexcept { return "finalize fan-in"; }
  Receipt operator()(FanInAggregate aggregate) const {
    int total = 0;
    if (aggregate.template get<0>().selected()) total += aggregate.template get<0>().get().amount;
    if (aggregate.template get<1>().selected()) total += aggregate.template get<1>().get().amount;
    if (aggregate.template get<2>().selected()) total += aggregate.template get<2>().get().amount;
    return Receipt{"order-" + std::to_string(total)};
  }
};

using OrderFanInPipeline = pb::from<Order>
    ::branch<AboveMinCase, BelowMaxCase, RoundCase>
    ::fan_in<FanInFinalize>
    ::to<Receipt>;

static_assert(pb::valid<OrderFanInPipeline>);

// ── Longer linear pipeline: a five-stage enrichment chain ───────────
//
// Demonstrates that the registry scales past the original three-stage demo:
// every stage is registered the same way and renders identically to the short
// chain, just with more nodes/edges.
struct IngestText {
  using input_type  = RawText;
  using output_type = OrderDraft;
  static constexpr auto stage_key()  noexcept { return "ingest"; }
  static constexpr auto stage_name() noexcept { return "ingest text"; }
  OrderDraft operator()(RawText input) const { return OrderDraft{std::move(input.value)}; }
};
struct NormalizeDraft {
  using input_type  = OrderDraft;
  using output_type = OrderDraft;
  static constexpr auto stage_key()  noexcept { return "normalize"; }
  static constexpr auto stage_name() noexcept { return "normalize draft"; }
  OrderDraft operator()(OrderDraft draft) const { return draft; }
};
struct EnrichDraft {
  using input_type  = OrderDraft;
  using output_type = ValidatedOrder;
  static constexpr auto stage_key()  noexcept { return "enrich"; }
  static constexpr auto stage_name() noexcept { return "enrich draft"; }
  ValidatedOrder operator()(OrderDraft draft) const { return ValidatedOrder{std::move(draft.id)}; }
};
struct AuditOrder {
  using input_type  = ValidatedOrder;
  using output_type = ValidatedOrder;
  static constexpr auto stage_key()  noexcept { return "audit"; }
  static constexpr auto stage_name() noexcept { return "audit order"; }
  ValidatedOrder operator()(ValidatedOrder order) const { return order; }
};
struct ArchiveOrder {
  using input_type  = ValidatedOrder;
  using output_type = Receipt;
  static constexpr auto stage_key()  noexcept { return "archive"; }
  static constexpr auto stage_name() noexcept { return "archive order"; }
  Receipt operator()(ValidatedOrder order) const { return Receipt{std::move(order.id)}; }
};

using OrderEnrichPipeline = pb::from<RawText>
    ::then<IngestText>
    ::then<NormalizeDraft>
    ::then<EnrichDraft>
    ::then<AuditOrder>
    ::then<ArchiveOrder>
    ::to<Receipt>;

static_assert(pb::valid<OrderEnrichPipeline>);

// ── Heterogeneous-variant branch: cases yield DIFFERENT output types ─
//
// Unlike OrderBranchPipeline (where both cases return Decision), the two cases
// here return distinct types, so the unified branch output is a std::variant.
// The join stage consumes that variant — exercising the heterogeneous-branch
// path through the same one-line registration.
struct Refund  { int amount{}; };
struct Charge  { std::string memo; };

struct RefundCasePred {
  using input_type  = Order;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "route.refund"; }
  static constexpr auto stage_name() noexcept { return "route refund"; }
  bool operator()(const Order& order) const { return order.amount < 0; }
};
struct ChargeCasePred {
  using input_type  = Order;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "route.charge"; }
  static constexpr auto stage_name() noexcept { return "route charge"; }
  bool operator()(const Order& order) const { return order.amount >= 0; }
};
struct MakeRefund {
  using input_type  = Order;
  using output_type = Refund;
  static constexpr auto stage_key()  noexcept { return "make.refund"; }
  static constexpr auto stage_name() noexcept { return "make refund"; }
  Refund operator()(Order order) const { return Refund{-order.amount}; }
};
struct MakeCharge {
  using input_type  = Order;
  using output_type = Charge;
  static constexpr auto stage_key()  noexcept { return "make.charge"; }
  static constexpr auto stage_name() noexcept { return "make charge"; }
  Charge operator()(Order order) const { return Charge{"order-" + std::to_string(order.amount)}; }
};
struct SettleVariant {
  using input_type  = std::variant<Refund, Charge>;
  using output_type = Receipt;
  static constexpr auto stage_key()  noexcept { return "settle"; }
  static constexpr auto stage_name() noexcept { return "settle variant"; }
  Receipt operator()(std::variant<Refund, Charge> outcome) const {
    if (std::holds_alternative<Refund>(outcome)) {
      return Receipt{"refund-" + std::to_string(std::get<Refund>(outcome).amount)};
    }
    return Receipt{std::get<Charge>(outcome).memo};
  }
};

using RefundCase = pb::case_<RefundCasePred>::then<MakeRefund>;
using ChargeCase = pb::case_<ChargeCasePred>::then<MakeCharge>;
using OrderVariantPipeline = pb::from<Order>
    ::branch<RefundCase, ChargeCase>
    ::join<SettleVariant>
    ::to<Receipt>;

static_assert(pb::valid<OrderVariantPipeline>);

} // namespace pb_cli_examples

// ─────────────────────────────────────────────────────────────
// helpers
// ─────────────────────────────────────────────────────────────

namespace {

[[nodiscard]] constexpr const char* yes_no(bool cond) noexcept {
  return cond ? "yes" : "no";
}

// ── Pipeline registry ────────────────────────────────────────
//
// Every built-in pipeline is registered here with a single `add<...>` line.
// The registry type-erases each pipeline's export closures, so the rest of the
// CLI (`list`, `describe`, the unknown-name error path) works against the
// registry instead of a hard-coded switch.  See the EXTENSION POINT note at the
// top of this file (and in include/pb/tooling/cli_registry.hpp) for how a fork
// adds its own pipelines.
[[nodiscard]] pb::tooling::pipeline_registry build_registry() {
  using namespace pb_cli_examples;
  pb::tooling::pipeline_registry registry;

  // The original three examples — names, descriptions, and DOT graph ids are
  // preserved EXACTLY so the golden `pb_cli describe` tests still pass.
  registry.add<OrderLinearPipeline>(
      "order-linear", "linear",
      "Three-stage parse → validate → persist linear pipeline.");
  registry.add<OrderBranchPipeline>(
      "order-branch", "branch",
      "Branch on order amount, then join on FinalizeDecision.");
  registry.add<OrderFanInPipeline>(
      "order-fan-in", "fan_in",
      "Fan-in: every passing predicate runs, results merged.");

  // Additional built-ins, registered the same way to demonstrate that the
  // registry scales past the original demo set.
  registry.add<OrderEnrichPipeline>(
      "order-enrich", "linear",
      "Five-stage ingest → normalize → enrich → audit → archive chain.");
  registry.add<OrderVariantPipeline>(
      "order-variant", "branch",
      "Heterogeneous branch: cases yield distinct types joined via std::variant.");

  return registry;
}

// Process-wide registry, built once on first use.
[[nodiscard]] const pb::tooling::pipeline_registry& registry() {
  static const pb::tooling::pipeline_registry instance = build_registry();
  return instance;
}

void print_banner() {
  std::cout << "Pipeline-c++ CLI v" << kVersion << "\n";
  std::cout << "Compile-time validated C++ pipeline builder\n\n";
  std::cout << "Usage: pb_cli <command> [options]\n\n";
  std::cout << "Commands:\n";
  std::cout << "  version                            Print version and capability summary\n";
  std::cout << "  features                           Print C++ feature-gate matrix\n";
  std::cout << "  backends                           Print runtime backend support matrix\n";
  std::cout << "  list                               List built-in example pipelines\n";
  std::cout << "  schema                             Print helper-export schema metadata\n";
  std::cout << "  describe <name> [options]          Emit graph for an example pipeline\n";
  std::cout << "       --format=<dot|json|text>      Export format (default: dot)\n";
  std::cout << "       --format <dot|json|text>      Split-form equivalent\n";
  std::cout << "       --out=<path>                  Write to file instead of stdout\n";
  std::cout << "       --out <path>                  Split-form equivalent\n";
  std::cout << "  export [--dot|--json|--text]       Print export-API documentation\n";
  std::cout << "  validate <pipeline>                (reserved) Validate a pipeline definition\n";
}

void print_version() {
  std::cout << "Pipeline-c++ v" << kVersion << "\n";
  std::cout << "Standard:      " << kStdVersion << "\n";
  std::cout << "Distribution:  " << kLibType << "\n";
  std::cout << "Topologies:    linear, branch/join, fan-in\n";
  std::cout << "Branch types:  homogeneous + heterogeneous (via std::variant)\n";
  std::cout << "Export:        DOT, JSON, compact text (schema_version pb.core.graph.v1)\n";
  std::cout << "Runtime:       sequential (built-in), thread-pool (built-in)\n";
}

void print_features() {
  std::cout << "Pipeline-c++ feature-gate matrix\n";
  std::cout << "=================================\n";
  std::cout << "Compiler __cplusplus:         " << __cplusplus << "\n\n";

  std::cout << "C++26 core language:          " << yes_no(PB_HAS_CPP26)       << "\n";
  std::cout << "Static reflection  (P2996):   " << yes_no(PB_HAS_REFLECTION)   << "\n";
  std::cout << "Contracts          (P2900):   " << yes_no(PB_HAS_CONTRACTS)    << "\n";
  std::cout << "Pack indexing      (P2662):   " << yes_no(PB_HAS_PACK_INDEXING) << "\n";
  std::cout << "std::expected      (C++23):   " << yes_no(PB_HAS_STD_EXPECTED)  << "\n";
  std::cout << "Deducing this      (P0847):   " << yes_no(PB_HAS_DEDUCING_THIS) << "\n";

  std::cout << "\nFeature-test macros (raw):\n";
#if defined(__cpp_reflection)
  std::cout << "  __cpp_reflection            = " << __cpp_reflection << "\n";
#else
  std::cout << "  __cpp_reflection            = <undefined>\n";
#endif
#if defined(__cpp_contracts)
  std::cout << "  __cpp_contracts             = " << __cpp_contracts << "\n";
#else
  std::cout << "  __cpp_contracts             = <undefined>\n";
#endif
#if defined(__cpp_pack_indexing)
  std::cout << "  __cpp_pack_indexing         = " << __cpp_pack_indexing << "\n";
#else
  std::cout << "  __cpp_pack_indexing         = <undefined>\n";
#endif
#if defined(__cpp_lib_expected)
  std::cout << "  __cpp_lib_expected          = " << __cpp_lib_expected << "\n";
#else
  std::cout << "  __cpp_lib_expected          = <undefined>\n";
#endif
#if defined(__cpp_explicit_this_parameter)
  std::cout << "  __cpp_explicit_this_parameter = " << __cpp_explicit_this_parameter << "\n";
#else
  std::cout << "  __cpp_explicit_this_parameter = <undefined>\n";
#endif
}

void print_backends() {
  std::cout << "Pipeline-c++ runtime backend matrix\n";
  std::cout << "====================================\n";
  std::cout << "name | model | support | external | default | note\n";
  for (const auto& backend : pb::backend_features()) {
    std::cout << backend.name << " | "
              << pb::backend_execution_model_name(backend.execution_model) << " | "
              << pb::backend_support_name(backend.support) << " | "
              << yes_no(backend.external_dependency) << " | "
              << yes_no(backend.default_build) << " | "
              << backend.note << "\n";
  }
  std::cout << "\nBoundary: external dependency backends are dormant unless their PB_ENABLE_* option\n";
  std::cout << "is configured and the dependency is found; roadmap entries are not working backends.\n";
}

void print_export_help(std::string_view format) {
  if (format.empty() || format == "--dot") {
    std::cout << "DOT export (Graphviz)\n";
    std::cout << "=====================\n";
    std::cout << "Library API:  pb::to_dot<Pipeline>(\"graph_name\")\n";
    std::cout << "CLI:          pb_cli describe <name> --format=dot\n";
    std::cout << "Supported topologies: linear, branch/join, fan-in.\n";
    if (format == "--dot") return;
    std::cout << "\n";
  }
  if (format.empty() || format == "--json") {
    std::cout << "JSON export\n";
    std::cout << "============\n";
    std::cout << "Library API:  pb::to_json<Pipeline>()\n";
    std::cout << "CLI:          pb_cli describe <name> --format=json\n";
    std::cout << "Schema:       pb.core.graph.v1 (helper schema; see docs/export-helper-schema.md).\n";
    if (format == "--json") return;
    std::cout << "\n";
  }
  if (format.empty() || format == "--text") {
    std::cout << "Compact text export\n";
    std::cout << "===================\n";
    std::cout << "Library API:  pb::to_text<Pipeline>()\n";
    std::cout << "CLI:          pb_cli describe <name> --format=text\n";
    std::cout << "Use case:     dropping a pipeline graph into compiler errors, CI logs,\n";
    std::cout << "              or terse documentation snippets where DOT/JSON would be\n";
    std::cout << "              too verbose.\n";
  }
}

int run_export(int argc, char* argv[]) {
  if (argc > 3) {
    std::cerr << "export: expected at most one option: --dot, --json, or --text.\n";
    return 2;
  }

  std::string_view fmt;
  if (argc >= 3) {
    fmt = argv[2];
    if (fmt != "--dot" && fmt != "--json" && fmt != "--text") {
      std::cerr << "export: unknown option '" << fmt
                << "'. Expected --dot, --json, or --text.\n";
      return 2;
    }
  }

  print_export_help(fmt);
  return 0;
}

void print_validate_help() {
  std::cout << "validate — (reserved for future use)\n";
  std::cout << "Will accept a pipeline definition file and report\n";
  std::cout << "compile-time validation results.\n";
}

void print_schema() {
  // pb_cli schema — emit the helper-export schema metadata that downstream
  // tooling needs in order to consume `pb_cli describe` output safely.
  // The version strings here come straight from the library so the CLI and
  // the underlying export helpers cannot silently drift apart.
  std::cout << "Pipeline-c++ helper-export schema\n";
  std::cout << "=================================\n";
  std::cout << "graph schema:    pb.core.graph.v1\n";
  std::cout << "descriptor:      " << pb::runtime::descriptor_schema_version << "\n";
  std::cout << "diagnostics:     " << pb::diagnostics::diagnostics_schema_version << "\n";
  std::cout << "error:           " << pb::runtime::error_schema_version << "\n";
  std::cout << "cancellation:    " << pb::cancellation_schema_version << "\n";
  std::cout << "observer:        " << pb::runtime::observer_schema_version << "\n";
  std::cout << "observer verbose:" << pb::runtime::verbose_observer_schema_version << "\n";
  std::cout << "trace:           " << pb::runtime::trace_schema_version << "\n";
  std::cout << "trace ndjson:    " << pb::runtime::trace_ndjson_schema_version << "\n";
  std::cout << "version field:   schema_version\n";
  std::cout << "formats:         dot, json, text\n";
  std::cout << "topologies:      linear, branch, fan_in\n";
  std::cout << "boundary:        helper output — see docs/graph-export-roadmap.md for the\n";
  std::cout << "                 stability boundary; not yet a frozen interchange contract.\n";
}

void print_examples() {
  std::cout << "Built-in example pipelines:\n";
  for (const auto& entry : registry().entries()) {
    std::cout << "  " << entry.name << "  [" << entry.topology << "]  — " << entry.description << "\n";
  }
  std::cout << "\nRender with: pb_cli describe <name> --format=<dot|json|text>\n";
}

// Split "--key=value" → {key, value}.  If no '=', value is empty.
struct option_pair {
  std::string_view key;
  std::string_view value;
};

[[nodiscard]] option_pair split_option(std::string_view arg) {
  const auto eq = arg.find('=');
  if (eq == std::string_view::npos) return {arg, {}};
  return {arg.substr(0, eq), arg.substr(eq + 1)};
}

[[nodiscard]] bool looks_like_option(std::string_view arg) noexcept {
  return arg.starts_with("--");
}

[[nodiscard]] bool consume_option_value(int argc, char* argv[], int& index,
                                        std::string_view inline_value,
                                        std::string_view& value) {
  if (!inline_value.empty()) {
    value = inline_value;
    return true;
  }
  if (index + 1 >= argc || looks_like_option(argv[index + 1])) {
    return false;
  }
  ++index;
  value = argv[index];
  return true;
}

int run_describe(int argc, char* argv[]) {
  if (argc < 3) {
    std::cerr << "describe: missing pipeline name. Try `pb_cli list`.\n";
    return 2;
  }
  const std::string_view name = argv[2];

  std::string_view format{"dot"};
  std::string_view out_path{};

  for (int i = 3; i < argc; ++i) {
    const auto [key, inline_value] = split_option(argv[i]);
    if (key == "--format") {
      std::string_view value;
      if (!consume_option_value(argc, argv, i, inline_value, value)) {
        std::cerr << "describe: --format requires a value: 'dot', 'json', or 'text'.\n";
        return 2;
      }
      if (value != "dot" && value != "json" && value != "text") {
        std::cerr << "describe: --format must be 'dot', 'json', or 'text' (got '"
                  << value << "').\n";
        return 2;
      }
      format = value;
    } else if (key == "--out") {
      std::string_view value;
      if (!consume_option_value(argc, argv, i, inline_value, value)) {
        std::cerr << "describe: --out requires a non-empty path.\n";
        return 2;
      }
      out_path = value;
    } else {
      std::cerr << "describe: unknown option '" << key << "'.\n";
      return 2;
    }
  }

  if (!registry().contains(name)) {
    std::cerr << "describe: unknown pipeline '" << name
              << "'. Run `pb_cli list` for available names.\n";
    return 2;
  }

  const std::string graph = registry().render(name, format);
  if (graph.empty()) {
    std::cerr << "describe: internal error rendering '" << name << "'.\n";
    return 3;
  }

  if (out_path.empty()) {
    std::cout << graph;
    if (!graph.empty() && graph.back() != '\n') {
      std::cout << '\n';
    }
    return 0;
  }

  std::ofstream out{std::string{out_path}};
  if (!out) {
    std::cerr << "describe: failed to open '" << out_path << "' for writing.\n";
    return 4;
  }
  out << graph;
  if (!out) {
    std::cerr << "describe: failed writing to '" << out_path << "'.\n";
    return 4;
  }
  return 0;
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_banner();
    return 0;
  }

  const std::string cmd = argv[1];

  if (cmd == "version") {
    print_version();
    return 0;
  }

  if (cmd == "features") {
    print_features();
    return 0;
  }

  if (cmd == "backends") {
    print_backends();
    return 0;
  }

  if (cmd == "list") {
    print_examples();
    return 0;
  }

  if (cmd == "schema") {
    print_schema();
    return 0;
  }

  if (cmd == "describe") {
    return run_describe(argc, argv);
  }

  if (cmd == "export") {
    return run_export(argc, argv);
  }

  if (cmd == "validate") {
    print_validate_help();
    return 0;
  }

  // ── unknown command ────────────────────────────────────────
  std::cerr << "Unknown command: " << cmd << "\n";
  std::cerr << "Run `pb_cli` without arguments for usage.\n";
  return 1;
}
