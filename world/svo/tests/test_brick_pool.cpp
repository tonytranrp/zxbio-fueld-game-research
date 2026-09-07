// Prompt 004 goal 260/262: the pool's two contracts.
//
//   1. IT NEVER GROWS. That is the whole point -- a pool that grows is not a budget, and the
//      resident footprint has to be independent of world size for streaming to mean anything.
//      `allocate()` returning kNoSlot when full is what forces an eviction path to exist instead of
//      letting the allocator quietly paper over the absence of one.
//   2. THE LRU ORDER IS RIGHT. Goal 262's GPU stream compaction separates slots used this frame
//      from the others and hands back the least recently used; this is the CPU reference it is
//      checked against, so its ordering has to be exactly right rather than approximately.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

#include "world/svo/brick_pool.hpp"

using world::svo::BrickPool;
using world::svo::kNoSlot;

TEST_CASE("a pool hands out every slot exactly once and then refuses", "[svo][pool]") {
    BrickPool pool(8);
    CHECK(pool.capacity() == 8);
    CHECK(pool.used() == 0);
    CHECK_FALSE(pool.full());

    std::set<std::uint32_t> seen;
    for (int i = 0; i < 8; ++i) {
        const std::uint32_t slot = pool.allocate();
        REQUIRE(slot != kNoSlot);
        REQUIRE(slot < 8u);
        CHECK(seen.insert(slot).second); // never the same slot twice
        CHECK(pool.live(slot));
    }
    CHECK(pool.used() == 8);
    CHECK(pool.full());

    // And then it REFUSES rather than growing. This is the assertion goal 260 is really about.
    CHECK(pool.allocate() == kNoSlot);
    CHECK(pool.capacity() == 8);
    CHECK(pool.used() == 8);
}

TEST_CASE("a released slot comes back, and releasing a dead one is a no-op", "[svo][pool]") {
    BrickPool pool(4);
    const std::uint32_t a = pool.allocate();
    const std::uint32_t b = pool.allocate();
    REQUIRE(a != kNoSlot);
    REQUIRE(b != kNoSlot);
    CHECK(pool.used() == 2);

    pool.release(a);
    CHECK(pool.used() == 1);
    CHECK_FALSE(pool.live(a));

    // Double release must not corrupt the free list into handing `a` out twice.
    pool.release(a);
    CHECK(pool.used() == 1);
    // Out of range is also harmless: a caller freeing a cell should not have to know whether it
    // ever got a slot.
    pool.release(kNoSlot);
    pool.release(9999u);
    CHECK(pool.used() == 1);

    std::set<std::uint32_t> seen{b};
    for (int i = 0; i < 3; ++i) {
        const std::uint32_t slot = pool.allocate();
        REQUIRE(slot != kNoSlot);
        CHECK(seen.insert(slot).second);
    }
    CHECK(pool.full());
}

TEST_CASE("usage stamps are idempotent and never go backwards", "[svo][pool]") {
    // Goal 261 relies on this: the GPU marks usage with a plain store and no atomic, which is only
    // safe because writing the same frame index twice is the same as writing it once. `max` also
    // means a late warp writing an OLDER frame cannot un-touch a slot.
    BrickPool pool(4);
    const std::uint32_t slot = pool.allocate();
    CHECK(pool.last_used(slot) == 0u);

    pool.touch(slot, 10u);
    CHECK(pool.last_used(slot) == 10u);
    pool.touch(slot, 10u);
    CHECK(pool.last_used(slot) == 10u);
    pool.touch(slot, 7u); // an older stamp must not win
    CHECK(pool.last_used(slot) == 10u);
    pool.touch(slot, 11u);
    CHECK(pool.last_used(slot) == 11u);

    // Touching a dead slot does nothing, so a stale request cannot resurrect one.
    pool.release(slot);
    pool.touch(slot, 50u);
    CHECK(pool.last_used(slot) == 0u);
}

