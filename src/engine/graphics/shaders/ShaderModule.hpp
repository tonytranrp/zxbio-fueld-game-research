#pragma once

#include "engine/core/Types.hpp"
#include <string_view>

// ==============================================================================
// Shader Module System — Convention & Configuration
// ==============================================================================
//
// This header defines ShaderModuleConfig, the data struct that describes a
// shader module. It also documents the convention that every shader module
// must follow.
//
// ------------------------------------------------------------------------------
// WHY A CONVENTION (NOT A BASE CLASS)?
// ------------------------------------------------------------------------------
//
// Shaders execute on the hot render path — every frame, potentially multiple
// times. Virtual dispatch (vtable indirection) adds overhead and prevents
// inlining. Instead, each shader module is a standalone class with a
// consistent API surface enforced by convention:
//
//   - No inheritance, no virtual methods, no abstract base class
//   - All shader data is constexpr / compile-time
//   - Module headers include ONLY engine/core/Types.hpp and <string_view>
//     (no raylib.h, no ShaderManager.hpp — compile-time priority)
//   - Typed shader exports live beside each module and are collected by the
//     generated typed registry
//
// This means zero per-frame overhead and fast compiles for module headers.
//
// ------------------------------------------------------------------------------
// WHAT EVERY SHADER MODULE PROVIDES
// ------------------------------------------------------------------------------
//
// Each module (e.g. BlurHModule, BlurVModule, etc.) is a class with these
// static constexpr members:
//
//   static constexpr std::string_view NAME             // "blur_h", etc.
//   static constexpr std::string_view FRAGMENT_SOURCE   // GLSL fragment code
//   static constexpr const char* VERTEX_SOURCE = nullptr // nullptr = Raylib default
//   static constexpr ShaderModuleConfig CONFIG          // Aggregated config
//
// Plus any uniform name constants the module exposes:
//
//   static constexpr std::string_view UNIFORM_TEXEL_SIZE = "texelSize";
//   static constexpr std::string_view UNIFORM_RADIUS     = "blurRadius";
//
// Example module structure:
//
//   // BlurHModule.hpp
//   #pragma once
//   #include "engine/graphics/shaders/ShaderModule.hpp"
//
//   namespace biofuel::engine::graphics::shader {
//
//   class BlurHModule {
//   public:
//       static constexpr std::string_view NAME = "blur_h";
//       static constexpr std::string_view FRAGMENT_SOURCE = R"(...)";
//       static constexpr const char* VERTEX_SOURCE = nullptr;
//       static constexpr ShaderModuleConfig CONFIG{
//           .name           = NAME,
//           .fragmentSource = FRAGMENT_SOURCE,
//           .vertexSource   = VERTEX_SOURCE,
//       };
//
//       static constexpr std::string_view UNIFORM_TEXEL_SIZE = "texelSize";
//       static constexpr std::string_view UNIFORM_RADIUS     = "blurRadius";
//   };
//
//   } // namespace biofuel::engine::graphics::shader
//
// ------------------------------------------------------------------------------
// HOW MODULES GET REGISTERED
// ------------------------------------------------------------------------------
//
// A module exports a typed shader asset and marker through the declaration
// helpers in `engine/runtime/typed/ShaderDeclare.hpp`:
//
//   BIOFUEL_EMBEDDED_SHADER_ASSET(...);
//   BIOFUEL_SHADER_MODULE(BlurHShaderModule, shader::BlurH)
//
// Runtime loading goes through ::biofuel::engine::runtime::typed::Shaders::load<TShader>().
//
// ==============================================================================

namespace biofuel::engine::graphics::shader {

// ------------------------------------------------------------------------------
// ShaderModuleConfig — Describes a single shader module
// ------------------------------------------------------------------------------
// This struct is the single source of truth for a module's shader metadata.
// It is designed to be constexpr-constructible so module configs live in
// read-only memory and never allocate.
// ------------------------------------------------------------------------------
struct ShaderModuleConfig {
    std::string_view name;              // Shader name for ShaderManager lookup
    std::string_view fragmentSource;    // GLSL fragment shader source (constexpr)
    const char* vertexSource = nullptr; // nullptr = use Raylib default vertex shader
};

} // namespace biofuel::engine::graphics::shader
