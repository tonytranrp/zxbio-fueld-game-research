/// @file policy_dsl_demo.cpp
/// @brief Demonstrates the runtime-enforced `::with<pb::policy::...>` DSL.
///
/// This example shows the research-plan "DSL for Policies" syntax (Part 4)
/// working end to end: an error/diagnostics policy is attached to the pipeline
/// *type* via `::with<...>`, and `pb::compile<P>(pb::runtime::sequential{})`
/// transparently selects the matching runtime engine wrapper — the call site
/// never names a wrapper explicitly.
///
///   - `::with<pb::policy::errors::throwing>`  -> run() throws on stage failure
///   - `::with<pb::policy::errors::ignoring>`  -> run() returns a fallback value
///   - `::with<pb::policy::diagnostics::verbose>` -> emits `[pb.verbose]` events
///   - no marker                                -> baseline result-returning engine
///
/// Build/run as the `pb_policy_dsl_demo` example target. main() returns 0.

#include <pb/pipeline.hpp>

#include <iostream>
#include <sstream>

namespace {

struct Order {
  int amount{};
};

struct Receipt {
  int amount{};
};

// A stage that succeeds: stamps the order into a receipt.
struct Stamp {
  using input_type = Order;
  using output_type = Receipt;
  static constexpr auto stage_key() noexcept { return "stamp"; }
  static constexpr auto stage_name() noexcept { return "stamp"; }
  Receipt operator()(Order order) const { return {order.amount}; }
};

// A stage that reports a declared (result<>-based) failure.
struct Reject {
  using input_type = Order;
  using output_type = Receipt;
  static constexpr auto stage_key() noexcept { return "reject"; }
  static constexpr auto stage_name() noexcept { return "reject"; }
  pb::runtime::result<Receipt> operator()(Order) const {
    return pb::runtime::result<Receipt>{pb::runtime::error{
        .category = pb::runtime::error_category::stage_failure,
        .message = "order rejected by policy",
    }};
  }
};

// Pipelines built purely through the type-level DSL. The policy marker is part
// of the TYPE; nothing at the call site selects a wrapper.
using ThrowingPipeline =
    pb::from<Order>::with<pb::policy::errors::throwing>::then<Reject>::to<Receipt>;
using IgnoringPipeline =
    pb::from<Order>::with<pb::policy::errors::ignoring>::then<Reject>::to<Receipt>;
using VerbosePipeline =
    pb::from<Order>::with<pb::policy::diagnostics::verbose>::then<Stamp>::to<Receipt>;
using PlainPipeline = pb::from<Order>::then<Stamp>::to<Receipt>;

// Compile-time confirmation that each axis is detected independently.
static_assert(pb::has_error_policy_v<ThrowingPipeline>);
static_assert(pb::has_error_policy_v<IgnoringPipeline>);
static_assert(pb::has_diagnostics_policy_v<VerbosePipeline>);
static_assert(!pb::has_error_policy_v<PlainPipeline>);
static_assert(!pb::has_diagnostics_policy_v<PlainPipeline>);

} // namespace

int main() {
  // 1. errors::throwing — a declared stage failure surfaces as an exception.
  {
    auto engine = pb::compile<ThrowingPipeline>(pb::runtime::sequential{});
    try {
      (void)engine.run(Order{.amount = 100});
      std::cout << "throwing: UNEXPECTED success\n";
    } catch (const pb::pipeline_exception& ex) {
      std::cout << "throwing: caught exception from stage '"
                << ex.diagnostic().stage.key << "'\n";
    }
  }

  // 2. errors::ignoring — the same failure yields a value-initialized fallback.
  {
    auto engine = pb::compile<IgnoringPipeline>(pb::runtime::sequential{});
    const auto receipt = engine.run(Order{.amount = 100});
    std::cout << "ignoring: fallback receipt amount = " << receipt.amount << "\n";
  }

  // 3. diagnostics::verbose — lifecycle events are written to a sink.
  {
    auto engine = pb::compile<VerbosePipeline>(pb::runtime::sequential{});
    std::ostringstream sink;
    engine.set_sink(&sink);
    const auto receipt = engine.run(Order{.amount = 42});
    std::cout << "verbose: receipt amount = " << receipt.amount
              << "; emitted " << sink.str().size() << " bytes of [pb.verbose] events\n";
  }

  // 4. No marker — baseline result-returning engine, unchanged behaviour.
  {
    auto engine = pb::compile<PlainPipeline>(pb::runtime::sequential{});
    const auto receipt = engine.run(Order{.amount = 7});
    std::cout << "plain: receipt amount = " << receipt.amount << "\n";
  }

  return 0;
}
