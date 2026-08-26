#include <pb/pipeline.hpp>

struct Raw {};
struct Parsed {};

struct IsRaw {
  using input_type = Raw;
  using output_type = bool;
};

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
};

struct ManualReview {
  using input_type = Raw;
  using output_type = Parsed;
};

// The eventual public ::branch builder must start from the same source type
// consumed by every branch case. A linear prefix that currently yields Parsed
// cannot branch into cases that consume Raw.
using BadBranch = pb::from<Parsed>::branch<pb::case_<IsRaw>::then<Parse>, pb::case_<IsRaw>::then<ManualReview>>;
static_assert(sizeof(BadBranch) > 0);

int main() { return 0; }
