#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};

struct Parser {
  Parsed operator()(Raw) const { return {}; }
};

using BrokenAdaptedStage = pb::adapt_functor<Parser, Raw, Receipt>;

static_assert(pb::adapted_stage<BrokenAdaptedStage>,
              "Adapted stage output mismatch: declared output_type must match the wrapped callable result");
