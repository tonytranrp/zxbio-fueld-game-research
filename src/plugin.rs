//! [`VoxelEnginePlugin`] — the crate's single Bevy `Plugin` entry point.

use bevy::app::{App, Plugin};
use bevy::asset::load_internal_asset;
use bevy::pbr::MaterialPlugin;
use bevy::shader::Shader;

use crate::render::material::VOXEL_SHADER_HANDLE;
use crate::render::VoxelMaterial;

/// Registers voxel rendering support: the [`VoxelMaterial`] pipeline and its embedded shader.
///
/// Add this alongside your own `DefaultPlugins` — it never adds `DefaultPlugins` itself or makes
/// windowing decisions; that stays entirely the consumer's call.
pub struct VoxelEnginePlugin;

impl Plugin for VoxelEnginePlugin {
    fn build(&self, app: &mut App) {
        load_internal_asset!(
            app,
            VOXEL_SHADER_HANDLE,
            "../assets/shaders/voxel_raymarch.wgsl",
            Shader::from_wgsl
        );
        app.add_plugins(MaterialPlugin::<VoxelMaterial>::default());
    }
}
