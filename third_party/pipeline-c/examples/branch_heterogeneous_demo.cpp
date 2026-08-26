/// @file  branch_heterogeneous_demo.cpp
/// @brief User-facing demonstration of heterogeneous branch routing with std::variant join.
///
/// This example models a request-processing pipeline where:
///   1. A Request arrives with a type tag ("text" or "number")
///   2. Branch predicates (IsText / IsNumber) classify the request
///   3. Each branch route produces a different output type:
///        - Text route   → TextResult (holds a string)
///        - Number route → NumResult  (holds an int)
///   4. Both types are unified via std::variant<TextResult, NumResult>
///   5. A join stage (Unify) visits the variant and produces a unified string output
///
/// Key design point: when branch cases produce *different* output types, the library
/// automatically wraps them in std::variant. The join stage must accept that variant
/// and use std::visit to handle each case polymorphically.
///
/// Every type relationship is verified at compile-time — miswiring the join's
/// input_type variant is a compiler error, not a runtime surprise.
///
/// Build & run:
///   cmake --build build --config Release --target pb_branch_heterogeneous_demo
///   ctest --test-dir build -C Release -R branch_heterogeneous_demo

#include <pb/pipeline.hpp>

#include <iostream>
#include <string>
#include <variant>

// ---------------------------------------------------------------------------
// Domain types
// ---------------------------------------------------------------------------

/// Incoming request with a type tag and numeric payload.
struct Request {
  std::string type;
  int payload{};
};

/// Result of processing a text-type request.
struct TextResult {
  std::string output{};
};

/// Result of processing a number-type request.
struct NumResult {
  int output{};
};

// ---------------------------------------------------------------------------
// Predicate stages — determine which branch case to select.
// ---------------------------------------------------------------------------

struct IsText {
  using input_type  = Request;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-text"; }
  static constexpr auto stage_key()  noexcept { return "is-text"; }

  bool operator()(const Request& r) const { return r.type == "text"; }
};

struct IsNumber {
  using input_type  = Request;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-number"; }
  static constexpr auto stage_key()  noexcept { return "is-number"; }

  bool operator()(const Request& r) const { return r.type == "number"; }
};

// ---------------------------------------------------------------------------
// Branch route stages — each produces a *different* type.
//
// TextRoute  → TextResult
// NumRoute   → NumResult
//
// Because the output types differ, the library wraps them in
// std::variant<TextResult, NumResult> at the branch output boundary.
// ---------------------------------------------------------------------------

struct TextRoute {
  using input_type  = Request;
  using output_type = TextResult;

  static constexpr auto stage_name() noexcept { return "text-route"; }
  static constexpr auto stage_key()  noexcept { return "text-route"; }

  TextResult operator()(const Request& r) const {
    return TextResult{"[TEXT] processed payload=" + std::to_string(r.payload)};
  }
};

struct NumRoute {
  using input_type  = Request;
  using output_type = NumResult;

  static constexpr auto stage_name() noexcept { return "num-route"; }
  static constexpr auto stage_key()  noexcept { return "num-route"; }

  NumResult operator()(const Request& r) const {
    return NumResult{r.payload * 100};
  }
};

// ---------------------------------------------------------------------------
// Join stage — accepts std::variant<TextResult, NumResult> and uses
// std::visit to handle each type polymorphically at zero runtime overhead
// (the compiler fully inlines the visit dispatch).
//
// The join's input_type MUST match the branch's output variant exactly.
// If you forget a type in the variant, the library rejects the pipeline
// at compile-time with a clear diagnostic.
// ---------------------------------------------------------------------------

struct Unify {
  using input_type  = std::variant<TextResult, NumResult>;
  using output_type = std::string;

  static constexpr auto stage_name() noexcept { return "unify"; }
  static constexpr auto stage_key()  noexcept { return "unify"; }

  std::string operator()(std::variant<TextResult, NumResult> var) const {
    return std::visit([](auto&& arg) -> std::string {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, TextResult>) {
        return arg.output;
      } else {
        return "NUM: " + std::to_string(arg.output);
      }
    }, var);
  }
};

// ---------------------------------------------------------------------------
// Pipeline definition
//
///   pb::from<Request>
///     ::branch<TextCase, NumCase>     // classify & dispatch
///     ::join<Unify>                    // visit variant → unified string
///     ::to<std::string>                // final sink type
///
/// Because TextRoute::output_type (TextResult) and NumRoute::output_type
/// (NumResult) differ, the branch output is automatically std::variant<TextResult, NumResult>.
/// The join stage's input_type matches this variant, satisfying connectable_v.
// ---------------------------------------------------------------------------

using TextCase = pb::case_<IsText>::then<TextRoute>;
using NumCase  = pb::case_<IsNumber>::then<NumRoute>;

using HeterogeneousPipeline = pb::from<Request>
                                  ::branch<TextCase, NumCase>
                                  ::join<Unify>
                                  ::to<std::string>;

// Compile-time validation
static_assert(pb::valid<HeterogeneousPipeline>,
              "HeterogeneousPipeline failed validation");
static_assert(pb::pipeline_size_v<HeterogeneousPipeline> == 2,
              "HeterogeneousPipeline: expected 2 runtime stages (1 branch + 1 join)");
static_assert(std::is_same_v<pb::pipeline_input_t<HeterogeneousPipeline>, Request>,
              "HeterogeneousPipeline input must be Request");
static_assert(std::is_same_v<pb::pipeline_output_t<HeterogeneousPipeline>, std::string>,
              "HeterogeneousPipeline output must be std::string");

// ---------------------------------------------------------------------------
// main — compile and run
// ---------------------------------------------------------------------------

int main() {
  auto engine = pb::compile<HeterogeneousPipeline>(pb::runtime::sequential{});

  auto run_req = [&](Request req) -> int {
    auto result = engine.run(std::move(req));
    if (!result.has_value()) {
      std::cerr << "  ERROR [" << result.error().stage.key
                << "]: " << result.error().message << '\n';
      return 1;
    }
    std::cout << "  Result: " << result.value() << '\n';
    return 0;
  };

  std::cout << "=== Heterogeneous Branch Routing Demo ===\n\n";

  std::cout << "[1] Routing a text request...\n";
  if (run_req(Request{"text", 42})) return 1;

  std::cout << "[2] Routing a number request...\n";
  if (run_req(Request{"number", 7})) return 2;

  // Unknown type: no predicate matches → contract violation error
  std::cout << "[3] Routing an unknown type...\n";
  if (run_req(Request{"unknown", 0})) {
    std::cout << "  (Expected: no predicate matched 'unknown')\n";
  }

  std::cout << "\nAll examples completed.\n";
  return 0;
}
