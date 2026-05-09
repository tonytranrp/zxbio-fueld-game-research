#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::utils::render::shader {

// ==============================================================================
// MainMenuBgModule — Raymarched fractal background for the main menu
// ==============================================================================
//
// Adapted from ShaderToy s3s3WN. Uses gl_FragCoord so no texture binding
// is required — draw any full-screen geometry (e.g. DrawRectangle) with
// this shader active and it will fill the viewport.
//
// GLSL source lives in assets/shaders/mainmenu_bg.glsl and is embedded at
// build time via CMake into shader_source::mainmenu_bg_source.
//
// Uniforms:
//   iResolution (vec3) — viewport width, height, 1.0
//   iTime       (float) — elapsed seconds since screen entered
//   uBrightness (float) — overall brightness envelope during screen intro
//   uRevealProgress (float) — background landing progress (0.0 → 1.0)
// ==============================================================================

class MainMenuBgModule {
public:
    static constexpr std::string_view NAME = "mainmenu_bg";
    static constexpr std::string_view FRAGMENT_SOURCE = shader_source::mainmenu_bg_source;
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };

    static constexpr std::string_view UNIFORM_IRESOLUTION = "iResolution";
    static constexpr std::string_view UNIFORM_ITIME = "iTime";
    static constexpr std::string_view UNIFORM_UBRIGHTNESS = "uBrightness";
    static constexpr std::string_view UNIFORM_UREVEAL_PROGRESS = "uRevealProgress";
};

} // namespace biofuel::utils::render::shader
