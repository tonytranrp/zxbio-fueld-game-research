#include <catch2/catch_test_macros.hpp>

#include "render/diligent/frustum.hpp"
#include "render/interface/camera.hpp"

using namespace render::diligent;
using render::interface::Camera;

namespace {

// A camera at the origin looking down -Z (identity orientation), square aspect, 90° vertical FOV
// -- at depth |z| the frustum's half-extent is exactly |z| in both x and y, which makes every
// expected in/out answer below hand-checkable.
Frustum reference_frustum(Camera& outCamera) {
    outCamera = Camera{};
    outCamera.fov_y_radians = glm::radians(90.0f);
    outCamera.near_plane = 0.1f;
    outCamera.far_plane = 100.0f;
    const glm::mat4 viewProj =
        render::interface::projection_matrix(outCamera, 1.0f) * render::interface::view_matrix(outCamera);
    return extract_frustum(viewProj);
}

Aabb unit_box_at(float x, float y, float z) {
    return Aabb{{x - 1.0f, y - 1.0f, z - 1.0f}, {x + 1.0f, y + 1.0f, z + 1.0f}};
}

} // namespace

TEST_CASE("Boxes in front of the camera are kept, boxes behind are culled", "[frustum]") {
    Camera camera;
    const Frustum frustum = reference_frustum(camera);

    CHECK(intersects(frustum, unit_box_at(0.0f, 0.0f, -10.0f)));
    CHECK_FALSE(intersects(frustum, unit_box_at(0.0f, 0.0f, +10.0f)));
}

TEST_CASE("Boxes beyond the far plane and inside the near plane are culled", "[frustum]") {
    Camera camera;
    const Frustum frustum = reference_frustum(camera);

    CHECK_FALSE(intersects(frustum, unit_box_at(0.0f, 0.0f, -200.0f))); // beyond far=100
    // Fully between the camera and the near plane: a box behind z=-0.1 by construction.
    CHECK_FALSE(intersects(frustum, Aabb{{-0.01f, -0.01f, -0.05f}, {0.01f, 0.01f, -0.02f}}));
}

TEST_CASE("Boxes outside the side planes are culled; the 90-degree FOV makes the boundary exact",
          "[frustum]") {
    Camera camera;
    const Frustum frustum = reference_frustum(camera);

    // At z=-10 the frustum spans x,y in [-10, 10]. A unit box at x=50 is far outside; at x=5 it
    // is comfortably inside; straddling the boundary at x=10 must be conservatively kept.
    CHECK_FALSE(intersects(frustum, unit_box_at(+50.0f, 0.0f, -10.0f)));
    CHECK_FALSE(intersects(frustum, unit_box_at(-50.0f, 0.0f, -10.0f)));
    CHECK_FALSE(intersects(frustum, unit_box_at(0.0f, +50.0f, -10.0f)));
    CHECK(intersects(frustum, unit_box_at(5.0f, 0.0f, -10.0f)));
    CHECK(intersects(frustum, unit_box_at(10.0f, 0.0f, -10.0f)));
}

TEST_CASE("A box enclosing the whole frustum is kept", "[frustum]") {
    Camera camera;
    const Frustum frustum = reference_frustum(camera);

    CHECK(intersects(frustum, Aabb{{-1000.0f, -1000.0f, -1000.0f}, {1000.0f, 1000.0f, 1000.0f}}));
}

TEST_CASE("Culling respects the quaternion camera orientation, not just position", "[frustum]") {
    // Rotate the camera 180 degrees about +Y: it now looks down +Z, so the in-front/behind
    // answers from the identity-orientation cases must exactly swap.
    Camera camera;
    camera.fov_y_radians = glm::radians(90.0f);
    camera.orientation = glm::angleAxis(glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 viewProj =
        render::interface::projection_matrix(camera, 1.0f) * render::interface::view_matrix(camera);
    const Frustum frustum = extract_frustum(viewProj);

    CHECK(intersects(frustum, unit_box_at(0.0f, 0.0f, +10.0f)));
    CHECK_FALSE(intersects(frustum, unit_box_at(0.0f, 0.0f, -10.0f)));
}
