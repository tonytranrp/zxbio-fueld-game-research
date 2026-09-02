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

use crate::storage::coords::{flatten, BRICK_SIZE};
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
    /// How many levels [`VoxelChunk::mip_level_count`] the shader's own mip hierarchy has —
    /// `0` for a chunk too small to have any, in which case the marcher starts at the brick
    /// level directly, matching `cast_ray`'s own CPU-side fallback.
    mip_level_count: u32,
    /// World-space distance (meters) beyond which the marcher stops descending past the brick
    /// level and uses [`VoxelMaterial::brick_lod_colors`] as the hit material instead of
    /// resolving the true per-voxel surface — see [`VoxelMaterial::set_lod_distance`]. Defaults
    /// to [`DEFAULT_LOD_DISTANCE`], effectively "never triggers," so a consumer that doesn't know
    /// about this feature gets the exact same full-detail rendering as before it existed.
    lod_distance: f32,
}

/// Effectively "LOD never triggers" — matches the `1e6` max_dist already used for the primary ray
/// in `fragment()`, so no chunk at any realistic scale crosses it by accident. A consumer opts
/// into the optimization explicitly via [`VoxelMaterial::set_lod_distance`]; this default keeps
/// existing rendering behavior identical for anyone who doesn't.
const DEFAULT_LOD_DISTANCE: f32 = 1e6;

/// Must match `MAX_STACK - 2` in `voxel_raymarch.wgsl` (the shader's fixed-size stack holds every
/// mip level plus the brick and voxel levels) — see that constant's own comment for why this is
/// generous headroom, not a real limitation, at any chunk size this engine has ever built.
const MAX_MIP_STACK_LEVELS: usize = 10;

