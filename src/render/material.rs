//! The [`VoxelMaterial`] custom Bevy `Material`: a fragment shader that ray-marches a single
//! [`VoxelChunk`]'s brick grid, rendered on a bounding cuboid mesh sized to exactly match the
//! chunk's voxel-space extent.

use bevy::asset::{uuid_handle, Assets, RenderAssetUsages};
use bevy::image::Image;
use bevy::material::AlphaMode;
use bevy::math::{UVec3, Vec3, Vec4};
use bevy::pbr::{Material, MaterialPipeline, MaterialPipelineKey};
use bevy::prelude::{Asset, Handle};
use bevy::reflect::TypePath;
use bevy::render::mesh::MeshVertexBufferLayoutRef;
use bevy::render::render_resource::{
    AsBindGroup, Extent3d, RenderPipelineDescriptor, ShaderType, SpecializedMeshPipelineError,
    TextureDimension, TextureFormat,
};
use bevy::render::storage::ShaderBuffer;
use bevy::shader::{Shader, ShaderRef};

use crate::storage::{VoxelChunk, VoxelPalette};

/// A stable handle identifying the embedded ray-march shader — see
/// [`crate::plugin::VoxelEnginePlugin`], which loads the real shader source into this handle via
/// `load_internal_asset!` so consumers never need their own copy of the `.wgsl` file on disk.
pub(crate) const VOXEL_SHADER_HANDLE: Handle<Shader> =
    uuid_handle!("c9b6e9f2-6f1b-4c2e-8f5a-2e6a1d9c4b7d");

/// Scalar parameters uploaded to the shader as a single uniform buffer. Field order and types
/// must exactly match `VoxelParams` in `assets/shaders/voxel_raymarch.wgsl`.
///
/// Holds no array: WGSL's `uniform` address space can't legally contain a runtime-sized array,
/// so the palette (see [`VoxelMaterial::palette`]) lives in its own dedicated storage buffer
/// instead. (An earlier version of this file wrongly attributed a real runtime binding error to
/// this — the actual cause was an unrelated hardcoded `@group(2)` in the shader, which should
/// have been `@group(#{MATERIAL_BIND_GROUP})`; see `voxel_raymarch.wgsl`'s own comment. Keeping
/// the palette in its own storage buffer is still the right call — it's a 256-entry lookup table,
/// exactly the shape `#[storage(N)]` is for — just not for the reason first written here.)
#[derive(Clone, Copy, ShaderType)]
struct VoxelParams {
    chunk_dims: UVec3,
    brick_dims: UVec3,
    chunk_origin: Vec3,
    voxel_size: f32,
    sun_direction: Vec3,
    sun_color: Vec3,
    sun_intensity: f32,
}

/// Renders one [`VoxelChunk`] via GPU ray marching. Constructed by [`crate::spawn_voxel_chunk`] —
/// not meant to be built directly, since its bind-group data (two 3D textures) must stay in sync
/// with the `VoxelChunk` it was built from.
#[derive(Asset, TypePath, AsBindGroup, Clone)]
pub struct VoxelMaterial {
    #[uniform(0)]
    params: VoxelParams,
    #[texture(1, dimension = "3d", sample_type = "u_int")]
    voxel_data: Handle<Image>,
    #[texture(2, dimension = "3d", sample_type = "u_int")]
    brick_occupancy: Handle<Image>,
    /// Indexed by [`VoxelId`](crate::storage::VoxelId) — always exactly 256 entries, uploaded as
    /// a genuine WGSL runtime-sized array (a [`ShaderBuffer`] built from a `Vec`, not a fixed
    /// `[Vec4; 256]`); see this struct's doc comment on `VoxelParams` for why that specific shape
    /// matters here.
    #[storage(3, read_only)]
    palette: Handle<ShaderBuffer>,
}

/// A directional light reasonable enough to see by if a consumer never calls
/// [`VoxelMaterial::set_sun`] — high noon, slightly off-vertical so faces aren't uniformly lit.
const DEFAULT_SUN_DIRECTION: Vec3 = Vec3::new(0.3, -0.8, 0.2);
const DEFAULT_SUN_COLOR: Vec3 = Vec3::new(1.0, 0.98, 0.92);
const DEFAULT_SUN_INTENSITY: f32 = 1.0;

