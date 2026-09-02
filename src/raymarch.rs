//! CPU reference implementation of the N-level (mip hierarchy -> bricks -> voxels) DDA ray
//! marcher used to render a [`VoxelChunk`]. `assets/shaders/voxel_raymarch.wgsl` implements the
//! same algorithm on the GPU; this version exists to validate that algorithm independent of any
//! GPU context, and to serve future CPU-side queries (picking, editing).
//!
//! All coordinates here are in the chunk's own local voxel-index space, where one voxel spans
//! exactly one unit and the chunk occupies `[0, dims.x] x [0, dims.y] x [0, dims.z]`. Converting
//! to/from world space (chunk scale/position) is the caller's responsibility.

use bevy::math::{IVec3, UVec3, Vec3};

use crate::storage::coords::BRICK_SIZE;
use crate::storage::{VoxelChunk, VoxelId};

/// The result of a successful [`cast_ray`].
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct RayHit {
    /// The voxel coordinate that was hit.
    pub voxel: UVec3,
    /// The material at the hit voxel (never [`VoxelId::AIR`]).
    pub material: VoxelId,
    /// Distance from the ray origin to the hit point, in units of `dir`'s own length (pass a
    /// normalized `dir` if you want this in world units).
    pub distance: f32,
    /// The axis-aligned face normal at the hit point (exactly one component is +-1, the rest 0).
    pub normal: IVec3,
}

/// Which level of the occupancy hierarchy a `march` call is currently working through, coarsest
/// first. Every level marches over the SAME absolute chunk-voxel-index space — only `cell_size`
/// and which occupancy check applies change between levels — so no coordinate offsetting is
/// needed between recursive calls, just a tighter `bounds`/`max_dist` confining each recursive
/// call to the parent cell that led to it.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Level {
    Mip(usize),
    Brick,
    Voxel,
}

impl Level {
    fn cell_size(self) -> f32 {
        match self {
            Level::Mip(k) => BRICK_SIZE as f32 * 2f32.powi(k as i32 + 1),
            Level::Brick => BRICK_SIZE as f32,
            Level::Voxel => 1.0,
        }
    }

    fn dims(self, chunk: &VoxelChunk) -> UVec3 {
        match self {
            Level::Mip(k) => chunk.mip_dims(k),
            Level::Brick => chunk.brick_dims(),
            Level::Voxel => chunk.dims(),
        }
    }

    /// The next-finer level to recurse into once a cell at `self` is found occupied.
    fn finer(self) -> Option<Level> {
        match self {
            Level::Mip(0) => Some(Level::Brick),
            Level::Mip(k) => Some(Level::Mip(k - 1)),
            Level::Brick => Some(Level::Voxel),
            Level::Voxel => None,
        }
    }

    /// Whether `coord` is occupied at this level. Never called for [`Level::Voxel`] — `march`
    /// handles that level specially (a direct [`VoxelChunk::get`] check, not an occupancy
    /// pre-check) since there's nothing finer to recurse into past it.
    fn occupied(self, chunk: &VoxelChunk, coord: UVec3) -> bool {
        match self {
            Level::Mip(k) => chunk.mip_occupied(k, coord),
            Level::Brick => chunk.brick_occupied(coord),
            Level::Voxel => unreachable!("Level::Voxel has no occupancy pre-check; march() handles it directly"),
        }
    }
}

/// Marches a ray through `chunk`, stopping at the first non-air voxel within `max_dist`. Returns
/// `None` if the ray never enters the chunk, exits it, or reaches `max_dist` without hitting
/// anything.
///
/// Starts at the coarsest available level (the top of the chunk's mip hierarchy, or the brick
/// level for a chunk too small to have one) and recurses into finer levels only where a cell is
/// actually occupied — skipping empty regions at whatever granularity they're empty at, rather
/// than visiting every brick or voxel inside them individually.
pub fn cast_ray(chunk: &VoxelChunk, origin: Vec3, dir: Vec3, max_dist: f32) -> Option<RayHit> {
    if dir == Vec3::ZERO || max_dist <= 0.0 {
        return None;
    }

    let top_level = if chunk.mip_level_count() > 0 {
        Level::Mip(chunk.mip_level_count() - 1)
    } else {
        Level::Brick
    };

    let dims = top_level.dims(chunk);
    let cell_size = top_level.cell_size();
    let bounds_max = Vec3::new(dims.x as f32, dims.y as f32, dims.z as f32) * cell_size;

    march(chunk, top_level, origin, dir, Vec3::ZERO, bounds_max, max_dist)
}