TEST_CASE("evictable returns the least recently used, oldest first", "[svo][pool]") {
    BrickPool pool(5);
    std::vector<std::uint32_t> slots;
    for (int i = 0; i < 5; ++i) {
        slots.push_back(pool.allocate());
    }
    // Stamps 5, 1, 9, 3, 7.
    const std::uint32_t stamps[5]{5u, 1u, 9u, 3u, 7u};
    for (int i = 0; i < 5; ++i) {
        pool.touch(slots[static_cast<std::size_t>(i)], stamps[i]);
    }

    // Everything older than frame 8: stamps 5, 1, 3, 7 -> ordered 1, 3, 5, 7.
    const std::vector<std::uint32_t> old = pool.evictable(8u, 10);
    REQUIRE(old.size() == 4);
    CHECK(pool.last_used(old[0]) == 1u);
    CHECK(pool.last_used(old[1]) == 3u);
    CHECK(pool.last_used(old[2]) == 5u);
    CHECK(pool.last_used(old[3]) == 7u);

    // Capped, and the cap takes the OLDEST -- taking the newest would evict exactly the wrong ones.
    const std::vector<std::uint32_t> two = pool.evictable(8u, 2);
    REQUIRE(two.size() == 2);
    CHECK(pool.last_used(two[0]) == 1u);
    CHECK(pool.last_used(two[1]) == 3u);

    // A slot used THIS frame is never evictable, which is the property that stops the pool
    // evicting something the current frame is still marching through.
    const std::vector<std::uint32_t> none = pool.evictable(1u, 10);
    CHECK(none.empty());
    CHECK(pool.evictable(8u, 0).empty());
}

TEST_CASE("a dead slot is never evictable", "[svo][pool]") {
    BrickPool pool(3);
    const std::uint32_t a = pool.allocate();
    const std::uint32_t b = pool.allocate();
    pool.touch(a, 1u);
    pool.touch(b, 1u);
    pool.release(a);
    const std::vector<std::uint32_t> out = pool.evictable(100u, 10);
    REQUIRE(out.size() == 1);
    CHECK(out[0] == b);
}

TEST_CASE("a budget becomes a slot count and back again", "[svo][pool]") {
    // The pool is sized ONCE from a byte budget, and this is the arithmetic that does it. 144 words
    // is a brick (16 mask + 128 material), so 512 MB is what the measured 902,616-brick world needs.
    constexpr std::size_t kBrickWords = 144;
    const std::size_t slots = BrickPool::slots_for_budget(512ull * 1024 * 1024, kBrickWords);
    CHECK(slots == (512ull * 1024 * 1024) / (144 * 4));
    CHECK(slots > 900000); // enough for the measured shipping world

    const BrickPool pool(slots);
    CHECK(pool.bytes(kBrickWords) <= 512ull * 1024 * 1024);
    CHECK(pool.bytes(kBrickWords) > 511ull * 1024 * 1024);
    CHECK(BrickPool::slots_for_budget(1000, 0) == 0); // no divide by zero
}

TEST_CASE("allocate and release churn does not leak or duplicate slots", "[svo][pool]") {
    // The property a free-list bug breaks quietly: after any sequence of operations, the live slots
    // and the free slots together are exactly the capacity, with no slot in both.
    BrickPool pool(64);
    std::vector<std::uint32_t> held;
    std::uint32_t rng = 12345u;
    const auto next = [&] {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    for (int step = 0; step < 5000; ++step) {
        if (held.empty() || (next() % 2u) == 0u) {
            const std::uint32_t slot = pool.allocate();
            if (slot != kNoSlot) {
                held.push_back(slot);
            }
        } else {
            const std::size_t i = next() % held.size();
            pool.release(held[i]);
            held[i] = held.back();
            held.pop_back();
        }
        REQUIRE(pool.used() + pool.free_slots() == pool.capacity());
        REQUIRE(pool.used() == held.size());
    }
    // Every held slot is distinct and live.
    const std::set<std::uint32_t> distinct(held.begin(), held.end());
    CHECK(distinct.size() == held.size());
    for (const std::uint32_t slot : held) {
        CHECK(pool.live(slot));
    }
}