/// Renders one [`VoxelChunk`] via GPU ray marching. Constructed by [`crate::spawn_voxel_chunk`] —
/// not meant to be built directly, since its bind-group data (two 3D textures) must stay in sync
/// with the `VoxelChunk` it was built from.
///
/// Its fragment shader never writes an explicit depth — depth-based rendering (opaque sorting,
/// `DepthPrepass`, `OcclusionCulling`) relies entirely on the bounding cuboid mesh's own
/// rasterized geometry depth. Correct only for non-overlapping chunk placement; see
/// [`crate::spawn_voxel_chunk`]'s own doc comment for the full reasoning and why that's an exact
/// guarantee for non-overlapping chunks, not just a usually-fine approximation.
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
    /// The flattened mip-occupancy hierarchy — see [`build_mip_occupancy_buffer`] for the exact
    /// layout and `voxel_raymarch.wgsl`'s own comment for how the shader indexes into it.
    #[storage(4, read_only)]
    mip_occupancy: Handle<ShaderBuffer>,
    /// Each brick's average color, indexed the same way `brick_occupancy` is — the material a ray
    /// gets when [`VoxelParams::lod_distance`] tells the shader to stop at the brick level instead
    /// of resolving the true voxel. See [`compute_brick_average_colors`] for how it's computed.
    #[storage(5, read_only)]
    brick_lod_colors: Handle<ShaderBuffer>,
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
        assert!(
            chunk.mip_level_count() <= MAX_MIP_STACK_LEVELS,
            "chunk has {} mip levels, more than the GPU marcher's fixed-size stack supports \
             ({}) — see MAX_STACK in voxel_raymarch.wgsl and MAX_MIP_STACK_LEVELS here, which \
             must stay in sync",
            chunk.mip_level_count(),
            MAX_MIP_STACK_LEVELS,
        );

        let palette_colors: Vec<Vec4> = (0u16..256)
            .map(|id| {
                let color = palette.get(crate::storage::VoxelId::new(id as u8)).color;
                Vec4::new(color.red, color.green, color.blue, color.alpha)
            })
            .collect();
        let mut palette_buffer = ShaderBuffer::default();
        palette_buffer.set_data(palette_colors);

        let mut mip_buffer = ShaderBuffer::default();
        mip_buffer.set_data(build_mip_occupancy_buffer(chunk));

        let mut lod_color_buffer = ShaderBuffer::default();
        lod_color_buffer.set_data(compute_brick_average_colors(chunk, palette));

        Self {
            params: VoxelParams {
                chunk_dims: chunk.dims(),
                brick_dims: chunk.brick_dims(),
                chunk_origin,
                voxel_size,
                sun_direction: DEFAULT_SUN_DIRECTION.normalize_or_zero(),
                sun_color: DEFAULT_SUN_COLOR,
                sun_intensity: DEFAULT_SUN_INTENSITY,
                mip_level_count: chunk.mip_level_count() as u32,
                lod_distance: DEFAULT_LOD_DISTANCE,
            },
            voxel_data,
            brick_occupancy,
            palette: buffers.add(palette_buffer),
            mip_occupancy: buffers.add(mip_buffer),
            brick_lod_colors: buffers.add(lod_color_buffer),
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

    /// Sets the world-space distance (meters) beyond which the marcher stops descending past the
    /// brick level and shades with that brick's average color instead of resolving the true
    /// per-voxel surface — real, measured motivation: on this engine's own 16-chunk test grid,
    /// forcing brick-level-only marching for every ray raised frame rate from ~22fps to ~62fps at
    /// a ground-level camera angle (same scene, same hardware), so the fine per-voxel descent is
    /// genuinely the dominant per-pixel cost, not just a theoretical target.
    ///
    /// Defaults to effectively "never triggers" ([`DEFAULT_LOD_DISTANCE`]) — full detail
    /// everywhere unless a consumer opts in. Picking a good distance is scene-dependent (how
    /// large is a voxel, how far can the camera actually get from content) and has no single
    /// correct default this library can pick on a consumer's behalf; expect a visible transition
    /// where a chunk's marching quality changes as a ray's distance crosses this threshold — this
    /// is a real, known, unaddressed limitation of a hard cutoff (see
    /// `compute_brick_average_colors`'s own doc comment), not something this method smooths over.
    pub fn set_lod_distance(&mut self, distance: f32) {
        self.params.lod_distance = distance;
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

/// Packs `chunk`'s full mip-occupancy hierarchy into one flat buffer for the `mip_occupancy`
/// storage binding: level 0 (finest, immediately above bricks) first, through the coarsest level
/// last, each level's own cells in the same x-major/y/z order as
/// [`crate::storage::coords::flatten`] — mirrored exactly in `voxel_raymarch.wgsl`'s own
/// `mip_level_offset`/`level_occupied`, which compute each level's flat offset instead of
/// uploading it, since it's a pure function of `brick_dims` and the level index (see that file's
/// own comment).
///
/// Always at least one element: a zero-length storage buffer isn't valid to create on every wgpu
/// backend, and a chunk with no mip levels at all (too small — see
/// [`VoxelChunk::mip_level_count`]) would otherwise produce an empty `Vec`. The shader never
/// indexes into this buffer when `mip_level_count == 0` (it starts marching at the brick level
/// directly instead); the dummy element only exists to satisfy wgpu.
fn build_mip_occupancy_buffer(chunk: &VoxelChunk) -> Vec<u32> {
    let mut data = Vec::new();
    for level in 0..chunk.mip_level_count() {
        let dims = chunk.mip_dims(level);
        for z in 0..dims.z {
            for y in 0..dims.y {
                for x in 0..dims.x {
                    data.push(chunk.mip_occupied(level, UVec3::new(x, y, z)) as u32);
                }
            }
        }
    }
    if data.is_empty() {
        data.push(0);
    }
    data
}

/// Computes each brick's average color across its own non-air voxels, looked up through
/// `palette` — the material [`VoxelMaterial::set_lod_distance`] shades with for a ray that stops
/// at the brick level instead of resolving the true per-voxel surface, indexed identically to
/// `brick_occupancy`.
///
/// An empty brick (no occupied voxels) reports transparent black (`Vec4::ZERO`) — the shader never
/// actually samples it (an unoccupied brick is already skipped via `brick_occupancy` before this
/// could ever be read), but a defined, harmless value beats uninitialized-looking garbage. A brick
/// mixing multiple materials gets their PLAIN AVERAGE (weighted equally per voxel, not per
/// material) — the simplest well-defined choice for a first version; a high-contrast material
/// boundary averaging to a muddy blend (grass green + rock gray -> olive) is a known, real
/// limitation of mean-based color LOD, not an oversight — a mode (most-common-material)
/// alternative would avoid it at the cost of tracking per-material counts instead of a single
/// running sum, deferred until this environment gains some way to actually SEE the tradeoff
/// (it currently has none) rather than guess at it.
fn compute_brick_average_colors(chunk: &VoxelChunk, palette: &VoxelPalette) -> Vec<Vec4> {
    let dims = chunk.dims();
    let brick_dims = chunk.brick_dims();
    let brick_count = (brick_dims.x * brick_dims.y * brick_dims.z) as usize;
    let mut sums = vec![Vec4::ZERO; brick_count];
    let mut counts = vec![0u32; brick_count];

    for z in 0..dims.z {
        for y in 0..dims.y {
            for x in 0..dims.x {
                let voxel = chunk.voxels()[flatten(UVec3::new(x, y, z), dims)];
                if voxel.is_air() {
                    continue;
                }
                let brick_coord = UVec3::new(x / BRICK_SIZE, y / BRICK_SIZE, z / BRICK_SIZE);
                let brick_idx = flatten(brick_coord, brick_dims);
                let color = palette.get(voxel).color;
                sums[brick_idx] += Vec4::new(color.red, color.green, color.blue, color.alpha);
                counts[brick_idx] += 1;
            }
        }
    }

    sums.into_iter()
        .zip(counts)
        .map(|(sum, count)| if count == 0 { Vec4::ZERO } else { sum / count as f32 })
        .collect()
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

#[cfg(test)]
mod tests {
    use bevy::color::LinearRgba;

    use super::*;
    use crate::storage::{VoxelId, VoxelMaterialInfo};

    fn palette_with(entries: &[(VoxelId, LinearRgba)]) -> VoxelPalette {
        let mut palette = VoxelPalette::new();
        for &(id, color) in entries {
            palette.set(id, VoxelMaterialInfo { color });
        }
        palette
    }

    fn vec4_of(color: LinearRgba) -> Vec4 {
        Vec4::new(color.red, color.green, color.blue, color.alpha)
    }

    /// Summing hundreds of `f32` colors then dividing accumulates real, expected rounding error
    /// (most decimal fractions like `0.1`/`0.8` have no exact binary representation to begin
    /// with) -- `assert_eq!` on the result is the wrong tool even though the underlying
    /// computation is correct. Confirmed the hard way: a first version of this test module used
    /// `assert_eq!` directly and failed on a 512-voxel brick averaging `0.8` to `0.79999673`, a
    /// ~3e-6 error, mathematically expected for naive (non-Kahan) summation at this count, not a
    /// bug in `compute_brick_average_colors`.
    fn assert_vec4_approx_eq(actual: Vec4, expected: Vec4, msg: &str) {
        const EPSILON: f32 = 1e-4;
        assert!(
            (actual - expected).abs().max_element() < EPSILON,
            "{msg}: expected {expected:?}, got {actual:?} (diff {:?} exceeds epsilon {EPSILON})",
            (actual - expected).abs(),
        );
    }

    #[test]
    fn a_brick_filled_with_one_material_averages_to_that_materials_color() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16)); // 2x2x2 bricks
        let red = LinearRgba::new(0.8, 0.1, 0.1, 1.0);
        let palette = palette_with(&[(VoxelId::new(1), red)]);

        for z in 0..8u32 {
            for y in 0..8u32 {
                for x in 0..8u32 {
                    chunk.set(UVec3::new(x, y, z), VoxelId::new(1));
                }
            }
        }

        let colors = compute_brick_average_colors(&chunk, &palette);
        assert_vec4_approx_eq(colors[0], vec4_of(red), "the fully-red brick should average to red");
    }

    #[test]
    fn an_empty_brick_reports_transparent_black_not_garbage() {
        let chunk = VoxelChunk::new(UVec3::splat(8)); // single brick, left entirely air
        let palette = VoxelPalette::new();

        let colors = compute_brick_average_colors(&chunk, &palette);
        assert_eq!(colors.len(), 1);
        assert_eq!(colors[0], Vec4::ZERO, "an all-air brick must not divide by zero or return garbage");
    }

    #[test]
    fn two_materials_in_equal_counts_average_to_their_exact_midpoint() {
        let mut chunk = VoxelChunk::new(UVec3::splat(8)); // single brick
        let black = LinearRgba::new(0.0, 0.0, 0.0, 1.0);
        let white = LinearRgba::new(1.0, 1.0, 1.0, 1.0);
        let palette = palette_with(&[(VoxelId::new(1), black), (VoxelId::new(2), white)]);

        // Exactly half the brick's 512 voxels each material -- x < 4 gets material 1, x >= 4 gets
        // material 2, so counts are provably equal (4*8*8 each) without needing to count by hand.
        for z in 0..8u32 {
            for y in 0..8u32 {
                for x in 0..8u32 {
                    let material = if x < 4 { VoxelId::new(1) } else { VoxelId::new(2) };
                    chunk.set(UVec3::new(x, y, z), material);
                }
            }
        }

        let colors = compute_brick_average_colors(&chunk, &palette);
        assert_vec4_approx_eq(colors[0], Vec4::new(0.5, 0.5, 0.5, 1.0), "black+white in equal counts should average to gray");
    }

    #[test]
    fn separate_bricks_are_computed_independently_not_cross_contaminated() {
        // 2x1x1 bricks (16x8x8 chunk) -- the brick_idx indexing math is exactly what this test
        // is checking; a bug there would show up as one brick's color leaking into the other's,
        // which none of the single-brick tests above could ever catch.
        let mut chunk = VoxelChunk::new(UVec3::new(16, 8, 8));
        let red = LinearRgba::new(1.0, 0.0, 0.0, 1.0);
        let blue = LinearRgba::new(0.0, 0.0, 1.0, 1.0);
        let palette = palette_with(&[(VoxelId::new(1), red), (VoxelId::new(2), blue)]);

        for z in 0..8u32 {
            for y in 0..8u32 {
                for x in 0..8u32 {
                    chunk.set(UVec3::new(x, y, z), VoxelId::new(1)); // brick (0,0,0): all red
                    chunk.set(UVec3::new(x + 8, y, z), VoxelId::new(2)); // brick (1,0,0): all blue
                }
            }
        }

        let colors = compute_brick_average_colors(&chunk, &palette);
        assert_eq!(colors.len(), 2);
        assert_vec4_approx_eq(colors[0], vec4_of(red), "brick (0,0,0) should be pure red, not blended with its neighbor");
        assert_vec4_approx_eq(colors[1], vec4_of(blue), "brick (1,0,0) should be pure blue, not blended with its neighbor");
    }
}
