#pragma once

#include "Core/Types.hpp"
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
//   - Module headers include ONLY Core/Types.hpp and <string_view>
//     (no raylib.h, no ShaderManager.hpp — compile-time priority)
//   - Registration happens in App::init(), not via auto-registration
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
//   #include "Utils/render/Shader/ShaderModule.hpp"
//
//   namespace biofuel::utils::render::shader {
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
//   } // namespace biofuel::utils::render::shader
//
// ------------------------------------------------------------------------------
// HOW MODULES GET REGISTERED
// ------------------------------------------------------------------------------
//
// In App::init(), each module is loaded into ShaderManager by hand:
//
//   auto& shaders = ShaderManager::instance();
//   shaders.loadFromMemory(
//       BlurHModule::CONFIG.name.data(),
//       BlurHModule::CONFIG.vertexSource,
//       BlurHModule::CONFIG.fragmentSource.data()
//   );
//
// No factory, no auto-registration, no macro magic. Explicit > implicit.
// With 16+ shaders planned, explicit registration is trivial to audit.
//
// ==============================================================================

namespace biofuel::utils::render::shader {

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

} // namespace biofuel::utils::render::shader
