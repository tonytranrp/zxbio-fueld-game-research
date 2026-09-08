#pragma once

// Prompt 005 goal 289: "one knob, not fifteen."
//
// `SvoRenderer::Settings` is nineteen fields and this prompt added three of them. Seven are
// APPEARANCE axes -- what the surface looks like -- and the rest are quality/performance
// (shadows, AO, TAA, the LOD multipliers, the beam tile). Only the first group belongs in a preset:
// a "look" that silently turned shadows off would be a performance setting wearing a costume.
//
// WHY A PLAIN FUNCTION OVER A STRUCT, not a policy template parameter. Same reasoning
// `PlayerTuning` records for itself: these are ART-DIRECTION VALUES a capture session wants to
// sweep from the command line, so they must be runtime data. `templates-and-metaprogramming.md`
// section 3's policy-based design is the right pattern when the axes are types with different
// behaviour; here every axis is a float or a bool with identical behaviour and only its value
// differs, so a policy parameter would buy nothing and cost a recompile per look.
//
// ORDERING, which is the whole contract and is deliberately simple: `--look` applies AT THE
// POSITION IT APPEARS. Anything after it overrides it; `--look` after a flag overwrites that flag.
// The alternative -- tracking which options were explicitly set so a preset never clobbers them --
// needs the parser to carry per-option provenance, and this repo has exactly one argv-indexing site
// precisely because that kind of state is where CLI bugs live. Put `--look` first.

#include <cstdint>
#include <string_view>

#include "render/diligent/svo_renderer.hpp"

namespace render::diligent {

enum class LookPreset : std::uint8_t {
    Shipping, ///< the shipped defaults: filtered albedo + the deliberate stipple
    Raw,      ///< no band-limiting at all -- what the world looked like before Prompt 005
    Flat,     ///< filtering on, every deliberate pattern off: the surface with no texture of its own
    Hatched,  ///< the stipple pushed toward the reference capture's own amplitude, for look sessions
};

/// Overwrites only the seven appearance fields; quality and performance settings are untouched.
inline void apply_look(SvoRenderer::Settings& s, LookPreset look) noexcept {
    switch (look) {
    case LookPreset::Shipping:
        s.filter_albedo = true;
        s.stipple = true;
        s.stipple_amount = 2.5f;
        s.stipple_period_px = 7.0f;
        s.grain = true;
        s.grain_amplitude = 0.10f;
        s.smooth_pixels = 6.0f;
        return;
    case LookPreset::Raw:
        // The A/B for everything Prompt 005 did. Goal 277's attribution numbers are this look.
        s.filter_albedo = false;
        s.stipple = false;
        s.stipple_amount = 0.0f;
        s.stipple_period_px = 7.0f;
        s.grain = true;
        s.grain_amplitude = 0.10f;
        s.smooth_pixels = 6.0f;
        return;
    case LookPreset::Flat:
        // Band-limited and otherwise bare. Not a shipping look -- it is the control that shows how
        // much of the frame's texture is deliberate, which goal 283's contrast numbers are about.
        s.filter_albedo = true;
        s.stipple = false;
        s.stipple_amount = 0.0f;
        s.stipple_period_px = 7.0f;
        s.grain = false;
        s.grain_amplitude = 0.0f;
        s.smooth_pixels = 6.0f;
        return;
    case LookPreset::Hatched:
        // Toward the reference capture: goal 284 measured its stone stipple at a 10.67 px period.
        // The amplitude is higher than shipping because this look exists to SHOW the hatch.
        s.filter_albedo = true;
        s.stipple = true;
        s.stipple_amount = 4.0f;
        s.stipple_period_px = 10.67f;
        s.grain = true;
        s.grain_amplitude = 0.10f;
        s.smooth_pixels = 6.0f;
        return;
    }
}

[[nodiscard]] constexpr std::string_view look_name(LookPreset look) noexcept {
    switch (look) {
    case LookPreset::Shipping:
        return "shipping";
    case LookPreset::Raw:
        return "raw";
    case LookPreset::Flat:
        return "flat";
    case LookPreset::Hatched:
        return "hatched";
    }
    return "shipping";
}

} // namespace render::diligent
