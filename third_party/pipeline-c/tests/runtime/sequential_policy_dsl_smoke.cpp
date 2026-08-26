#include <pb/pipeline.hpp>

#include <array>
#include <cstdlib>
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

struct CountingStage {
  using input_type = Input;
  using output_type = Output;

  int calls{};

  Output operator()(Input input) {
    ++calls;
    return {input.value + calls};
  }
};

using Pipeline = pb::from<Input>::then<CountingStage>::to<Output>;

int main() {
  auto per_run = pb::compile<Pipeline>(pb::runtime::policy::sequential<pb::runtime::policy::storage::construct_per_run>{});
  using PerRunEngine = decltype(per_run);
  static_assert(std::is_same_v<typename PerRunEngine::stage_storage_policy, pb::runtime::construct_stages_per_run>);

  const auto per_run_first = per_run.run(Input{10});
  const auto per_run_second = per_run.try_run(Input{10});
  pb_test_require(per_run_second.has_value());
  pb_test_require(per_run_first.value == 11);
  pb_test_require(per_run_second.value().value == 11);

  const std::array per_run_inputs{Input{10}, Input{10}, Input{10}};
  const auto per_run_batch = per_run.try_run_each(per_run_inputs);
  pb_test_require(per_run_batch.size() == per_run_inputs.size());
  for (const auto& outcome : per_run_batch) {
    pb_test_require(outcome.has_value());
    pb_test_require(outcome.value().value == 11);
  }

  auto stateful = pb::compile<Pipeline>(pb::runtime::policy::sequential<pb::runtime::policy::storage::store_in_engine>{});
  using StatefulEngine = decltype(stateful);
  static_assert(std::is_same_v<typename StatefulEngine::stage_storage_policy, pb::runtime::store_stages_in_engine>);

  const auto stateful_first = stateful.run(Input{10});
  const auto stateful_second = stateful.try_run(Input{10});
  const auto stateful_third = stateful.run(Input{10});
  pb_test_require(stateful_second.has_value());
  pb_test_require(stateful_first.value == 11);
  pb_test_require(stateful_second.value().value == 12);
  pb_test_require(stateful_third.value == 13);

  const std::array stateful_inputs{Input{10}, Input{10}};
  const auto stateful_batch = stateful.try_run_each(stateful_inputs);
  pb_test_require(stateful_batch.size() == stateful_inputs.size());
  pb_test_require(stateful_batch[0].has_value());
  pb_test_require(stateful_batch[1].has_value());
  pb_test_require(stateful_batch[0].value().value == 14);
  pb_test_require(stateful_batch[1].value().value == 15);
}
