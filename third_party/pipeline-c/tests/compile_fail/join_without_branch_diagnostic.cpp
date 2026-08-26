#include <pb/pipeline.hpp>

struct Input {};
struct Output {};
struct SomeStage {
  using input_type = Input;
  using output_type = Output;
  auto operator()(Input) const -> Output { return {}; }
};

using BadJoin = pb::from<Input>::join<SomeStage>::to<Output>;
static_assert(sizeof(BadJoin) > 0);

int main() { return 0; }
