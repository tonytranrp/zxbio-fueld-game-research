#include <catch2/catch_test_macros.hpp>

#include "svo_world.hpp"

// Goal 347's regression guard, and it exists because NOTHING asserted this before.
//
// `SvoWorld::should_rebuild` is the whole of the LOD centre's locomotion: the finest voxels are
// placed around `buildCenter_`, and this predicate is the only thing that ever moves it. It shipped
// with a hysteresis latch that could never clear, so after the first post-spawn rebuild the centre
// froze for the rest of the run and the world around the player resolved at ~19 cm instead of
// 7.81 mm. Four "verified" prompts measured performance on that world without noticing, because a
// stranded LOD centre does not fail any check that was being run -- it just quietly renders less.
//
// The tests below drive the predicate directly rather than launching builds: `request_build` starts
// a real worker thread over a real terrain sampler, which is a different (and much slower) question
// than "does the trigger ever arm again".
namespace {

// The macro field off, so the constructor is a thread pool and nothing else -- these tests are
// about the trigger's arithmetic, not about terrain.
[[nodiscard]] app::SvoWorldOptions trigger_test_options() {
    app::SvoWorldOptions options;
    options.macro_field = false;
    options.worker_threads = 1;
    return options;
}

} // namespace

TEST_CASE("the rebuild trigger arms again after the camera walks away", "[svo][rebuild]") {
    const app::SvoWorldOptions options = trigger_test_options();
    app::SvoWorld world(options);

    const float trigger = options.rebuild_trigger_metres;
    const double settled = static_cast<double>(options.rebuild_min_interval_seconds) * 10.0;

    // Inside the trigger radius of the initial centre (the origin): nothing to do.
    REQUIRE_FALSE(world.should_rebuild(glm::vec3{trigger * 0.5f, 0.0f, 0.0f}, 0.0f, settled));

    // Past it: arm.
    const glm::vec3 away{trigger * 2.0f, 0.0f, 0.0f};
    REQUIRE(world.should_rebuild(away, 0.0f, settled));

    // THE REGRESSION. Asking twice must not disarm the trigger. The old latch was set by the first
    // call and could only be cleared by a camera sitting within 3 m of a build centre it had
    // already left -- so this second call returned false, and so did every call after it, forever.
    REQUIRE(world.should_rebuild(away, 0.0f, settled));
    REQUIRE(world.should_rebuild(away, 0.0f, settled + 100.0));
    REQUIRE(world.should_rebuild(away * 10.0f, 0.0f, settled + 1000.0));
}

TEST_CASE("the rebuild trigger is a pure predicate", "[svo][rebuild]") {
    // It is `const noexcept` and, since goal 347, actually const: no `mutable` state, so asking the
    // question never changes the answer. That property is what the test above depends on, and it is
    // worth pinning separately -- reintroducing a latch would break this one first.
    const app::SvoWorldOptions options = trigger_test_options();
    app::SvoWorld world(options);
    const glm::vec3 away{options.rebuild_trigger_metres * 3.0f, 0.0f, 0.0f};
    const double settled = static_cast<double>(options.rebuild_min_interval_seconds) * 10.0;

    const bool first = world.should_rebuild(away, 0.0f, settled);
    for (int i = 0; i < 16; ++i) {
        REQUIRE(world.should_rebuild(away, 0.0f, settled) == first);
    }
    REQUIRE(first);
}

TEST_CASE("the rebuild trigger still defers while moving fast", "[svo][rebuild]") {
    // Goal 250's speed gate survives goal 347 -- removing the latch must not remove the reason the
    // rebuild storm was fixed in the first place.
    const app::SvoWorldOptions options = trigger_test_options();
    app::SvoWorld world(options);
    const glm::vec3 away{options.rebuild_trigger_metres * 3.0f, 0.0f, 0.0f};
    const double settled = static_cast<double>(options.rebuild_min_interval_seconds) * 10.0;

    REQUIRE_FALSE(world.should_rebuild(away, options.rebuild_max_speed + 1.0f, settled));
    REQUIRE(world.should_rebuild(away, options.rebuild_max_speed - 1.0f, settled));
}

TEST_CASE("the rebuild trigger honours the minimum interval", "[svo][rebuild]") {
    // Goal 249's rate bound, likewise. It is measured from the last ADOPTION, so a fresh world
    // (which has adopted nothing) must not be gated by it -- the initial build has to be able to
    // start immediately.
    const app::SvoWorldOptions options = trigger_test_options();
    app::SvoWorld world(options);
    const glm::vec3 away{options.rebuild_trigger_metres * 3.0f, 0.0f, 0.0f};

    REQUIRE(world.should_rebuild(away, 0.0f, 0.0));

    world.note_adopted(100.0, away);
    const double tooSoon = 100.0 + static_cast<double>(options.rebuild_min_interval_seconds) * 0.5;
    REQUIRE_FALSE(world.should_rebuild(away, 0.0f, tooSoon));
    const double longEnough = 100.0 + static_cast<double>(options.rebuild_min_interval_seconds) * 2.0;
    REQUIRE(world.should_rebuild(away, 0.0f, longEnough));
}
