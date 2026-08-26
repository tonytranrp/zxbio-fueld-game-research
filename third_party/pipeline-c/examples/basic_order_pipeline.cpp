#include <pb/pipeline.hpp>

#if PB_EXAMPLE_HAS_PIPELINE_CORE && __has_include(<pb/runtime/sequential.hpp>)
#include <pb/runtime/sequential.hpp>
#define PB_EXAMPLE_HAS_SEQUENTIAL_RUNTIME 1
#else
#define PB_EXAMPLE_HAS_SEQUENTIAL_RUNTIME 0
#endif

#include <string>
#include <utility>

namespace domain {
struct RawText { std::string value; };
struct OrderDraft { std::string id; };
struct ValidatedOrder { std::string id; };
struct Receipt { std::string id; };
struct ParseError { std::string message; };
struct ValidationError { std::string message; };
} // namespace domain

namespace legacy {
auto parse_order(domain::RawText input) -> domain::OrderDraft {
  return domain::OrderDraft{std::move(input.value)};
}

struct validate_order {
  auto operator()(domain::OrderDraft draft) const -> domain::ValidatedOrder {
    return domain::ValidatedOrder{std::move(draft.id)};
  }
};
} // namespace legacy

namespace stage_names {
struct parse_order { static constexpr auto value = "parse_order"; };
struct validate_order { static constexpr auto value = "validate_order"; };
} // namespace stage_names

using ParseOrder = pb::adapt<pb::name<stage_names::parse_order>, pb::fn<legacy::parse_order>,
                             pb::in<domain::RawText>, pb::out<domain::OrderDraft>,
                             pb::err<domain::ParseError>>;
using ValidateOrder = pb::adapt<pb::name<stage_names::validate_order>,
                                pb::functor<legacy::validate_order>, pb::in<domain::OrderDraft>,
                                pb::out<domain::ValidatedOrder>,
                                pb::err<domain::ValidationError>>;

struct PersistOrder {
  using input_type = domain::ValidatedOrder;
  using output_type = domain::Receipt;
  using error_type = pb::no_error;
  static constexpr auto stage_name() noexcept { return "persist_order"; }
  auto operator()(domain::ValidatedOrder order) const -> domain::Receipt {
    return domain::Receipt{std::move(order.id)};
  }
};

using OrderPipeline = pb::from<domain::RawText>
                          ::then<ParseOrder>
                          ::then<ValidateOrder>
                          ::then<PersistOrder>
                          ::to<domain::Receipt>;

static_assert(pb::adapted_stage<ParseOrder>);
static_assert(pb::adapted_stage<ValidateOrder>);
static_assert(pb::valid<OrderPipeline>);
static_assert(ParseOrder::stage_name() == "parse_order");

int main() {
  auto engine = pb::compile<OrderPipeline>(pb::runtime::sequential{});
  auto receipt = engine.run(domain::RawText{"order-1002"});
  return receipt.id == "order-1002" ? 0 : 1;
}
