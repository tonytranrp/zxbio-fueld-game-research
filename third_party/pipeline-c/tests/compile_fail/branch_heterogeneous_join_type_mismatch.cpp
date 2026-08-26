/// @file  branch_heterogeneous_join_type_mismatch.cpp
/// @brief Compile-fail test: join stage with wrong variant type (missing a branch output type).
///
/// When branch cases produce heterogeneous outputs (OutputA, OutputB), the branch output
/// type becomes std::variant<OutputA, OutputB>. The join stage must accept exactly that
/// variant. If the join's input_type is std::variant<OutputA> (missing OutputB), the
/// library must emit a "Pipeline edge mismatch" diagnostic at compile-time.

#include <pb/pipeline.hpp>
#include <variant>

struct Input {
  int value{};
};

struct OutputA {};
struct OutputB {};

struct PredA {
  using input_type = Input;
  using output_type = bool;
  bool operator()(Input) const { return true; }
};

struct PredB {
  using input_type = Input;
  using output_type = bool;
  bool operator()(Input) const { return false; }
};

struct StageA {
  using input_type = Input;
  using output_type = OutputA;
  OutputA operator()(Input) const { return {}; }
};

struct StageB {
  using input_type = Input;
  using output_type = OutputB;
  OutputB operator()(Input) const { return {}; }
};

// WrongJoin: input_type is std::variant<OutputA> but branch outputs are
// std::variant<OutputA, OutputB>. The types do not match.
struct WrongJoin {
  using input_type = std::variant<OutputA>; // ERROR: missing OutputB
  using output_type = int;
  int operator()(std::variant<OutputA>) const { return 0; }
};

using CaseA = pb::case_<PredA>::then<StageA>;
using CaseB = pb::case_<PredB>::then<StageB>;

// This should fail: the join's input_type (variant<OutputA>) does not match
// the branch output type (variant<OutputA, OutputB>).
using BadPipeline = pb::from<Input>::branch<CaseA, CaseB>::join<WrongJoin>::to<int>;

static_assert(sizeof(BadPipeline) > 0);

int main() { return 0; }
