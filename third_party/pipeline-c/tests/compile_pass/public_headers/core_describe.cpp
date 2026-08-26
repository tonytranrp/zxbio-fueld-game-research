#include <pb/core/describe.hpp>

#include <type_traits>

namespace {
struct raw {};
struct parsed {};
struct done {};

struct parse {
  using input_type = raw;
  using output_type = parsed;
};

struct finish {
  using input_type = parsed;
  using output_type = done;
};

using edge = pb::core::edge_descriptor<0, parse, finish>;
} // namespace

static_assert(std::is_same_v<edge::from_stage_type, parse>);
static_assert(std::is_same_v<edge::to_stage_type, finish>);
static_assert(std::is_same_v<edge::from_input_type, raw>);
static_assert(std::is_same_v<edge::from_output_type, parsed>);
static_assert(std::is_same_v<edge::to_input_type, parsed>);
static_assert(std::is_same_v<edge::to_output_type, done>);

int pb_public_header_core_describe() { return 0; }
