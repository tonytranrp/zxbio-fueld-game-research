#include <pb/pipeline.hpp>

struct Raw { int value{}; };
struct Parsed {};
struct Reviewed {};

struct IsParse {
  using input_type = Raw;
  using output_type = bool;
  auto operator()(Raw) const -> bool { return true; }
};
struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  auto operator()(Raw) const -> Parsed { return {}; }
};
struct BadJoin {
  using input_type = Reviewed; // WRONG - should be Parsed
  using output_type = int;
  auto operator()(Reviewed) const -> int { return 0; }
};

using ParseCase = pb::case_<IsParse>::then<Parse>;
using BadPipeline = pb::from<Raw>::branch<ParseCase>::join<BadJoin>::to<int>;
static_assert(sizeof(BadPipeline) > 0);

int main() { return 0; }
