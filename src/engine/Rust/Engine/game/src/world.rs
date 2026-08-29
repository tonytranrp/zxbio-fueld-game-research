//! The headless Bevy App wrapping this crate's ECS and native Rapier
//! physics. No rendering plugins at all -- C++/Raylib renders every frame's
//! resulting object batch; Bevy only ever runs `MinimalPlugins` here.
use bevy::app::App;
use bevy::MinimalPlugins;

pub(crate) struct GameWorld {
    app: App,
}

pub(crate) fn new_game_world() -> GameWorld {
    let mut app = App::new();
    app.add_plugins(MinimalPlugins);
    GameWorld { app }
}

pub(crate) fn step_game(world: &mut GameWorld, _dt: f32) -> u64 {
    world.app.update();
    0
}

pub(crate) fn read_game_objects(_world: &GameWorld, _out: &mut [crate::ffi::BridgeGameObject]) -> u64 {
    0
}
