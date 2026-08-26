#include <pb/pipeline.hpp>

struct Input {};
struct Middle {};
struct Output {};
struct FirstError {};
struct SecondError {};

struct CheckedFirst {
  using input_type = Input;
  using output_type = Middle;
  using error_type = FirstError;

  pb::runtime::result<Middle, FirstError> operator()(Input) const { return Middle{}; }
};

struct CheckedSecond {
  using input_type = Middle;
  using output_type = Output;
  using error_type = SecondError;

  pb::runtime::result<Output, SecondError> operator()(Middle) const { return SecondError{}; }
};

using Broken = pb::from<Input>::then<CheckedFirst>::then<CheckedSecond>::to<Output>;
static_assert(pb::valid<Broken>);

int main() {
  auto engine = pb::compile<Broken>(pb::runtime::sequential{});
  (void)engine.run(Input{});
  return 0;
}
