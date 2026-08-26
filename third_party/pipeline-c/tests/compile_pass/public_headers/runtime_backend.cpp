#include <pb/runtime/backend.hpp>
#include <pb/runtime/thread_pool_backend.hpp>

#include <cstddef>
#include <type_traits>

static_assert(std::is_same_v<decltype(pb::runtime::backend_features().size()), std::size_t>);
static_assert(pb::runtime::backend_supported("sequential"));
static_assert(pb::runtime::backend_available("sequential"));
static_assert(pb::runtime::backend_support_name(pb::runtime::backend_support::supported) == "supported");
static_assert(pb::runtime::backend_execution_model_name(pb::runtime::backend_execution_model::thread_pool) == "thread_pool");
static_assert(pb::runtime::backend_supported("thread_pool"));
static_assert(pb::runtime::backend_available("thread_pool"));
static_assert(std::is_same_v<decltype(pb::runtime::thread_pool_backend{}.worker_count), std::size_t>);
static_assert(!pb::runtime::backend_supported("taskflow"));
static_assert(!pb::runtime::backend_available("unknown"));
static_assert(!pb::runtime::backend_supported("oneTBB"));
static_assert(!pb::runtime::backend_supported("stdexec"));

int pb_public_header_runtime_backend() { return 0; }