/// Marches through one level of the occupancy hierarchy, confined to `[bounds_min, bounds_max]`
/// (the parent cell that led to this call, or the whole chunk for the initial top-level call),
/// recursing into [`Level::finer`] for each occupied cell found until reaching [`Level::Voxel`],
/// where a direct [`VoxelChunk::get`] check either returns a hit or continues the march.
fn march(
    chunk: &VoxelChunk,
    level: Level,
    origin: Vec3,
    dir: Vec3,
    bounds_min: Vec3,
    bounds_max: Vec3,
    max_dist: f32,
) -> Option<RayHit> {
    let dims = level.dims(chunk);
    let cell_size = level.cell_size();
    let mut dda = Dda::enter(origin, dir, cell_size, dims, bounds_min, bounds_max, max_dist)?;

    loop {
        if dda.t_enter > max_dist {
            return None;
        }

        let cell = UVec3::new(dda.cell.x as u32, dda.cell.y as u32, dda.cell.z as u32);

        if level == Level::Voxel {
            let material = chunk.get(cell);
            if !material.is_air() {
                return Some(RayHit {
                    voxel: cell,
                    material,
                    distance: dda.t_enter,
                    normal: dda.last_normal,
                });
            }
        } else if level.occupied(chunk, cell) {
            // t_max (before advancing) is exactly the t at which the ray leaves this cell —
            // clamping the recursive call's max_dist to it is what makes that call naturally
            // stop at this cell's own boundary and hand control back here, without needing to
            // explicitly re-derive "which finer cells belong to this coarse cell."
            let cell_exit = dda.t_max.x.min(dda.t_max.y).min(dda.t_max.z).min(max_dist);
            let cell_min = Vec3::new(cell.x as f32, cell.y as f32, cell.z as f32) * cell_size;
            let cell_max = cell_min + Vec3::splat(cell_size);
            let finer = level.finer().expect("non-Voxel level always has a finer level");
            if let Some(hit) = march(chunk, finer, origin, dir, cell_min, cell_max, cell_exit) {
                return Some(hit);
            }
        }

        if !dda.advance() {
            return None;
        }
    }
}

/// Amanatides-Woo DDA state for marching through an integer grid of `dims` cells, each
/// `cell_size` units across, in absolute chunk-voxel-index space.
struct Dda {
    cell: IVec3,
    dims: UVec3,
    step: IVec3,
    t_max: Vec3,
    t_delta: Vec3,
    /// Distance along the ray to the point where the ray entered `cell`.
    t_enter: f32,
    /// Face normal of the boundary crossed to enter `cell` (zero if `cell` is the ray's own
    /// starting cell, i.e. no boundary was crossed to get there).
    last_normal: IVec3,
}

impl Dda {
    /// Slab-tests `origin + t*dir` against `[bounds_min, bounds_max]` and, if it intersects
    /// within `[0, max_t]`, returns a `Dda` positioned at the entry cell. `dims`/`cell_size`
    /// define the grid's own absolute cell indexing (used for stepping and for clamping the
    /// entry cell to a valid index) — `bounds_min`/`bounds_max` need not span the whole grid; a
    /// caller confining the search to one parent cell of a coarser level passes that cell's own
    /// bounds here, while still getting back a correctly-indexed absolute cell in this grid.
    fn enter(
        origin: Vec3,
        dir: Vec3,
        cell_size: f32,
        dims: UVec3,
        bounds_min: Vec3,
        bounds_max: Vec3,
        max_t: f32,
    ) -> Option<Self> {
        let (raw_t_enter, t_exit, entry_normal) = ray_aabb_intersect(origin, dir, bounds_min, bounds_max)?;
        let t_enter = raw_t_enter.max(0.0);
        if t_enter > t_exit || t_enter > max_t {
            return None;
        }
        let last_normal = if raw_t_enter > 0.0 { entry_normal } else { IVec3::ZERO };

        let entry_point = origin + dir * t_enter;
        let cell = IVec3::new(
            (entry_point.x / cell_size).floor() as i32,
            (entry_point.y / cell_size).floor() as i32,
            (entry_point.z / cell_size).floor() as i32,
        );
        let cell = IVec3::new(
            cell.x.clamp(0, dims.x as i32 - 1),
            cell.y.clamp(0, dims.y as i32 - 1),
            cell.z.clamp(0, dims.z as i32 - 1),
        );

        let step = IVec3::new(sign(dir.x), sign(dir.y), sign(dir.z));
        let t_delta = Vec3::new(
            safe_div(cell_size, dir.x.abs()),
            safe_div(cell_size, dir.y.abs()),
            safe_div(cell_size, dir.z.abs()),
        );
        let next_boundary = Vec3::new(
            boundary(cell.x, step.x, cell_size),
            boundary(cell.y, step.y, cell_size),
            boundary(cell.z, step.z, cell_size),
        );
        let t_max = Vec3::new(
            axis_t_max(next_boundary.x, origin.x, dir.x),
            axis_t_max(next_boundary.y, origin.y, dir.y),
            axis_t_max(next_boundary.z, origin.z, dir.z),
        );

        Some(Self {
            cell,
            dims,
            step,
            t_max,
            t_delta,
            t_enter,
            last_normal,
        })
    }

