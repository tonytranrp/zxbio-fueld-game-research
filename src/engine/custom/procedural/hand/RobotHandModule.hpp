#pragma once

#include "engine/custom/procedural/animation/HandAnimation.hpp"
#include "engine/custom/procedural/hand/ProceduralHand.hpp"
#include "engine/custom/procedural/hand/RobotHandPreset.hpp"
#include "engine/custom/procedural/hand/RobotHandRenderer.hpp"
#include "engine/custom/procedural/materials/ProceduralTextureCache.hpp"
#include "engine/custom/procedural/mesh/ProceduralMeshCache.hpp"
#include <filesystem>
#include <spdlog/spdlog.h>

namespace biofuel::engine::custom::procedural::hand {

template<typename TModuleTag>
class RobotHandModule final {
public:
    using AnimationSet = ::biofuel::engine::custom::procedural::animation::DefaultHandAnimationSet;
    using AnimationController = ::biofuel::engine::custom::procedural::animation::HandDemoController<AnimationSet>;
    using LeftHand = ProceduralHand<LeftRobotHand>;
    using RightHand = ProceduralHand<RightRobotHand>;

    RobotHandModule() = default;
    RobotHandModule(const RobotHandModule&) = delete;
    RobotHandModule& operator=(const RobotHandModule&) = delete;
    RobotHandModule(RobotHandModule&&) = delete;
    RobotHandModule& operator=(RobotHandModule&&) = delete;

    [[nodiscard]] RobotHandPresetStore& presets() noexcept {
        return m_presets;
    }

    [[nodiscard]] const RobotHandPresetStore& presets() const noexcept {
        return m_presets;
    }

    void applyPreset(const RobotHandPreset& preset) {
        m_preset = preset;
        refreshTextures();
    }

    [[nodiscard]] const RobotHandPreset& preset() const noexcept {
        return m_preset;
    }

    template<typename THandTag>
    [[nodiscard]] ProceduralHand<THandTag> createHand(const Vector3 origin) const noexcept {
        ProceduralHand<THandTag> hand;
        hand.reset(origin, m_preset.dimensions);
        return hand;
    }

    template<typename THandTag>
    void resetHand(ProceduralHand<THandTag>& hand, const Vector3 origin) const noexcept {
        hand.reset(origin, m_preset.dimensions);
    }

    void solve(LeftHand& left, RightHand& right, const ::biofuel::engine::custom::procedural::ik::IkSolveSettings settings) noexcept {
        left.solve(settings);
        right.solve(settings);
    }

    [[nodiscard]] RobotHandRenderContext renderContext() noexcept {
        return RobotHandRenderContext{.meshes = m_meshes, .textures = m_textures};
    }

    [[nodiscard]] RobotHandPalette palette(const HandSide side) const noexcept {
        RobotHandPalette paletteValue = robotHandPalette<BiofuelGreenRobotMaterial>(side);
        paletteValue.textures = m_textureHandles;
        return paletteValue;
    }

    void render(const LeftHand& hand, const RobotHandRenderOptions options) noexcept {
        ProceduralHandRenderer<RobotHandStyle>::render(hand, renderContext(), palette(HandSide::Left), options);
    }

    void render(const RightHand& hand, const RobotHandRenderOptions options) noexcept {
        ProceduralHandRenderer<RobotHandStyle>::render(hand, renderContext(), palette(HandSide::Right), options);
    }

    void renderTracked(const TrackedRobotHandPose& pose, const RobotHandRenderOptions options) noexcept {
        ProceduralHandRenderer<RobotHandStyle>::renderTracked(pose, renderContext(), palette(pose.side), options);
    }

    [[nodiscard]] AnimationController& animation() noexcept {
        return m_animation;
    }

    [[nodiscard]] const AnimationController& animation() const noexcept {
        return m_animation;
    }

    void clearResources() noexcept {
        m_meshes.clear();
        m_textures.clear();
        m_textureHandles = {};
    }

private:
    using TexturePattern = ::biofuel::engine::custom::procedural::materials::TexturePattern;

    void refreshTextures() {
        const auto withFallback = [this](const RobotHandMaterialSlot slot, const TexturePattern pattern, const Color a, const Color b) {
            const std::filesystem::path& path = m_preset.textures.slots[static_cast<usize>(slot)];
            return m_textures.fromPngOrGenerated(path, pattern, a, b);
        };

        const auto left = robotHandPalette<BiofuelGreenRobotMaterial>(HandSide::Left);
        m_textureHandles[static_cast<usize>(RobotHandMaterialSlot::Shell)] = withFallback(RobotHandMaterialSlot::Shell, TexturePattern::Solid, left.shell, left.shell);
        m_textureHandles[static_cast<usize>(RobotHandMaterialSlot::ShellShadow)] = withFallback(RobotHandMaterialSlot::ShellShadow, TexturePattern::Solid, left.shellShadow, left.shellShadow);
        m_textureHandles[static_cast<usize>(RobotHandMaterialSlot::Accent)] = withFallback(RobotHandMaterialSlot::Accent, TexturePattern::Stripes, left.accent, left.palmPanel);
        m_textureHandles[static_cast<usize>(RobotHandMaterialSlot::Joint)] = withFallback(RobotHandMaterialSlot::Joint, TexturePattern::Solid, left.joint, left.joint);
        m_textureHandles[static_cast<usize>(RobotHandMaterialSlot::JointEdge)] = withFallback(RobotHandMaterialSlot::JointEdge, TexturePattern::Solid, left.jointEdge, left.jointEdge);
        m_textureHandles[static_cast<usize>(RobotHandMaterialSlot::Target)] = withFallback(RobotHandMaterialSlot::Target, TexturePattern::Solid, left.target, left.target);
        m_textureHandles[static_cast<usize>(RobotHandMaterialSlot::PalmPanel)] = withFallback(RobotHandMaterialSlot::PalmPanel, TexturePattern::Stripes, left.palmPanel, left.accent);
    }

    RobotHandPresetStore m_presets{};
    RobotHandPreset m_preset{};
    ::biofuel::engine::custom::procedural::mesh::ProceduralMeshCache m_meshes{};
    ::biofuel::engine::custom::procedural::materials::ProceduralTextureCache m_textures{};
    std::array<::biofuel::engine::custom::procedural::materials::TextureHandle, static_cast<usize>(RobotHandMaterialSlot::Count)> m_textureHandles{};
    AnimationController m_animation{};
};

} // namespace biofuel::engine::custom::procedural::hand
