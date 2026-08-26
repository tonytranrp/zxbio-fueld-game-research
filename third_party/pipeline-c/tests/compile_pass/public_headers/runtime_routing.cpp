#include <pb/runtime/routing.hpp>

#include <array>
#include <string_view>

constexpr auto descriptor = pb::runtime::make_route_descriptor(
    std::array{pb::runtime::route_case_record{.index = 0, .key = "parse", .name = "parse_order"},
               pb::runtime::route_case_record{.index = 1, .key = "review", .name = "manual_review"}});

static_assert(pb::runtime::route_descriptor_schema_version == std::string_view{"pb.runtime.route.v1"});
static_assert(decltype(descriptor)::case_count == 2);
static_assert(!descriptor.empty());
static_assert(descriptor.case_records()[0].key == std::string_view{"parse"});
static_assert(descriptor.case_records()[1].name == std::string_view{"manual_review"});

int pb_public_header_runtime_routing() { return 0; }
