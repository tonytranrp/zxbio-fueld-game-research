#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};

Parsed parse_receipt(Receipt) { return {}; }

struct parse_receipt_stage { static constexpr auto value = "parse_receipt"; };

using BrokenAdaptedStage = pb::adapt<pb::name<parse_receipt_stage>, pb::fn<parse_receipt>,
                                     pb::in<Raw>, pb::out<Parsed>>;

static_assert(pb::Stage<BrokenAdaptedStage>);
static_assert(pb::connectable_v<Raw, BrokenAdaptedStage>);
static_assert(pb::adapted_stage<BrokenAdaptedStage>,
              "Adapted stage callable mismatch: declared input_type must be invocable by wrapped callable");
