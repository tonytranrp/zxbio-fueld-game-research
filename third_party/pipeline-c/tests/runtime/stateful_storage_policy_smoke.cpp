#include <pb/pipeline.hpp>

#include <cstdlib>
#include <memory>
#include <type_traits>


namespace {
void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}
}  // namespace

struct Input {
  int value{};
};

struct Output {
  int value{};
};

struct MoveOnlyCounter {
  using input_type = Input;
  using output_type = Output;

  std::unique_ptr<int> calls{std::make_unique<int>(0)};

  MoveOnlyCounter() = default;
  MoveOnlyCounter(const MoveOnlyCounter&) = delete;
  auto operator=(const MoveOnlyCounter&) -> MoveOnlyCounter& = delete;
  MoveOnlyCounter(MoveOnlyCounter&&) noexcept = default;
  auto operator=(MoveOnlyCounter&&) noexcept -> MoveOnlyCounter& = default;

  Output operator()(Input input) {
    ++(*calls);
    return {input.value + *calls};
  }
};

static_assert(!std::copy_constructible<MoveOnlyCounter>);
static_assert(std::move_constructible<MoveOnlyCounter>);

using Pipeline = pb::from<Input>::then<MoveOnlyCounter>::to<Output>;

int main() {
  auto per_run = pb::compile<Pipeline>(pb::runtime::sequential{});
  const auto per_run_first = per_run.run(Input{20});
  const auto per_run_second = per_run.try_run(Input{20});
  pb_test_require(per_run_second.has_value());
  pb_test_require(per_run_first.value == 21);
  pb_test_require(per_run_second.value().value == 21);

  auto stateful = pb::compile<Pipeline>(pb::runtime::stateful_sequential{});
  const auto stateful_first = stateful.run(Input{20});
  const auto stateful_second = stateful.try_run(Input{20});
  const auto stateful_third = stateful.run(Input{20});
  pb_test_require(stateful_second.has_value());
  pb_test_require(stateful_first.value == 21);
  pb_test_require(stateful_second.value().value == 22);
  pb_test_require(stateful_third.value == 23);
}