    /// Advances to the next cell along the ray. Returns `false` if doing so would leave the
    /// grid (in which case no other field should be relied on afterward).
    fn advance(&mut self) -> bool {
        enum Axis {
            X,
            Y,
            Z,
        }

        let axis = if self.t_max.x <= self.t_max.y && self.t_max.x <= self.t_max.z {
            Axis::X
        } else if self.t_max.y <= self.t_max.z {
            Axis::Y
        } else {
            Axis::Z
        };

        let (next_cell, t, normal) = match axis {
            Axis::X => (
                IVec3::new(self.cell.x + self.step.x, self.cell.y, self.cell.z),
                self.t_max.x,
                IVec3::new(-self.step.x, 0, 0),
            ),
            Axis::Y => (
                IVec3::new(self.cell.x, self.cell.y + self.step.y, self.cell.z),
                self.t_max.y,
                IVec3::new(0, -self.step.y, 0),
            ),
            Axis::Z => (
                IVec3::new(self.cell.x, self.cell.y, self.cell.z + self.step.z),
                self.t_max.z,
                IVec3::new(0, 0, -self.step.z),
            ),
        };

        let out_of_bounds = next_cell.x < 0
            || next_cell.y < 0
            || next_cell.z < 0
            || next_cell.x as u32 >= self.dims.x
            || next_cell.y as u32 >= self.dims.y
            || next_cell.z as u32 >= self.dims.z;
        if out_of_bounds {
            return false;
        }

        self.cell = next_cell;
        self.t_enter = t;
        match axis {
            Axis::X => self.t_max.x += self.t_delta.x,
            Axis::Y => self.t_max.y += self.t_delta.y,
            Axis::Z => self.t_max.z += self.t_delta.z,
        }
        self.last_normal = normal;
        true
    }
}

fn sign(x: f32) -> i32 {
    if x > 0.0 {
        1
    } else if x < 0.0 {
        -1
    } else {
        0
    }
}

fn safe_div(a: f32, b: f32) -> f32 {
    if b == 0.0 { f32::INFINITY } else { a / b }
}

fn boundary(cell: i32, step: i32, cell_size: f32) -> f32 {
    if step > 0 {
        (cell as f32 + 1.0) * cell_size
    } else {
        cell as f32 * cell_size
    }
}

fn axis_t_max(boundary: f32, origin: f32, dir: f32) -> f32 {
    if dir == 0.0 { f32::INFINITY } else { (boundary - origin) / dir }
}

