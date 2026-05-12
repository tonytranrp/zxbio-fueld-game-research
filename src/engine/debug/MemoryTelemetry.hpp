#pragma once

#include "engine/core/Types.hpp"
#include <array>
#include <string_view>

namespace biofuel::engine::debug {

enum class ResourceKind : u8 {
    RenderSurface,
    Shader,
    ModelAsset,
    ModelInstance,
    VideoFrameQueue,
    AudioChunkQueue,
    AudioAsset,
    Animation,
    Count
};

struct ResourceStats {
    i64 liveCount = 0;
    i64 peakCount = 0;
    i64 liveBytes = 0;
    i64 peakBytes = 0;
};

struct ProcessMemoryStats {
    u64 workingSetBytes = 0;
    u64 privateBytes = 0;
};

class MemoryTelemetry {
public:
    static void add(ResourceKind kind, i64 count, i64 bytes) noexcept;
    static void remove(ResourceKind kind, i64 count, i64 bytes) noexcept;
    static void set(ResourceKind kind, i64 count, i64 bytes) noexcept;
    static void snapshot(std::string_view label) noexcept;

    [[nodiscard]] static ResourceStats stats(ResourceKind kind) noexcept;
    [[nodiscard]] static ProcessMemoryStats processMemory() noexcept;
    [[nodiscard]] static std::string_view name(ResourceKind kind) noexcept;
};

} // namespace biofuel::engine::debug
