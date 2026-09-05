#include <catch2/catch_test_macros.hpp>

#include "world/chunk/block_type.hpp"

using namespace world::chunk;

TEST_CASE("every MaterialID has a real kBlockTable row", "[block_type]") {
    REQUIRE(kBlockTable.size() == kMaterialCount);
}

TEST_CASE("is_occupied reproduces the old mesh-extraction occupancy rule", "[block_type]") {
    // The exact semantics the removed mesh_extractor.cpp local `is_solid(m) = m != Air` helper
    // had: everything except Air needs a mesh boundary against open air, water included.
    CHECK_FALSE(is_occupied(MaterialID::Air));
    CHECK(is_occupied(MaterialID::Stone));
    CHECK(is_occupied(MaterialID::Dirt));
    CHECK(is_occupied(MaterialID::Water));
    CHECK(is_occupied(MaterialID::Sand));
    CHECK(is_occupied(MaterialID::Grass));
}

TEST_CASE("is_solid excludes water (gameplay collision, not mesh occupancy)", "[block_type]") {
    CHECK_FALSE(properties_of(MaterialID::Air).is_solid);
    CHECK_FALSE(properties_of(MaterialID::Water).is_solid);
    CHECK(properties_of(MaterialID::Stone).is_solid);
    CHECK(properties_of(MaterialID::Grass).is_solid);
}

TEST_CASE("only Water is a liquid", "[block_type]") {
    for (std::size_t i = 0; i < kMaterialCount; ++i) {
        const auto id = static_cast<MaterialID>(i);
        const bool expected = (id == MaterialID::Water);
        CHECK(properties_of(id).is_liquid == expected);
    }
}
