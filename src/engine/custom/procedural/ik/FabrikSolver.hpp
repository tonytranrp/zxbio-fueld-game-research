#pragma once

#include "engine/custom/procedural/ik/IkTypes.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <raymath.h>

namespace biofuel::engine::custom::procedural::ik {

template<typename TChain, usize TMaxSegments = 8U>
struct FabrikSolver {
    static_assert(TMaxSegments + 1U >= TChain::jointCount, "FABRIK segment buffer is too small for this typed chain");

    static IkSolveResult solve(
        const std::span<Vector3> points,
        const Vector3 target,
        const IkSolveSettings settings = {}) noexcept
    {
        IkSolveResult result{};
        if (points.size() < 2U) {
            result.reached = true;
            return result;
        }

        constexpr f32 minLength = 0.0001f;
        const Vector3 root = points.front();
        std::array<f32, TMaxSegments> lengths{};
        const usize segmentCount = std::min(points.size() - 1U, lengths.size());
        f32 totalLength = 0.0f;

        for (usize index = 0U; index < segmentCount; ++index) {
            lengths[index] = std::max(Vector3Distance(points[index], points[index + 1U]), minLength);
            totalLength += lengths[index];
        }

        if (segmentCount == 0U) {
            result.reached = true;
            return result;
        }

        const f32 rootToTarget = Vector3Distance(root, target);
        if (rootToTarget >= totalLength) {
            const Vector3 direction = safeDirection(Vector3Subtract(target, root), Vector3{0.0f, 1.0f, 0.0f});
            for (usize index = 1U; index < points.size(); ++index) {
                const usize lengthIndex = std::min(index - 1U, segmentCount - 1U);
                points[index] = Vector3Add(points[index - 1U], Vector3Scale(direction, lengths[lengthIndex]));
            }
            result.error = Vector3Distance(points.back(), target);
            result.iterations = 1;
            result.reached = result.error <= settings.tolerance;
            return result;
        }

        const i32 maxIterations = std::max(settings.maxIterations, 1);
        for (i32 iteration = 0; iteration < maxIterations; ++iteration) {
            points.back() = target;
            for (usize reverseIndex = points.size() - 1U; reverseIndex > 0U; --reverseIndex) {
                const usize parent = reverseIndex - 1U;
                const usize lengthIndex = std::min(parent, segmentCount - 1U);
                const Vector3 direction = safeDirection(
                    Vector3Subtract(points[parent], points[reverseIndex]),
                    Vector3{0.0f, -1.0f, 0.0f});
                points[parent] = Vector3Add(points[reverseIndex], Vector3Scale(direction, lengths[lengthIndex]));
            }

            points.front() = root;
            for (usize index = 0U; index < segmentCount; ++index) {
                const Vector3 direction = safeDirection(
                    Vector3Subtract(points[index + 1U], points[index]),
                    Vector3{0.0f, 1.0f, 0.0f});
                points[index + 1U] = Vector3Add(points[index], Vector3Scale(direction, lengths[index]));
            }

            result.error = Vector3Distance(points.back(), target);
            result.iterations = iteration + 1;
            if (result.error <= settings.tolerance) {
                result.reached = true;
                return result;
            }
        }

        result.reached = result.error <= settings.tolerance;
        return result;
    }

private:
    [[nodiscard]] static Vector3 safeDirection(const Vector3 value, const Vector3 fallback) noexcept {
        const f32 length = Vector3Length(value);
        if (length <= 0.0001f) {
            return fallback;
        }
        return Vector3Scale(value, 1.0f / length);
    }
};

} // namespace biofuel::engine::custom::procedural::ik
