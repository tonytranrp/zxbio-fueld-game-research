//! First-person viewmodel hands -- loads `assets/models/viewmodel_hands/
//! viewmodel_hands.glb`, the same file `ModelSystem.cpp` loads via
//! raylib's `LoadModel`/`LoadModelAnimations`, and renders it through a
//! second, fixed (non-tracking) camera + `RenderLayers` pair. This mirrors
//! `engine/graphics/ViewmodelPass.hpp`'s depth-isolated offscreen-composite
//! approach with Bevy's own native multi-camera compositing instead of a
//! manually managed `RenderTexture`: a camera with a higher `order` and
//! `ClearColorConfig::None` draws its (`RenderLayers`-isolated) scene on
//! top of the world camera's output without re-clearing, and each camera
//! gets its own depth buffer for free -- the same "viewmodel can never clip
//! into or be clipped by world geometry" property the C++ version's
//! separate depth buffer exists for.
#![forbid(unsafe_code)]

use bevy::animation::graph::{AnimationGraph, AnimationGraphHandle, AnimationNodeIndex};
use bevy::animation::AnimationPlayer;
use bevy::app::{App, Update};
use bevy::asset::{AssetServer, Assets, Handle};
use bevy::camera::visibility::RenderLayers;
use bevy::camera::{Camera, Camera3d, ClearColorConfig, PerspectiveProjection, Projection};
use bevy::ecs::system::Local;
use bevy::gltf::Gltf;
use bevy::math::{Quat, Vec3};
use bevy::prelude::{Children, Commands, Component, Entity, Query, Res, ResMut, Resource, Transform};
use bevy::world_serialization::WorldAssetRoot;

// Entities rendered only by the viewmodel camera, never the world camera
// (and vice versa) -- RenderLayers intersect-to-render, so a camera with
// only this layer never sees layer-0 (the default every world entity from
// level.rs/player.rs implicitly belongs to) and a layer-0 camera never sees
// this layer.
const VIEWMODEL_LAYER: usize = 1;

#[derive(Resource)]
struct ViewmodelAssets {
    gltf: Handle<Gltf>,
}

// Carries the data `wire_up_viewmodel_animation_and_layers` needs once the
// glTF scene's child hierarchy (and the `AnimationPlayer` entity somewhere
// inside it) has actually been instantiated -- `WorldAssetRoot` spawning
// happens over multiple frames as its own asset dependencies (meshes,
// skins, ...) finish loading, so this can't all happen in the system that
// spawns the root.
#[derive(Component)]
struct ViewmodelPendingAnimation {
    idle_node: AnimationNodeIndex,
    graph_handle: Handle<AnimationGraph>,
}

pub(crate) fn setup(app: &mut App) {
    let gltf = app
        .world()
        .resource::<AssetServer>()
        .load("models/viewmodel_hands/viewmodel_hands.glb");
    app.insert_resource(ViewmodelAssets { gltf });

    // Fixed, non-tracking camera -- this first pass deliberately does not
    // track player look (yaw/pitch), matching ExplorationScreen's own
    // ViewmodelLayerTag::render() (see its doc comment). Its rotation looks
    // toward world +Z: raylib's target-based viewmodel camera used
    // {position: origin, target: (0,0,1)}, i.e. world +Z, which is NOT
    // Bevy's own default forward (-Z) -- looking_at reproduces the same
    // world-space view direction regardless of that convention difference.
    // Near/far mirror ViewmodelPass::kNearPlane/kFarPlane (0.01/10.0),
    // compressed relative to the world camera's own range for reasonable
    // depth precision at this small, close-up scale.
    app.world_mut().spawn((
        Camera3d::default(),
        Camera {
            order: 1,
            clear_color: ClearColorConfig::None,
            ..Default::default()
        },
        Projection::Perspective(PerspectiveProjection {
            fov: 60f32.to_radians(),
            near: 0.01,
            far: 10.0,
            ..Default::default()
        }),
        Transform::IDENTITY.looking_at(Vec3::Z, Vec3::Y),
        RenderLayers::layer(VIEWMODEL_LAYER),
        // Tonemapping::default() is TonyMcMapface, which needs a LUT
        // texture the tonemapping_luts feature isn't enabled to provide --
        // confirmed by a real ERROR-level log line without this ("TonyMcMapFace
        // tonemapping requires the `tonemapping_luts` feature") that lines
        // up with this camera specifically (the world camera in session.rs
        // already overrides this the same way). Same KhronosPbrNeutral
        // choice as that camera, for the same reason: preserves color
        // fidelity without the extra Cargo weight of shipping real LUTs.
        bevy::core_pipeline::tonemapping::Tonemapping::KhronosPbrNeutral,
    ));

    app.add_systems(
        Update,
        (spawn_viewmodel_once_loaded, wire_up_viewmodel_animation_and_layers),
    );
}

