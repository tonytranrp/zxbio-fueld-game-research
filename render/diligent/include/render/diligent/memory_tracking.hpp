#pragma once

#include <atomic>
#include <cstdint>

namespace render::diligent {

// Self-tracked GPU allocation accounting (Phase 1 completion brief §2.3): a running byte total
// this project maintains itself, incremented/decremented on every chunk vertex/index buffer
// create/destroy. Answers "how much VRAM is our own chunk system using" regardless of what any
// graphics API exposes -- the VK_EXT_memory_budget number (Group E, task 30) answers the
// different question "how much VRAM exists/is used machine-wide" and complements, not replaces,
// this. Thread-safe: upload/teardown will eventually run from job-system completions (Group D).
class GpuAllocationTracker {
public:
    void on_allocate(std::uint64_t bytes) noexcept {
        const std::uint64_t now = allocated_.fetch_add(bytes) + bytes;
        // Racy-loop CAS so a concurrent larger peak is never overwritten with a smaller one.
        std::uint64_t peak = peak_.load();
        while (now > peak && !peak_.compare_exchange_weak(peak, now)) {
        }
    }

    void on_free(std::uint64_t bytes) noexcept { allocated_.fetch_sub(bytes); }

    [[nodiscard]] std::uint64_t allocated_bytes() const noexcept { return allocated_.load(); }
    [[nodiscard]] std::uint64_t peak_bytes() const noexcept { return peak_.load(); }

private:
    // seq_cst throughout (skill rule: name the specific safe race before relaxing) -- these are
    // read once a frame for an overlay, nowhere near hot enough to justify weaker ordering.
    std::atomic<std::uint64_t> allocated_{0};
    std::atomic<std::uint64_t> peak_{0};
};

} // namespace render::diligent
