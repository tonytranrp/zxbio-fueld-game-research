#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};
struct User {
  Receipt operator()(Parsed) const { return {}; }
  void record(Receipt) const {}
};

Raw parse(std::string) { return {}; }
void consume_receipt(Receipt) {}

struct Parser {
  Receipt operator()(Parsed) const { return {}; }
};

struct ReceiptSink {
  void operator()(Receipt) const {}
};

struct Stage {
  using input_type = Raw;
  using output_type = Parsed;
  Parsed operator()(Raw) const { return {}; }
};

using StageFn = pb::adapt_fn<&parse, std::string, Raw>;
using StageMember = pb::adapt_member<&User::operator(), Parsed, Receipt>;
using StageFunctor = pb::adapt_functor<Parser, Parsed, Receipt>;
using VoidFnStage = pb::adapt_fn<&consume_receipt, Receipt, void>;
using VoidMemberStage = pb::adapt_member<&User::record, Receipt, void>;
using VoidFunctorStage = pb::adapt_functor<ReceiptSink, Receipt, void>;

using Pipeline = pb::from<std::string>::then<StageFn>::then<Stage>::then<StageMember>::to<Receipt>;
using VoidPipeline = pb::from<std::string>::then<StageFn>::then<Stage>::then<StageMember>::then<VoidFnStage>::to<void>;

static_assert(pb::ValidPipeline<Pipeline>);
static_assert(pb::ValidPipeline<VoidPipeline>);
static_assert(pb::connectable_v<std::string, StageFn>);
static_assert(pb::connectable_v<Raw, Stage>);
static_assert(pb::connectable_v<Parsed, StageMember>);
static_assert(pb::adapted_stage<StageFn>);
static_assert(pb::adapted_stage<StageMember>);
static_assert(pb::adapted_stage<StageFunctor>);
static_assert(pb::adapted_stage<VoidFnStage>);
static_assert(pb::adapted_stage<VoidMemberStage>);
static_assert(pb::adapted_stage<VoidFunctorStage>);
static_assert(std::same_as<decltype(VoidFnStage{}(Receipt{})), void>);
static_assert(std::same_as<decltype(VoidMemberStage{}(Receipt{})), void>);
static_assert(std::same_as<decltype(VoidFunctorStage{}(Receipt{})), void>);
