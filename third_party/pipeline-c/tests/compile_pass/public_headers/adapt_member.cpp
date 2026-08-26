#include <pb/adapt/fn.hpp>

#include <string_view>

struct Counter {
  int bump(int value) const { return value + 1; }
};

struct named_bump {
  static constexpr std::string_view value{"counter.bump"};
};

using BumpStage = pb::adapt_member<&Counter::bump, int, int>;
using DirectNamedBumpStage = pb::adapt<pb::name<named_bump>, pb::member<&Counter::bump>, pb::in<int>, pb::out<int>>;
using DirectUnnamedBumpStage = pb::adapt<pb::member<&Counter::bump>, pb::in<int>, pb::out<int>>;

static_assert(pb::core::Stage<BumpStage>);
static_assert(std::same_as<decltype(BumpStage{}(41)), int>);
static_assert(pb::adapted_stage<BumpStage>);
static_assert(pb::core::Stage<DirectNamedBumpStage>);
static_assert(std::same_as<decltype(DirectNamedBumpStage{}(41)), int>);
static_assert(DirectNamedBumpStage::stage_name() == "counter.bump");
static_assert(pb::adapted_stage<DirectNamedBumpStage>);
static_assert(pb::core::Stage<DirectUnnamedBumpStage>);
static_assert(std::same_as<decltype(DirectUnnamedBumpStage{}(41)), int>);
static_assert(DirectUnnamedBumpStage::stage_name() == "unnamed");
static_assert(pb::adapted_stage<DirectUnnamedBumpStage>);
