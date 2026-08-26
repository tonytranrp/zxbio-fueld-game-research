#include <cassert>
#include <memory>
#include <stdexcept>
#include <utility>

#include <pb/pipeline.hpp>

namespace domain {

// Move-only input: can be moved but not copied.
struct MoveOnly {
  MoveOnly() = default;
  explicit MoveOnly(int sel, int val) : selector(sel), value(val) {}
  MoveOnly(MoveOnly&&) = default;
  MoveOnly& operator=(MoveOnly&&) = default;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  int selector{};
  int value{};
};

struct Routed {
  int value{};
};

struct OwningMoveOnly {
  OwningMoveOnly(int sel, std::unique_ptr<int> owned) : selector(sel), value(std::move(owned)) {}
  OwningMoveOnly(OwningMoveOnly&&) = default;
  OwningMoveOnly& operator=(OwningMoveOnly&&) = default;
  OwningMoveOnly(const OwningMoveOnly&) = delete;
  OwningMoveOnly& operator=(const OwningMoveOnly&) = delete;

  int selector{};
  std::unique_ptr<int> value;
};

// Predicates: inspect by const& (required for move-only inputs).
struct IsEven {
  using input_type = MoveOnly;
  using output_type = bool;

  bool operator()(const MoveOnly& input) const { return input.selector == 0; }
};

struct IsOdd {
  using input_type = MoveOnly;
  using output_type = bool;

  bool operator()(const MoveOnly& input) const { return input.selector == 1; }
};

struct IsDefault {
  using input_type = MoveOnly;
  using output_type = bool;

  bool operator()(const MoveOnly& input) const { return input.selector == 2; }
};

struct TakesOwningRoute {
  using input_type = OwningMoveOnly;
  using output_type = bool;

  bool operator()(const OwningMoveOnly& input) const {
    return input.selector == 7 && input.value != nullptr;
  }
};

struct IgnoresOwningRoute {
  using input_type = OwningMoveOnly;
  using output_type = bool;

  bool operator()(const OwningMoveOnly& input) const {
    return input.selector == 8 && input.value != nullptr;
  }
};

// Branch stages: accept by const& (for the common case).
// Move-only input is forwarded as Input&&, which binds to const Input&
// when the stage takes const&.
struct DoubleStage {
  using input_type = MoveOnly;
  using output_type = Routed;

  auto operator()(const MoveOnly& input) const -> Routed {
    ++call_count();
    return Routed{input.value * 2};
  }

  static int& call_count() {
    static int count = 0;
    return count;
  }
};

struct TripleStage {
  using input_type = MoveOnly;
  using output_type = Routed;

  auto operator()(const MoveOnly& input) const -> Routed {
    ++call_count();
    return Routed{input.value * 3};
  }

  static int& call_count() {
    static int count = 0;
    return count;
  }
};

struct AddOneStage {
  using input_type = MoveOnly;
  using output_type = Routed;

  auto operator()(const MoveOnly& input) const -> Routed {
    ++call_count();
    return Routed{input.value + 1};
  }

  static int& call_count() {
    static int count = 0;
    return count;
  }
};

struct ConsumeByValueStage {
  using input_type = OwningMoveOnly;
  using output_type = Routed;

  auto operator()(OwningMoveOnly input) const -> Routed {
    assert(input.value != nullptr);
    ++call_count();
    return Routed{*input.value + input.selector};
  }

  static int& call_count() {
    static int count = 0;
    return count;
  }
};

struct UnselectedOwningStage {
  using input_type = OwningMoveOnly;
  using output_type = Routed;

  auto operator()(OwningMoveOnly input) const -> Routed {
    assert(input.value != nullptr);
    ++call_count();
    return Routed{*input.value};
  }

  static int& call_count() {
    static int count = 0;
    return count;
  }
};

using CaseEven = pb::case_<IsEven>::then<DoubleStage>;
using CaseOdd = pb::case_<IsOdd>::then<TripleStage>;
using CaseDefault = pb::case_<IsDefault>::then<AddOneStage>;

using BranchPipeline = pb::from<MoveOnly>::branch<CaseEven, CaseOdd, CaseDefault>::to<Routed>;
using OwningCase = pb::case_<TakesOwningRoute>::then<ConsumeByValueStage>;
using UnselectedOwningCase = pb::case_<IgnoresOwningRoute>::then<UnselectedOwningStage>;
using OwningBranchPipeline = pb::from<OwningMoveOnly>::branch<OwningCase, UnselectedOwningCase>::to<Routed>;

struct CountingObserver : pb::runtime::observer {
  void on_case_selected(const pb::runtime::stage_id&, std::size_t case_index,
                        const pb::runtime::stage_id&) override {
    last_selected_case = case_index;
    ++selection_count;
  }