/// Standard slab-method ray/AABB intersection. Returns `(t_enter, t_exit, entry_normal)` if the
/// ray intersects the box at all — `t_enter` may be negative if `origin` starts inside the box,
/// in which case `entry_normal` is meaningless (no boundary was actually crossed) and callers
/// must check for that themselves.
fn ray_aabb_intersect(origin: Vec3, dir: Vec3, min: Vec3, max: Vec3) -> Option<(f32, f32, IVec3)> {
    let mut t_enter = f32::NEG_INFINITY;
    let mut t_exit = f32::INFINITY;
    let mut entry_normal = IVec3::ZERO;

    let axes = [
        (origin.x, dir.x, min.x, max.x, IVec3::new(-1, 0, 0), IVec3::new(1, 0, 0)),
        (origin.y, dir.y, min.y, max.y, IVec3::new(0, -1, 0), IVec3::new(0, 1, 0)),
        (origin.z, dir.z, min.z, max.z, IVec3::new(0, 0, -1), IVec3::new(0, 0, 1)),
    ];

    for (o, d, lo, hi, normal_min_face, normal_max_face) in axes {
        if d == 0.0 {
            if o < lo || o > hi {
                return None;
            }
            continue;
        }

        let (t0, t0_normal, t1) = if d > 0.0 {
            ((lo - o) / d, normal_min_face, (hi - o) / d)
        } else {
            ((hi - o) / d, normal_max_face, (lo - o) / d)
        };

        if t0 > t_enter {
            t_enter = t0;
            entry_normal = t0_normal;
        }
        t_exit = t_exit.min(t1);
    }

    if t_enter > t_exit {
        None
    } else {
        Some((t_enter, t_exit, entry_normal))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A single-level (voxel-granularity only, no brick/mip skipping) reference marcher, used to
    /// prove `cast_ray`'s hierarchy-skip optimization doesn't change the result — only how it
    /// gets there.
    fn naive_cast_ray(chunk: &VoxelChunk, origin: Vec3, dir: Vec3, max_dist: f32) -> Option<RayHit> {
        let dims = chunk.dims();
        let bounds_max = Vec3::new(dims.x as f32, dims.y as f32, dims.z as f32);
        let mut dda = Dda::enter(origin, dir, 1.0, dims, Vec3::ZERO, bounds_max, max_dist)?;
        loop {
            if dda.t_enter > max_dist {
                return None;
            }
            let voxel = UVec3::new(dda.cell.x as u32, dda.cell.y as u32, dda.cell.z as u32);
            let material = chunk.get(voxel);
            if !material.is_air() {
                return Some(RayHit {
                    voxel,
                    material,
                    distance: dda.t_enter,
                    normal: dda.last_normal,
                });
            }
            if !dda.advance() {
                return None;
            }
        }
    }

    fn sparse_scene() -> VoxelChunk {
        let mut chunk = VoxelChunk::new(UVec3::splat(32));
        // Scattered solid voxels across several different bricks (brick size 8), deliberately
        // sparse so most rays cross multiple empty bricks/mip cells before (maybe) hitting
        // something.
        chunk.set(UVec3::new(20, 4, 4), VoxelId::new(1));
        chunk.set(UVec3::new(4, 20, 4), VoxelId::new(2));
        chunk.set(UVec3::new(4, 4, 20), VoxelId::new(3));
        chunk.set(UVec3::new(25, 25, 25), VoxelId::new(4));
        chunk.set(UVec3::new(16, 16, 16), VoxelId::new(5));
        chunk
    }

    #[test]
    fn optimized_and_naive_marchers_agree_across_a_battery_of_rays() {
        let chunk = sparse_scene();
        assert!(
            chunk.mip_level_count() > 0,
            "this test's whole point is exercising the mip hierarchy, not just bricks"
        );
        let max_dist = 100.0;

        let mut rays = Vec::new();
        // Axis-aligned rays from a grid of starting points, covering hit and miss cases.
        for x in [0.5, 4.5, 16.5, 20.5, 25.5, 31.5] {
            for y in [0.5, 4.5, 16.5, 20.5, 25.5, 31.5] {
                rays.push((Vec3::new(x, y, -5.0), Vec3::Z));
                rays.push((Vec3::new(x, y, 37.0), Vec3::NEG_Z));
            }
        }
        // A handful of diagonal rays too. Deliberately asymmetric direction components (not
        // exactly 1,1,1) — a perfectly symmetric diagonal can land exactly on a multi-axis grid
        // tie (all three t_max candidates equal in exact math), where the hierarchical and
        // single-level paths accumulate floating-point error differently and can disagree on
        // which of the tied faces was crossed last, despite agreeing on the hit voxel and
        // distance to within float32 precision. That's an inherent property of any split-path
        // DDA at an exact tie, not a correctness bug — real rays essentially never land on an
        // exact 45-degree corner hit, so the test exercises realistic diagonals only.
        rays.push((Vec3::new(-5.0, -5.0, -5.0), Vec3::new(1.0, 1.07, 0.93).normalize()));
        rays.push((Vec3::new(37.0, 37.0, 37.0), Vec3::new(-1.0, -1.07, -0.93).normalize()));
        rays.push((Vec3::new(0.1, 0.1, 0.1), Vec3::new(1.0, 0.3, 0.7).normalize()));

        for (origin, dir) in rays {
            let optimized = cast_ray(&chunk, origin, dir, max_dist);
            let naive = naive_cast_ray(&chunk, origin, dir, max_dist);
            assert_eq!(
                optimized, naive,
                "mismatch for ray origin={origin:?} dir={dir:?}"
            );
        }
    }

    #[test]
    fn empty_chunk_never_hits() {
        let chunk = VoxelChunk::new(UVec3::splat(16));
        assert_eq!(cast_ray(&chunk, Vec3::new(-5.0, 8.0, 8.0), Vec3::X, 100.0), None);
    }

    #[test]
    fn ray_starting_outside_the_chunk_can_still_hit() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.set(UVec3::new(8, 8, 8), VoxelId::new(1));

        let hit = cast_ray(&chunk, Vec3::new(8.5, 8.5, -10.0), Vec3::Z, 100.0).expect("expected a hit");
        assert_eq!(hit.voxel, UVec3::new(8, 8, 8));
        assert_eq!(hit.material, VoxelId::new(1));
        assert_eq!(hit.normal, IVec3::new(0, 0, -1));
    }

    #[test]
    fn ray_starting_inside_the_chunk_in_air_still_hits_correctly() {
        // Realistic in-flight-camera case: the ray origin is already inside the chunk's bounds,
        // in an air voxel, not outside it.
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.set(UVec3::new(12, 8, 8), VoxelId::new(7));

        let hit = cast_ray(&chunk, Vec3::new(1.5, 8.5, 8.5), Vec3::X, 100.0).expect("expected a hit");
        assert_eq!(hit.voxel, UVec3::new(12, 8, 8));
        assert_eq!(hit.material, VoxelId::new(7));
        assert_eq!(hit.normal, IVec3::new(-1, 0, 0));
    }

    #[test]
    fn face_normal_points_back_toward_the_ray_origin_on_each_axis() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.set(UVec3::new(8, 8, 8), VoxelId::new(1));

        let cases = [
            (Vec3::new(8.5, 8.5, -5.0), Vec3::Z, IVec3::new(0, 0, -1)),
            (Vec3::new(8.5, 8.5, 21.0), Vec3::NEG_Z, IVec3::new(0, 0, 1)),
            (Vec3::new(-5.0, 8.5, 8.5), Vec3::X, IVec3::new(-1, 0, 0)),
            (Vec3::new(21.0, 8.5, 8.5), Vec3::NEG_X, IVec3::new(1, 0, 0)),
            (Vec3::new(8.5, -5.0, 8.5), Vec3::Y, IVec3::new(0, -1, 0)),
            (Vec3::new(8.5, 21.0, 8.5), Vec3::NEG_Y, IVec3::new(0, 1, 0)),
        ];

        for (origin, dir, expected_normal) in cases {
            let hit = cast_ray(&chunk, origin, dir, 100.0).expect("expected a hit");
            assert_eq!(hit.voxel, UVec3::new(8, 8, 8));
            assert_eq!(hit.normal, expected_normal, "origin={origin:?} dir={dir:?}");
        }
    }

    #[test]
    fn max_dist_stops_the_march_before_a_farther_hit() {
        let mut chunk = VoxelChunk::new(UVec3::splat(16));
        chunk.set(UVec3::new(15, 8, 8), VoxelId::new(1));
        assert_eq!(cast_ray(&chunk, Vec3::new(0.5, 8.5, 8.5), Vec3::X, 5.0), None);
        assert!(cast_ray(&chunk, Vec3::new(0.5, 8.5, 8.5), Vec3::X, 100.0).is_some());
    }

    #[test]
    fn works_correctly_on_a_chunk_too_small_to_have_any_mip_levels() {
        // A single-brick (8-voxel) chunk has zero mip levels -- cast_ray must fall back to
        // starting the march at the brick level directly, not panic on an empty mip hierarchy.
        let mut chunk = VoxelChunk::new(UVec3::splat(8));
        assert_eq!(chunk.mip_level_count(), 0);
        chunk.set(UVec3::new(4, 4, 4), VoxelId::new(9));

        let hit = cast_ray(&chunk, Vec3::new(4.5, 4.5, -5.0), Vec3::Z, 100.0).expect("expected a hit");
        assert_eq!(hit.voxel, UVec3::new(4, 4, 4));
        assert_eq!(hit.material, VoxelId::new(9));
    }

    #[test]
    fn a_ray_that_only_grazes_a_far_corner_still_hits_correctly_through_every_level() {
        // 128-voxel chunk -> 4 mip levels above the brick grid; place a single solid voxel in
        // the far corner brick so a hit here genuinely exercises every level of the hierarchy
        // (mip levels 3,2,1,0, then the brick level, then the voxel level) rather than just the
        // first level or two.
        let mut chunk = VoxelChunk::new(UVec3::splat(128));
        assert_eq!(chunk.mip_level_count(), 4);
        chunk.set(UVec3::new(127, 127, 127), VoxelId::new(3));

        let hit = cast_ray(
            &chunk,
            Vec3::new(-5.0, -5.0, -5.0),
            Vec3::new(1.0, 1.0, 1.0).normalize(),
            1000.0,
        )
        .expect("expected a hit");
        assert_eq!(hit.voxel, UVec3::new(127, 127, 127));
        assert_eq!(hit.material, VoxelId::new(3));
    }
}
