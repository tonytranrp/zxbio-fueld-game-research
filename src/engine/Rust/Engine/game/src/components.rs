//! Components every game-object entity carries. Deliberately plain Bevy ECS
//! composition (a Transform + whatever else an entity needs) rather than a
//! custom typed-registry system -- different object kinds are just entities
//! with different component sets, which is what Bevy's own query system
//! already gives for free.
use bevy::prelude::Component;
use rapier3d::prelude as r3;

pub(crate) const KIND_STATIC: u8 = 0;
// Not read yet -- the demo entity that will use this lands in the next
// phase (a moving entity alongside the static level ported here).
#[allow(dead_code)]
pub(crate) const KIND_DEMO: u8 = 1;

// The native Rapier body backing this entity's collision. Fixed for static
// level geometry, Dynamic/Kinematic for anything that moves.
#[derive(Component, Clone, Copy)]
pub(crate) struct RapierBody(pub(crate) r3::RigidBodyHandle);

// Everything C++/Raylib needs to draw this entity that Rapier's own
// collider doesn't carry (Rapier has no concept of color, and half-extents
// live on the collider shape, not something read back generically without
// downcasting it every frame).
#[derive(Component, Clone, Copy)]
pub(crate) struct RenderShape {
    pub(crate) half_extents: r3::Vector,
    pub(crate) color_rgba: [u8; 4],
    pub(crate) kind: u8,
}
