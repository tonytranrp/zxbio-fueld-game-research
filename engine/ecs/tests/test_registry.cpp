#include <catch2/catch_test_macros.hpp>

#include "engine/ecs/components.hpp"
#include "engine/ecs/registry.hpp"

using namespace engine::ecs;

TEST_CASE("Registry creates entities carrying Transform and Name components", "[ecs]") {
    Registry registry;
    const Entity e = registry.create();
    registry.emplace<Transform>(e, glm::vec3{1.0f, 2.0f, 3.0f});
    registry.emplace<Name>(e, "test-entity");

    REQUIRE(registry.valid(e));
    REQUIRE(registry.get<Transform>(e).position == glm::vec3{1.0f, 2.0f, 3.0f});
    REQUIRE(registry.get<Name>(e).value == "test-entity");
}

TEST_CASE("Registry destroys entities and invalidates them", "[ecs]") {
    Registry registry;
    const Entity e = registry.create();
    registry.emplace<Name>(e, "temporary");

    registry.destroy(e);

    REQUIRE_FALSE(registry.valid(e));
}
