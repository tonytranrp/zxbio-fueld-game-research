/// @file  thread_pool_fan_in_demo.cpp
/// @brief User-facing demonstration of fan-in execution on the thread-pool backend.
///
/// This example models an order-enrichment pipeline that scores a single
/// incoming Order through several *independent* scoring / validation cases
/// running concurrently, then joins their results into a final risk decision:
///
///   1. Receive a raw Order.
///   2. Fan out to independent cases — each guarded by a predicate — that run
///      in parallel on a pb::runtime::thread_pool_backend:
///        • FraudScore        — looks up a fraud heuristic
///        • CreditScore       — evaluates the customer's credit headroom
///        • InventoryCheck    — confirms the items can be fulfilled
///        • ComplianceReview  — only runs for high-value orders (predicate-gated)
///   3. Join all case results through Decide, which produces an OrderDecision.
///
/// The fan-in branch node aggregates each case into a typed slot accessible via
/// `get<N>()`. Each slot reports whether its case completed(), was skipped()
/// (predicate returned false), or failed() — and the aggregate slot ORDER is
/// deterministic (slot N always corresponds to case N) regardless of the
/// non-deterministic order in which the worker threads actually finish.
///
/// Every type relationship is verified at compile-time — miswiring a predicate
/// input_type or a case output_type is a compiler error, not a runtime surprise.
///
/// Build & run:
///   cmake --build build --target pb_thread_pool_fan_in_demo
///   ctest --test-dir build -R pb_example_thread_pool_fan_in_demo

#include <pb/pipeline.hpp>

#include <iostream>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// Domain types
// ---------------------------------------------------------------------------

/// Incoming order to be enriched / validated.
struct Order {
  int    id{};
  int    amount{};   ///< Order value in whole currency units.
  bool   risky{};    ///< Pre-flagged as potentially fraudulent.
  bool   in_stock{}; ///< Whether the warehouse reports stock.
};

/// A single independent score / check result.
struct Score {
  std::string label;
  int         value{};
};

/// Final aggregated decision produced by the join stage.
struct OrderDecision {
  int         order_id{};
  int         total_score{};
  std::string summary;
};

// ---------------------------------------------------------------------------
// Predicate stages — decide whether each fan-in case should run.
//
// Fan-in predicates must accept the input by const& (every predicate inspects
// the same input before any passing case receives its own copy) and return bool.
// ---------------------------------------------------------------------------

struct AlwaysRun {
  using input_type  = Order;
  using output_type = bool;
  bool operator()(const Order&) const { return true; }
};

/// Compliance review only fires for high-value orders — demonstrates a
/// predicate-skipped slot in the aggregate.
struct IsHighValue {
  using input_type  = Order;
  using output_type = bool;
  bool operator()(const Order& order) const { return order.amount >= 1000; }
};

// ---------------------------------------------------------------------------
// Case stages — each produces a Score. They run concurrently on the pool, so
// they must be independently computable from the (shared, const) Order input.
// ---------------------------------------------------------------------------

struct FraudScore {
  using input_type  = Order;
  using output_type = Score;
  Score operator()(const Order& order) const {
    // Simulate a little work so the pool genuinely overlaps the cases.
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    return Score{"fraud", order.risky ? 80 : 5};
  }
};

struct CreditScore {
  using input_type  = Order;
  using output_type = Score;
  Score operator()(const Order& order) const {
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    return Score{"credit", order.amount > 500 ? 30 : 10};
  }
};

struct InventoryCheck {
  using input_type  = Order;
  using output_type = Score;
  Score operator()(const Order& order) const {
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    return Score{"inventory", order.in_stock ? 0 : 50};
  }
};

struct ComplianceReview {
  using input_type  = Order;
  using output_type = Score;
  Score operator()(const Order& order) const {
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    return Score{"compliance", order.amount / 100};
  }
};

// ---------------------------------------------------------------------------
// Case wiring — each case pairs a predicate with the stage to run when it holds.
// ---------------------------------------------------------------------------

using FraudCase      = pb::case_<AlwaysRun>::then<FraudScore>;
using CreditCase     = pb::case_<AlwaysRun>::then<CreditScore>;
using InventoryCase  = pb::case_<AlwaysRun>::then<InventoryCheck>;
using ComplianceCase = pb::case_<IsHighValue>::then<ComplianceReview>;

