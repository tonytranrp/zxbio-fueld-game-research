#include <pb/pipeline.hpp>

#include <string>
#include <utility>

namespace domain {
struct RawText { std::string value; };
struct OrderDraft { std::string id; };
struct Receipt { std::string id; };
struct ParseError { std::string message; };
} // namespace domain

namespace legacy {
auto parse_order(domain::RawText input) -> domain::OrderDraft {
  return domain::OrderDraft{std::move(input.value)};
}
} // namespace legacy

namespace stage_names {
struct parse_order { static constexpr auto value = "parse_order"; };
} // namespace stage_names

using ParseOrder = pb::adapt<pb::name<stage_names::parse_order>, pb::fn<legacy::parse_order>,
                             pb::in<domain::RawText>, pb::out<domain::OrderDraft>,
                             pb::err<domain::ParseError>>;

struct PersistOrder {
  using input_type = domain::Receipt; // Intentional mismatch: ParseOrder produces OrderDraft.
  using output_type = domain::Receipt;
  using error_type = pb::no_error;
  static constexpr auto stage_name() noexcept { return "persist_order"; }
  auto operator()(domain::Receipt receipt) const -> domain::Receipt { return receipt; }
};

#if defined(PB_EXAMPLE_ENABLE_COMPILE_FAILURE)
using BrokenPipeline = pb::from<domain::RawText>
                           ::then<ParseOrder>
                           ::then<PersistOrder>
                           ::to<domain::Receipt>;
static_assert(pb::valid<BrokenPipeline>);
#endif

int main() {
  auto draft = ParseOrder{}(domain::RawText{"order-1001"});
  (void)draft;
  return 0;
}
