#include <pb/pipeline.hpp>

#include <array>
#include <cassert>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct Input { int value{}; };
struct Middle { int value{}; };
struct Output { int value{}; };

struct AddOne {
  using input_type = Input;
  using output_type = Middle;
  static constexpr auto key = pb::fixed_string{"runtime.add_one"};
  static constexpr auto name = pb::fixed_string{"add_one"};
  Middle operator()(Input input) const { return {input.value + 1}; }
};

struct DoubleValue {
  using input_type = Middle;
  using output_type = Output;
  static constexpr auto key = pb::fixed_string{"runtime.double"};
  static constexpr auto name = pb::fixed_string{"double"};
  Output operator()(Middle input) const { return {input.value * 2}; }
};

using Pipeline = pb::from<Input>::then<AddOne>::then<DoubleValue>::to<Output>;
using PackAliasPipeline = pb::from<Input>::then_all<AddOne, DoubleValue>::done;
using PipeAliasPipeline = pb::from<Input>::pipe<AddOne>::through<DoubleValue>::returns<Output>;
static_assert(std::is_same_v<Pipeline, PackAliasPipeline>);
static_assert(std::is_same_v<Pipeline, PipeAliasPipeline>);

struct InputIterator {
  const Input* current{};

  const Input& operator*() const { return *current; }
  InputIterator& operator++() {
    ++current;
    return *this;
  }
};

struct InputSentinel {
  const Input* end{};
};

bool operator!=(InputIterator iterator, InputSentinel sentinel) {
  return iterator.current != sentinel.end;
}

struct InputSentinelRange {
  const Input* first{};
  const Input* last{};

  InputIterator begin() const { return InputIterator{first}; }
  InputSentinel end() const { return InputSentinel{last}; }
};

struct MoveInput {
  std::unique_ptr<int> value{};
};

struct MoveMiddle {
  std::unique_ptr<int> value{};
};

struct MoveOutput {
  std::unique_ptr<int> value{};
};

static_assert(!std::copy_constructible<MoveInput>);
static_assert(!std::copy_constructible<MoveMiddle>);
static_assert(!std::copy_constructible<MoveOutput>);

struct MoveAddOne {
  using input_type = MoveInput;
  using output_type = MoveMiddle;

  MoveMiddle operator()(MoveInput input) const {
    return {std::make_unique<int>(*input.value + 1)};
  }
};

struct MoveDoubleValue {
  using input_type = MoveMiddle;
  using output_type = MoveOutput;

  MoveOutput operator()(MoveMiddle input) const {
    return {std::make_unique<int>(*input.value * 2)};
  }
};

using MoveOnlyPipeline = pb::from<MoveInput>::then<MoveAddOne>::then<MoveDoubleValue>::to<MoveOutput>;
static_assert(pb::valid<MoveOnlyPipeline>);

struct StatefulInput { int value{}; };
struct StatefulOutput { int value{}; };

struct CountingStage {
  using input_type = StatefulInput;
  using output_type = StatefulOutput;

  int calls{};

  StatefulOutput operator()(StatefulInput input) {
    ++calls;
    return {input.value + calls};
  }
};

