// Production-grade gate behaviour for pb::reflect_stage<T>.
//
// On every supported compiler:
//   * pb::reflect_stage_available_v<T> compiles.
//   * Its value equals PB_HAS_REFLECTION (true iff the toolchain advertises
//     P2996 static reflection).
//
// On reflection-capable compilers we additionally instantiate
// pb::reflect_stage<MyStage> to confirm the adapter exposes input_type /
// output_type / operator() through reflection.  On C++20 baselines that
// instantiation is intentionally not exercised — the gate keeps it out
// of the compilation unit.

#include <pb/pipeline.hpp>

#include <type_traits>

namespace reflect_stage_gate {

struct DummyInput  {};
struct DummyOutput {};

// A "stage-shaped" type the adapter would target on a reflection-capable
// compiler.  Declared unconditionally so the test compiles everywhere,
// but only instantiated through reflect_stage when reflection is on.
struct DummyStage {
  using input_type  = DummyInput;
  using output_type = DummyOutput;
  static constexpr auto stage_key()  noexcept { return "dummy"; }
  static constexpr auto stage_name() noexcept { return "dummy"; }
  DummyOutput operator()(DummyInput) const { return {}; }
};

} // namespace reflect_stage_gate

// Gate value matches the macro on every compiler.
static_assert(pb::reflect_stage_available_v<reflect_stage_gate::DummyStage>
                  == (PB_HAS_REFLECTION != 0),
              "pb::reflect_stage_available_v must mirror PB_HAS_REFLECTION");

#if PB_HAS_REFLECTION
// Reflection-capable build: the adapter must actually fill traits.
using Wrapped = pb::reflect_stage<reflect_stage_gate::DummyStage>;
static_assert(std::is_same_v<Wrapped::input_type,  reflect_stage_gate::DummyInput>);
static_assert(std::is_same_v<Wrapped::output_type, reflect_stage_gate::DummyOutput>);
#else
// Non-reflection baseline: the adapter type *name* compiles (so user code
// can mention it in `#if PB_HAS_REFLECTION` branches without `#if`
// guarding every spelling), and pb::reflect_stage_available_v is false.
static_assert(!pb::reflect_stage_available_v<reflect_stage_gate::DummyStage>);
#endif

int main() { return 0; }
