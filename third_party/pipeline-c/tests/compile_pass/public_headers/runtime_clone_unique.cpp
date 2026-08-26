#include <pb/runtime/clone.hpp>

#include <type_traits>

struct CloneHeavy {
  int value{};
};

using HeavyClone = pb::unique_clone<CloneHeavy>;

static_assert(pb::is_unique_clone_v<HeavyClone>);
static_assert(!pb::is_unique_clone_v<CloneHeavy>);
static_assert(std::is_same_v<HeavyClone::value_type, CloneHeavy>);
static_assert(std::is_copy_constructible_v<HeavyClone>);
static_assert(std::is_same_v<decltype(pb::make_unique_clone(CloneHeavy{})), HeavyClone>);

int pb_public_header_runtime_clone_unique() {
  auto clone = pb::make_unique_clone(CloneHeavy{.value = 3});
  auto copy  = clone;
  copy->value = 9;
  return (clone->value == 3 && copy.clone().value == 9) ? 0 : 1;
}
