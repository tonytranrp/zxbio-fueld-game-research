#include <pb/pipeline.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

struct EmptyInput {};
using EmptyOutput = EmptyInput;
using DescriptorPipeline = pb::from<EmptyInput>::to<EmptyOutput>;

struct Input {
  int value{};
};

using Pipeline = pb::from<Input>::to<Input>;

struct counting_observer final : pb::runtime::observer {
  std::size_t starts{};
  std::size_t successes{};
  std::size_t failures{};
  std::size_t exceptions{};

  void on_stage_start(const pb::runtime::stage_id&) override { ++starts; }
  void on_stage_success(const pb::runtime::stage_id&) override { ++successes; }
  void on_stage_failure(const pb::runtime::stage_id&, const pb::runtime::error&) override { ++failures; }
  void on_stage_exception(const pb::runtime::stage_id&, const pb::runtime::error&) override { ++exceptions; }
};
using Traits = pb::pipeline_traits<Pipeline>;
static_assert(pb::valid<Pipeline>);
static_assert(Traits::empty);
static_assert(pb::pipeline_empty_v<Pipeline>);
static_assert(Traits::stage_count == 0);
static_assert(std::same_as<Traits::input_type, Input>);
static_assert(std::same_as<Traits::output_type, Input>);
static_assert(std::same_as<pb::pipeline_input_t<Pipeline>, Input>);
static_assert(std::same_as<pb::pipeline_output_t<Pipeline>, Input>);
static_assert(std::same_as<pb::pipeline_stages_t<Pipeline>, pb::meta::type_list<>>);
static_assert(pb::describe<Pipeline>().stage_names().empty());
static_assert(pb::describe<Pipeline>().stage_records().empty());
static_assert(pb::make_descriptor<Pipeline>().empty);
static_assert(pb::make_descriptor<Pipeline>().schema_version == std::string_view{"pb.descriptor.v1"});
static_assert(pb::make_descriptor<Pipeline>().topology == pb::runtime::descriptor_topology::linear);
static_assert(pb::make_descriptor<Pipeline>().stage_records().empty());
static_assert(pb::make_descriptor<Pipeline>().edge_records().empty());
static_assert(pb::make_descriptor<Pipeline>().branch_case_records().empty());

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  counting_observer observer{};
  engine.set_observer(&observer);

  const auto runtime_descriptor = engine.descriptor();
  if (!runtime_descriptor.empty || runtime_descriptor.stage_count != 0 || runtime_descriptor.edge_count != 0) {
    return 1;
  }
  if (!runtime_descriptor.stage_records().empty() || !runtime_descriptor.edge_records().empty()) {
    return 1;
  }

  auto output = engine.run(Input{17});
  if (output.value != 17) {
    return 1;
  }

  auto safe_output = engine.try_run(Input{23});
  if (!safe_output.has_value() || safe_output.value().value != 23) {
    return 1;
  }

  const std::array batch{Input{1}, Input{2}, Input{3}};
  const auto batch_results = engine.try_run_each(batch);
  if (batch_results.size() != batch.size() || !batch_results[0].has_value() ||
      !batch_results[1].has_value() || !batch_results[2].has_value()) {
    return 1;
  }
  if (batch_results[0].value().value != 1 || batch_results[1].value().value != 2 ||
      batch_results[2].value().value != 3) {
    return 1;
  }

  const auto range_results = engine.try_run_range(batch.begin() + 1, batch.end());
  if (range_results.size() != 2 || !range_results[0].has_value() || !range_results[1].has_value()) {
    return 1;
  }
  if (range_results[0].value().value != 2 || range_results[1].value().value != 3) {
    return 1;
  }

  if (observer.starts != 0 || observer.successes != 0 || observer.failures != 0 || observer.exceptions != 0) {
    return 1;
  }

  constexpr auto stage_names = pb::describe<Pipeline>().stage_names();
  constexpr auto stage_records = pb::describe<Pipeline>().stage_records();
  constexpr auto view = pb::descriptor_view<Pipeline>();
  if (!stage_names.empty() || !stage_records.empty() || !view.stage_records().empty()) {
    return 1;
  }

  constexpr auto empty_names = pb::describe<DescriptorPipeline>().stage_names();
  constexpr auto empty_records = pb::describe<DescriptorPipeline>().stage_records();
  constexpr auto empty_view = pb::descriptor_view<DescriptorPipeline>();
  if (!empty_names.empty() || !empty_records.empty() || !empty_view.stage_records().empty()) {
    return 1;
  }
  if (empty_view.stage_records().size() != pb::pipeline_size_v<DescriptorPipeline>) {
    return 1;
  }
  return 0;
}
