#include <pb/pipeline.hpp>

#include <cassert>
#include <string>

struct Input {
  int value{};
};

struct Middle {
  int value{};
};

struct Output {
  int value{};
};

struct PipelineError {
  std::string message{};

  PipelineError() = default;
  PipelineError(std::string value) : message{std::move(value)} {}
  PipelineError(pb::runtime::error diagnostic) : message{std::move(diagnostic.message)} {}
};

struct CheckedAddOne {
  using input_type = Input;
  using output_type = Middle;
  using error_type = PipelineError;

  pb::runtime::result<Middle, PipelineError> operator()(Input input) const {
    if (input.value < -1) {
      return PipelineError{"negative input"};
    }
    return Middle{input.value + 1};
  }
};

struct CheckedDouble {
  using input_type = Middle;
  using output_type = Output;
  using error_type = PipelineError;

  pb::runtime::result<Output, PipelineError> operator()(Middle input) const {
    if (input.value == 0) {
      return PipelineError{"zero middle"};
    }
    return Output{input.value * 2};
  }
};

using Pipeline = pb::from<Input>::then<CheckedAddOne>::then<CheckedDouble>::to<Output>;

static_assert(pb::valid<Pipeline>);

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});

  auto ok = engine.run(Input{20});
  assert(ok.has_value());
  assert(ok.value().value == 42);

  auto failed_first = engine.run(Input{-2});
  assert(!failed_first.has_value());
  assert(failed_first.error().message == "negative input");

  auto failed_second = engine.run(Input{-1});
  assert(!failed_second.has_value());
  assert(failed_second.error().message == "zero middle");

  return 0;
}
