#include <pb/pipeline.hpp>

struct Raw { int value{}; };
struct Parsed { int value{}; };
struct Normalized { int value{}; };
struct Done { int value{}; };

struct Parse {
  using input_type = Raw;
  using output_type = Parsed;
  static constexpr auto stage_name() noexcept { return "parse"; }
  Parsed operator()(Raw raw) const { return {raw.value + 1}; }
};

struct Normalize {
  using input_type = Parsed;
  using output_type = Normalized;
  static constexpr auto stage_name() noexcept { return "normalize"; }
  Normalized operator()(Parsed parsed) const { return {parsed.value * 2}; }
};

struct Finish {
  using input_type = Normalized;
  using output_type = Done;
  static constexpr auto stage_name() noexcept { return "finish"; }
  Done operator()(Normalized normalized) const { return {normalized.value - 3}; }
};

using Pipeline = pb::from<Raw>::then<Parse>::then<Normalize>::then<Finish>::to<Done>;
static_assert(pb::valid<Pipeline>);

int main() {
  auto engine = pb::compile<Pipeline>(pb::runtime::sequential{});
  auto done = engine.run(Raw{4});
  return done.value == 7 ? 0 : 1;
}
