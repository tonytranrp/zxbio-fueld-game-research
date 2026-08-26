#include <pb/runtime/observer.hpp>

#include <string_view>
#include <type_traits>

static_assert(pb::runtime::observer_schema_version == std::string_view{"pb.observer.v1"});
static_assert(pb::runtime::verbose_observer_schema_version ==
              std::string_view{"pb.observer.verbose.v1"});
static_assert(std::has_virtual_destructor_v<pb::runtime::observer>);

struct test_observer final : pb::runtime::observer {
  void on_stage_start(const pb::runtime::stage_id&) override {}
  void on_stage_success(const pb::runtime::stage_id&) override {}
  void on_stage_failure(const pb::runtime::stage_id&, const pb::runtime::error&) override {}
  void on_stage_exception(const pb::runtime::stage_id&, const pb::runtime::error&) override {}
};

int pb_public_header_runtime_observer() {
  test_observer observer{};
  pb::runtime::stage_id stage{.key = "stage", .name = "stage"};
  pb::runtime::error diagnostic{.stage = stage, .message = "diagnostic"};
  observer.on_stage_start(stage);
  observer.on_stage_success(stage);
  observer.on_stage_failure(stage, diagnostic);
  observer.on_stage_exception(stage, diagnostic);
  return 0;
}
