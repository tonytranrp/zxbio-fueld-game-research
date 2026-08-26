#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};

struct Parser {
  Parsed parse(Raw) const { return {}; }
};

// Wrong output type for the adapt_member convenience alias; should fail the
// adapted_stage output validation before it can be used in a pipeline.
using BrokenAdaptedMember = pb::adapt_member<&Parser::parse, Raw, Receipt>;

static_assert(pb::adapted_stage<BrokenAdaptedMember>,
              "Adapted stage output mismatch: declared output_type must match the wrapped callable result");
