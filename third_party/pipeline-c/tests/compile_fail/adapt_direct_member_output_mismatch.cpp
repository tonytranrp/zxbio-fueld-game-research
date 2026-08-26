#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};
struct Receipt {};

struct Parser {
  Parsed parse(Raw) const { return {}; }
};

// Wrong output type for direct unnamed member adapter; should fail adapted_stage output validation.
using BrokenDirectMember = pb::adapt<pb::member<&Parser::parse>, pb::in<Raw>, pb::out<Receipt>>;

static_assert(pb::adapted_stage<BrokenDirectMember>,
              "Adapted stage output mismatch: declared output_type must match the wrapped callable result");
