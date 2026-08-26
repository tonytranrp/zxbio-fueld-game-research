#include <pb/pipeline.hpp>

struct Raw {
  int value{};
};

struct ResultInt {
  using input_type = Raw;
  using output_type = int;

  pb::runtime::result<int> operator()(Raw input) const {
    return pb::runtime::result<int>{input.value};
  }
};

using Pipeline = pb::from<Raw>::then<ResultInt>::to<int>;

static_assert(pb::valid<Pipeline>);
static_assert(std::same_as<pb::pipeline_output_t<Pipeline>, int>);

int main() { return 0; }
