// raycast_3d split out of world3d.rs purely to keep that file under the
// project's ~400-line-per-file target; broad_phase/narrow_phase/bodies/
// colliders are already pub(crate) for character_controller.rs, so this
// needed no further visibility changes.
use crate::convert::{bridge_vec3, point3, vec3};
use crate::ffi;
use crate::handles::pack_collider_3d;
use crate::world3d::RapierWorld3D;
use rapier3d::prelude as r3;

pub fn raycast_3d(
    world: &RapierWorld3D,
    origin: ffi::BridgeVec3,
    direction: ffi::BridgeVec3,
    max_distance: f32,
    solid: bool,
) -> ffi::BridgeRayHit3D {
    let dir = vec3(direction);
    let norm = dir.length();
    if !norm.is_finite() || norm <= f32::EPSILON || !max_distance.is_finite() || max_distance <= 0.0
    {
        return ffi::BridgeRayHit3D {
            valid: false,
            collider: 0,
            point: origin,
            normal: ffi::BridgeVec3 {
                x: 0.0,
                y: 0.0,
                z: 0.0,
            },
            time_of_impact: 0.0,
        };
    }
    let unit_dir = dir / norm;
    let ray = r3::Ray::new(point3(&origin), unit_dir);
    let query = world.broad_phase.as_query_pipeline(
        world.narrow_phase.query_dispatcher(),
        &world.bodies,
        &world.colliders,
        r3::QueryFilter::default(),
    );
    if let Some((handle, hit)) = query.cast_ray_and_get_normal(&ray, max_distance, solid) {
        let point = ray.point_at(hit.time_of_impact);
        return ffi::BridgeRayHit3D {
            valid: true,
            collider: pack_collider_3d(handle),
            point: ffi::BridgeVec3 {
                x: point.x,
                y: point.y,
                z: point.z,
            },
            normal: bridge_vec3(hit.normal),
            time_of_impact: hit.time_of_impact,
        };
    }
    ffi::BridgeRayHit3D {
        valid: false,
        collider: 0,
        point: origin,
        normal: ffi::BridgeVec3 {
            x: 0.0,
            y: 0.0,
            z: 0.0,
        },
        time_of_impact: 0.0,
    }
}
