// Fan-in cloning policy smoke: verify pb::unique_clone gives each passing
// fan-in case its OWN independently-owned value (deep copy) instead of a
// shared view.  Contrast with fan_in_shared_view_smoke, where every case
// observes one shared underlying value.

#include <pb/pipeline.hpp>

#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace unique_clone_smoke {

// ---------------------------------------------------------------------------
// Part 1: a copy-constructible "Heavy" payload behind unique_clone.  We assert
// that each case receives a DISTINCT object (different address) and that
// mutating one case's copy does not leak into another's.
// ---------------------------------------------------------------------------

struct Heavy {
  int counter{};
  std::string tag{};
};

using HeavyClone = pb::unique_clone<Heavy>;

struct AlwaysOn {
  using input_type  = HeavyClone;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.always"; }
  static constexpr auto stage_name() noexcept { return "always"; }
  bool operator()(const HeavyClone& v) const { return v->counter >= 0; }
};

struct PositiveOnly {
  using input_type  = HeavyClone;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.positive"; }
  static constexpr auto stage_name() noexcept { return "positive"; }
  bool operator()(const HeavyClone& v) const { return v->counter > 0; }
};

// Each case mutates its own owned value before reading it; if cases aliased a
// shared value the mutation would corrupt the other case's result.
struct CounterStage {
  using input_type  = HeavyClone;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "stage.counter"; }
  static constexpr auto stage_name() noexcept { return "counter"; }
  int operator()(HeavyClone v) const {
    v->counter += 100;  // mutate this case's independent copy
    return v->counter;
  }
};

struct TagStage {
  using input_type  = HeavyClone;
  using output_type = std::string;
  static constexpr auto stage_key()  noexcept { return "stage.tag"; }
  static constexpr auto stage_name() noexcept { return "tag"; }
  std::string operator()(HeavyClone v) const {
    v->tag += "!";  // mutate this case's independent copy
    return v->tag;
  }
};

using CounterCase = pb::case_<AlwaysOn>::then<CounterStage>;
using TagCase     = pb::case_<PositiveOnly>::then<TagStage>;

using FanInAggregate = pb::fan_in_output_t<CounterCase, TagCase>;

struct Merge {
  using input_type  = FanInAggregate;
  using output_type = std::string;
  static constexpr auto stage_key()  noexcept { return "merge"; }
  static constexpr auto stage_name() noexcept { return "merge"; }
  std::string operator()(FanInAggregate aggregate) const {
    std::string out;
    if (aggregate.template get<0>().selected()) {
      out += "counter=" + std::to_string(aggregate.template get<0>().get());
    } else {
      out += "counter=skip";
    }
    out += ";";
    if (aggregate.template get<1>().selected()) {
      out += "tag=" + aggregate.template get<1>().get();
    } else {
      out += "tag=skip";
    }
    return out;
  }
};

using Pipeline = pb::from<HeavyClone>
    ::branch<CounterCase, TagCase>
    ::fan_in<Merge>
    ::to<std::string>;

static_assert(pb::valid<Pipeline>);

// unique_clone<T> is copy-constructible, so the runtime selects the copy
// strategy (each passing case receives its own deep copy), never the borrow
// strategy.
using FanInNode = pb::pipeline_stage_t<Pipeline, 0>;
static_assert(pb::fan_in_uses_copy_v<FanInNode>);
static_assert(!pb::fan_in_uses_borrow_v<FanInNode>);

static_assert(pb::is_unique_clone_v<HeavyClone>);
static_assert(!pb::is_unique_clone_v<Heavy>);
static_assert(!pb::is_unique_clone_v<pb::shared_view<Heavy>>);

// ---------------------------------------------------------------------------
// Part 2: a move-only-but-cloneable underlying type carried via a custom Clone
// functor.  This proves unique_clone supports types whose copy constructor is
// deleted but that still know how to deep-copy themselves.
// ---------------------------------------------------------------------------

struct MoveOnly {
  std::unique_ptr<int> value;

  explicit MoveOnly(int v) : value{std::make_unique<int>(v)} {}
  MoveOnly(const MoveOnly&)            = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;
  MoveOnly(MoveOnly&&) noexcept            = default;
  MoveOnly& operator=(MoveOnly&&) noexcept = default;
};

struct DeepCloneMoveOnly {
  MoveOnly operator()(const MoveOnly& src) const {
    return MoveOnly{*src.value};  // deep-copy the owned int into a fresh node
  }
};

using MoveOnlyClone = pb::unique_clone<MoveOnly, DeepCloneMoveOnly>;

static_assert(pb::is_unique_clone_v<MoveOnlyClone>);
static_assert(std::is_copy_constructible_v<MoveOnlyClone>);
static_assert(!std::is_copy_constructible_v<MoveOnly>);

} // namespace unique_clone_smoke

int main() {
  using namespace unique_clone_smoke;

  // Drive the fan-in pipeline.  Both predicates pass (counter > 0), so both
  // cases run, each on its OWN deep copy of the Heavy payload.
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto clone = pb::make_unique_clone(Heavy{.counter = 42, .tag = "ok"});
  const auto result = engine.run(clone);
  assert(result.has_value());
  // CounterStage added 100 to its own copy (42 -> 142); TagStage appended "!"
  // to its own copy ("ok" -> "ok!").  Neither mutation leaked across cases,
  // and the caller's original `clone` is untouched.
  assert(result.value() == "counter=142;tag=ok!");
  assert(clone->counter == 42);
  assert(clone->tag == "ok");

  // Independence check at the wrapper level: copying a unique_clone yields a
  // distinct underlying object (different address) and mutating one does not
  // touch the other.
  auto a = pb::make_unique_clone(Heavy{.counter = 7, .tag = "seven"});
  auto b = a;  // deep copy
  assert(std::addressof(*a) != std::addressof(*b));
  a->counter = 999;
  a->tag     = "changed";
  assert(b->counter == 7);
  assert(b->tag == "seven");

  // clone() also produces an independent value.
  Heavy detached = b.clone();
  detached.counter = -1;
  assert(b->counter == 7);

  // When the positive predicate fails (counter == 0) only the always-on case
  // runs; per-case cloning still respects skipping.
  auto zero = pb::make_unique_clone(Heavy{.counter = 0, .tag = "zero"});
  const auto zero_result = engine.run(zero);
  assert(zero_result.has_value());
  assert(zero_result.value() == "counter=100;tag=skip");

  // Move-only-but-cloneable underlying type via a custom Clone functor: copying
  // the wrapper deep-copies through DeepCloneMoveOnly into a fresh node.
  auto mo = pb::make_unique_clone<DeepCloneMoveOnly>(MoveOnly{5});
  assert(*mo->value == 5);
  auto mo_copy = mo;  // invokes DeepCloneMoveOnly
  assert(mo->value.get() != mo_copy->value.get());  // distinct owned nodes
  assert(*mo_copy->value == 5);
  *mo_copy->value = 11;
  assert(*mo->value == 5);  // original unaffected

  MoveOnly detached_mo = mo.clone();
  assert(*detached_mo.value == 5);

  return 0;
}
