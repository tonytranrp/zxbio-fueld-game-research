#include "MemoryTelemetry.hpp"

#include <algorithm>
#include <atomic>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Psapi.h>
#endif

namespace biofuel::engine::debug {

namespace {

struct AtomicResourceStats {
    std::atomic<i64> liveCount{0};
    std::atomic<i64> peakCount{0};
    std::atomic<i64> liveBytes{0};
    std::atomic<i64> peakBytes{0};
};

std::array<AtomicResourceStats, static_cast<size_t>(ResourceKind::Count)> g_stats{};

[[nodiscard]] AtomicResourceStats& slot(const ResourceKind kind) noexcept {
    return g_stats[static_cast<size_t>(kind)];
}

void updatePeak(std::atomic<i64>& peak, const i64 value) noexcept {
    i64 current = peak.load(std::memory_order_relaxed);
    while (value > current &&
        !peak.compare_exchange_weak(current, value, std::memory_order_relaxed)) {
    }
}

[[nodiscard]] f64 mib(const u64 bytes) noexcept {
    return static_cast<f64>(bytes) / (1024.0 * 1024.0);
}

[[nodiscard]] f64 mibSigned(const i64 bytes) noexcept {
    return static_cast<f64>(bytes) / (1024.0 * 1024.0);
}

} // namespace

void MemoryTelemetry::add([[maybe_unused]] const ResourceKind kind, [[maybe_unused]] const i64 count, [[maybe_unused]] const i64 bytes) noexcept {
#ifndef NDEBUG
    auto& stats = slot(kind);
    const i64 liveCount = stats.liveCount.fetch_add(count, std::memory_order_relaxed) + count;
    const i64 liveBytes = stats.liveBytes.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    updatePeak(stats.peakCount, liveCount);
    updatePeak(stats.peakBytes, liveBytes);
#else
#endif
}

void MemoryTelemetry::remove([[maybe_unused]] const ResourceKind kind, [[maybe_unused]] const i64 count, [[maybe_unused]] const i64 bytes) noexcept {
#ifndef NDEBUG
    auto& stats = slot(kind);
    stats.liveCount.fetch_sub(count, std::memory_order_relaxed);
    stats.liveBytes.fetch_sub(bytes, std::memory_order_relaxed);
#else
#endif
}

void MemoryTelemetry::set([[maybe_unused]] const ResourceKind kind, [[maybe_unused]] const i64 count, [[maybe_unused]] const i64 bytes) noexcept {
#ifndef NDEBUG
    auto& stats = slot(kind);
    stats.liveCount.store(count, std::memory_order_relaxed);
    stats.liveBytes.store(bytes, std::memory_order_relaxed);
    updatePeak(stats.peakCount, count);
    updatePeak(stats.peakBytes, bytes);
#else
#endif
}

void MemoryTelemetry::snapshot([[maybe_unused]] const std::string_view label) noexcept {
#ifndef NDEBUG
    const ProcessMemoryStats process = processMemory();
    spdlog::debug(
        "MemoryTelemetry [{}]: process working={:.1f} MiB private={:.1f} MiB",
        label,
        mib(process.workingSetBytes),
        mib(process.privateBytes));

    for (size_t index = 0; index < static_cast<size_t>(ResourceKind::Count); ++index) {
        const auto kind = static_cast<ResourceKind>(index);
        const ResourceStats resource = stats(kind);
        if (resource.liveCount == 0 && resource.peakCount == 0 && resource.liveBytes == 0 && resource.peakBytes == 0) {
            continue;
        }

        spdlog::debug(
            "MemoryTelemetry [{}]: {} live={} peak={} bytes={:.2f} MiB peakBytes={:.2f} MiB",
            label,
            name(kind),
            resource.liveCount,
            resource.peakCount,
            mibSigned(resource.liveBytes),
            mibSigned(resource.peakBytes));
    }
#else
#endif
}

ResourceStats MemoryTelemetry::stats(const ResourceKind kind) noexcept {
    const auto& stats = slot(kind);
    return ResourceStats{
        .liveCount = stats.liveCount.load(std::memory_order_relaxed),
        .peakCount = stats.peakCount.load(std::memory_order_relaxed),
        .liveBytes = stats.liveBytes.load(std::memory_order_relaxed),
        .peakBytes = stats.peakBytes.load(std::memory_order_relaxed),
    };
}

ProcessMemoryStats MemoryTelemetry::processMemory() noexcept {
    ProcessMemoryStats stats{};
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            static_cast<DWORD>(sizeof(counters)))) {
        stats.workingSetBytes = static_cast<u64>(counters.WorkingSetSize);
        stats.privateBytes = static_cast<u64>(counters.PrivateUsage);
    }
#endif
    return stats;
}

std::string_view MemoryTelemetry::name(const ResourceKind kind) noexcept {
    switch (kind) {
    case ResourceKind::RenderSurface: return "render_surface";
    case ResourceKind::Shader: return "shader";
    case ResourceKind::ModelAsset: return "model_asset";
    case ResourceKind::ModelInstance: return "model_instance";
    case ResourceKind::VideoFrameQueue: return "video_frame_queue";
    case ResourceKind::AudioChunkQueue: return "audio_chunk_queue";
    case ResourceKind::AudioAsset: return "audio_asset";
    case ResourceKind::Animation: return "animation";
    case ResourceKind::Count: break;
    }
    return "unknown";
}

} // namespace biofuel::engine::debug
