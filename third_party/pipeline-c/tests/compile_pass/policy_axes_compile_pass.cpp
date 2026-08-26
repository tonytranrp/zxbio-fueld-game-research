/// @file policy_axes_compile_pass.cpp
/// @brief Compile-time verification of the three independent ::with<> policy
///        axes: errors, diagnostics, copying.
///
/// Each axis has its own marker trait (`is_*_policy_v`), its own pipeline
/// query (`has_*_policy_v`), and its own extraction alias
/// (`pipeline_*_policy_t`).  A pipeline carries exactly the markers it was
/// built with; the three axes are reported independently and never bleed into
/// one another.

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

// ---------------------------------------------------------------------------
// Marker traits: each marker belongs to exactly one axis.
// ---------------------------------------------------------------------------

// Diagnostics axis: verbose + quiet are diagnostics policies; the legacy
// silent/normal introspection tags are not.
static_assert(pb::policy::is_diagnostics_policy_v<pb::policy::diagnostics::verbose>);
static_assert(pb::policy::is_diagnostics_policy_v<pb::policy::diagnostics::quiet>);
static_assert(!pb::policy::is_diagnostics_policy_v<pb::policy::diagnostics::silent>);
static_assert(!pb::policy::is_diagnostics_policy_v<pb::policy::diagnostics::normal>);
static_assert(!pb::policy::is_diagnostics_policy_v<pb::policy::errors::throwing>);
static_assert(!pb::policy::is_diagnostics_policy_v<pb::policy::copying::move_only>);

// Copying axis: the four copying markers are copying policies; nothing else.
static_assert(pb::policy::is_copying_policy_v<pb::policy::copying::value>);
static_assert(pb::policy::is_copying_policy_v<pb::policy::copying::move_only>);
static_assert(pb::policy::is_copying_policy_v<pb::policy::copying::shared>);
static_assert(pb::policy::is_copying_policy_v<pb::policy::copying::clone>);
static_assert(!pb::policy::is_copying_policy_v<pb::policy::memory::move>);
static_assert(!pb::policy::is_copying_policy_v<pb::policy::errors::throwing>);
static_assert(!pb::policy::is_copying_policy_v<pb::policy::diagnostics::verbose>);

// Error axis is unaffected by the new markers.
static_assert(!pb::policy::is_error_policy_v<pb::policy::diagnostics::verbose>);
static_assert(!pb::policy::is_error_policy_v<pb::policy::copying::move_only>);

// ---------------------------------------------------------------------------
// Plain pipeline: all three axes report false.
// ---------------------------------------------------------------------------
using Plain = pb::from<Raw>::then<Stage>::to<Done>;
static_assert(!pb::has_error_policy_v<Plain>);
static_assert(!pb::has_diagnostics_policy_v<Plain>);
static_assert(!pb::has_copying_policy_v<Plain>);
static_assert(std::is_same_v<pb::pipeline_diagnostics_policy_t<Plain>, void>);
static_assert(std::is_same_v<pb::pipeline_copying_policy_t<Plain>, void>);

// ---------------------------------------------------------------------------
// Single-axis pipelines: only the carried axis reports true.
// ---------------------------------------------------------------------------
using VerboseOnly = pb::from<Raw>::with<pb::policy::diagnostics::verbose>::then<Stage>::to<Done>;
static_assert(pb::has_diagnostics_policy_v<VerboseOnly>);
static_assert(!pb::has_error_policy_v<VerboseOnly>);
static_assert(!pb::has_copying_policy_v<VerboseOnly>);
static_assert(std::is_same_v<pb::pipeline_diagnostics_policy_t<VerboseOnly>,
                             pb::policy::diagnostics::verbose>);

using QuietOnly = pb::from<Raw>::with<pb::policy::diagnostics::quiet>::then<Stage>::to<Done>;
static_assert(pb::has_diagnostics_policy_v<QuietOnly>);
static_assert(std::is_same_v<pb::pipeline_diagnostics_policy_t<QuietOnly>,
                             pb::policy::diagnostics::quiet>);

using MoveOnly = pb::from<Raw>::with<pb::policy::copying::move_only>::then<Stage>::to<Done>;
static_assert(pb::has_copying_policy_v<MoveOnly>);
static_assert(!pb::has_error_policy_v<MoveOnly>);
static_assert(!pb::has_diagnostics_policy_v<MoveOnly>);
static_assert(std::is_same_v<pb::pipeline_copying_policy_t<MoveOnly>,
                             pb::policy::copying::move_only>);

using ValueAndDiagnostics = pb::from<Raw>::with<
    pb::policy::copying::value, pb::policy::diagnostics::quiet>::then<Stage>::to<Done>;
static_assert(pb::has_copying_policy_v<ValueAndDiagnostics>);
static_assert(pb::has_diagnostics_policy_v<ValueAndDiagnostics>);
static_assert(std::is_same_v<pb::pipeline_copying_policy_t<ValueAndDiagnostics>,
                             pb::policy::copying::value>);

using ThrowingOnly = pb::from<Raw>::with<pb::policy::errors::throwing>::then<Stage>::to<Done>;
static_assert(pb::has_error_policy_v<ThrowingOnly>);
static_assert(!pb::has_diagnostics_policy_v<ThrowingOnly>);
static_assert(!pb::has_copying_policy_v<ThrowingOnly>);

// ---------------------------------------------------------------------------
// Mixed-axis pipeline: all three axes report independently and correctly.
// ---------------------------------------------------------------------------
using AllThreeInOneWith = pb::from<Raw>::with<
    pb::policy::errors::throwing, pb::policy::diagnostics::verbose,
    pb::policy::copying::move_only>::then<Stage>::to<Done>;
static_assert(pb::has_error_policy_v<AllThreeInOneWith>);
static_assert(pb::has_diagnostics_policy_v<AllThreeInOneWith>);
static_assert(pb::has_copying_policy_v<AllThreeInOneWith>);

using AllThree = pb::from<Raw>::with<pb::policy::errors::throwing>::with<
    pb::policy::diagnostics::verbose>::with<pb::policy::copying::move_only>::then<Stage>::to<Done>;

static_assert(pb::has_error_policy_v<AllThree>);
static_assert(pb::has_diagnostics_policy_v<AllThree>);
static_assert(pb::has_copying_policy_v<AllThree>);

static_assert(std::is_same_v<pb::pipeline_error_policy_t<AllThree>, pb::policy::errors::throwing>);
static_assert(std::is_same_v<pb::pipeline_diagnostics_policy_t<AllThree>,
                             pb::policy::diagnostics::verbose>);
static_assert(std::is_same_v<pb::pipeline_copying_policy_t<AllThree>,
                             pb::policy::copying::move_only>);

// Order independence: declaring the axes in a different order yields the same
// per-axis answers.
using AllThreeReordered = pb::from<Raw>::with<pb::policy::copying::clone>::with<
    pb::policy::diagnostics::quiet>::with<pb::policy::errors::ignoring>::then<Stage>::to<Done>;

static_assert(std::is_same_v<pb::pipeline_error_policy_t<AllThreeReordered>,
                             pb::policy::errors::ignoring>);
static_assert(std::is_same_v<pb::pipeline_diagnostics_policy_t<AllThreeReordered>,
                             pb::policy::diagnostics::quiet>);
static_assert(std::is_same_v<pb::pipeline_copying_policy_t<AllThreeReordered>,
                             pb::policy::copying::clone>);

} // namespace

int main() { return 0; }
