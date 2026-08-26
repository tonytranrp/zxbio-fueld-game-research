#include <pb/pipeline.hpp>

struct Raw {};
struct OtherRaw {};
struct Parsed {};
struct OtherParsed {};

struct RawPredicate {
  using input_type = Raw;
  using output_type = bool;
};

struct OtherPredicate {
  using input_type = OtherRaw;
  using output_type = bool;
};

struct ParseRaw {
  using input_type = Raw;
  using output_type = Parsed;
};

struct ParseOther {
  using input_type = OtherRaw;
  using output_type = OtherParsed;
};

using RawCase = pb::case_<RawPredicate>::then<ParseRaw>;
using OtherCase = pb::case_<OtherPredicate>::then<ParseOther>;

// Branch output packs describe one source fan-out; mixed source inputs cannot
// be validated as one branch output set.
using BadOutputs = pb::branch_outputs<RawCase, OtherCase>;
static_assert(sizeof(BadOutputs) > 0);

int main() { return 0; }
