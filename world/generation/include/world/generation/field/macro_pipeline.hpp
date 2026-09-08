#pragma once

// Prompt 006 Group AM-A/AM-B: the stages that fill the macro field, and the order they run in.
//
// THE SHAPE OF THIS FILE IS THE ARCHITECTURE DECISION, so it is worth stating why it is a plain
// ordered list of free functions over one `TerrainField&` rather than a policy-parameterised
// pipeline of stage types (`templates-and-metaprogramming.md` §3) or a vector of type-erased stage
// handles (§5).
//
//   * The stages are NOT independently swappable in the way policy-based design pays for. They are
//     a dependency CHAIN with one valid order -- flow accumulation cannot precede depression
//     filling, and the biome index cannot precede the two climate planes. A policy parameter buys
//     the ability to substitute one implementation for another at compile time; nothing here wants
//     that, and it would cost a full recompile of the pipeline per experiment.
//   * Type erasure (§5) buys a runtime-varying stage list. The list does not vary at runtime: it
//     is the same seven stages every world.
//   * What the stages DO share is a signature and a field, and that is expressible as a function
//     pointer table -- which is what `kStages` is. `--field-stages N` can then stop after N stages,
//     which is how a stage's contribution is isolated for a capture or a statistic.
//
// So: dependency inversion (`modular-architecture.md` §2) applies at the FIELD, not at the stage.
// Every stage knows `TerrainField` and nothing knows another stage.

#include <cstdint>
#include <span>
#include <string_view>

#include "world/generation/field/terrain_field.hpp"

namespace world::generation::field {

/// Everything a stage may depend on that is not already in the field.
struct MacroParams {
    int seed = 1337;
    /// Metres of sea level above the field's own zero. The hypsometry stage sets land fraction by
    /// choosing this, rather than by scaling heights -- research Part 7 §9.3 wants an Earth-like
    /// BIMODAL area-elevation curve with ~29% land, which is a property of where the cut is.
    float sea_level = 0.0f;
};

/// One stage. Takes the whole field because most stages read one plane and write another.
using StageFn = void (*)(TerrainField&, const MacroParams&);

struct Stage {
    std::string_view name; ///< what the loading screen prints, and what a per-stage timing reports
    StageFn run;
};

/// The pipeline, in dependency order. Research Part 7 §10.2's seven stages.
[[nodiscard]] std::span<const Stage> stages() noexcept;

/// Runs the first `count` stages (or all of them when `count` is negative), reporting each stage's
/// name and wall time through `on_stage` so goal 299's loading screen can name what it is doing and
/// the log can say which stage costs what.
void run_pipeline(TerrainField& out, const MacroParams& params, int count = -1,
                  void (*on_stage)(std::string_view, double, void*) = nullptr, void* user = nullptr);

} // namespace world::generation::field
