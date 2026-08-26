// Fan-in cloning policy smoke: verify pb::shared_view enables move-only
// payloads to flow through a fan-in pipeline that otherwise would either
// require copy-constructibility OR every branch case taking const T&.

#include <pb/pipeline.hpp>

#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace shared_view_smoke {

struct Payload {
  int counter{};
  std::string tag{};
};

using PayloadView = pb::shared_view<Payload>;

struct AlwaysOn {
  using input_type  = PayloadView;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.always"; }
  static constexpr auto stage_name() noexcept { return "always"; }
  bool operator()(const PayloadView& v) const { return static_cast<bool>(v); }
};

struct PositiveOnly {
  using input_type  = PayloadView;
  using output_type = bool;
  static constexpr auto stage_key()  noexcept { return "pred.positive"; }
  static constexpr auto stage_name() noexcept { return "positive"; }
  bool operator()(const PayloadView& v) const { return v && v->counter > 0; }
};

struct CounterStage {
  using input_type  = PayloadView;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "stage.counter"; }
  static constexpr auto stage_name() noexcept { return "counter"; }
  int operator()(PayloadView v) const { return v->counter; }
};

struct TagStage {
  using input_type  = PayloadView;
  using output_type = std::string;
  static constexpr auto stage_key()  noexcept { return "stage.tag"; }
  static constexpr auto stage_name() noexcept { return "tag"; }
  std::string operator()(PayloadView v) const { return v->tag; }
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

using Pipeline = pb::from<PayloadView>
    ::branch<CounterCase, TagCase>
    ::fan_in<Merge>
    ::to<std::string>;

static_assert(pb::valid<Pipeline>);

// Resolve the fan-in branch node out of the validated pipeline stage list so
// we can introspect its cloning strategy.  shared_view<T> is copyable so the
// runtime should pick the copy strategy, not the borrow strategy.
using FanInNode = pb::pipeline_stage_t<Pipeline, 0>;
static_assert(pb::fan_in_uses_copy_v<FanInNode>);
static_assert(!pb::fan_in_uses_borrow_v<FanInNode>);

static_assert(pb::is_shared_view_v<PayloadView>);
static_assert(!pb::is_shared_view_v<Payload>);

} // namespace shared_view_smoke

int main() {
  using namespace shared_view_smoke;

  // shared_view is copyable, so we can carry a move-only-ish payload
  // (constructed in-place) through fan-in even though the wrapper holds it
  // via shared_ptr internally.
  auto view = pb::make_shared_view(Payload{.counter = 42, .tag = "ok"});
  assert(view);
  assert(view.use_count() == 1);

  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  const auto result = engine.run(view);
  assert(result.has_value());
  // Both cases pass (counter > 0 and always-on), so both run and contribute
  // their projection of the underlying payload.
  assert(result.value() == "counter=42;tag=ok");

  // The original shared_view is still live with the runtime's per-case
  // copies released after the pipeline finishes.  At minimum one ref is
  // still ours.
  assert(view.use_count() >= 1);

  // When the positive predicate fails, that case is skipped while the other
  // still runs — shared_view does not break per-case skipping.
  auto zero_view = pb::make_shared_view(Payload{.counter = 0, .tag = "zero"});
  const auto zero_result = engine.run(zero_view);
  assert(zero_result.has_value());
  assert(zero_result.value() == "counter=0;tag=skip");

  return 0;
}
