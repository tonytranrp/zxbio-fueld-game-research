/// @file  branch_error_handling.cpp
/// @brief Demonstrates error propagation and diagnostics in branch pipelines.
///
/// This example covers three error scenarios:
///   1. A branch route stage that returns an explicit error (try_run path)
///   2. A contract violation when no predicate matches the input (try_run path)
///   3. Successful execution through try_run
///
/// Every error carries:
///   • stage.key   — which stage produced the error
///   • message     — human-readable description
///   • category    — stage_failure | contract_violation | exception | expected_error
///
/// Build & run:
///   cmake --build build --config Release --target pb_branch_error_handling
///   ctest --test-dir build -C Release -R branch_error_handling

#include <pb/pipeline.hpp>

#include <iostream>
#include <string>

// ---------------------------------------------------------------------------
// Domain types
// ---------------------------------------------------------------------------

/// Simple integer wrapper for pipeline input.
struct Input {
  int value{};
};

/// Simple integer wrapper for pipeline output.
struct Output {
  int result{};
};

// ---------------------------------------------------------------------------
// Predicate stages — route based on Input::value sign.
// ---------------------------------------------------------------------------

struct IsPositive {
  using input_type  = Input;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-positive"; }
  static constexpr auto stage_key()  noexcept { return "is-positive"; }

  bool operator()(const Input& in) const { return in.value > 0; }
};

struct IsZero {
  using input_type  = Input;
  using output_type = bool;

  static constexpr auto stage_name() noexcept { return "is-zero"; }
  static constexpr auto stage_key()  noexcept { return "is-zero"; }

  bool operator()(const Input& in) const { return in.value == 0; }
};

// ---------------------------------------------------------------------------
// Branch route stages
// ---------------------------------------------------------------------------

/// Happy path: doubles the input value.
struct Double {
  using input_type  = Input;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "double"; }
  static constexpr auto stage_key()  noexcept { return "double"; }

  Output operator()(const Input& in) const { return Output{in.value * 2}; }
};

/// Unhappy path: always returns an error for zero-valued inputs.
///
/// A branch stage may return pb::runtime::result<T> instead of T.
/// The runtime will unwrap the result — on success the value continues
/// through the pipeline; on failure the error is propagated with the
/// stage identity automatically attached.
struct FailingStage {
  using input_type  = Input;
  using output_type = Output;

  static constexpr auto stage_name() noexcept { return "failing-stage"; }
  static constexpr auto stage_key()  noexcept { return "failing-stage"; }

  pb::runtime::result<Output> operator()(const Input& /*in*/) const {
    return pb::runtime::result<Output>{
        pb::runtime::error{.message = "Zero input rejected by FailingStage"}
    };
  }
};

// ---------------------------------------------------------------------------
// Pipeline definitions
// ---------------------------------------------------------------------------

/// Pipeline with two cases:
///   PositiveCase — IsPositive predicate → Double route
///   ZeroCase     — IsZero predicate     → FailingStage route
///
/// Negative inputs match neither predicate → contract_violation error.
using PositiveCase = pb::case_<IsPositive>::then<Double>;
using ZeroCase     = pb::case_<IsZero>::then<FailingStage>;

using Pipeline = pb::from<Input>
                     ::branch<PositiveCase, ZeroCase>
                     ::to<Output>;

static_assert(pb::valid<Pipeline>, "Pipeline failed validation");
static_assert(pb::pipeline_size_v<Pipeline> == 1,
              "Pipeline: expected 1 runtime stage (branch node)");

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  std::cout << "=== Branch Error Handling ===\n\n";

  // ------------------------------------------------------------------
  // Scenario 1: Successful execution through try_run
  // ------------------------------------------------------------------
  // Input 5 → IsPositive matches → Double returns Output{10}
  // try_run wraps the value in result<Output>.
  {
    std::cout << "[1] try_run with positive input (should succeed)...\n";
    auto ok = engine.try_run(Input{5});
    if (ok.has_value()) {
      std::cout << "    Success: 5 doubled = " << ok.value().result << "\n\n";
    } else {
      std::cerr << "    UNEXPECTED ERROR: " << ok.error().message << "\n\n";
      return 1;
    }
  }

  // ------------------------------------------------------------------
  // Scenario 2: Branch stage returns an error
  // ------------------------------------------------------------------
  // Input 0 → IsZero matches → FailingStage returns an error.
  // The runtime annotates the error with the stage identity and
  // propagates it through try_run.
  {
    std::cout << "[2] try_run with zero input (should fail in FailingStage)...\n";
    auto err = engine.try_run(Input{0});
    if (!err.has_value()) {
      const auto& e = err.error();
      std::cout << "    Error stage  : " << e.stage.key << '\n';
      std::cout << "    Error message: " << e.message << '\n';
      std::cout << "    Category     : " << pb::category_name(e.category) << "\n\n";
    } else {
      std::cerr << "    UNEXPECTED SUCCESS: " << err.value().result << "\n\n";
      return 2;
    }
  }

  // ------------------------------------------------------------------
  // Scenario 3: Contract violation — no predicate matches
  // ------------------------------------------------------------------
  // Input -5 → IsPositive returns false, IsZero returns false.
  // No predicate matched → the branch node produces a contract_violation
  // error with a descriptive message.
  {
    std::cout << "[3] try_run with negative input (no predicate should match)...\n";
    auto no_match = engine.try_run(Input{-5});
    if (!no_match.has_value()) {
      const auto& e = no_match.error();
      std::cout << "    Error stage  : " << e.stage.key << '\n';
      std::cout << "    Error message: " << e.message << '\n';
      std::cout << "    Category     : " << pb::category_name(e.category) << '\n';

      // Confirm we received the expected contract_violation.
      if (e.category == pb::runtime::error_category::contract_violation) {
        std::cout << "    (Expected: contract_violation — no branch case matched)\n\n";
      }
    } else {
      std::cerr << "    UNEXPECTED SUCCESS: " << no_match.value().result << "\n\n";
      return 3;
    }
  }

  std::cout << "All error-handling examples completed.\n";
  return 0;
}
