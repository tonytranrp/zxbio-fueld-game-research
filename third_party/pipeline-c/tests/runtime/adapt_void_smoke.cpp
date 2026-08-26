#include <pb/pipeline.hpp>

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Test types
// ---------------------------------------------------------------------------
struct Input {
  int value{};
};

struct Processed {
  int value{};
};

struct Output {
  int value{};
};

// ---------------------------------------------------------------------------
// Side-effect recording functor for void adapter
// ---------------------------------------------------------------------------
struct SideEffectLogger {
  std::vector<std::string>* log = nullptr;
  std::string label;

  SideEffectLogger() = default;
  SideEffectLogger(std::vector<std::string>* l, std::string lbl)
      : log(l), label(std::move(lbl)) {}

  void operator()(Processed p) const {
    if (log) log->push_back(label + ":" + std::to_string(p.value));
  }
};

struct SideEffectNoexceptLogger {
  std::vector<std::string>* log = nullptr;
  std::string label;

  SideEffectNoexceptLogger() = default;
  SideEffectNoexceptLogger(std::vector<std::string>* l, std::string lbl)
      : log(l), label(std::move(lbl)) {}

  void operator()(Processed p) const noexcept {
    if (log) log->push_back(label + ":noexcept:" + std::to_string(p.value));
  }
};

// ---------------------------------------------------------------------------
// Default-constructible functor for void adapter
// ---------------------------------------------------------------------------
struct CountingLogger {
  int* counter = nullptr;

  CountingLogger() = default;
  explicit CountingLogger(int* c) : counter(c) {}

  void operator()(Processed p) const {
    (void)p;
    if (counter) *counter += 1;
  }
};

// ---------------------------------------------------------------------------
// Pipeline stages
// ---------------------------------------------------------------------------
struct ProcessStage {
  using input_type = Input;
  using output_type = Processed;

  static constexpr std::string_view stage_name() noexcept { return "process"; }

  Processed operator()(Input input) const { return {input.value + 1}; }
};

struct FinalizeStage {
  using input_type = Processed;
  using output_type = Output;

  static constexpr std::string_view stage_name() noexcept { return "finalize"; }

  Output operator()(Processed input) const { return {input.value * 2}; }
};

// ---------------------------------------------------------------------------
// Void adapter type aliases
// ---------------------------------------------------------------------------
using FnVoidAdapter = pb::void_adapter<SideEffectLogger, Processed>;
using NoexceptFnVoidAdapter = pb::void_adapter<SideEffectNoexceptLogger, Processed>;
using FunctorVoidAdapter = pb::void_adapter<CountingLogger, Processed>;

// ---------------------------------------------------------------------------
// Pipeline type aliases
// ---------------------------------------------------------------------------
using FnVoidPipeline =
    pb::from<Input>::then<ProcessStage>::then<FnVoidAdapter>::then<FinalizeStage>::to<Output>;

using NoexceptFnVoidPipeline =
    pb::from<Input>::then<ProcessStage>::then<NoexceptFnVoidAdapter>::then<FinalizeStage>::to<Output>;

using FunctorVoidPipeline =
    pb::from<Input>::then<ProcessStage>::then<FunctorVoidAdapter>::then<FinalizeStage>::to<Output>;

// Compile-time assertions
static_assert(pb::Stage<FnVoidAdapter>);
static_assert(pb::Stage<NoexceptFnVoidAdapter>);
static_assert(pb::Stage<FunctorVoidAdapter>);
static_assert(pb::valid<FnVoidPipeline>);
static_assert(pb::valid<NoexceptFnVoidPipeline>);
static_assert(pb::valid<FunctorVoidPipeline>);
static_assert(pb::AdjacentStages<ProcessStage, FnVoidAdapter>);
static_assert(pb::AdjacentStages<FnVoidAdapter, FinalizeStage>);
static_assert(pb::AdjacentStages<ProcessStage, NoexceptFnVoidAdapter>);
static_assert(pb::AdjacentStages<NoexceptFnVoidAdapter, FinalizeStage>);

