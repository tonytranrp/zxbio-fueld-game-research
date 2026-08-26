// Compile-pass smoke test for the optional execution-backend SCAFFOLDS.
//
// Includes all three gated backend headers and asserts that, on the default
// (standard-library-only) build, every `*_available_v` constant is false. This
// proves the integration seam is dormant: none of `PB_HAS_TBB`,
// `PB_HAS_TASKFLOW`, or `PB_HAS_STDEXEC` is defined, the backend tag structs are
// not declared, and the default build is unaffected by the scaffolds.
//
// The headers are included directly here (not via pb/pipeline.hpp) so the public
// include surface stays unchanged.

#include <pb/backends/stdexec.hpp>
#include <pb/backends/taskflow.hpp>
#include <pb/backends/tbb.hpp>

// The availability constants are visible UNGATED on every toolchain.
static_assert(pb::runtime::tbb_backend_available_v == false,
              "default build must not enable the oneTBB backend scaffold");
static_assert(pb::runtime::taskflow_backend_available_v == false,
              "default build must not enable the Taskflow backend scaffold");
static_assert(pb::runtime::stdexec_backend_available_v == false,
              "default build must not enable the stdexec backend scaffold");

#if defined(PB_HAS_TBB) || defined(PB_HAS_TASKFLOW) || defined(PB_HAS_STDEXEC)
#error "default build must not define any PB_HAS_* backend macro"
#endif

int main() { return 0; }
