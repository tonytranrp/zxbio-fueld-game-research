//! The headless Bevy App wrapping this crate's ECS and native Rapier
//! physics. No rendering plugins at all -- C++/Raylib renders every frame's
//! resulting object batch; Bevy only ever runs `MinimalPlugins` here.
use crate::components::{RapierBody, RenderShape};
use crate::level::spawn_level;
use bevy::app::{App, Update};
use bevy::prelude::{Query, Res, Resource, Transform};
use bevy::MinimalPlugins;
use rapier3d::prelude as r3;

// Every native-Rapier physics state this crate owns, as one Bevy resource --
// stepped by `step_physics` each `Update`, then `sync_transforms_from_physics`
// copies the resulting body poses onto each entity's own `Transform`. Two
// separate systems (not one) so a future system could read stepped poses
// (e.g. for gameplay logic) after physics but before the sync, same
// ordering Bevy's own `bevy_rapier` uses.
#[derive(Resource)]
pub(crate) struct RapierPhysics {
    pipeline: r3::PhysicsPipeline,
    gravity: r3::Vector,
    integration: r3::IntegrationParameters,
    islands: r3::IslandManager,
    broad_phase: r3::BroadPhaseBvh,
    narrow_phase: r3::NarrowPhase,
    pub(crate) bodies: r3::RigidBodySet,
    pub(crate) colliders: r3::ColliderSet,
    impulse_joints: r3::ImpulseJointSet,
    multibody_joints: r3::MultibodyJointSet,
    ccd_solver: r3::CCDSolver,
}

impl RapierPhysics {
    fn new() -> Self {
        Self {
            pipeline: r3::PhysicsPipeline::new(),
            gravity: r3::Vector::new(0.0, -9.81, 0.0),
            integration: r3::IntegrationParameters::default(),
            islands: r3::IslandManager::new(),
            broad_phase: r3::BroadPhaseBvh::new(),
            narrow_phase: r3::NarrowPhase::new(),
            bodies: r3::RigidBodySet::new(),
            colliders: r3::ColliderSet::new(),
            impulse_joints: r3::ImpulseJointSet::new(),
            multibody_joints: r3::MultibodyJointSet::new(),
            ccd_solver: r3::CCDSolver::new(),
        }
    }

    fn step(&mut self, dt: f32) {
        if !dt.is_finite() || dt <= 0.0 {
            return;
        }
        self.integration.dt = dt;
        self.pipeline.step(
            self.gravity,
            &self.integration,
            &mut self.islands,
            &mut self.broad_phase,
            &mut self.narrow_phase,
            &mut self.bodies,
            &mut self.colliders,
            &mut self.impulse_joints,
            &mut self.multibody_joints,
            &mut self.ccd_solver,
            &(),
            &(),
        );
    }
}

fn sync_transforms_from_physics(
    physics: Res<RapierPhysics>,
    mut query: Query<(&RapierBody, &mut Transform)>,
) {
    for (body, mut transform) in &mut query {
        if let Some(rigid_body) = physics.bodies.get(body.0) {
            let translation = rigid_body.translation();
            let rotation = rigid_body.rotation();
            transform.translation = bevy::prelude::Vec3::new(translation.x, translation.y, translation.z);
            transform.rotation =
                bevy::prelude::Quat::from_xyzw(rotation.x, rotation.y, rotation.z, rotation.w);
        }
    }
}

pub(crate) struct GameWorld {
    app: App,
}

pub(crate) fn new_game_world() -> GameWorld {
    let mut app = App::new();
    app.add_plugins(MinimalPlugins);
    app.insert_resource(RapierPhysics::new());
    app.add_systems(Update, sync_transforms_from_physics);
    spawn_level(app.world_mut());
    GameWorld { app }
}

pub(crate) fn step_game(world: &mut GameWorld, dt: f32) -> u64 {
    world
        .app
        .world_mut()
        .resource_mut::<RapierPhysics>()
        .step(dt);
    world.app.update();
    let mut query = world.app.world_mut().query::<&RenderShape>();
    query.iter(world.app.world()).count() as u64
}

pub(crate) fn read_game_objects(world: &mut GameWorld, out: &mut [crate::ffi::BridgeGameObject]) -> u64 {
    let mut query = world.app.world_mut().query::<(&Transform, &RenderShape)>();
    let mut written: usize = 0;
    for (transform, shape) in query.iter(world.app.world()) {
        if written >= out.len() {
            break;
        }
        out[written] = crate::ffi::BridgeGameObject {
            entity_kind: shape.kind,
            position: crate::ffi::BridgeVec3 {
                x: transform.translation.x,
                y: transform.translation.y,
                z: transform.translation.z,
            },
            rotation: crate::ffi::BridgeQuat {
                x: transform.rotation.x,
                y: transform.rotation.y,
                z: transform.rotation.z,
                w: transform.rotation.w,
            },
            half_extents: crate::ffi::BridgeVec3 {
                x: shape.half_extents.x,
                y: shape.half_extents.y,
                z: shape.half_extents.z,
            },
            color_rgba: shape.color_rgba,
        };
        written += 1;
    }
    written as u64
}
