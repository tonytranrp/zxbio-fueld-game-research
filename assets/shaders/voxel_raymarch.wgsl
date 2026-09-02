// Two-level (brick + voxel) DDA ray marcher for a single VoxelChunk, rendered on a bounding
// cuboid mesh sized to exactly match the chunk's voxel-space extent. Mirrors the CPU reference
// algorithm in `src/raymarch.rs` — see that file for the derivation/rationale of the math below.

#import bevy_pbr::forward_io::VertexOutput
#import bevy_pbr::mesh_view_bindings::view

struct VoxelParams {
    chunk_dims: vec3<u32>,
    brick_dims: vec3<u32>,
    chunk_origin: vec3<f32>,
    voxel_size: f32,
    sun_direction: vec3<f32>,
    sun_color: vec3<f32>,
    sun_intensity: f32,
}

// The material bind group is #{MATERIAL_BIND_GROUP} (3 in Bevy 0.19.1, per
// bevy_pbr::material::MATERIAL_BIND_GROUP_INDEX), not a hardcoded 2 — every stock Bevy material
// shader uses this shader-def substitution rather than a literal group number specifically so it
// keeps working if that index ever changes again.
@group(#{MATERIAL_BIND_GROUP}) @binding(0) var<uniform> params: VoxelParams;
@group(#{MATERIAL_BIND_GROUP}) @binding(1) var voxel_data: texture_3d<u32>;
@group(#{MATERIAL_BIND_GROUP}) @binding(2) var brick_occupancy: texture_3d<u32>;
@group(#{MATERIAL_BIND_GROUP}) @binding(3) var<storage, read> palette: array<vec4<f32>>;

const BRICK_SIZE: f32 = 8.0;
const MAX_BRICK_STEPS: i32 = 256;
const MAX_VOXEL_STEPS: i32 = 32;
const SHADOW_MAX_DIST: f32 = 1000.0;
const NORMAL_NUDGE: f32 = 0.01;

struct MarchResult {
    hit: bool,
    voxel: vec3<i32>,
    material: u32,
    normal: vec3<f32>,
    distance: f32,
}

fn empty_result() -> MarchResult {
    var r: MarchResult;
    r.hit = false;
    r.voxel = vec3<i32>(0);
    r.material = 0u;
    r.normal = vec3<f32>(0.0);
    r.distance = 0.0;
    return r;
}

// Standard slab-method ray/AABB intersection. Returns (t_enter, t_exit); if there's no
// intersection, t_enter > t_exit (callers must check this, matching the CPU reference).
fn ray_aabb(origin: vec3<f32>, dir: vec3<f32>, box_min: vec3<f32>, box_max: vec3<f32>) -> vec2<f32> {
    let inv_dir = 1.0 / dir;
    let t0 = (box_min - origin) * inv_dir;
    let t1 = (box_max - origin) * inv_dir;
    let t_small = min(t0, t1);
    let t_big = max(t0, t1);
    let t_enter = max(max(t_small.x, t_small.y), t_small.z);
    let t_exit = min(min(t_big.x, t_big.y), t_big.z);
    return vec2<f32>(t_enter, t_exit);
}

// Fine-level DDA within a single occupied brick. `local_origin` is relative to that brick's own
// (0,0,0) corner (i.e. the outer chunk-space origin shifted by `-brick_i * BRICK_SIZE`), so the
// t values this returns are directly comparable to the coarse march's — the shift is a pure
// translation and doesn't change distances along the ray.
fn march_voxels(local_origin: vec3<f32>, dir: vec3<f32>, brick_i: vec3<i32>) -> MarchResult {
    var result = empty_result();

    let dims_f = vec3<f32>(BRICK_SIZE, BRICK_SIZE, BRICK_SIZE);
    let bounds = ray_aabb(local_origin, dir, vec3<f32>(0.0), dims_f);
    var t_enter = max(bounds.x, 0.0);
    let t_exit = bounds.y;
    if (t_enter > t_exit) {
        return result;
    }

    let step = sign(dir);
    var cell = clamp(floor(local_origin + dir * t_enter), vec3<f32>(0.0), dims_f - vec3<f32>(1.0));
    let t_delta = 1.0 / abs(dir);
    var t_max = ((cell + max(step, vec3<f32>(0.0))) - local_origin) / dir;
    var last_normal = vec3<f32>(0.0);

    for (var i: i32 = 0; i < MAX_VOXEL_STEPS; i = i + 1) {
        if (t_enter > t_exit) {
            break;
        }

        let voxel = brick_i * i32(BRICK_SIZE) + vec3<i32>(cell);
        let material = textureLoad(voxel_data, voxel, 0).r;
        if (material > 0u) {
            result.hit = true;
            result.voxel = voxel;
            result.material = material;
            result.normal = last_normal;
            result.distance = t_enter;
            return result;
        }

        if (t_max.x < t_max.y && t_max.x < t_max.z) {
            cell.x = cell.x + step.x;
            t_enter = t_max.x;
            t_max.x = t_max.x + t_delta.x;
            last_normal = vec3<f32>(-step.x, 0.0, 0.0);
        } else if (t_max.y < t_max.z) {
            cell.y = cell.y + step.y;
            t_enter = t_max.y;
            t_max.y = t_max.y + t_delta.y;
            last_normal = vec3<f32>(0.0, -step.y, 0.0);
        } else {
            cell.z = cell.z + step.z;
            t_enter = t_max.z;
            t_max.z = t_max.z + t_delta.z;
            last_normal = vec3<f32>(0.0, 0.0, -step.z);
        }

        if (cell.x < 0.0 || cell.y < 0.0 || cell.z < 0.0
            || cell.x >= dims_f.x || cell.y >= dims_f.y || cell.z >= dims_f.z) {
            break;
        }
    }

    return result;
}

// Coarse-level DDA over the chunk's brick grid, skipping whole empty bricks (checked via
// `brick_occupancy`) and dropping into `march_voxels` only for occupied ones. `local_origin`/
// `dir` are in the chunk's own voxel-index space (one voxel = one unit).
fn march_chunk(local_origin: vec3<f32>, dir: vec3<f32>, max_dist: f32) -> MarchResult {
    var result = empty_result();

    let dims_f = vec3<f32>(params.chunk_dims);
    let bricks_f = vec3<f32>(params.brick_dims);

    let bounds = ray_aabb(local_origin, dir, vec3<f32>(0.0), dims_f);
    var t_enter = max(bounds.x, 0.0);
    let t_exit = min(bounds.y, max_dist);
    if (t_enter > t_exit) {
        return result;
    }

    let step = sign(dir);
    var brick_cell = clamp(
        floor((local_origin + dir * t_enter) / BRICK_SIZE),
        vec3<f32>(0.0),
        bricks_f - vec3<f32>(1.0),
    );
    let t_delta = BRICK_SIZE / abs(dir);
    var t_max = ((brick_cell + max(step, vec3<f32>(0.0))) * BRICK_SIZE - local_origin) / dir;

    for (var i: i32 = 0; i < MAX_BRICK_STEPS; i = i + 1) {
        if (t_enter > t_exit) {
            break;
        }

        let brick_i = vec3<i32>(brick_cell);
        let occupied = textureLoad(brick_occupancy, brick_i, 0).r;
        if (occupied > 0u) {
            let brick_origin = vec3<f32>(brick_i) * BRICK_SIZE;
            let fine = march_voxels(local_origin - brick_origin, dir, brick_i);
            if (fine.hit && fine.distance <= max_dist) {
                return fine;
            }
        }

        if (t_max.x < t_max.y && t_max.x < t_max.z) {
            brick_cell.x = brick_cell.x + step.x;
            t_enter = t_max.x;
            t_max.x = t_max.x + t_delta.x;
        } else if (t_max.y < t_max.z) {
            brick_cell.y = brick_cell.y + step.y;
            t_enter = t_max.y;
            t_max.y = t_max.y + t_delta.y;
        } else {
            brick_cell.z = brick_cell.z + step.z;
            t_enter = t_max.z;
            t_max.z = t_max.z + t_delta.z;
        }

        if (brick_cell.x < 0.0 || brick_cell.y < 0.0 || brick_cell.z < 0.0
            || brick_cell.x >= bricks_f.x || brick_cell.y >= bricks_f.y || brick_cell.z >= bricks_f.z) {
            break;
        }
    }

    return result;
}

@fragment
fn fragment(mesh: VertexOutput) -> @location(0) vec4<f32> {
    let origin_world = view.world_position;
    let dir_world = normalize(mesh.world_position.xyz - origin_world);

    // World space -> chunk-local voxel-index space (one voxel = one unit). `dir_local` is
    // deliberately left unnormalized: its length is 1/voxel_size, which makes every `distance`
    // the marcher reports come back out already in world-space units, with no extra conversion.
    let origin_local = (origin_world - params.chunk_origin) / params.voxel_size;
    let dir_local = dir_world / params.voxel_size;

    let primary = march_chunk(origin_local, dir_local, 1e6);
    if (!primary.hit) {
        discard;
    }

    let hit_local = origin_local + dir_local * primary.distance;
    let normal_world = primary.normal; // axis-aligned, unaffected by uniform voxel_size scaling

    let sun_dir = normalize(params.sun_direction);
    let ndotl = max(dot(normal_world, sun_dir), 0.0);

    var shadow = 1.0;
    if (ndotl > 0.0) {
        let shadow_origin = hit_local + primary.normal * NORMAL_NUDGE;
        let shadow_dir_local = sun_dir / params.voxel_size;
        let occluder = march_chunk(shadow_origin, shadow_dir_local, SHADOW_MAX_DIST);
        if (occluder.hit) {
            shadow = 0.0;
        }
    }

    let base_color = palette[primary.material].rgb;
    let ambient = 0.15;
    let lit = base_color * (ambient + (1.0 - ambient) * ndotl * shadow * params.sun_intensity * params.sun_color);

    return vec4<f32>(lit, 1.0);
}
