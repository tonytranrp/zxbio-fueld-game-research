#pragma once

#include "engine/core/Types.hpp"
#include "engine/custom/procedural/hand/HandTypes.hpp"
#include "engine/custom/procedural/materials/ProceduralTextureCache.hpp"
#include <array>
#include <filesystem>
#include <raylib.h>

namespace biofuel::engine::custom::procedural::hand {

struct BiofuelGreenRobotMaterial {};

enum class RobotHandMaterialSlot : u8 {
    Shell,
    ShellShadow,
    Accent,
    Joint,
    JointEdge,
    Target,
    PalmPanel,
    Count,
};

template<typename TMaterialTag>
struct RobotHandMaterialSpec;

template<>
struct RobotHandMaterialSpec<BiofuelGreenRobotMaterial> {
    static constexpr Color shell = Color{218, 232, 226, 255};
    static constexpr Color shellShadow = Color{154, 172, 170, 255};
    static constexpr Color leftAccent = Color{78, 215, 136, 255};
    static constexpr Color rightAccent = Color{104, 235, 160, 255};
    static constexpr Color joint = Color{35, 43, 52, 255};
    static constexpr Color jointEdge = Color{91, 107, 118, 255};
    static constexpr Color target = Color{255, 196, 72, 255};
    static constexpr Color targetLine = Color{255, 220, 118, 190};
    static constexpr Color debugLine = Color{74, 184, 255, 170};
    static constexpr Color palmPanel = Color{49, 134, 88, 255};
};

struct RobotHandMaterialState {
    f32 shellIntensity = 1.0f;
    f32 accentIntensity = 1.0f;
    f32 jointIntensity = 1.0f;
    f32 opacity = 1.0f;
};

struct RobotHandTexturePaths {
    std::array<std::filesystem::path, static_cast<usize>(RobotHandMaterialSlot::Count)> slots{};
};

struct RobotHandPalette {
    Color shell{};
    Color shellShadow{};
    Color accent{};
    Color joint{};
    Color jointEdge{};
    Color target{};
    Color targetLine{};
    Color debugLine{};
    Color palmPanel{};
    std::array<::biofuel::engine::custom::procedural::materials::TextureHandle, static_cast<usize>(RobotHandMaterialSlot::Count)> textures{};
};

template<typename TMaterialTag>
[[nodiscard]] constexpr RobotHandPalette robotHandPalette(const HandSide side) noexcept {
    using Spec = RobotHandMaterialSpec<TMaterialTag>;
    return RobotHandPalette{
        .shell = Spec::shell,
        .shellShadow = Spec::shellShadow,
        .accent = side == HandSide::Left ? Spec::leftAccent : Spec::rightAccent,
        .joint = Spec::joint,
        .jointEdge = Spec::jointEdge,
        .target = Spec::target,
        .targetLine = Spec::targetLine,
        .debugLine = Spec::debugLine,
        .palmPanel = Spec::palmPanel,
        .textures = {},
    };
}

[[nodiscard]] inline Color tintColor(const Color color, const f32 amount) noexcept {
    const auto channel = [amount](const u8 value) noexcept -> u8 {
        const f32 adjusted = static_cast<f32>(value) * amount;
        return static_cast<u8>(adjusted < 0.0f ? 0.0f : (adjusted > 255.0f ? 255.0f : adjusted));
    };
    return Color{channel(color.r), channel(color.g), channel(color.b), color.a};
}

[[nodiscard]] inline Color opacityColor(const Color color, const f32 opacity) noexcept {
    const f32 clamped = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    return Color{color.r, color.g, color.b, static_cast<u8>(static_cast<f32>(color.a) * clamped)};
}

[[nodiscard]] inline RobotHandPalette applyMaterialState(RobotHandPalette palette, const RobotHandMaterialState state) noexcept {
    palette.shell = tintColor(palette.shell, state.shellIntensity);
    palette.shellShadow = tintColor(palette.shellShadow, state.shellIntensity);
    palette.accent = tintColor(palette.accent, state.accentIntensity);
    palette.palmPanel = tintColor(palette.palmPanel, state.accentIntensity);
    palette.joint = tintColor(palette.joint, state.jointIntensity);
    palette.jointEdge = tintColor(palette.jointEdge, state.jointIntensity);
    palette.shell = opacityColor(palette.shell, state.opacity);
    palette.shellShadow = opacityColor(palette.shellShadow, state.opacity);
    palette.accent = opacityColor(palette.accent, state.opacity);
    palette.joint = opacityColor(palette.joint, state.opacity);
    palette.jointEdge = opacityColor(palette.jointEdge, state.opacity);
    palette.target = opacityColor(palette.target, state.opacity);
    palette.targetLine = opacityColor(palette.targetLine, state.opacity);
    palette.debugLine = opacityColor(palette.debugLine, state.opacity);
    palette.palmPanel = opacityColor(palette.palmPanel, state.opacity);
    return palette;
}

} // namespace biofuel::engine::custom::procedural::hand
