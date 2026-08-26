#include <pb/runtime/sequential.hpp>

#include <type_traits>

static_assert(std::is_same_v<pb::runtime::sequential::stage_storage_policy, pb::runtime::construct_stages_per_run>);
static_assert(
    std::is_same_v<pb::runtime::stateful_sequential::stage_storage_policy, pb::runtime::store_stages_in_engine>);
static_assert(std::is_same_v<pb::runtime::policy::storage::construct_per_run,
                             pb::runtime::construct_stages_per_run>);
static_assert(std::is_same_v<pb::runtime::policy::storage::store_in_engine,
                             pb::runtime::store_stages_in_engine>);
static_assert(std::is_same_v<pb::runtime::policy::sequential_per_run::stage_storage_policy,
                             pb::runtime::construct_stages_per_run>);
static_assert(std::is_same_v<pb::runtime::policy::sequential_stateful::stage_storage_policy,
                             pb::runtime::store_stages_in_engine>);

int pb_public_header_runtime_sequential() { return 0; }
