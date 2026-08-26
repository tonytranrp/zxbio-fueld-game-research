#include <pb/pipeline.hpp>

#include <algorithm>
#include <cstdlib>
#include <string_view>

namespace {
constexpr bool expected_available(std::string_view backend) {
  if (backend == std::string_view{"taskflow"}) {
#if defined(PB_HAS_TASKFLOW)
    return true;
#else
    return false;
#endif
  }
  if (backend == std::string_view{"oneTBB"}) {
#if defined(PB_HAS_TBB)
    return true;
#else
    return false;
#endif
  }
  if (backend == std::string_view{"stdexec"}) {
#if defined(PB_HAS_STDEXEC)
    return true;
#else
    return false;
#endif
  }
  return false;
}

void pb_test_require(bool condition) {
  if (!condition) {
    std::abort();
  }
}
}  // namespace

int main() {
  const auto features = pb::backend_features();
  pb_test_require(features.size() == pb::backend_feature_matrix.size());

  const auto named = [&](std::string_view name) {
    return std::find_if(features.begin(), features.end(), [&](const pb::backend_feature& feature) {
      return feature.name == name;
    });
  };

  const auto sequential = named("sequential");
  pb_test_require(sequential != features.end());
  pb_test_require(sequential->support == pb::backend_support::supported);
  pb_test_require(sequential->execution_model == pb::backend_execution_model::sequential);
  pb_test_require(!sequential->external_dependency);
  pb_test_require(sequential->default_build);
  pb_test_require(sequential->compile_time_available);
  pb_test_require(sequential->availability_macro.empty());
  pb_test_require(pb::backend_supported("sequential"));
  pb_test_require(pb::backend_available("sequential"));
  pb_test_require(pb::backend_support_name(sequential->support) == std::string_view{"supported"});
  pb_test_require(pb::backend_execution_model_name(sequential->execution_model) == std::string_view{"sequential"});

  const auto thread_pool = named("thread_pool");
  pb_test_require(thread_pool != features.end());
  pb_test_require(thread_pool->support == pb::backend_support::supported);
  pb_test_require(thread_pool->execution_model == pb::backend_execution_model::thread_pool);
  pb_test_require(!thread_pool->external_dependency);
  pb_test_require(!thread_pool->default_build);
  pb_test_require(thread_pool->compile_time_available);
  pb_test_require(thread_pool->availability_macro.empty());
  pb_test_require(pb::backend_supported("thread_pool"));
  pb_test_require(pb::backend_available("thread_pool"));
  pb_test_require(pb::backend_execution_model_name(thread_pool->execution_model) == std::string_view{"thread_pool"});

  for (const auto backend : {"taskflow", "oneTBB", "stdexec"}) {
    const auto feature = named(backend);
    pb_test_require(feature != features.end());
    pb_test_require(!feature->default_build);
    pb_test_require(feature->compile_time_available == expected_available(backend));
    pb_test_require(!feature->availability_macro.empty());
    pb_test_require(!pb::backend_supported(backend));
    pb_test_require(pb::backend_available(backend) == expected_available(backend));
  }

  pb_test_require(!pb::backend_supported("unknown"));
  pb_test_require(!pb::backend_available("unknown"));
}
