#pragma once

#include "engine/core/Types.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <raylib.h>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::physics {

struct PoseBounds {
    Vector3 min{-1.0f, -1.0f, -1.0f};
    Vector3 max{1.0f, 1.0f, 1.0f};
};

template<usize N>
[[nodiscard]] Vector3 poseCenter(const std::array<Vector3, N>& points) noexcept {
    Vector3 center{0.0f, 0.0f, 0.0f};
    for (const Vector3 point : points) {
        center = Vector3Add(center, point);
    }
    return Vector3Scale(center, 1.0f / static_cast<f32>(N));
}

template<usize N>
[[nodiscard]] Vector3 poseWeightedCenter(const std::array<Vector3, N>& points, const std::array<usize, 5U>& indices) noexcept {
    Vector3 center{0.0f, 0.0f, 0.0f};
    for (const usize index : indices) {
        center = Vector3Add(center, points[index]);
    }
    return Vector3Scale(center, 1.0f / static_cast<f32>(indices.size()));
}

template<usize N>
void translatePose(std::array<Vector3, N>& points, const Vector3 delta) noexcept {
    for (Vector3& point : points) {
        point = Vector3Add(point, delta);
    }
}

template<usize N>
void smoothPose(std::array<Vector3, N>& points, const std::array<Vector3, N>& previous, const f32 alpha) noexcept {
    const f32 t = std::clamp(alpha, 0.0f, 1.0f);
    for (usize index = 0U; index < points.size(); ++index) {
        points[index] = Vector3Lerp(previous[index], points[index], t);
    }
}

template<usize N>
void fitPoseInsideBounds(std::array<Vector3, N>& points, const PoseBounds bounds) noexcept {
    const auto measureBounds = [](const std::array<Vector3, N>& measuredPoints, Vector3& minPoint, Vector3& maxPoint) noexcept {
        minPoint = Vector3{std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max(), std::numeric_limits<f32>::max()};
        maxPoint = Vector3{-std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max(), -std::numeric_limits<f32>::max()};
        for (const Vector3 point : measuredPoints) {
            minPoint.x = std::min(minPoint.x, point.x);
            minPoint.y = std::min(minPoint.y, point.y);
            minPoint.z = std::min(minPoint.z, point.z);
            maxPoint.x = std::max(maxPoint.x, point.x);
            maxPoint.y = std::max(maxPoint.y, point.y);
            maxPoint.z = std::max(maxPoint.z, point.z);
        }
    };

    Vector3 minPoint{};
    Vector3 maxPoint{};
    measureBounds(points, minPoint, maxPoint);

    const Vector3 poseSize = Vector3Subtract(maxPoint, minPoint);
    const Vector3 boundsSize = Vector3Subtract(bounds.max, bounds.min);
    f32 shrink = 1.0f;
    if (poseSize.x > boundsSize.x && poseSize.x > 0.0001f) { shrink = std::min(shrink, boundsSize.x / poseSize.x); }
    if (poseSize.y > boundsSize.y && poseSize.y > 0.0001f) { shrink = std::min(shrink, boundsSize.y / poseSize.y); }
    if (poseSize.z > boundsSize.z && poseSize.z > 0.0001f) { shrink = std::min(shrink, boundsSize.z / poseSize.z); }

    if (shrink < 1.0f) {
        const Vector3 center = poseCenter(points);
        for (Vector3& point : points) {
            point = Vector3Add(center, Vector3Scale(Vector3Subtract(point, center), shrink));
        }
        measureBounds(points, minPoint, maxPoint);
    }

    Vector3 delta{0.0f, 0.0f, 0.0f};
    if (minPoint.x < bounds.min.x) { delta.x += bounds.min.x - minPoint.x; }
    if (maxPoint.x > bounds.max.x) { delta.x -= maxPoint.x - bounds.max.x; }
    if (minPoint.y < bounds.min.y) { delta.y += bounds.min.y - minPoint.y; }
    if (maxPoint.y > bounds.max.y) { delta.y -= maxPoint.y - bounds.max.y; }
    if (minPoint.z < bounds.min.z) { delta.z += bounds.min.z - minPoint.z; }
    if (maxPoint.z > bounds.max.z) { delta.z -= maxPoint.z - bounds.max.z; }
    translatePose(points, delta);
}

template<usize N>
void separatePoses(
    std::array<Vector3, N>& first,
    std::array<Vector3, N>& second,
    const Vector3 firstCenter,
    const Vector3 secondCenter,
    const f32 minimumDistance) noexcept
{
    Vector3 delta = Vector3Subtract(secondCenter, firstCenter);
    delta.y *= 0.35f;
    f32 distance = Vector3Length(delta);
    if (distance < 0.0001f) {
        delta = Vector3{secondCenter.x >= firstCenter.x ? 1.0f : -1.0f, 0.0f, 0.0f};
        distance = 1.0f;
    }

    if (distance >= minimumDistance) {
        return;
    }

    const Vector3 direction = Vector3Scale(delta, 1.0f / distance);
    const f32 push = (minimumDistance - distance) * 0.5f;
    translatePose(first, Vector3Scale(direction, -push));
    translatePose(second, Vector3Scale(direction, push));
}

} // namespace biofuel::engine::custom::procedural::physics