fn spawn_viewmodel_once_loaded(
    mut commands: Commands,
    assets: Res<ViewmodelAssets>,
    gltfs: Res<Assets<Gltf>>,
    mut spawned: Local<bool>,
    mut graphs: ResMut<Assets<AnimationGraph>>,
) {
    if *spawned {
        return;
    }
    let Some(gltf) = gltfs.get(&assets.gltf) else {
        return;
    };
    let Some(scene) = gltf.default_scene.clone().or_else(|| gltf.scenes.first().cloned()) else {
        return;
    };
    // Index-based, not name-based: ModelSystem.cpp's
    // VIEWMODEL_HANDS_ANIMATION_STATES table addresses these same two clips
    // by clipIndex (0=idle, 1=walk), not by name.
    let Some(idle_clip) = gltf.animations.first().cloned() else {
        return;
    };

    // Only idle plays this phase -- switching to walk based on player
    // movement state (ExplorationScreen.cpp's own horizontalSpeed/grounded
    // check) is a follow-up once WASD input can actually be exercised
    // end-to-end; wiring both clips into the graph now, unplayed, means
    // that follow-up doesn't need to touch this setup code again.
    let mut graph = AnimationGraph::new();
    let idle_node = graph.add_clip(idle_clip, 1.0, graph.root);
    if let Some(walk_clip) = gltf.animations.get(1).cloned() {
        graph.add_clip(walk_clip, 1.0, graph.root);
    }
    let graph_handle = graphs.add(graph);

    // Mirrors ModelRenderState{position: (0,-0.28,0.45), rotationAxis:
    // (1,0,0), rotationDegrees: 90} from ExplorationScreen.cpp's own
    // viewmodel render call. The rig's rest pose reaches along local +Y
    // (verified numerically from the glTF's own node transforms -- see
    // assets/models/viewmodel_hands/README.md), so this rotation points
    // the reach direction along +Z, this fixed camera's forward, instead
    // of straight up -- same +90-degrees-about-X the C++ side applies, no
    // sign flip needed (both raylib and Bevy are right-handed/Y-up here).
    commands.spawn((
        WorldAssetRoot(scene),
        Transform::from_xyz(0.0, -0.28, 0.45).with_rotation(Quat::from_axis_angle(Vec3::X, 90f32.to_radians())),
        RenderLayers::layer(VIEWMODEL_LAYER),
        ViewmodelPendingAnimation { idle_node, graph_handle },
    ));

    *spawned = true;
}

fn wire_up_viewmodel_animation_and_layers(
    mut commands: Commands,
    pending_query: Query<(Entity, &ViewmodelPendingAnimation)>,
    children_query: Query<&Children>,
    mut player_query: Query<&mut AnimationPlayer>,
) {
    for (root, pending) in &pending_query {
        // Every descendant needs the same layer tag as the root -- Bevy
        // does not propagate RenderLayers down an entity hierarchy on its
        // own (confirmed by reading bevy_camera-0.19.1's own visibility
        // culling system: it reads Option<&RenderLayers> per-entity, no
        // inherited/ancestor lookup), so without this the mesh primitives
        // the glTF loader actually spawns would default to layer 0 and get
        // drawn (at the wrong, camera-relative-only position) by the WORLD
        // camera instead of this module's viewmodel camera.
        for descendant in children_query.iter_descendants(root) {
            commands.entity(descendant).insert(RenderLayers::layer(VIEWMODEL_LAYER));
            // The AnimationPlayer Bevy's glTF loader creates lands on some
            // descendant node, not necessarily the root itself -- this scan
            // finds it wherever it ended up.
            if let Ok(mut player) = player_query.get_mut(descendant) {
                commands
                    .entity(descendant)
                    .insert(AnimationGraphHandle(pending.graph_handle.clone()));
                player.play(pending.idle_node).repeat();
                commands.entity(root).remove::<ViewmodelPendingAnimation>();
            }
        }
    }
}