using StatefulPipeline = pb::from<StatefulInput>::then<CountingStage>::to<StatefulOutput>;
static_assert(pb::valid<StatefulPipeline>);

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  using Engine = decltype(engine);
  static_assert(std::is_same_v<typename Engine::pipeline_type, Pipeline>);
  static_assert(std::is_same_v<typename Engine::input_type, Input>);
  static_assert(std::is_same_v<typename Engine::output_type, Output>);
  static_assert(std::is_same_v<typename Engine::stage_storage_policy, pb::runtime::construct_stages_per_run>);
  static_assert(std::is_same_v<typename Engine::try_result_type, pb::runtime::result<Output>>);
  static_assert(Engine::stage_count == 2);

  constexpr auto descriptor_view = pb::descriptor_view<Pipeline>();
  static_assert(decltype(descriptor_view)::stage_count == 2);
  static_assert(!decltype(descriptor_view)::empty);
  static_assert(descriptor_view.stage_records()[0].index == 0);
  static_assert(descriptor_view.stage_records()[0].key == std::string_view{"runtime.add_one"});
  static_assert(descriptor_view.stage_records()[0].name == std::string_view{"add_one"});
  static_assert(descriptor_view.stage_records()[1].index == 1);
  static_assert(descriptor_view.stage_records()[1].key == std::string_view{"runtime.double"});
  static_assert(descriptor_view.stage_records()[1].name == std::string_view{"double"});

  auto output = engine.run(Input{20});
  if (output.value != 42) {
    return 1;
  }

  const std::array batch{Input{1}, Input{2}, Input{3}};
  auto batch_results = engine.try_run_each(batch);
  assert(batch_results.size() == batch.size());
  assert(batch_results[0].has_value());
  assert(batch_results[1].has_value());
  assert(batch_results[2].has_value());
  assert(batch_results[0].value().value == 4);
  assert(batch_results[1].value().value == 6);
  assert(batch_results[2].value().value == 8);

  auto range_results = engine.try_run_range(batch.begin() + 1, batch.end());
  assert(range_results.size() == 2);
  assert(range_results[0].has_value());
  assert(range_results[1].has_value());
  assert(range_results[0].value().value == 6);
  assert(range_results[1].value().value == 8);

  const InputSentinelRange sentinel_batch{batch.data(), batch.data() + batch.size()};
  auto sentinel_results = engine.try_run_each(sentinel_batch);
  assert(sentinel_results.size() == batch.size());
  assert(sentinel_results[0].has_value());
  assert(sentinel_results[1].has_value());
  assert(sentinel_results[2].has_value());
  assert(sentinel_results[0].value().value == 4);
  assert(sentinel_results[1].value().value == 6);
  assert(sentinel_results[2].value().value == 8);

  auto throwing_engine = pb::with_throw_on_error(pb::compile<Pipeline>(pb::runtime::sequential{}));
  auto throwing_batch = throwing_engine.try_run_each(batch);
  assert(throwing_batch.size() == batch.size());
  assert(throwing_batch[0].has_value());
  assert(throwing_batch[0].value().value == 4);

  auto terminating_engine = pb::with_terminate_on_error(pb::compile<Pipeline>(pb::runtime::sequential{}));
  auto terminating_batch = terminating_engine.try_run_range(batch.begin(), batch.end());
  assert(terminating_batch.size() == batch.size());
  assert(terminating_batch[2].has_value());
  assert(terminating_batch[2].value().value == 8);

  auto ignoring_engine = pb::with_ignore_errors(pb::compile<Pipeline>(pb::runtime::sequential{}),
                                                Output{.value = -1});
  auto ignoring_batch = ignoring_engine.try_run_each(batch);
  assert(ignoring_batch.size() == batch.size());
  assert(ignoring_batch[1].has_value());
  assert(ignoring_batch[1].value().value == 6);

  auto propagating_engine = pb::with_propagate_exceptions(pb::compile<Pipeline>(pb::runtime::sequential{}));
  auto propagating_batch = propagating_engine.try_run_each(batch);
  assert(propagating_batch.size() == batch.size());
  assert(propagating_batch[2].has_value());
  assert(propagating_batch[2].value().value == 8);

  std::ostringstream verbose_log;
  auto verbose_engine = pb::with_verbose_diagnostics(pb::compile<Pipeline>(pb::runtime::sequential{}),
                                                     &verbose_log);
  auto verbose_batch = verbose_engine.try_run_each(batch);
  assert(verbose_batch.size() == batch.size());
  assert(verbose_batch[0].has_value());
  assert(verbose_log.str().find("[pb.verbose]") != std::string::npos);

  auto move_engine = pb::compile<MoveOnlyPipeline>(pb::runtime::sequential{});
  auto move_output = move_engine.run(MoveInput{std::make_unique<int>(20)});
  if (*move_output.value != 42) {
    return 1;
  }

  auto safe_move_output = move_engine.try_run(MoveInput{std::make_unique<int>(21)});
  assert(safe_move_output.has_value());
  assert(*safe_move_output.value().value == 44);

  std::vector<MoveInput> move_batch;
  move_batch.push_back(MoveInput{std::make_unique<int>(20)});
  move_batch.push_back(MoveInput{std::make_unique<int>(21)});
  auto move_batch_results = move_engine.try_run_range(std::make_move_iterator(move_batch.begin()),
                                                      std::make_move_iterator(move_batch.end()));
  assert(move_batch_results.size() == 2);
  assert(move_batch_results[0].has_value());
  assert(move_batch_results[1].has_value());
  assert(*move_batch_results[0].value().value == 42);
  assert(*move_batch_results[1].value().value == 44);

  std::vector<MoveInput> move_each_batch;
  move_each_batch.push_back(MoveInput{std::make_unique<int>(22)});
  move_each_batch.push_back(MoveInput{std::make_unique<int>(23)});
  auto move_each_results = move_engine.try_run_each(std::move(move_each_batch));
  assert(move_each_results.size() == 2);
  assert(move_each_results[0].has_value());
  assert(move_each_results[1].has_value());
  assert(*move_each_results[0].value().value == 46);
  assert(*move_each_results[1].value().value == 48);

  auto per_run_engine = pb::compile<StatefulPipeline>(pb::runtime::sequential{});
  const auto per_run_first = per_run_engine.run(StatefulInput{10});
  const auto per_run_second = per_run_engine.run(StatefulInput{10});
  if (per_run_first.value != 11 || per_run_second.value != 11) {
    return 1;
  }

  auto stateful_engine = pb::compile<StatefulPipeline>(pb::runtime::stateful_sequential{});
  using StatefulEngine = decltype(stateful_engine);
  static_assert(std::is_same_v<typename StatefulEngine::stage_storage_policy, pb::runtime::store_stages_in_engine>);
  const auto stateful_first = stateful_engine.run(StatefulInput{10});
  const auto stateful_second = stateful_engine.run(StatefulInput{10});
  auto stateful_third = stateful_engine.try_run(StatefulInput{10});
  assert(stateful_third.has_value());
  if (stateful_first.value != 11 || stateful_second.value != 12 || stateful_third.value().value != 13) {
    return 1;
  }

  return 0;
}
