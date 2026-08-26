#include <pb/pipeline.hpp>

#include <type_traits>

// ---------------------------------------------------------------------------
// Stage fixtures
// ---------------------------------------------------------------------------
struct Raw     { int value{}; };
struct Parsed  { double value{}; };
struct Done    { bool ok{}; };

struct Parse {
  using input_type  = Raw;
  using output_type = Parsed;
  Parsed operator()(Raw r) const { return {static_cast<double>(r.value)}; }
};

struct Finish {
  using input_type  = Parsed;
  using output_type = Done;
  Done operator()(Parsed) const { return {}; }
};

// ---------------------------------------------------------------------------
// A two-stage pipeline: Raw -> Parse -> Finish -> Done
// ---------------------------------------------------------------------------
using P = pb::from<Raw>::then<Parse>::then<Finish>::to<Done>;

static_assert(pb::pipeline_size_v<P> == 2);

// pipeline_stage_input_t
static_assert(std::is_same_v<pb::pipeline_stage_input_t<P, 0>, Raw>);
static_assert(std::is_same_v<pb::pipeline_stage_input_t<P, 1>, Parsed>);

// pipeline_stage_output_t
static_assert(std::is_same_v<pb::pipeline_stage_output_t<P, 0>, Parsed>);
static_assert(std::is_same_v<pb::pipeline_stage_output_t<P, 1>, Done>);

// Symmetry: output of stage N == input of stage N+1 for linear pipelines
static_assert(std::is_same_v<pb::pipeline_stage_output_t<P, 0>,
                              pb::pipeline_stage_input_t<P, 1>>);

// Relation to the existing pipeline_stage_t alias
static_assert(std::is_same_v<pb::pipeline_stage_input_t<P, 0>,
                              pb::stage_input_t<pb::pipeline_stage_t<P, 0>>>);
static_assert(std::is_same_v<pb::pipeline_stage_output_t<P, 1>,
                              pb::stage_output_t<pb::pipeline_stage_t<P, 1>>>);

// ---------------------------------------------------------------------------
// One-stage pipeline
// ---------------------------------------------------------------------------
struct Identity {
  using input_type  = int;
  using output_type = int;
  int operator()(int x) const { return x; }
};
using P1 = pb::from<int>::then<Identity>::to<int>;
static_assert(std::is_same_v<pb::pipeline_stage_input_t<P1, 0>,  int>);
static_assert(std::is_same_v<pb::pipeline_stage_output_t<P1, 0>, int>);

int main() { return 0; }
