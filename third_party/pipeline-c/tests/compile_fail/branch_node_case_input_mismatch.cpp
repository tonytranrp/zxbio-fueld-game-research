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

// Phase 5 branch nodes describe one source fan-out; cases with different
// source inputs must be rejected before any runtime topology exists.
using BadBranch = pb::branch_node<RawCase, OtherCase>;
static_assert(sizeof(BadBranch) > 0);

int main() { return 0; }
