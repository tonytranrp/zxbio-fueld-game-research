/// @file with_policy_dsl_compile_pass.cpp
/// @brief Compile-time verification of the runtime-enforced error-policy DSL.
///
/// A pipeline built with `::with<pb::policy::errors::...>` carries the marker
/// in its type and is detected by `pb::has_error_policy_v`; a plain pipeline
/// carries no marker.  `pb::pipeline_error_policy_t` extracts the marker (or
/// `void` when absent).

#include <pb/pipeline.hpp>

#include <type_traits>

namespace {

struct Raw {
  int value{};
};
struct Done {
  int value{};
};

struct Stage {
  using input_type = Raw;
  using output_type = Done;
  static constexpr auto stage_name() noexcept { return "stage"; }
  constexpr auto operator()(Raw r) const noexcept -> Done { return {r.value + 1}; }
};

// A plain pipeline carries no error policy.
using Plain = pb::from<Raw>::then<Stage>::to<Done>;
static_assert(!pb::has_error_policy_v<Plain>,
              "a pipeline without ::with<error policy> must not report an error policy");
static_assert(std::is_same_v<pb::pipeline_error_policy_t<Plain>, void>);

// A ::with<...> pipeline carries the marker and reports it.
using Throwing = pb::from<Raw>::with<pb::policy::errors::throwing>::then<Stage>::to<Done>;
static_assert(pb::has_error_policy_v<Throwing>,
              "::with<pb::policy::errors::throwing> must report an error policy");
static_assert(std::is_same_v<pb::pipeline_error_policy_t<Throwing>, pb::policy::errors::throwing>);

using Terminating = pb::from<Raw>::with<pb::policy::errors::terminating>::then<Stage>::to<Done>;
static_assert(pb::has_error_policy_v<Terminating>);
static_assert(std::is_same_v<pb::pipeline_error_policy_t<Terminating>, pb::policy::errors::terminating>);

using Ignoring = pb::from<Raw>::with<pb::policy::errors::ignoring>::then<Stage>::to<Done>;
static_assert(pb::has_error_policy_v<Ignoring>);
static_assert(std::is_same_v<pb::pipeline_error_policy_t<Ignoring>, pb::policy::errors::ignoring>);

// propagating / result are error-policy markers (is_error_policy_v) but select
// no compile<> wrapper — they are still reported by has_error_policy_v.
using Propagating = pb::from<Raw>::with<pb::policy::errors::propagating>::then<Stage>::to<Done>;
static_assert(pb::has_error_policy_v<Propagating>);
static_assert(std::is_same_v<pb::pipeline_error_policy_t<Propagating>, pb::policy::errors::propagating>);

// is_error_policy trait: exactly the five errors markers are error policies;
// the legacy introspection tags are not.
static_assert(pb::policy::is_error_policy_v<pb::policy::errors::throwing>);
static_assert(pb::policy::is_error_policy_v<pb::policy::errors::terminating>);
static_assert(pb::policy::is_error_policy_v<pb::policy::errors::ignoring>);
static_assert(pb::policy::is_error_policy_v<pb::policy::errors::propagating>);
static_assert(pb::policy::is_error_policy_v<pb::policy::errors::result>);
static_assert(!pb::policy::is_error_policy_v<pb::policy::errors::expected>);
static_assert(!pb::policy::is_error_policy_v<pb::policy::errors::terminate>);
static_assert(!pb::policy::is_error_policy_v<pb::policy::errors::ignore>);
static_assert(!pb::policy::is_error_policy_v<pb::policy::storage::persistent>);

// A non-error policy marker on a pipeline does not make has_error_policy_v true.
using WithStorage = pb::from<Raw>::with<pb::policy::storage::persistent>::then<Stage>::to<Done>;
static_assert(!pb::has_error_policy_v<WithStorage>,
              "a non-error policy marker must not be reported as an error policy");

} // namespace

int main() { return 0; }
