#pragma once

// Prompt 004 goal 260: a fixed-capacity brick pool with slot allocation, and the usage stamps
// goals 261-262 build the LRU on.
//
// WHY BRICKS AND NOT ALSO NODES, decided from the measurement rather than from the architecture
// diagram. On the shipping 512 m region the tree is 902,616 bricks totalling 519.9 MB out of
// 549.8 MB -- **94.6% of the resident bytes are bricks**, and the node array is 5.4%. And every
// brick is exactly `kBrickWords` long, so a brick pool is a perfect fixed-size slot allocator with
// **no fragmentation at all**, where a node pool would need variable-size spans and the compaction
// that comes with them. Ninety-five percent of the win for none of the difficulty is the right
// trade, and the remaining 5% is recorded rather than hidden.
//
// WHAT "FIXED CAPACITY" BUYS, which is goal 260's actual Check. The point is not to save memory --
// there is VRAM to spare (7,180 MiB budget against 550 MB resident). The point is that the
// footprint becomes **independent of world size**: the pools are sized once from a budget, they
// never grow, and a camera that flies for a minute occupies exactly what a camera that has just
// started does. That is the property a streaming architecture is built on, and `allocate()`
// returning `kNoSlot` when full is what forces the eviction path to exist rather than letting the
// allocator paper over it.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace world::svo {

inline constexpr std::uint32_t kNoSlot = 0xFFFFFFFFu;

/// A fixed-capacity allocator of equally sized brick slots, with a per-slot "last used" stamp.
///
/// Never allocates after construction. `allocate()` returns `kNoSlot` when full rather than growing
/// -- deliberately, because a pool that grows is not a budget.
class BrickPool {
public:
    BrickPool() = default;
    explicit BrickPool(std::size_t capacity) : lastUsed_(capacity, 0u), live_(capacity, false) {
        free_.reserve(capacity);
        // Descending, so the first allocations come off the FRONT of the pool. Slot order is
        // otherwise arbitrary, and a predictable one makes a dump readable.
        for (std::size_t i = capacity; i > 0; --i) {
            free_.push_back(static_cast<std::uint32_t>(i - 1));
        }
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return live_.size(); }
    [[nodiscard]] std::size_t free_slots() const noexcept { return free_.size(); }
    [[nodiscard]] std::size_t used() const noexcept { return capacity() - free_slots(); }
    [[nodiscard]] bool full() const noexcept { return free_.empty(); }
    [[nodiscard]] bool live(std::uint32_t slot) const noexcept { return slot < live_.size() && live_[slot]; }

    /// One slot, or `kNoSlot` when the pool is full. A fresh slot's stamp is 0, which is older than
    /// any real frame index -- so a slot allocated and never touched is the first thing evicted.
    [[nodiscard]] std::uint32_t allocate() noexcept {
        if (free_.empty()) {
            return kNoSlot;
        }
        const std::uint32_t slot = free_.back();
        free_.pop_back();
        live_[slot] = true;
        lastUsed_[slot] = 0u;
        return slot;
    }

    /// Returns a slot to the pool. Releasing a slot that is not live is a no-op rather than an
    /// error: the caller that frees a cell should not have to know whether it ever got one.
    void release(std::uint32_t slot) noexcept {
        if (slot >= live_.size() || !live_[slot]) {
            return;
        }
        live_[slot] = false;
        lastUsed_[slot] = 0u;
        free_.push_back(slot);
    }

    /// Goal 261: mark a slot used on `frame`. Idempotent -- writing the same frame index twice is
    /// the same as writing it once, which is the property that lets the GPU do this with a plain
    /// store and no atomic.
    void touch(std::uint32_t slot, std::uint32_t frame) noexcept {
        if (slot < lastUsed_.size() && live_[slot]) {
            lastUsed_[slot] = std::max(lastUsed_[slot], frame);
        }
    }
    [[nodiscard]] std::uint32_t last_used(std::uint32_t slot) const noexcept {
        return slot < lastUsed_.size() ? lastUsed_[slot] : 0u;
    }

    /// Goal 262: live slots not touched since `frame`, **oldest first**, at most `max` of them.
    ///
    /// This is the CPU reference for the stream compaction the GPU does: separate the slots used
    /// this frame from the others and hand back the least recently used. A caller evicts from the
    /// front of what this returns.
    [[nodiscard]] std::vector<std::uint32_t> evictable(std::uint32_t frame, std::size_t max) const {
        std::vector<std::uint32_t> out;
        if (max == 0) {
            return out;
        }
        out.reserve(std::min(max, used()));
        for (std::uint32_t slot = 0; slot < live_.size(); ++slot) {
            if (live_[slot] && lastUsed_[slot] < frame) {
                out.push_back(slot);
            }
        }
        std::sort(out.begin(), out.end(), [this](std::uint32_t a, std::uint32_t b) {
            return lastUsed_[a] != lastUsed_[b] ? lastUsed_[a] < lastUsed_[b] : a < b;
        });
        if (out.size() > max) {
            out.resize(max);
        }
        return out;
    }

    /// Bytes this pool's slots occupy on the GPU, which is what a budget is stated in.
    [[nodiscard]] std::uint64_t bytes(std::size_t words_per_slot) const noexcept {
        return static_cast<std::uint64_t>(capacity()) * words_per_slot * sizeof(std::uint32_t);
    }

    /// Slots for a byte budget. The pool is sized from this once and never again.
    [[nodiscard]] static std::size_t slots_for_budget(std::uint64_t budget_bytes,
                                                      std::size_t words_per_slot) noexcept {
        const std::uint64_t per = static_cast<std::uint64_t>(words_per_slot) * sizeof(std::uint32_t);
        return per == 0 ? 0 : static_cast<std::size_t>(budget_bytes / per);
    }

private:
    std::vector<std::uint32_t> lastUsed_;
    std::vector<bool> live_;
    std::vector<std::uint32_t> free_;
};

} // namespace world::svo
