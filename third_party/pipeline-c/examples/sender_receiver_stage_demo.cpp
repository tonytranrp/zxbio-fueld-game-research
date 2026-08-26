/// @file sender_receiver_stage_demo.cpp
/// @brief Demonstrates the dependency-free synchronous sender/receiver stage scaffold.
///
/// `pb::sync_sender_stage<Factory, Input>` adapts a factory that returns a
/// sender-like object with `connect(receiver).start()` into an ordinary Pipeline-c++
/// stage.  The scaffold is intentionally synchronous and value-only: it is a
/// standard-library-only seam for experimenting with sender-shaped adapters, not
/// a stdexec/P2300 backend and not an asynchronous scheduler.
///
/// Runs as the `pb_sender_receiver_stage_demo` example target. main() returns 0.

#include <pb/pipeline.hpp>
#include <pb/adapt/sender_receiver.hpp>

#include <iostream>
#include <string>
#include <utility>

namespace {

struct RawOrder {
  int amount{};
};

struct ValidatedOrder {
  int amount{};
};

struct RiskScore {
  int score{};
};

struct Receipt {
  std::string id;
};

struct ValidateOrder {
  using input_type = RawOrder;
  using output_type = ValidatedOrder;

  static constexpr auto stage_name() noexcept { return "validate order"; }

  ValidatedOrder operator()(RawOrder order) const { return ValidatedOrder{order.amount}; }
};

// A sender factory: the pipeline input arrives synchronously, and the factory
// returns a sender-like object that will complete with exactly one RiskScore
// when started by pb::sync_sender_stage.
struct MakeRiskScoreSender {
  using output_type = RiskScore;

  auto operator()(ValidatedOrder order) const {
    const int score = order.amount > 1000 ? 90 : 10;
    return pb::sync_just(RiskScore{score});
  }
};

struct FinalizeReceipt {
  using input_type = RiskScore;
  using output_type = Receipt;

  static constexpr auto stage_name() noexcept { return "finalize receipt"; }

  Receipt operator()(RiskScore score) const {
    return Receipt{score.score >= 50 ? "manual-review" : "auto-approved"};
  }
};

using RiskSenderStage = pb::sync_sender_stage<MakeRiskScoreSender, ValidatedOrder>;
using Pipeline = pb::from<RawOrder>
    ::then<ValidateOrder>
    ::then<RiskSenderStage>
    ::then<FinalizeReceipt>
    ::to<Receipt>;

static_assert(pb::valid<Pipeline>);

} // namespace

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  const auto low_risk = engine.run(RawOrder{.amount = 40});
  const auto high_risk = engine.run(RawOrder{.amount = 5000});

  std::cout << "low-risk order: " << low_risk.id << '\n';
  std::cout << "high-risk order: " << high_risk.id << '\n';

  return low_risk.id == "auto-approved" && high_risk.id == "manual-review" ? 0 : 1;
}
