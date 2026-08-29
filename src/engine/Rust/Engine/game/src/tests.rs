use crate::ffi::BridgeGameObject;
use crate::world::{new_game_world, read_game_objects, step_game};

// Matches ExplorationLevel.cpp's real box count: ground(1) + boundary(4) +
// barn shell(3) + front door segments(2) + roof(1) + crates/drums/hay(5) +
// landmark(1) = 17.
const EXPECTED_LEVEL_OBJECT_COUNT: usize = 17;

#[test]
fn new_world_spawns_the_full_level() {
    let mut world = new_game_world();
    let mut out = vec![BridgeGameObject::default(); EXPECTED_LEVEL_OBJECT_COUNT + 4];
    assert_eq!(
        read_game_objects(&mut world, &mut out) as usize,
        EXPECTED_LEVEL_OBJECT_COUNT
    );
}

#[test]
fn stepping_does_not_panic_and_keeps_the_same_object_count() {
    let mut world = new_game_world();
    for _ in 0..3 {
        step_game(&mut world, 1.0 / 60.0);
    }
    let mut out = vec![BridgeGameObject::default(); EXPECTED_LEVEL_OBJECT_COUNT + 4];
    assert_eq!(
        read_game_objects(&mut world, &mut out) as usize,
        EXPECTED_LEVEL_OBJECT_COUNT
    );
}

#[test]
fn read_game_objects_is_truncation_safe() {
    let mut world = new_game_world();
    let mut out = vec![BridgeGameObject::default(); 3];
    // Real count (17) exceeds the output buffer (3) -- must report only
    // what fits, never write past `out`'s length, matching physics/'s own
    // body_poses_3d truncation-safe pattern.
    assert_eq!(read_game_objects(&mut world, &mut out), 3);
}

#[test]
fn ground_object_matches_exploration_level_cpp() {
    let mut world = new_game_world();
    let mut out = vec![BridgeGameObject::default(); EXPECTED_LEVEL_OBJECT_COUNT];
    read_game_objects(&mut world, &mut out);
    // First spawned object is always the ground box (see level.rs's
    // level_boxes(), ported from ExplorationLevel.cpp's own addBox call
    // order).
    let ground = &out[0];
    assert_eq!(ground.position.y, -0.1);
    assert_eq!(ground.half_extents.x, 14.0);
    assert_eq!(ground.color_rgba, [110, 100, 80, 255]);
}
