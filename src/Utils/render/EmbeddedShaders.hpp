#pragma once

// ------------------------------------------------------------------------------
// Embedded GLSL Fragment Shaders — Backward-Compatible Aliases
// ------------------------------------------------------------------------------
// This file now delegates to individual shader modules in the Shader/ subfolder.
// The GLSL source strings live in BlurHModule.hpp and BlurVModule.hpp.
//
// These aliases exist for backward compatibility — new code should use the
// module classes directly (e.g., BlurHModule::FRAGMENT_SOURCE).
//
// To add a new shader: create a new Module.hpp in Utils/render/Shader/,
// then add a backward-compatible alias here if needed.
// ------------------------------------------------------------------------------

#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"

namespace biofuel::utils::render::embedded {

// Backward-compatible aliases — prefer using BlurHModule::FRAGMENT_SOURCE directly
inline constexpr std::string_view BLUR_H_FS = shader::BlurHModule::FRAGMENT_SOURCE;
inline constexpr std::string_view BLUR_V_FS = shader::BlurVModule::FRAGMENT_SOURCE;

} // namespace biofuel::utils::render::embedded