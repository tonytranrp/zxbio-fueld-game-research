// pb::projected smoke: wrap a stage that operates on a sub-view of the
// pipeline input so the same stage can run inside a linear chain AND a
// fan-in branch without rewriting the stage itself.

#include <pb/pipeline.hpp>

#include <cassert>
#include <memory>
#include <string>
#include <utility>

namespace projected_smoke {

// Big, move-only-ish payload — only the wrapper owns the unique_ptr.
struct Order {
  std::shared_ptr<int> header;   // shared_ptr so the wrapper is copyable
  std::string body;
};

struct Header { int value{}; };

// Existing stage that operates on a Header — we want to reuse it as a
// fan-in case stage over an Order pipeline.
struct TagHeader {
  using input_type  = Header;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "tag.header"; }
  static constexpr auto stage_name() noexcept { return "tag.header"; }
  int operator()(Header header) const { return header.value * 2; }
};

struct ProjectHeader {
  Header operator()(const Order& order) const { return Header{*order.header}; }
};

using TagOrderHeader = pb::projected<Order, ProjectHeader, TagHeader>;

static_assert(std::is_same_v<TagOrderHeader::input_type, Order>);
static_assert(std::is_same_v<TagOrderHeader::output_type, int>);
static_assert(pb::Stage<TagOrderHeader>);

// Use the projected stage inside a normal linear pipeline.
using Pipeline = pb::from<Order>::then<TagOrderHeader>::to<int>;
static_assert(pb::valid<Pipeline>);

// And inside a fan-in branch — the projection borrows the Order, so the
// branch lowering uses the copy strategy (shared_ptr copies are cheap).
struct AlwaysOn {
  using input_type  = Order;
  using output_type = bool;
  bool operator()(const Order& order) const { return order.header != nullptr; }
};

struct PassthroughInt {
  using input_type  = int;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "passthrough"; }
  int operator()(int v) const { return v; }
};

using TagCase    = pb::case_<AlwaysOn>::then<TagOrderHeader>;
using FanIn      = pb::fan_in_output_t<TagCase>;

struct CollectFanIn {
  using input_type  = FanIn;
  using output_type = int;
  static constexpr auto stage_key()  noexcept { return "collect"; }
  int operator()(FanIn aggregate) const {
    if (aggregate.template get<0>().selected()) {
      return aggregate.template get<0>().get();
    }
    return -1;
  }
};

using FanInPipeline = pb::from<Order>::branch<TagCase>::fan_in<CollectFanIn>::to<int>;
static_assert(pb::valid<FanInPipeline>);

} // namespace projected_smoke

int main() {
  using namespace projected_smoke;

  Order order{.header = std::make_shared<int>(21), .body = "ignored"};

  // ── Linear projection: TagHeader runs over the projected Header. ──
  {
    auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
    const auto result = engine.run(order);
    assert(result == 42);
  }

  // ── Fan-in projection: same wrapped stage, used as a case. ──
  {
    auto engine = pb::compile<FanInPipeline>(pb::runtime::sequential{});
    const auto outcome = engine.run(order);
    assert(outcome.has_value());
    assert(outcome.value() == 42);
  }

  return 0;
}
