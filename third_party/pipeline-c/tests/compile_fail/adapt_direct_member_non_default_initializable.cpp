#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct StatefulParser {
  explicit StatefulParser(int) {}

  Parsed parse(Raw) const { return {}; }
};

using BrokenDirectMember = pb::adapt_member<&StatefulParser::parse, Raw, Parsed>;

static_assert(pb::adapted_stage<BrokenDirectMember>,
              "Adapted stage member object must be default-initializable");
