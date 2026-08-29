use crate::ffi::BridgeGameObject;
use crate::world::{new_game_world, read_game_objects, step_game};

#[test]
fn new_world_has_zero_objects_before_stepping() {
    let world = new_game_world();
    let mut out = vec![BridgeGameObject::default(); 4];
    assert_eq!(read_game_objects(&world, &mut out), 0);
}

#[test]
fn stepping_does_not_panic_and_still_reports_zero_objects() {
    let mut world = new_game_world();
    for _ in 0..3 {
        step_game(&mut world, 1.0 / 60.0);
    }
    let mut out = vec![BridgeGameObject::default(); 4];
    assert_eq!(read_game_objects(&world, &mut out), 0);
}
