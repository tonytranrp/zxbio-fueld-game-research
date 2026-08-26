#include <pb/pipeline.hpp>

struct Input {};
struct Done {};

struct Join {
  using input_type = Input;
  using output_type = Done;
  Done operator()(Input) const { return {}; }
};

using Bad = pb::from<Input>::fan_in<Join>::to<Done>;

int main() { return 0; }
