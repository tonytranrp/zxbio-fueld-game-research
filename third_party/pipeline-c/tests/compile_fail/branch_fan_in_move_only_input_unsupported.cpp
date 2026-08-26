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

struct Done {
  int value{};
};

struct Always {
  using input_type = MoveOnly;
  using output_type = bool;
  bool operator()(const MoveOnly&) const { return true; }
};

struct Route {
  using input_type = MoveOnly;
  using output_type = Routed;
  Routed operator()(MoveOnly input) const { return Routed{input.value ? *input.value : 0}; }
};

using Case = pb::case_<Always>::then<Route>;
using FanInInput = pb::fan_in_output_t<Case>;

struct Join {
  using input_type = FanInInput;
  using output_type = Done;
  Done operator()(FanInInput) const { return {}; }
};

using Bad = pb::from<MoveOnly>::branch<Case>::fan_in<Join>::to<Done>;

int main() { return 0; }
