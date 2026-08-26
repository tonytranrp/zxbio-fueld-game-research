#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};

Parsed parse(Raw) { return {}; }

using BrokenAdaptedStage = pb::adapt_fn<&parse, Raw, Receipt>;

static_assert(pb::adapted_stage<BrokenAdaptedStage>,
              "Adapted stage output mismatch: declared output_type must match the wrapped callable result");