// ---------------------------------------------------------------------------
// Runtime tests
// ---------------------------------------------------------------------------
int main() {
  // Test 1: void_adapter pass-through with stateful functor
  {
    std::vector<std::string> log;
    SideEffectLogger logger{&log, "test1"};
    auto adapter = pb::void_adapter<SideEffectLogger, Processed>{logger};
    Processed input{42};
    auto result = adapter(input);
    if (result.value != 42) return 1; // pass-through
    if (log.size() != 1) return 1;
    if (log[0] != "test1:42") return 1;
  }

  // Test 2: noexcept void_adapter pass-through
  {
    std::vector<std::string> log;
    SideEffectNoexceptLogger logger{&log, "test2"};
    auto adapter = pb::void_adapter<SideEffectNoexceptLogger, Processed>{logger};
    Processed input{17};
    auto result = adapter(input);
    if (result.value != 17) return 1; // pass-through
    if (log.size() != 1) return 1;
    if (log[0] != "test2:noexcept:17") return 1;
  }

  // Test 3: Default-constructible functor void_adapter
  {
    int counter = 0;
    CountingLogger logger{&counter};
    auto adapter = pb::void_adapter<CountingLogger, Processed>{logger};
    Processed input{5};
    auto result = adapter(input);
    if (result.value != 5) return 1;
    if (counter != 1) return 1;
  }

  // Test 4: Pipeline with void adapter (construct_per_run — default-constructed logger)
  //         The default-constructed SideEffectLogger has null log pointer, so
  //         operator() is safely a no-op. The pipeline still runs correctly.
  {
    auto engine = pb::compile<FnVoidPipeline>(pb::runtime::sequential{});

    auto result = engine.run(Input{5});
    if (result.value != 12) return 1; // (5+1)*2 = 12
  }

  // Test 5: Pipeline with noexcept void adapter
  {
    auto engine = pb::compile<NoexceptFnVoidPipeline>(pb::runtime::sequential{});

    auto result = engine.run(Input{3});
    if (result.value != 8) return 1; // (3+1)*2 = 8
  }

  // Test 6: Pipeline with functor void adapter (construct_per_run)
  {
    auto engine = pb::compile<FunctorVoidPipeline>(pb::runtime::sequential{});

    auto result = engine.run(Input{10});
    if (result.value != 22) return 1; // (10+1)*2 = 22
    // Functor with null counter still passes through without crashing
  }

  // Test 7: Noexcept detection matches actual behavior
  {
    static_assert(pb::is_noexcept_stage_v<NoexceptFnVoidAdapter, Processed>,
                  "noexcept void adapter must be noexcept");
    static_assert(!pb::is_noexcept_stage_v<FnVoidAdapter, Processed>,
                  "non-noexcept void adapter must not be detected as noexcept");
  }

  // Test 8: Void adapter as final stage (pipeline ending with void adapter)
  {
    using VoidFinalPipeline = pb::from<Input>::then<ProcessStage>::then<FnVoidAdapter>::to<Processed>;

    auto engine = pb::compile<VoidFinalPipeline>(pb::runtime::sequential{});

    auto result = engine.run(Input{7});
    if (result.value != 8) return 1; // 7+1=8
  }

  // Test 9: Multiple void adapters chained
  {
    using MultiVoidPipeline =
        pb::from<Input>::then<ProcessStage>::then<FnVoidAdapter>::then<FnVoidAdapter>::then<FinalizeStage>::to<Output>;

    auto engine = pb::compile<MultiVoidPipeline>(pb::runtime::sequential{});

    auto result = engine.run(Input{1});
    if (result.value != 4) return 1; // (1+1)*2 = 4
  }

  // Test 10: void_adapter satisfies adapted_stage
  {
    static_assert(pb::adapted_stage<FnVoidAdapter>,
                  "void_adapter with functor must satisfy adapted_stage");
    static_assert(pb::adapted_stage<FunctorVoidAdapter>,
                  "void_adapter with CountingLogger must satisfy adapted_stage");
  }

  // Test 11: try_run through void adapter pipeline — try_run always returns result<T>
  {
    auto engine = pb::compile<FnVoidPipeline>(pb::runtime::sequential{});

    auto result = engine.try_run(Input{2});
    if (!result.has_value()) return 1;
    if (result.value().value != 6) return 1; // (2+1)*2 = 6
  }

  return 0;
}
