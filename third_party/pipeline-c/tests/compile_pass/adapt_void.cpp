#include <pb/pipeline.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// Functor void adapter (stateless, default-constructible)
// ---------------------------------------------------------------------------
struct LogEntry {
  int value{};
};

static int side_effect_counter = 0;

struct LogFn {
  void operator()(LogEntry entry) const { (void)entry; ++side_effect_counter; }
};

struct LogNoexceptFn {
  void operator()(LogEntry entry) const noexcept { (void)entry; ++side_effect_counter; }
};

using VoidLogAdapter = pb::void_adapter<LogFn, LogEntry>;
using VoidLogNoexceptAdapter = pb::void_adapter<LogNoexceptFn, LogEntry>;

// ---------------------------------------------------------------------------
// Lambda void adapter (stateless, default-constructible in C++20)
// ---------------------------------------------------------------------------
struct Token {
  int id{};
};

inline constexpr auto consume_token_lambda = [](Token) { /* side effect */ };
inline constexpr auto consume_token_noexcept_lambda = [](Token) noexcept { /* side effect */ };

using VoidLambdaAdapter = pb::void_adapter<decltype(consume_token_lambda), Token>;
using VoidLambdaNoexceptAdapter = pb::void_adapter<decltype(consume_token_noexcept_lambda), Token>;

// ---------------------------------------------------------------------------
// Stateful functor void adapter (default-constructible with null counter)
// ---------------------------------------------------------------------------
struct CountingLogger {
  int* counter = nullptr;

  void operator()(LogEntry entry) const {
    (void)entry;
    if (counter) *counter += 1;
  }
};

using VoidCountingAdapter = pb::void_adapter<CountingLogger, LogEntry>;

// ---------------------------------------------------------------------------
// Pipeline integration
// ---------------------------------------------------------------------------
// Use LogEntry consistently so void_adapter input_type matches prior stage output_type
struct ParseLogStage {
  using input_type = int;
  using output_type = LogEntry;

  static constexpr std::string_view stage_name() noexcept { return "parse_log"; }

  LogEntry operator()(int raw) const { return {raw}; }
};

struct FinalLogStage {
  using input_type = LogEntry;
  using output_type = int;

  static constexpr std::string_view stage_name() noexcept { return "final_log"; }

  int operator()(LogEntry entry) const { return entry.value + 100; }
};

using LogPipeline = pb::from<int>::then<ParseLogStage>::then<VoidLogAdapter>::then<FinalLogStage>::to<int>;
using LogNoexceptPipeline = pb::from<int>::then<ParseLogStage>::then<VoidLogNoexceptAdapter>::then<FinalLogStage>::to<int>;

// ---------------------------------------------------------------------------
// Compile-time verification
// ---------------------------------------------------------------------------

// Stage concept satisfaction
static_assert(pb::Stage<VoidLogAdapter>,
              "void_adapter must satisfy the Stage concept");
static_assert(pb::Stage<VoidLogNoexceptAdapter>,
              "void_adapter<noexcept fn> must satisfy the Stage concept");
static_assert(pb::Stage<VoidLambdaAdapter>,
              "void_adapter<lambda> must satisfy the Stage concept");
static_assert(pb::Stage<VoidLambdaNoexceptAdapter>,
              "void_adapter<noexcept lambda> must satisfy the Stage concept");
static_assert(pb::Stage<VoidCountingAdapter>,
              "void_adapter<CountingLogger> must satisfy the Stage concept");

// adapted_stage concept satisfaction
static_assert(pb::adapted_stage<VoidLogAdapter>,
              "void_adapter must satisfy the adapted_stage concept");
static_assert(pb::adapted_stage<VoidCountingAdapter>,
              "void_adapter<CountingLogger> must satisfy the adapted_stage concept");

// Pass-through: output_type == input_type
static_assert(std::same_as<pb::stage_output_t<VoidLogAdapter>, LogEntry>,
              "void_adapter output_type must equal input_type");
static_assert(std::same_as<pb::stage_output_t<VoidLambdaAdapter>, Token>,
              "void_adapter<lambda> output_type must equal input_type");
static_assert(std::same_as<pb::stage_output_t<VoidCountingAdapter>, LogEntry>,
              "void_adapter<CountingLogger> output_type must equal input_type");

// input_type is preserved
static_assert(std::same_as<pb::stage_input_t<VoidLogAdapter>, LogEntry>,
              "void_adapter input_type must be LogEntry");
static_assert(std::same_as<pb::stage_input_t<VoidLambdaAdapter>, Token>,
              "void_adapter<lambda> input_type must be Token");

// is_noexcept_stage_v detection
static_assert(!pb::is_noexcept_stage_v<VoidLogAdapter, LogEntry>,
              "void_adapter with throwing fn must not be noexcept");
static_assert(pb::is_noexcept_stage_v<VoidLogNoexceptAdapter, LogEntry>,
              "void_adapter with noexcept fn must be noexcept");
static_assert(!pb::is_noexcept_stage_v<VoidLambdaAdapter, Token>,
              "void_adapter with non-noexcept lambda must not be noexcept");
static_assert(pb::is_noexcept_stage_v<VoidLambdaNoexceptAdapter, Token>,
              "void_adapter with noexcept lambda must be noexcept");
static_assert(!noexcept(std::declval<VoidLogAdapter>()(LogEntry{})),
              "throwing void_adapter call expression must remain throwing");
static_assert(noexcept(std::declval<VoidLogNoexceptAdapter>()(LogEntry{})),
              "noexcept void_adapter call expression must preserve noexcept");
static_assert(std::same_as<decltype(std::declval<VoidLogAdapter>()(LogEntry{})), LogEntry>,
              "void_adapter must return the original input type by value");

// Pipeline validity
static_assert(pb::valid<LogPipeline>,
              "Pipeline with void_adapter must be valid");
static_assert(pb::valid<LogNoexceptPipeline>,
              "Pipeline with noexcept void_adapter must be valid");

// Pipeline type relationships
static_assert(std::same_as<pb::pipeline_input_t<LogPipeline>, int>,
              "Pipeline input type mismatch");
static_assert(std::same_as<pb::pipeline_output_t<LogPipeline>, int>,
              "Pipeline output type must pass through chain ending at FinalLogStage");

// Connectable: void_adapter connects from its own input_type
static_assert(pb::connectable_v<LogEntry, VoidLogAdapter>,
              "void_adapter must be connectable from its input_type");
static_assert(pb::connectable_v<LogEntry, VoidLogNoexceptAdapter>,
              "void_adapter<noexcept> must be connectable from its input_type");

// AdjacentStages: ParseLogStage -> VoidLogAdapter
static_assert(pb::AdjacentStages<ParseLogStage, VoidLogAdapter>,
              "ParseLogStage must be adjacent to VoidLogAdapter");
static_assert(pb::AdjacentStages<VoidLogAdapter, FinalLogStage>,
              "VoidLogAdapter must be adjacent to FinalLogStage");

// ---------------------------------------------------------------------------
// constexpr test: noexcept detection for generic stages
// ---------------------------------------------------------------------------
struct NoexceptStage {
  using input_type = int;
  using output_type = int;
  constexpr int operator()(int x) const noexcept { return x * 2; }
};

struct ThrowingStage {
  using input_type = int;
  using output_type = int;
  int operator()(int x) const { return x * 2; }
};

static_assert(pb::is_noexcept_stage_v<NoexceptStage, int>,
              "noexcept stage must be detected as noexcept");
static_assert(!pb::is_noexcept_stage_v<ThrowingStage, int>,
              "throwing stage must not be detected as noexcept");

int main() {
  return 0;
}
