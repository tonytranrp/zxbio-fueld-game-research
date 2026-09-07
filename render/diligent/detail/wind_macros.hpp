#pragma once

#include "world/water/gerstner.hpp"
#include "world/wind/wind_field.hpp"

#include "Graphics/GraphicsTools/interface/ShaderMacroHelper.hpp"

namespace render::diligent::detail {

// The wind field's shader-facing half (Prompt 001 Group B). shaders/wind.fxh carries the FORMULA;
// every constant in it arrives from here, straight out of world/wind's own header. That is what
// makes "one source of truth" a property of the build rather than a promise in a comment: there is
// no second copy of these numbers to drift, and a shader naming a macro this does not define fails
// to compile.
//
// The per-run tuning (base speed, direction, gust amplitude) is NOT here -- it comes through the
// constant buffer, because --wind-speed has to change it without recompiling a shader. Only the
// wave SHAPE, which is art direction frozen at build time, is a macro.
inline void add_wind_macros(Diligent::ShaderMacroHelper& macros) {
    using namespace world::wind::detail;
    macros.AddShaderMacro("WIND_GUST_K0X", kGustK0x);
    macros.AddShaderMacro("WIND_GUST_K0Z", kGustK0z);
    macros.AddShaderMacro("WIND_GUST_K1X", kGustK1x);
    macros.AddShaderMacro("WIND_GUST_K1Z", kGustK1z);
    macros.AddShaderMacro("WIND_GUST_K2X", kGustK2x);
    macros.AddShaderMacro("WIND_GUST_K2Z", kGustK2z);
    macros.AddShaderMacro("WIND_GUST_W0", kGustW0);
    macros.AddShaderMacro("WIND_GUST_W1", kGustW1);
    macros.AddShaderMacro("WIND_GUST_W2", kGustW2);
    macros.AddShaderMacro("WIND_GUST_P1", kGustP1);
    macros.AddShaderMacro("WIND_GUST_P2", kGustP2);
    macros.AddShaderMacro("WIND_FLUTTER_F0X", kFlutterF0x);
    macros.AddShaderMacro("WIND_FLUTTER_F0Y", kFlutterF0y);
    macros.AddShaderMacro("WIND_FLUTTER_F0Z", kFlutterF0z);
    macros.AddShaderMacro("WIND_FLUTTER_F1X", kFlutterF1x);
    macros.AddShaderMacro("WIND_FLUTTER_F1Y", kFlutterF1y);
    macros.AddShaderMacro("WIND_FLUTTER_F1Z", kFlutterF1z);
    macros.AddShaderMacro("WIND_FLUTTER_W0", kFlutterW0);
    macros.AddShaderMacro("WIND_FLUTTER_W1", kFlutterW1);
    macros.AddShaderMacro("WIND_FLUTTER_RATIO", kFlutterRatio);
    macros.AddShaderMacro("WIND_TWO_PI", kTwoPi);

    // The wave field's shape constants (world/water). The per-run waves themselves are DERIVED on
    // the CPU and passed in the constant buffer -- only the array size and g are fixed at build
    // time, and both come from world/water's own header so the HLSL array cannot get out of step
    // with the C++ that fills it.
    macros.AddShaderMacro("WAVE_COUNT", static_cast<Diligent::Uint32>(world::water::kWaveCount));
    macros.AddShaderMacro("WAVE_GRAVITY", world::water::kGravity);
}

} // namespace render::diligent::detail
