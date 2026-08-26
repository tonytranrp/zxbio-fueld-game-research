#include <pb/core/stage_traits.hpp>

#include <type_traits>

namespace {
struct stage_with_error {
  using input_type = int;
  using output_type = long;
  using error_type = short;
};

struct stage_without_error {
  using input_type = int;
  using output_type = long;
};
} // namespace

static_assert(pb::core::is_stage_v<stage_with_error>);
static_assert(std::is_same_v<pb::core::stage_input_t<stage_with_error>, int>);
static_assert(std::is_same_v<pb::core::stage_output_t<stage_with_error>, long>);
static_assert(std::is_same_v<pb::core::stage_error_t<stage_with_error>, short>);
static_assert(std::is_same_v<pb::core::stage_error_t<stage_without_error>, pb::no_error>);

int pb_public_header_core_stage_traits() { return 0; }
