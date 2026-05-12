#pragma once

#include "engine/custom/procedural/hand/ProceduralHand.hpp"
#include "engine/custom/procedural/hand/RobotHandMaterials.hpp"
#include "engine/custom/procedural/materials/ProceduralTextureCache.hpp"
#include "engine/custom/procedural/mesh/ProceduralMeshCache.hpp"
#include <span>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::hand {

struct RobotHandRenderOptions {
    bool showBones = false;
    bool showTargets = true;
    bool selectedTargetOnly = false;
    FingerId selectedFinger = FingerId::Index;
    RobotHandMaterialState materials{};
};

struct RobotHandRenderContext {
    ::biofuel::engine::custom::procedural::mesh::ProceduralMeshCache& meshes;
    ::biofuel::engine::custom::procedural::materials::ProceduralTextureCache& textures;
};

template<typename TStyleTag>
struct ProceduralHandRenderer;

template<>
struct ProceduralHandRenderer<RobotHandStyle> {
    template<typename THandTag>
    static void render(
        const ProceduralHand<THandTag>& hand,
        RobotHandRenderContext context,
        const RobotHandPalette& basePalette,
        const RobotHandRenderOptions options) noexcept
    {
        RobotHandPalette palette = applyMaterialState(basePalette, options.materials);
        drawPalm(context, hand.joints(), palette);

        usize boneIndex = 0U;
        for (const auto& bone : hand.bones()) {
            if (bone[0] == bone[1]) {
                continue;
            }
            const Vector3 a = hand.joints()[bone[0]];
            const Vector3 b = hand.joints()[bone[1]];
            const f32 length = Vector3Distance(a, b);
            if (length <= 0.0001f) {
                continue;
            }

            const bool wristBone = boneIndex == 0U;
            const bool palmToFinger = bone[0] == 1U;
            if (palmToFinger) {
                ++boneIndex;
                continue;
            }

            const f32 radius = wristBone ? 0.026f : 0.0175f;
            drawSegment(context, a, b, radius, wristBone ? palette.joint : palette.shell, textureFor(context, palette, wristBone ? RobotHandMaterialSlot::Joint : RobotHandMaterialSlot::Shell));
            if (!wristBone) {
                drawSegment(context, a, b, radius * 0.42f, palette.accent, textureFor(context, palette, RobotHandMaterialSlot::Accent));
            }
            if (options.showBones) {
                DrawLine3D(a, b, palette.debugLine);
            }
            ++boneIndex;
        }

        drawJointCaps(context, hand, palette);
        drawFingertipPads(context, hand, palette);
        drawTargets(context, hand, options, palette);
    }

private:
    [[nodiscard]] static Texture2D* textureFor(
        RobotHandRenderContext context,
        const RobotHandPalette& palette,
        const RobotHandMaterialSlot slot) noexcept
    {
        return context.textures.texture(palette.textures[static_cast<usize>(slot)]);
    }

