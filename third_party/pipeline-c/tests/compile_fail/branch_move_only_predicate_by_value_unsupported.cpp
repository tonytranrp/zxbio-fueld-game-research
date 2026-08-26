#include <pb/pipeline.hpp>

#include <memory>

struct MoveOnly {
  explicit MoveOnly(int initial = 0) : value(std::make_unique<int>(initial)) {}
  MoveOnly(MoveOnly&&) noexcept = default;
  MoveOnly& operator=(MoveOnly&&) noexcept = default;
  MoveOnly(const MoveOnly&) = delete;
  MoveOnly& operator=(const MoveOnly&) = delete;

  std::unique_ptr<int> value;
};

struct Routed {
  int value{};
};

struct ConsumingPredicate {
  using input_type = MoveOnly;
  using output_type = bool;

  bool operator()(MoveOnly input) const { return input.value != nullptr; }
};

struct Route {
  using input_type = MoveOnly;
  using output_type = Routed;

  Routed operator()(MoveOnly input) const { return Routed{input.value ? *input.value : 0}; }
};

using BadCase = pb::case_<ConsumingPredicate>::then<Route>;
using BadPipeline = pb::from<MoveOnly>::branch<BadCase>::to<Routed>;
static_assert(pb::valid<BadPipeline>);

int main() { return 0; }
