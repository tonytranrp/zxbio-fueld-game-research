#include <pb/runtime/cancellation.hpp>

#include <string_view>
#include <type_traits>

static_assert(pb::cancellation_schema_version == std::string_view{"pb.cancel.v1"});
static_assert(std::is_copy_constructible_v<pb::cancellation_token>);
static_assert(std::is_copy_assignable_v<pb::cancellation_token>);
static_assert(std::is_copy_constructible_v<pb::cancellation_source>);
static_assert(std::is_copy_assignable_v<pb::cancellation_source>);

int pb_public_header_runtime_cancellation() {
  pb::cancellation_source source;
  pb::cancellation_token token = source.token();
  pb::cancellation_token token_copy = token;
  pb::cancellation_source source_copy = source;
  pb::cancellation_token copied_source_token = source_copy.token();
  if (!token.can_be_cancelled() || !token_copy.can_be_cancelled() ||
      !copied_source_token.can_be_cancelled()) {
    return 1;
  }
  if (token.stop_requested() || token_copy.stop_requested() ||
      copied_source_token.stop_requested()) {
    return 2;
  }
  source.request_stop();
  if (!source.stop_requested() || !source_copy.stop_requested() ||
      !token.stop_requested() || !token_copy.stop_requested() ||
      !copied_source_token.stop_requested()) {
    return 3;
  }
  // A default-constructed token never reports a stop.
  pb::cancellation_token defaulted{};
  return (defaulted.can_be_cancelled() || defaulted.stop_requested()) ? 4 : 0;
}
