#pragma once

#include "engine/core/Types.hpp"
#include <algorithm>
#include <raylib.h>

namespace biofuel::engine::core::units {

struct PixelToMeterScale {
    f32 pixelsPerMeter = 32.0f;

    [[nodiscard]] constexpr f32 pixelsToMeters(const f32 pixels) const noexcept {
        return pixelsPerMeter <= 0.0f ? pixels : pixels / pixelsPerMeter;
    }

    [[nodiscard]] constexpr f32 metersToPixels(const f32 meters) const noexcept {
        return pixelsPerMeter <= 0.0f ? meters : meters * pixelsPerMeter;
    }

    [[nodiscard]] constexpr Vector2 pixelsToMeters(const Vector2 pixels) const noexcept {
        return Vector2{pixelsToMeters(pixels.x), pixelsToMeters(pixels.y)};
    }

    [[nodiscard]] constexpr Vector2 metersToPixels(const Vector2 meters) const noexcept {
        return Vector2{metersToPixels(meters.x), metersToPixels(meters.y)};
    }
};

struct TileSizePixels {
    f32 value = 32.0f;
};

struct WorldMeters2D {
    f32 x = 0.0f;
    f32 y = 0.0f;

    constexpr WorldMeters2D() noexcept = default;
    constexpr WorldMeters2D(const f32 xValue, const f32 yValue) noexcept
        : x(xValue), y(yValue) {}

    [[nodiscard]] constexpr Vector2 toVector2() const noexcept { return Vector2{x, y}; }
    [[nodiscard]] static constexpr WorldMeters2D fromVector2(const Vector2 value) noexcept {
        return WorldMeters2D{value.x, value.y};
    }
};

struct WorldMeters3D {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    constexpr WorldMeters3D() noexcept = default;
    constexpr WorldMeters3D(const f32 xValue, const f32 yValue, const f32 zValue) noexcept
        : x(xValue), y(yValue), z(zValue) {}

    [[nodiscard]] constexpr Vector3 toVector3() const noexcept { return Vector3{x, y, z}; }
    [[nodiscard]] static constexpr WorldMeters3D fromVector3(const Vector3 value) noexcept {
        return WorldMeters3D{value.x, value.y, value.z};
    }
};

struct ScreenPixels2D {
    f32 x = 0.0f;
    f32 y = 0.0f;

    constexpr ScreenPixels2D() noexcept = default;
    constexpr ScreenPixels2D(const f32 xValue, const f32 yValue) noexcept
        : x(xValue), y(yValue) {}

    [[nodiscard]] constexpr Vector2 toVector2() const noexcept { return Vector2{x, y}; }
    [[nodiscard]] static constexpr ScreenPixels2D fromVector2(const Vector2 value) noexcept {
        return ScreenPixels2D{value.x, value.y};
    }
};

struct TileCoord {
    i32 x = 0;
    i32 y = 0;
};

struct NormalizedCameraCoord2D {
    f32 x = 0.0f;
    f32 y = 0.0f;

    constexpr NormalizedCameraCoord2D() noexcept = default;
    constexpr NormalizedCameraCoord2D(const f32 xValue, const f32 yValue) noexcept
        : x(xValue), y(yValue) {}

    [[nodiscard]] constexpr Vector2 toVector2() const noexcept { return Vector2{x, y}; }
    [[nodiscard]] static constexpr NormalizedCameraCoord2D fromVector2(const Vector2 value) noexcept {
        return NormalizedCameraCoord2D{value.x, value.y};
    }
};

struct NormalizedCameraCoord3D {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    constexpr NormalizedCameraCoord3D() noexcept = default;
    constexpr NormalizedCameraCoord3D(const f32 xValue, const f32 yValue, const f32 zValue) noexcept
        : x(xValue), y(yValue), z(zValue) {}

    [[nodiscard]] constexpr Vector3 toVector3() const noexcept { return Vector3{x, y, z}; }
    [[nodiscard]] static constexpr NormalizedCameraCoord3D fromVector3(const Vector3 value) noexcept {
        return NormalizedCameraCoord3D{value.x, value.y, value.z};
    }
};

[[nodiscard]] constexpr ScreenPixels2D toScreenPixels(
    const WorldMeters2D value,
    const PixelToMeterScale scale) noexcept
{
    return ScreenPixels2D{scale.metersToPixels(value.x), scale.metersToPixels(value.y)};
}

[[nodiscard]] constexpr WorldMeters2D toWorldMeters(
    const ScreenPixels2D value,
    const PixelToMeterScale scale) noexcept
{
    return WorldMeters2D{scale.pixelsToMeters(value.x), scale.pixelsToMeters(value.y)};
}

[[nodiscard]] constexpr TileCoord toTileCoord(
    const ScreenPixels2D value,
    const TileSizePixels tileSize) noexcept
{
    const f32 safeTile = tileSize.value <= 0.0f ? 1.0f : tileSize.value;
    return TileCoord{
        static_cast<i32>(value.x / safeTile),
        static_cast<i32>(value.y / safeTile),
    };
}

[[nodiscard]] constexpr NormalizedCameraCoord2D clampNormalized(
    const NormalizedCameraCoord2D value) noexcept
{
    return NormalizedCameraCoord2D{
        std::clamp(value.x, 0.0f, 1.0f),
        std::clamp(value.y, 0.0f, 1.0f),
    };
}

[[nodiscard]] constexpr NormalizedCameraCoord3D clampNormalized(
    const NormalizedCameraCoord3D value) noexcept
{
    return NormalizedCameraCoord3D{
        std::clamp(value.x, 0.0f, 1.0f),
        std::clamp(value.y, 0.0f, 1.0f),
        value.z,
    };
}

} // namespace biofuel::engine::core::units
