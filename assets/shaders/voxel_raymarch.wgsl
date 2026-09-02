// N-level (mip hierarchy -> bricks -> voxels) DDA ray marcher for a single VoxelChunk, rendered
// on a bounding cuboid mesh sized to exactly match the chunk's voxel-space extent. Mirrors the
// CPU reference algorithm in `src/raymarch.rs` (`march`/`Dda`/`Level`/`pop_and_advance`) as
// closely as WGSL allows -- see that file for the derivation/rationale of the math below; this
// file's own comments focus on where and why the WGSL port had to diverge in SHAPE (never in
// behavior) from the Rust original.
//
// WGSL has no recursion and no `Vec` -- the two things `march()`'s explicit-stack design was
// itself already written to not need (see that function's own doc comment: it was deliberately
// converted from recursive to iterative on the CPU first, specifically so this port would be a
// mechanical translation rather than a redesign). The one remaining gap is `Vec`'s dynamic
// growth: the stack here is a fixed-size array (`MAX_STACK`), generously sized for far more mip
// levels than any plausible chunk needs (see that constant's own comment) and guarded on the Rust
// side (`MAX_MIP_STACK_LEVELS` in `src/render/material.rs`, which must stay in sync) so a chunk
// that would overflow it fails loudly at spawn time instead of silently corrupting GPU memory.

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
    mip_level_count: u32,
}

