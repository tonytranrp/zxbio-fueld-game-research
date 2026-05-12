#pragma once

#include "engine/core/Types.hpp"
#include <algorithm>
#include <raylib.h>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::ik {

struct JointLimitData {
    f32 minBend = -1.2f;
    f32 maxBend = 0.2f;
    f32 minSplay = -0.35f;
    f32 maxSplay = 0.35f;
};

template<typename TJoint>
struct JointLimit {
    static constexpr JointLimitData value{};

    [[nodiscard]] static Vector3 clampDirection(
        const Vector3 direction,
        const f32 curlBias,
        const f32 splayBias) noexcept
    {
        Vector3 clamped = direction;
        clamped.x = std::clamp(clamped.x + splayBias, value.minSplay, value.maxSplay);
        clamped.y = std::max(clamped.y + curlBias, 0.04f);
        clamped.z = std::clamp(clamped.z, value.minBend, value.maxBend);

        const f32 length = Vector3Length(clamped);
        if (length <= 0.0001f) {
            return Vector3{0.0f, 1.0f, 0.0f};
        }
        return Vector3Scale(clamped, 1.0f / length);
    }
};

} // namespace biofuel::engine::custom::procedural::ik