/// The join stage's input is the aggregate of every case's typed slot. The
/// `fan_in_output_t` helper derives that aggregate type from the case list.
using FanInResults = pb::fan_in_output_t<FraudCase, CreditCase, InventoryCase, ComplianceCase>;

// ---------------------------------------------------------------------------
// Join stage — receives the aggregated slots (in deterministic case order) and
// folds them into the final OrderDecision.
// ---------------------------------------------------------------------------

struct Decide {
  using input_type  = FanInResults;
  using output_type = OrderDecision;

  OrderDecision operator()(const FanInResults& results) const {
    int total = 0;
    std::string summary;

    // Slot 0 = FraudCase, 1 = CreditCase, 2 = InventoryCase, 3 = ComplianceCase.
    // Ordering is fixed by the case list, NOT by thread completion order.
    if (results.template get<0>().completed()) {
      const auto& s = results.template get<0>().get();
      total += s.value;
      summary += s.label + "=" + std::to_string(s.value) + " ";
    }
    if (results.template get<1>().completed()) {
      const auto& s = results.template get<1>().get();
      total += s.value;
      summary += s.label + "=" + std::to_string(s.value) + " ";
    }
    if (results.template get<2>().completed()) {
      const auto& s = results.template get<2>().get();
      total += s.value;
      summary += s.label + "=" + std::to_string(s.value) + " ";
    }
    if (results.template get<3>().completed()) {
      const auto& s = results.template get<3>().get();
      total += s.value;
      summary += s.label + "=" + std::to_string(s.value) + " ";
    } else if (results.template get<3>().skipped()) {
      summary += "compliance=skipped ";
    }

    return OrderDecision{0, total, summary};
  }
};

// ---------------------------------------------------------------------------
// Pipeline definition:
//
//   pb::from<Order>
//     ::branch<FraudCase, CreditCase, InventoryCase, ComplianceCase>
//     ::fan_in<Decide>
//     ::to<OrderDecision>
// ---------------------------------------------------------------------------

using OrderPipeline = pb::from<Order>
                          ::branch<FraudCase, CreditCase, InventoryCase, ComplianceCase>
                          ::fan_in<Decide>
                          ::to<OrderDecision>;

static_assert(pb::valid<OrderPipeline>, "OrderPipeline failed validation");
static_assert(std::is_same_v<pb::pipeline_input_t<OrderPipeline>, Order>,
              "OrderPipeline input must be Order");
static_assert(std::is_same_v<pb::pipeline_output_t<OrderPipeline>, OrderDecision>,
              "OrderPipeline output must be OrderDecision");

// ---------------------------------------------------------------------------
// main — compile the pipeline onto the thread-pool backend and run examples.
// ---------------------------------------------------------------------------

int main() {
  // Compile onto a thread-pool backend: each passing fan-in case is dispatched
  // to a worker thread and runs concurrently. The aggregate it returns still
  // preserves deterministic, case-indexed slot ordering.
  auto engine = pb::compile<OrderPipeline>(pb::runtime::thread_pool_backend{.worker_count = 4});

  std::cout << "=== Thread-Pool Fan-In Order Enrichment ===\n";
  std::cout << "Worker threads: " << engine.worker_count() << "\n\n";

  auto run_order = [&](Order order) {
    auto result = engine.run(order);
    if (!result.has_value()) {
      std::cerr << "  ERROR [" << result.error().stage.key
                << "]: " << result.error().message << '\n';
      return false;
    }
    const auto& decision = result.value();
    std::cout << "  Order #" << order.id
              << "  total_score=" << decision.total_score
              << "  [" << decision.summary << "]\n";
    return true;
  };

  // A high-value, in-stock order — every case runs (including compliance).
  std::cout << "[1] High-value, in-stock order...\n";
  if (!run_order(Order{.id = 101, .amount = 1500, .risky = false, .in_stock = true})) return 1;

  // A low-value risky order — compliance is predicate-skipped; fraud scores high.
  std::cout << "[2] Low-value risky / out-of-stock order...\n";
  if (!run_order(Order{.id = 102, .amount = 200, .risky = true, .in_stock = false})) return 2;

  std::cout << "\nAll fan-in cases joined in deterministic case order.\n";
  return 0;
}
