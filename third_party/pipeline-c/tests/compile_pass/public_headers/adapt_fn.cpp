#include <pb/adapt/fn.hpp>

struct Raw {
  int value{};
};

struct Parsed {
  int value{};
};

Parsed parse(Raw input) noexcept { return Parsed{input.value + 1}; }

struct parse_name {
  static constexpr auto value = "parse";
};

using ParseStage = pb::adapt_fn<&parse, Raw, Parsed, pb::no_error, parse_name>;
using DirectParseStage = pb::adapt<pb::fn<&parse>, pb::in<Raw>, pb::out<Parsed>>;

static_assert(pb::core::Stage<ParseStage>);
static_assert(pb::adapted_stage<ParseStage>);
static_assert(ParseStage::stage_name() == "parse");
static_assert(std::same_as<decltype(ParseStage{}(Raw{1})), Parsed>);
static_assert(noexcept(ParseStage{}(Raw{1})));
static_assert(pb::is_noexcept_stage_v<ParseStage, Raw>);
static_assert(pb::core::Stage<DirectParseStage>);
static_assert(pb::adapted_stage<DirectParseStage>);
static_assert(DirectParseStage::stage_name() == "unnamed");
static_assert(std::same_as<decltype(DirectParseStage{}(Raw{1})), Parsed>);

int pb_public_header_adapt_fn() { return 0; }
