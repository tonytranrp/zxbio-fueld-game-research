#include <pb/pipeline.hpp>

#include <utility>
#include <string_view>
#include <type_traits>

struct Raw {
  int value{};
};

struct Parsed {
  int value{};
};

struct Receipt {
  int value{};
};

struct parse_stage_name {
  static constexpr std::string_view value{"parse"};
};

struct finish_stage_name {
  static constexpr std::string_view value{"finish"};
};

struct Parser {
  Parsed operator()(Raw input) const { return {input.value + 1}; }
};

struct Finisher {
  Receipt operator()(Parsed input) const { return {input.value * 2}; }
};

struct NoexceptParser {
  Parsed operator()(Raw input) const noexcept { return {input.value + 3}; }
};

struct MoveOnlyRaw {
  int value{};
  MoveOnlyRaw() = default;
  explicit MoveOnlyRaw(int initial) : value{initial} {}
  MoveOnlyRaw(const MoveOnlyRaw&) = delete;
  MoveOnlyRaw& operator=(const MoveOnlyRaw&) = delete;
  MoveOnlyRaw(MoveOnlyRaw&&) = default;
  MoveOnlyRaw& operator=(MoveOnlyRaw&&) = default;
};

struct MoveOnlyParser {
  Parsed operator()(MoveOnlyRaw input) const { return {input.value + 4}; }
};

using ParseStage = pb::adapt_functor<Parser, Raw, Parsed, pb::no_error, parse_stage_name>;
using FinishStage = pb::adapt_functor<Finisher, Parsed, Receipt, pb::no_error, finish_stage_name>;
using NoexceptParseStage = pb::adapt_functor<NoexceptParser, Raw, Parsed>;
using MoveOnlyParseStage = pb::adapt_functor<MoveOnlyParser, MoveOnlyRaw, Parsed>;
using Pipeline = pb::from<Raw>::then<ParseStage>::then<FinishStage>::to<Receipt>;

static_assert(pb::adapted_stage<ParseStage>);
static_assert(pb::adapted_stage<FinishStage>);
static_assert(pb::connectable_v<Raw, ParseStage>);
static_assert(pb::AdjacentStages<ParseStage, FinishStage>);
static_assert(pb::valid<Pipeline>);
static_assert(pb::stage_name<ParseStage>() == std::string_view{"parse"});
static_assert(pb::stage_name<FinishStage>() == std::string_view{"finish"});
static_assert(std::same_as<decltype(ParseStage{}(Raw{1})), Parsed>);
static_assert(std::same_as<decltype(FinishStage{}(Parsed{2})), Receipt>);
static_assert(noexcept(NoexceptParseStage{}(Raw{1})));
static_assert(pb::is_noexcept_stage_v<NoexceptParseStage, Raw>);
static_assert(!pb::is_noexcept_stage_v<ParseStage, Raw>);
static_assert(std::same_as<decltype(MoveOnlyParseStage{}(MoveOnlyRaw{1})), Parsed>);

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto receipt = engine.run(Raw{20});
  auto moved = MoveOnlyParseStage{}(MoveOnlyRaw{5});
  return receipt.value == 42 && moved.value == 9 ? 0 : 1;
}