impl VoxelMaterial {
    #[allow(clippy::too_many_arguments)]
    pub(crate) fn new(
        chunk: &VoxelChunk,
        palette: &VoxelPalette,
        chunk_origin: Vec3,
        voxel_size: f32,
        voxel_data: Handle<Image>,
        brick_occupancy: Handle<Image>,
        buffers: &mut Assets<ShaderBuffer>,
    ) -> Self {
        let palette_colors: Vec<Vec4> = (0u16..256)
            .map(|id| {
                let color = palette.get(crate::storage::VoxelId::new(id as u8)).color;
                Vec4::new(color.red, color.green, color.blue, color.alpha)
            })
            .collect();
        let mut palette_buffer = ShaderBuffer::default();
        palette_buffer.set_data(palette_colors);

        Self {
            params: VoxelParams {
                chunk_dims: chunk.dims(),
                brick_dims: chunk.brick_dims(),
                chunk_origin,
                voxel_size,
                sun_direction: DEFAULT_SUN_DIRECTION.normalize_or_zero(),
                sun_color: DEFAULT_SUN_COLOR,
                sun_intensity: DEFAULT_SUN_INTENSITY,
            },
            voxel_data,
            brick_occupancy,
            palette: buffers.add(palette_buffer),
        }
    }

    /// Sets the single directional light the ray marcher shades and shadows against.
    /// `direction` points *from* a lit surface *toward* the light (i.e. the direction light
    /// travels backward along) and need not be pre-normalized.
    pub fn set_sun(&mut self, direction: Vec3, color: Vec3, intensity: f32) {
        self.params.sun_direction = direction.normalize_or_zero();
        self.params.sun_color = color;
        self.params.sun_intensity = intensity;
    }
}

impl Material for VoxelMaterial {
    fn fragment_shader() -> ShaderRef {
        VOXEL_SHADER_HANDLE.into()
    }

    fn alpha_mode(&self) -> AlphaMode {
        AlphaMode::Opaque
    }

    fn specialize(
        _pipeline: &MaterialPipeline,
        descriptor: &mut RenderPipelineDescriptor,
        _layout: &MeshVertexBufferLayoutRef,
        _key: MaterialPipelineKey<Self>,
    ) -> Result<(), SpecializedMeshPipelineError> {
        // The camera can end up *inside* the bounding cuboid (once the flycam flies into the
        // chunk's volume) — with default backface culling, every face's front side then points
        // away from the camera and the whole mesh stops rasterizing. Rendering both sides keeps
        // the fragment shader fed with a ray-origin point no matter which side the camera is on;
        // depth testing still resolves the correct (nearest) face when viewed from outside.
        descriptor.primitive.cull_mode = None;
        Ok(())
    }
}

/// Packs `chunk`'s voxel material IDs into a 3D `R8Uint` image — the exact layout
/// `voxel_raymarch.wgsl`'s `voxel_data` binding expects, read back via `textureLoad`.
pub(crate) fn build_voxel_image(chunk: &VoxelChunk) -> Image {
    let dims = chunk.dims();
    let data: Vec<u8> = chunk.voxels().iter().map(|v| v.0).collect();
    Image::new(
        Extent3d {
            width: dims.x,
            height: dims.y,
            depth_or_array_layers: dims.z,
        },
        TextureDimension::D3,
        data,
        TextureFormat::R8Uint,
        RenderAssetUsages::RENDER_WORLD,
    )
}

/// Packs `chunk`'s per-brick occupancy into a 3D `R8Uint` image (`1` = occupied, `0` = empty) —
/// the exact layout `voxel_raymarch.wgsl`'s `brick_occupancy` binding expects.
pub(crate) fn build_occupancy_image(chunk: &VoxelChunk) -> Image {
    let dims = chunk.brick_dims();
    let data: Vec<u8> = chunk
        .brick_occupancy()
        .iter()
        .map(|&count| if count > 0 { 1u8 } else { 0u8 })
        .collect();
    Image::new(
        Extent3d {
            width: dims.x,
            height: dims.y,
            depth_or_array_layers: dims.z,
        },
        TextureDimension::D3,
        data,
        TextureFormat::R8Uint,
        RenderAssetUsages::RENDER_WORLD,
    )
}
