#pragma once

// ------------------------------------------------------------------------------
// Embedded GLSL Fragment Shaders — Backward-Compatible Aliases
// ------------------------------------------------------------------------------
// The authoritative GLSL source now lives in assets/shaders/*.glsl files.
// At build time, CMake reads those files and generates
// ${CMAKE_BINARY_DIR}/generated/ShaderSources.hpp with constexpr string_view
// constants (e.g., shader_source::blur_h_source).
//
// Shader modules (BlurHModule, BlurVModule) reference those generated constants
// as their FRAGMENT_SOURCE. These aliases provide backward compatibility for any
// code still using the old BLUR_H_FS / BLUR_V_FS names.
//
// New code should use module constants directly:
//   BlurHModule::FRAGMENT_SOURCE  instead of  BLUR_H_FS
//
// To add a new shader:
//   1. Create assets/shaders/my_shader.glsl
//   2. Add to CMake foreach(SHADER_FILE ...) list in src/CMakeLists.txt
//   3. Create a module header in Shader/ referencing the generated source
//   4. Register in LoadingScreen::buildTasks()
// ------------------------------------------------------------------------------

#include "Utils/render/Shader/BlurHModule.hpp"
#include "Utils/render/Shader/BlurVModule.hpp"

namespace biofuel::utils::render::embedded {

// Backward-compatible aliases — prefer using BlurHModule::FRAGMENT_SOURCE directly
inline constexpr std::string_view BLUR_H_FS = shader::BlurHModule::FRAGMENT_SOURCE;
inline constexpr std::string_view BLUR_V_FS = shader::BlurVModule::FRAGMENT_SOURCE;

} // namespace biofuel::utils::render::embedded