    static void drawJointCaps(
        RobotHandRenderContext context,
        const auto& hand,
        const RobotHandPalette& palette) noexcept
    {
        const auto cube = context.meshes.cube();
        const auto sphere = context.meshes.sphere(8, 8);
        for (usize index = 0U; index < hand.joints().size(); ++index) {
            const Vector3 position = hand.joints()[index];
            if (index == 0U) {
                context.meshes.draw(cube, position, Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.090f, 0.052f, 0.060f}, palette.joint, textureFor(context, palette, RobotHandMaterialSlot::Joint));
            } else if (index == 1U) {
                context.meshes.draw(cube, position, Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.180f, 0.050f, 0.072f}, palette.joint, textureFor(context, palette, RobotHandMaterialSlot::Joint));
            } else {
                const f32 radius = isFingerBase(index) ? 0.026f : 0.020f;
                const Vector3 blockScale = isFingerBase(index)
                    ? Vector3{0.042f, 0.034f, 0.038f}
                    : Vector3{0.032f, 0.026f, 0.030f};
                context.meshes.draw(cube, position, Vector3{0.0f, 1.0f, 0.0f}, 0.0f, blockScale, palette.joint, textureFor(context, palette, RobotHandMaterialSlot::Joint));
                context.meshes.draw(sphere, Vector3Add(position, Vector3{0.0f, 0.003f, -0.002f}), Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{radius * 0.54f, radius * 0.54f, radius * 0.54f}, palette.jointEdge, textureFor(context, palette, RobotHandMaterialSlot::JointEdge));
            }
        }
    }

    [[nodiscard]] static bool isFingerBase(const usize jointIndex) noexcept {
        if (jointIndex < 2U) {
            return false;
        }
        return ((jointIndex - 2U) % 5U) == 0U;
    }

    static void drawSegment(
        RobotHandRenderContext context,
        const Vector3 a,
        const Vector3 b,
        const f32 radius,
        const Color color,
        Texture2D* texture) noexcept
    {
        const f32 length = Vector3Distance(a, b);
        if (length <= 0.0001f) {
            return;
        }
        context.meshes.draw(
            context.meshes.cylinder(8),
            ::biofuel::engine::custom::procedural::mesh::midpoint(a, b),
            ::biofuel::engine::custom::procedural::mesh::rotationAxisForSegment(a, b),
            ::biofuel::engine::custom::procedural::mesh::rotationDegreesForSegment(a, b),
            Vector3{radius, length, radius},
            color,
            texture);
    }

    static void drawPalm(
        RobotHandRenderContext context,
        const std::span<const Vector3> joints,
        const RobotHandPalette& palette) noexcept
    {
        const auto cube = context.meshes.cube();
        const Vector3 palmCenter = Vector3Add(joints[1], Vector3{0.0f, -0.01f, 0.0f});
        context.meshes.draw(cube, palmCenter, Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.270f, 0.145f, 0.074f}, palette.shell, textureFor(context, palette, RobotHandMaterialSlot::Shell));
        context.meshes.draw(cube, Vector3Add(palmCenter, Vector3{0.0f, -0.062f, 0.0f}), Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.286f, 0.020f, 0.082f}, palette.joint, textureFor(context, palette, RobotHandMaterialSlot::Joint));
        context.meshes.draw(cube, Vector3Add(palmCenter, Vector3{-0.108f, 0.0f, -0.001f}), Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.020f, 0.122f, 0.082f}, palette.jointEdge, textureFor(context, palette, RobotHandMaterialSlot::JointEdge));
        context.meshes.draw(cube, Vector3Add(palmCenter, Vector3{0.108f, 0.0f, -0.001f}), Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.020f, 0.122f, 0.082f}, palette.jointEdge, textureFor(context, palette, RobotHandMaterialSlot::JointEdge));
        context.meshes.draw(cube, Vector3Add(palmCenter, Vector3{0.0f, 0.004f, -0.040f}), Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.182f, 0.058f, 0.010f}, palette.palmPanel, textureFor(context, palette, RobotHandMaterialSlot::PalmPanel));
        context.meshes.draw(cube, Vector3Add(palmCenter, Vector3{0.0f, 0.046f, -0.047f}), Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.226f, 0.010f, 0.012f}, palette.accent, textureFor(context, palette, RobotHandMaterialSlot::Accent));
    }

    template<typename THandTag>
    static void drawFingertipPads(
        RobotHandRenderContext context,
        const ProceduralHand<THandTag>& hand,
        const RobotHandPalette& palette) noexcept
    {
        const auto cube = context.meshes.cube();
        const auto joints = hand.joints();
        for (const auto& finger : hand.debugFingers()) {
            const usize base = 2U + static_cast<usize>(finger.id) * ProceduralHand<THandTag>::JOINTS_PER_FINGER;
            const usize tip = base + ProceduralHand<THandTag>::JOINTS_PER_FINGER - 1U;
            context.meshes.draw(cube, joints[tip], Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.045f, 0.032f, 0.030f}, palette.shell, textureFor(context, palette, RobotHandMaterialSlot::Shell));
            context.meshes.draw(cube, Vector3Add(joints[tip], Vector3{0.0f, 0.012f, -0.018f}), Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.030f, 0.010f, 0.007f}, palette.accent, textureFor(context, palette, RobotHandMaterialSlot::Accent));
        }
    }

    template<typename THandTag>
    static void drawTargets(
        RobotHandRenderContext context,
        const ProceduralHand<THandTag>& hand,
        const RobotHandRenderOptions options,
        const RobotHandPalette& palette) noexcept
    {
        if (!options.showTargets) {
            return;
        }

        const auto sphere = context.meshes.sphere(8, 8);
        for (const auto& finger : hand.debugFingers()) {
            if (options.selectedTargetOnly && finger.id != options.selectedFinger) {
                continue;
            }
            context.meshes.draw(sphere, finger.target, Vector3{0.0f, 1.0f, 0.0f}, 0.0f, Vector3{0.028f, 0.028f, 0.028f}, palette.target, textureFor(context, palette, RobotHandMaterialSlot::Target));
            DrawLine3D(finger.target, Vector3Add(finger.target, Vector3{0.0f, 0.0f, 0.09f}), palette.targetLine);
        }
    }
};

} // namespace biofuel::engine::custom::procedural::hand
