#include <pb/pipeline.hpp>

struct Input {};
struct A {};
struct Done {};

struct Always {
  using input_type = Input;
  using output_type = bool;
  bool operator()(const Input&) const { return true; }
};

struct StageA {
  using input_type = Input;
  using output_type = A;
  A operator()(Input) const { return {}; }
};

struct BadJoin {
  using input_type = A;
  using output_type = Done;
  Done operator()(A) const { return {}; }
};

using CaseA = pb::case_<Always>::then<StageA>;
using Bad = pb::from<Input>::branch<CaseA>::fan_in<BadJoin>::to<Done>;

int main() { return 0; }
