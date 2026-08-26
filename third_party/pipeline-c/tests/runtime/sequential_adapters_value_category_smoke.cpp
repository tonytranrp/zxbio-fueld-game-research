#include <pb/pipeline.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>


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

struct CvParser {
  Output parse_const(Input input) const { return Output{input.value + 1}; }
  Output parse_const_noexcept(Input input) const noexcept { return Output{input.value + 10}; }

  Output parse_throwing_const(Input input) const {
    if (input.value < 0) {
      throw std::runtime_error{"const parse failed"};
    }
    return Output{input.value + 100};
  }
};

using ConstAdapter = pb::adapt<pb::member<&CvParser::parse_const>, pb::in<Input>, pb::out<Output>>;
using ConstNoexceptAdapter =
    pb::adapt<pb::member<&CvParser::parse_const_noexcept>, pb::in<Input>, pb::out<Output>>;
using ThrowingConstAdapter =
    pb::adapt<pb::member<&CvParser::parse_throwing_const>, pb::in<Input>, pb::out<Output>>;

using ConstPipeline = pb::from<Input>::then<ConstAdapter>::to<Output>;
using ConstNoexceptPipeline = pb::from<Input>::then<ConstNoexceptAdapter>::to<Output>;
using ThrowingConstPipeline = pb::from<Input>::then<ThrowingConstAdapter>::to<Output>;

static_assert(pb::valid<ConstPipeline>);
static_assert(pb::valid<ConstNoexceptPipeline>);
static_assert(pb::valid<ThrowingConstPipeline>);
static_assert(noexcept(ConstNoexceptAdapter{}(Input{1})));
static_assert(!noexcept(ThrowingConstAdapter{}(Input{1})));

int main() {
  auto const_engine = pb::compile<ConstPipeline>(pb::runtime::sequential{});
  auto const_output = const_engine.run(Input{7});
  pb_test_require(const_output.value == 8);

  auto noexcept_engine = pb::compile<ConstNoexceptPipeline>(pb::runtime::sequential{});
  auto noexcept_output = noexcept_engine.run(Input{7});
  pb_test_require(noexcept_output.value == 17);

  auto throwing_engine = pb::compile<ThrowingConstPipeline>(pb::runtime::sequential{});
  auto throw_success = throwing_engine.run(Input{3});
  pb_test_require(throw_success.value == 103);

  auto throw_failure = throwing_engine.try_run(Input{-1});
  pb_test_require(!throw_failure.has_value());
  pb_test_require(throw_failure.error().category == pb::runtime::error_category::exception);
  pb_test_require(throw_failure.error().message.find("const parse failed") != std::string::npos);

  return 0;
}