  void on_case_skipped(const pb::runtime::stage_id&, std::size_t /*case_index*/,
                       const pb::runtime::stage_id&) override {
    ++skip_count;
  }

  std::size_t last_selected_case = static_cast<std::size_t>(-1);
  int selection_count = 0;
  int skip_count = 0;
};

} // namespace domain

void move_only_branch_selection_smoke() {
  auto engine = pb::compile<domain::BranchPipeline>(pb::runtime::sequential{});

  // Reset call counters
  domain::DoubleStage::call_count() = 0;
  domain::TripleStage::call_count() = 0;
  domain::AddOneStage::call_count() = 0;

  // Select even branch (case 0) -> DoubleStage
  auto result0 = engine.run(domain::MoveOnly(0, 5));
  assert(result0.has_value());
  assert(result0.value().value == 10);
  assert(domain::DoubleStage::call_count() == 1);
  assert(domain::TripleStage::call_count() == 0);
  assert(domain::AddOneStage::call_count() == 0);

  // Select odd branch (case 1) -> TripleStage
  auto result1 = engine.run(domain::MoveOnly(1, 3));
  assert(result1.has_value());
  assert(result1.value().value == 9);
  assert(domain::DoubleStage::call_count() == 1);
  assert(domain::TripleStage::call_count() == 1);
  assert(domain::AddOneStage::call_count() == 0);

  // Select default branch (case 2) -> AddOneStage
  auto result2 = engine.run(domain::MoveOnly(2, 7));
  assert(result2.has_value());
  assert(result2.value().value == 8);
  assert(domain::DoubleStage::call_count() == 1);
  assert(domain::TripleStage::call_count() == 1);
  assert(domain::AddOneStage::call_count() == 1);

  // No predicate matches -> error
  auto result_no_match = engine.try_run(domain::MoveOnly(99, 1));
  assert(!result_no_match.has_value());
  assert(result_no_match.error().category == pb::runtime::error_category::contract_violation);
}

void move_only_branch_observer_smoke() {
  auto engine = pb::compile<domain::BranchPipeline>(pb::runtime::sequential{});
  domain::CountingObserver observer;
  engine.set_observer(&observer);

  auto result = engine.run(domain::MoveOnly(1, 4));
  assert(result.has_value());
  assert(result.value().value == 12);
  assert(observer.last_selected_case == 1);
  assert(observer.selection_count == 1);
  assert(observer.skip_count >= 1); // case 0 was skipped
}

void move_only_branch_stateful_smoke() {
  auto engine = pb::compile<domain::BranchPipeline>(pb::runtime::stateful_sequential{});

  domain::DoubleStage::call_count() = 0;
  domain::TripleStage::call_count() = 0;
  domain::AddOneStage::call_count() = 0;

  // Run twice — stateful stores stages, call counters persist
  auto result1 = engine.run(domain::MoveOnly(0, 3));
  assert(result1.has_value());
  assert(result1.value().value == 6);

  auto result2 = engine.run(domain::MoveOnly(1, 4));
  assert(result2.has_value());
  assert(result2.value().value == 12);
}

void move_only_branch_try_run_smoke() {
  auto engine = pb::compile<domain::BranchPipeline>(pb::runtime::sequential{});

  auto result = engine.try_run(domain::MoveOnly(0, 11));
  assert(result.has_value());
  assert(result.value().value == 22);

  auto fail_result = engine.try_run(domain::MoveOnly(99, 0));
  assert(!fail_result.has_value());
}

void move_only_branch_stage_consumes_by_value_smoke() {
  auto engine = pb::compile<domain::OwningBranchPipeline>(pb::runtime::sequential{});

  domain::ConsumeByValueStage::call_count() = 0;
  domain::UnselectedOwningStage::call_count() = 0;

  auto result = engine.run(domain::OwningMoveOnly{7, std::make_unique<int>(35)});
  assert(result.has_value());
  assert(result.value().value == 42);
  assert(domain::ConsumeByValueStage::call_count() == 1);
  assert(domain::UnselectedOwningStage::call_count() == 0);
}

int main() {
  move_only_branch_selection_smoke();
  move_only_branch_observer_smoke();
  move_only_branch_stateful_smoke();
  move_only_branch_try_run_smoke();
  move_only_branch_stage_consumes_by_value_smoke();
  return 0;
}