// The material bind group is #{MATERIAL_BIND_GROUP} (3 in Bevy 0.19.1, per
// bevy_pbr::material::MATERIAL_BIND_GROUP_INDEX), not a hardcoded 2 -- every stock Bevy material
// shader uses this shader-def substitution rather than a literal group number specifically so it
// keeps working if that index ever changes again.
@group(#{MATERIAL_BIND_GROUP}) @binding(0) var<uniform> params: VoxelParams;
@group(#{MATERIAL_BIND_GROUP}) @binding(1) var voxel_data: texture_3d<u32>;
@group(#{MATERIAL_BIND_GROUP}) @binding(2) var brick_occupancy: texture_3d<u32>;
@group(#{MATERIAL_BIND_GROUP}) @binding(3) var<storage, read> palette: array<vec4<f32>>;
// The whole mip hierarchy, flattened: level 0 (finest, immediately above bricks) first through
// the coarsest level last, each level's own cells in the same x-major/y/z order as
// `storage::coords::flatten` -- see `mip_level_offset` below for how a level's start is found,
// and `src/render/material.rs`'s `build_mip_occupancy_buffer` for the matching upload-side
// packing. One `u32` per cell (not bit-packed) -- the whole hierarchy above a 128-voxel chunk's
// bricks is under 3,000 cells, so packing would save a few KB nobody has measured a need for yet.
@group(#{MATERIAL_BIND_GROUP}) @binding(4) var<storage, read> mip_occupancy: array<u32>;

const BRICK_SIZE: f32 = 8.0;
// Effectively infinite for every comparison this algorithm does (it's only ever compared against
// or added to other finite t-values, never subtracted from another INF), but a large FINITE
// value on purpose -- a real WGSL infinity is only reachable via a runtime divide-by-zero, and
// const-evaluating one at this scope risks the shader compiler rejecting the division outright.
const INF: f32 = 1e30;
// Hard cap on total DDA steps across every level combined, purely as a GPU-hang safety net (a
// fragment shader stuck in a runaway loop hangs the driver, not just this one pixel) -- sized
// generously above any legitimate traversal (see `MAX_BRICK_STEPS`/`MAX_VOXEL_STEPS` in this
// file's own prior two-level version for the same philosophy at a smaller scale) and not expected
// to ever actually bind.
const MAX_MARCH_ITERATIONS: i32 = 2048;
// Mip levels + the brick level + the voxel level. Levels are encoded as a single i32: `>= 0` is
// `Mip(that index)` (0 = finest, matching `Level::Mip` in `src/raymarch.rs`), `-1` is Brick,
// `-2` is Voxel. 10 mip levels covers brick-grid dims up to 2^10 = 1024 per axis (a single chunk
// of 8192 voxels/axis) -- far past anything this engine has ever tested; `MAX_MIP_STACK_LEVELS`
// in `src/render/material.rs` asserts a chunk never actually needs more than this before it ever
// reaches the GPU.
const MAX_STACK: i32 = 12;
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
    r.voxel = vec3<i32>(0, 0, 0);
    r.material = 0u;
    r.normal = vec3<f32>(0.0, 0.0, 0.0);
    r.distance = 0.0;
    return r;
}

fn sign_i(x: f32) -> i32 {
    if (x > 0.0) {
        return 1;
    }
    if (x < 0.0) {
        return -1;
    }
    return 0;
}

fn safe_div(a: f32, b: f32) -> f32 {
    if (b == 0.0) {
        return INF;
    }
    return a / b;
}

fn axis_boundary(cell: i32, step: i32, cell_size: f32) -> f32 {
    if (step > 0) {
        return (f32(cell) + 1.0) * cell_size;
    }
    return f32(cell) * cell_size;
}

fn axis_t_max(boundary: f32, origin: f32, dir: f32) -> f32 {
    if (dir == 0.0) {
        return INF;
    }
    return (boundary - origin) / dir;
}

// This level's cell size, in chunk-voxel-index units. Level::Mip(k)'s own `2^(k+1)` factor (one
// full brick, doubled per level above it) mirrors `Level::cell_size` in `src/raymarch.rs` exactly
// -- computed via an exact integer shift, not `pow`, since these exponents are always small
// non-negative integers and a shift is exact where a float `pow` need not be.
fn level_cell_size(lvl: i32) -> f32 {
    if (lvl == -2) {
        return 1.0;
    }
    if (lvl == -1) {
        return BRICK_SIZE;
    }
    return BRICK_SIZE * f32(1u << u32(lvl + 1));
}

// A mip level's own dims are `ceil(brick_dims / 2^(lvl+1))` in one shot, not `lvl+1` iterated
// halvings -- a real identity (`ceil(ceil(n/a)/b) == ceil(n/(a*b))` for positive integers),
// re-derived here rather than assumed: iterating `OccupancyMip::hierarchy_for`'s own halving
// step-by-step would need a loop with no fixed bound at shader-compile time, while this closed
// form needs none. Cross-checked against `VoxelChunk`'s own mip-hierarchy tests (e.g. dims
// `[8,4,2,1]` for `brick_dims=16`) by hand before trusting it here.
fn mip_dims_at(lvl: i32, brick_dims: vec3<u32>) -> vec3<u32> {
    let divisor = 1u << u32(lvl + 1);
    return (brick_dims + vec3<u32>(divisor - 1u)) / divisor;
}

fn level_dims(lvl: i32, chunk_dims: vec3<u32>, brick_dims: vec3<u32>) -> vec3<u32> {
    if (lvl == -2) {
        return chunk_dims;
    }
    if (lvl == -1) {
        return brick_dims;
    }
    return mip_dims_at(lvl, brick_dims);
}

// Where level `lvl`'s own cells start in the flat `mip_occupancy` buffer -- the sum of every
// finer level's cell count, matching exactly how `build_mip_occupancy_buffer` concatenates them
// (level 0 first). `lvl` is always small (bounded by `MAX_STACK`), so this loop is cheap; it is
// recomputed on every mip-level occupancy check rather than uploaded as a precomputed table,
// which is a real, deliberate simplicity-over-speed choice -- revisit if profiling ever shows it
// matters (nothing has measured that yet).
fn mip_level_offset(lvl: i32, brick_dims: vec3<u32>) -> u32 {
    var offset: u32 = 0u;
    for (var j: i32 = 0; j < lvl; j = j + 1) {
        let d = mip_dims_at(j, brick_dims);
        offset = offset + d.x * d.y * d.z;
    }
    return offset;
}

// Whether `cell` (in level `lvl`'s own coordinate space) is occupied. Never called for the Voxel
// level (-2) -- `march_hierarchical` handles that directly via `voxel_data`, matching
// `Level::occupied`'s own `unreachable!()` for `Level::Voxel` in the CPU reference.
fn level_occupied(lvl: i32, cell: vec3<u32>, brick_dims: vec3<u32>) -> bool {
    if (lvl == -1) {
        return textureLoad(brick_occupancy, vec3<i32>(cell), 0).r > 0u;
    }
    let dims = mip_dims_at(lvl, brick_dims);
    // Out-of-range is unoccupied, matching `OccupancyMip::get`'s own convention exactly -- `cell`
    // should always be in-bounds by construction here (dda_enter/dda_enter_at clamp on entry,
    // dda_advance refuses to step out of bounds), but indexing a storage buffer past this level's
    // own slice without checking would silently read a NEIGHBORING level's data instead of
    // failing loudly, which is a real risk worth guarding against even though nothing has found a
    // way to actually trigger it.
    if (cell.x >= dims.x || cell.y >= dims.y || cell.z >= dims.z) {
        return false;
    }
    let offset = mip_level_offset(lvl, brick_dims);
    let idx = cell.x + cell.y * dims.x + cell.z * dims.x * dims.y;
    return mip_occupancy[offset + idx] > 0u;
}

struct AabbHit {
    valid: bool,
    t_enter: f32,
    t_exit: f32,
    entry_normal: vec3<i32>,
}

// Standard slab-method ray/AABB intersection, ported directly from `ray_aabb_intersect` in
// `src/raymarch.rs` -- written out per-axis rather than in a loop (WGSL has no easy way to
// iterate three axes of a mixed scalar/vector tuple the way the Rust version's `for (o, d, lo,
// hi, ...) in axes` does) so it can be visually diffed against that function's own per-axis
// branches rather than trusted on faith.
fn ray_aabb_full(origin: vec3<f32>, dir: vec3<f32>, box_min: vec3<f32>, box_max: vec3<f32>) -> AabbHit {
    var result: AabbHit;
    var t_enter = -INF;
    var t_exit = INF;
    var entry_normal = vec3<i32>(0, 0, 0);

    if (dir.x == 0.0) {
        if (origin.x < box_min.x || origin.x > box_max.x) {
            result.valid = false;
            return result;
        }
    } else if (dir.x > 0.0) {
        let t0 = (box_min.x - origin.x) / dir.x;
        let t1 = (box_max.x - origin.x) / dir.x;
        if (t0 > t_enter) {
            t_enter = t0;
            entry_normal = vec3<i32>(-1, 0, 0);
        }
        t_exit = min(t_exit, t1);
    } else {
        let t0 = (box_max.x - origin.x) / dir.x;
        let t1 = (box_min.x - origin.x) / dir.x;
        if (t0 > t_enter) {
            t_enter = t0;
            entry_normal = vec3<i32>(1, 0, 0);
        }
        t_exit = min(t_exit, t1);
    }

    if (dir.y == 0.0) {
        if (origin.y < box_min.y || origin.y > box_max.y) {
            result.valid = false;
            return result;
        }
    } else if (dir.y > 0.0) {
        let t0 = (box_min.y - origin.y) / dir.y;
        let t1 = (box_max.y - origin.y) / dir.y;
        if (t0 > t_enter) {
            t_enter = t0;
            entry_normal = vec3<i32>(0, -1, 0);
        }
        t_exit = min(t_exit, t1);
    } else {
        let t0 = (box_max.y - origin.y) / dir.y;
        let t1 = (box_min.y - origin.y) / dir.y;
        if (t0 > t_enter) {
            t_enter = t0;
            entry_normal = vec3<i32>(0, 1, 0);
        }
        t_exit = min(t_exit, t1);
    }

    if (dir.z == 0.0) {
        if (origin.z < box_min.z || origin.z > box_max.z) {
            result.valid = false;
            return result;
        }
    } else if (dir.z > 0.0) {
        let t0 = (box_min.z - origin.z) / dir.z;
        let t1 = (box_max.z - origin.z) / dir.z;
        if (t0 > t_enter) {
            t_enter = t0;
            entry_normal = vec3<i32>(0, 0, -1);
        }
        t_exit = min(t_exit, t1);
    } else {
        let t0 = (box_max.z - origin.z) / dir.z;
        let t1 = (box_min.z - origin.z) / dir.z;
        if (t0 > t_enter) {
            t_enter = t0;
            entry_normal = vec3<i32>(0, 0, 1);
        }
        t_exit = min(t_exit, t1);
    }

    result.valid = t_enter <= t_exit;
    result.t_enter = t_enter;
    result.t_exit = t_exit;
    result.entry_normal = entry_normal;
    return result;
}

struct DdaState {
    cell: vec3<i32>,
    dims: vec3<u32>,
    step: vec3<i32>,
    t_max: vec3<f32>,
    t_delta: vec3<f32>,
    t_enter: f32,
    last_normal: vec3<i32>,
}

struct DdaEnterResult {
    valid: bool,
    dda: DdaState,
}

// Ported from `Dda::enter`: slab-tests `origin + t*dir` against `[bounds_min, bounds_max]` and,
// if it intersects within `[0, max_t]`, returns a `DdaState` positioned at the entry cell.
fn dda_enter(origin: vec3<f32>, dir: vec3<f32>, cell_size: f32, dims: vec3<u32>, bounds_min: vec3<f32>, bounds_max: vec3<f32>, max_t: f32) -> DdaEnterResult {
    var result: DdaEnterResult;
    result.valid = false;

    let aabb = ray_aabb_full(origin, dir, bounds_min, bounds_max);
    if (!aabb.valid) {
        return result;
    }
    let raw_t_enter = aabb.t_enter;
    let t_exit = aabb.t_exit;
    let t_enter = max(raw_t_enter, 0.0);
    if (t_enter > t_exit || t_enter > max_t) {
        return result;
    }
    var last_normal = vec3<i32>(0, 0, 0);
    if (raw_t_enter > 0.0) {
        last_normal = aabb.entry_normal;
    }

    let entry_point = origin + dir * t_enter;
    var cell = vec3<i32>(floor(entry_point / cell_size));
    let dims_i = vec3<i32>(dims);
    cell = clamp(cell, vec3<i32>(0, 0, 0), dims_i - vec3<i32>(1, 1, 1));

    let step = vec3<i32>(sign_i(dir.x), sign_i(dir.y), sign_i(dir.z));
    let t_delta = vec3<f32>(safe_div(cell_size, abs(dir.x)), safe_div(cell_size, abs(dir.y)), safe_div(cell_size, abs(dir.z)));
    let t_max = vec3<f32>(
        axis_t_max(axis_boundary(cell.x, step.x, cell_size), origin.x, dir.x),
        axis_t_max(axis_boundary(cell.y, step.y, cell_size), origin.y, dir.y),
        axis_t_max(axis_boundary(cell.z, step.z, cell_size), origin.z, dir.z),
    );

    result.valid = true;
    result.dda.cell = cell;
    result.dda.dims = dims;
    result.dda.step = step;
    result.dda.t_max = t_max;
    result.dda.t_delta = t_delta;
    result.dda.t_enter = t_enter;
    result.dda.last_normal = last_normal;
    return result;
}

// Ported from `Dda::enter_at`: enters a finer grid at an already-known valid point on the ray
// (typically a coarser frame's own current `t_enter`/entry point when descending into it), skipping
// the full slab test `dda_enter` needs for a genuinely unknown starting point.
fn dda_enter_at(origin: vec3<f32>, entry_point: vec3<f32>, entry_t: f32, dir: vec3<f32>, cell_size: f32, dims: vec3<u32>, last_normal: vec3<i32>) -> DdaState {
    var cell = vec3<i32>(floor(entry_point / cell_size));
    let dims_i = vec3<i32>(dims);
    cell = clamp(cell, vec3<i32>(0, 0, 0), dims_i - vec3<i32>(1, 1, 1));

    let step = vec3<i32>(sign_i(dir.x), sign_i(dir.y), sign_i(dir.z));
    let t_delta = vec3<f32>(safe_div(cell_size, abs(dir.x)), safe_div(cell_size, abs(dir.y)), safe_div(cell_size, abs(dir.z)));
    let t_max = vec3<f32>(
        axis_t_max(axis_boundary(cell.x, step.x, cell_size), origin.x, dir.x),
        axis_t_max(axis_boundary(cell.y, step.y, cell_size), origin.y, dir.y),
        axis_t_max(axis_boundary(cell.z, step.z, cell_size), origin.z, dir.z),
    );

    var result: DdaState;
    result.cell = cell;
    result.dims = dims;
    result.step = step;
    result.t_max = t_max;
    result.t_delta = t_delta;
    result.t_enter = entry_t;
    result.last_normal = last_normal;
    return result;
}

struct DdaAdvanceResult {
    ok: bool,
    dda: DdaState,
}

// Ported from `Dda::advance`. Takes `state` by value and returns the advanced copy (rather than
// mutating through a pointer) -- WGSL supports `ptr<function, T>` parameters, but a plain
// value-in/value-out function is one fewer thing to get subtly wrong in a hot path that already
// has enough moving parts, and `DdaState` is small enough that the copy is free.
fn dda_advance(state: DdaState) -> DdaAdvanceResult {
    var result: DdaAdvanceResult;
    result.dda = state;

    var axis = 0;
    if (state.t_max.x <= state.t_max.y && state.t_max.x <= state.t_max.z) {
        axis = 0;
    } else if (state.t_max.y <= state.t_max.z) {
        axis = 1;
    } else {
        axis = 2;
    }

    var next_cell = state.cell;
    var t: f32;
    var normal = vec3<i32>(0, 0, 0);
    if (axis == 0) {
        next_cell.x = state.cell.x + state.step.x;
        t = state.t_max.x;
        normal = vec3<i32>(-state.step.x, 0, 0);
    } else if (axis == 1) {
        next_cell.y = state.cell.y + state.step.y;
        t = state.t_max.y;
        normal = vec3<i32>(0, -state.step.y, 0);
    } else {
        next_cell.z = state.cell.z + state.step.z;
        t = state.t_max.z;
        normal = vec3<i32>(0, 0, -state.step.z);
    }

    let dims_i = vec3<i32>(state.dims);
    let out_of_bounds = next_cell.x < 0 || next_cell.y < 0 || next_cell.z < 0
        || next_cell.x >= dims_i.x || next_cell.y >= dims_i.y || next_cell.z >= dims_i.z;
    if (out_of_bounds) {
        result.ok = false;
        return result;
    }

    result.dda.cell = next_cell;
    result.dda.t_enter = t;
    if (axis == 0) {
        result.dda.t_max.x = state.t_max.x + state.t_delta.x;
    } else if (axis == 1) {
        result.dda.t_max.y = state.t_max.y + state.t_delta.y;
    } else {
        result.dda.t_max.z = state.t_max.z + state.t_delta.z;
    }
    result.dda.last_normal = normal;
    result.ok = true;
    return result;
}

// Ported from `pop_and_advance`: pops the exhausted frame at `*top` (the caller's current top,
// already known to need popping) and advances whichever frame is now on top, popping further if
// that one turns out to already be exhausted too -- exactly like unwinding a recursive call stack
// one level at a time. Returns `false` once `*top` goes negative, meaning the whole march
// concluded with no hit anywhere; leaves `*top` at `-1` in that case so callers can check it
// directly instead of re-deriving "empty" from anything else.
fn pop_and_advance(levels: ptr<function, array<i32, MAX_STACK>>, ddas: ptr<function, array<DdaState, MAX_STACK>>, top: ptr<function, i32>) -> bool {
    if (*top == 0) {
        *top = -1;
        return false;
    }
    *top = *top - 1;
    // A bounded `for`, not a `break`-less `loop` -- naga's function validator rejects a function
    // ending in an unconditional `loop` with only internal `return`s (tried that shape first; it
    // fails to build even with an unreachable `return` textually after the loop, so this isn't
    // just a missing-trailing-statement issue). `*top` strictly decreases every iteration, so
    // `MAX_STACK` iterations is already provably more than enough to either return or reach
    // `*top == 0` -- the bound exists to give the validator a real fall-through path, not because
    // this can actually run that long.
    for (var guard = 0; guard < MAX_STACK; guard = guard + 1) {
        let adv = dda_advance((*ddas)[*top]);
        if (adv.ok) {
            (*ddas)[*top] = adv.dda;
            return true;
        }
        if (*top == 0) {
            *top = -1;
            return false;
        }
        *top = *top - 1;
    }
    return false;
}

// The engine's real ray marcher: starts at the coarsest available level (the top of the mip
// hierarchy, or the brick level for a chunk too small to have one) and descends into finer levels
// only where a cell is actually occupied, via a fixed-size explicit stack -- see this file's own
// module doc comment for why the stack is fixed-size here where `src/raymarch.rs`'s own version
// can use a growable `Vec`, and `MAX_STACK`'s comment for why that bound is not a real limitation
// in practice. Structurally a direct port of `march`/`pop_and_advance` in that file; read this
// function side-by-side with that one, not on its own, if verifying a change to either.
fn march_hierarchical(origin: vec3<f32>, dir: vec3<f32>, max_dist: f32) -> MarchResult {
    var result = empty_result();

    // Matches `cast_ray`'s own early-out (src/raymarch.rs) exactly -- a zero direction can't slab-
    // test against anything (every axis would hit the `dir==0` branch in ray_aabb_full with no
    // way to ever establish a valid t_enter/t_exit), and a non-positive max_dist can never contain
    // a valid hit. Both call sites below (primary ray, shadow ray) look unreachable in practice
    // (primary comes from a normalize() of a presumably-nonzero vector; the shadow ray is gated
    // behind ndotl > 0.0, which already implies a non-degenerate direction) -- kept as an explicit
    // guard anyway rather than relying on that, matching the CPU reference's own defensiveness.
    if ((dir.x == 0.0 && dir.y == 0.0 && dir.z == 0.0) || max_dist <= 0.0) {
        return result;
    }

    let chunk_dims = params.chunk_dims;
    let brick_dims = params.brick_dims;
    let mip_count = i32(params.mip_level_count);

    var top_level = -1;
    if (mip_count > 0) {
        top_level = mip_count - 1;
    }

    let top_dims = level_dims(top_level, chunk_dims, brick_dims);
    let top_cell_size = level_cell_size(top_level);
    let bounds_max = vec3<f32>(top_dims) * top_cell_size;

    let entered = dda_enter(origin, dir, top_cell_size, top_dims, vec3<f32>(0.0, 0.0, 0.0), bounds_max, max_dist);
    if (!entered.valid) {
        return result;
    }

    var levels: array<i32, MAX_STACK>;
    var ddas: array<DdaState, MAX_STACK>;
    var max_dists: array<f32, MAX_STACK>;
    var top: i32 = 0;
    levels[0] = top_level;
    ddas[0] = entered.dda;
    max_dists[0] = max_dist;

    for (var iter = 0; iter < MAX_MARCH_ITERATIONS; iter = iter + 1) {
        let level = levels[top];
        let level_max_dist = max_dists[top];
        let t_enter = ddas[top].t_enter;

        if (t_enter > level_max_dist) {
            if (!pop_and_advance(&levels, &ddas, &top)) {
                return result;
            }
            continue;
        }

        let cell = ddas[top].cell;

        if (level == -2) {
            let material = textureLoad(voxel_data, cell, 0).r;
            if (material > 0u) {
                result.hit = true;
                result.voxel = cell;
                result.material = material;
                result.normal = vec3<f32>(ddas[top].last_normal);
                result.distance = t_enter;
                return result;
            }
            let adv = dda_advance(ddas[top]);
            if (adv.ok) {
                ddas[top] = adv.dda;
            } else if (!pop_and_advance(&levels, &ddas, &top)) {
                return result;
            }
        } else if (level_occupied(level, vec3<u32>(cell), brick_dims)) {
            let dda = ddas[top];
            let cell_exit = min(min(dda.t_max.x, dda.t_max.y), dda.t_max.z);
            let confined_exit = min(cell_exit, level_max_dist);
            let entry_t = max(dda.t_enter, 0.0);
            let entry_point = origin + dir * entry_t;
            let last_normal = dda.last_normal;

            let finer_level = level - 1;
            let finer_dims = level_dims(finer_level, chunk_dims, brick_dims);
            let finer_cell_size = level_cell_size(finer_level);
            let finer_dda = dda_enter_at(origin, entry_point, entry_t, dir, finer_cell_size, finer_dims, last_normal);

            top = top + 1;
            levels[top] = finer_level;
            ddas[top] = finer_dda;
            max_dists[top] = confined_exit;
        } else {
            let adv = dda_advance(ddas[top]);
            if (adv.ok) {
                ddas[top] = adv.dda;
            } else if (!pop_and_advance(&levels, &ddas, &top)) {
                return result;
            }
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

    let primary = march_hierarchical(origin_local, dir_local, 1e6);
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
        let occluder = march_hierarchical(shadow_origin, shadow_dir_local, SHADOW_MAX_DIST);
        if (occluder.hit) {
            shadow = 0.0;
        }
    }

    let base_color = palette[primary.material].rgb;
    let ambient = 0.15;
    let lit = base_color * (ambient + (1.0 - ambient) * ndotl * shadow * params.sun_intensity * params.sun_color);

    return vec4<f32>(lit, 1.0);
}
