//! Headless, GPU-free benchmark comparing `cast_ray` throughput on the sparse, dense, and
//! procedurally-generated terrain test scenes (the same ones `voxel_scene.rs` renders). Run with
//! `cargo run --release --example benchmark_raymarch` (release mode matters a great deal here --
//! this is exactly the kind of tight numeric loop debug builds make look far slower than it
//! actually is).
//!
//! **Real measured results, worth understanding rather than assuming.** Sparse is by far the
//! slowest of the three scenes (dense and terrain both run at roughly 0.57-0.58x sparse's time --
//! i.e. dense/terrain are ~1.7x faster), the opposite of what "the occupancy skip should help the
//! sparse scene most" naively predicts.
//!
//! A first pass at explaining this (see git history on this file) chalked it up entirely to hit
//! RATE: sparse's rays mostly miss (a handful of small spheres in a big empty volume), and a
//! miss means marching the full chunk depth through every hierarchy level before concluding
//! "nothing here" -- the most expensive outcome for any DDA marcher -- while dense's rays almost
//! all hit within the first few voxels. That's real, but adding the terrain scene as a third data
//! point shows it's incomplete: terrain hits only ~35% of the time (much closer to sparse's ~15%
//! than to dense's 100%), yet runs at essentially the SAME speed as the 100%-hit dense scene
//! (208.7 vs. 210.4 ns/ray on one measured run) -- a pure hit-rate model would predict terrain
//! landing much closer to sparse.
//!
//! The more complete (though not independently, controlled-experiment verified -- this is an
//! inference from the pattern across three scenes, not an isolated test of this one variable)
//! explanation: what actually seems to matter is less "what fraction of rays hit" and more
//! "how CONTIGUOUS the empty space is." Terrain's empty region (the open sky above the ground) is
//! one large, uniform block spanning the whole X/Z footprint -- the mip hierarchy can skip through
//! it almost as efficiently as it does the fully-empty chunk in the mip-hierarchy ablation
//! benchmark (`src/raymarch.rs`'s own `#[ignore]`d test), so even terrain's MISSING rays resolve
//! cheaply. Sparse's empty space is fragmented into many separate pockets by the scattered small
//! spheres, which breaks up exactly that large-scale skip the hierarchy is built for, even though
//! sparse is "more empty" by raw volume than terrain is. Worth remembering when reasoning about
//! this engine's real-world performance: raw occupancy percentage is a weaker predictor than the
//! SHAPE of the empty space.
//!
//! This benchmark remains a realistic end-to-end comparison, not a controlled isolation of the
//! occupancy-skip optimization specifically -- that would need either a same-hit-rate,
//! same-contiguity pair of scenes, or a direct `cast_ray` vs. brute-force-per-voxel comparison on
//! the SAME scene (not implemented here as a separate example -- see the engine's own
//! mip-hierarchy memory/notes for why that's a real duplication-risk tradeoff against reusing the
//! DDA internals, and for the controlled fully-empty-chunk ablation that IS implemented, as a
//! test rather than an example).

// common/mod.rs is shared across three examples now (this one, voxel_scene, voxel_world), each
// using a different subset -- this headless benchmark has no App/render loop, so it never calls
// common::log_fps_once_per_second. `#[allow(dead_code)]` at the inclusion site says that's
// expected for a shared support module, without hiding genuine dead code inside common/mod.rs
// itself if a helper ever stops being used by ANY example.
#[path = "common/mod.rs"]
#[allow(dead_code)]
mod common;

use std::time::Instant;

use bevy::math::{IVec3, Vec3};
use voxel_engine::cast_ray;

const RAYS_PER_AXIS: u32 = 128;
const RAY_MAX_DIST: f32 = 300.0;

fn main() {
    let sparse = common::build_sparse_chunk();
    let dense = common::build_dense_chunk();
    let terrain = common::build_terrain_chunk(1, IVec3::ZERO);

    let sparse_result = benchmark("sparse", &sparse);
    let dense_result = benchmark("dense", &dense);
    let terrain_result = benchmark("terrain", &terrain);

    println!();
    println!(
        "dense/sparse:   {:.2}x (same {} rays, same chunk size)",
        dense_result.avg_nanos_per_ray / sparse_result.avg_nanos_per_ray,
        RAYS_PER_AXIS * RAYS_PER_AXIS,
    );
    println!(
        "terrain/sparse: {:.2}x",
        terrain_result.avg_nanos_per_ray / sparse_result.avg_nanos_per_ray,
    );
    println!(
        "terrain/dense:  {:.2}x",
        terrain_result.avg_nanos_per_ray / dense_result.avg_nanos_per_ray,
    );
}

struct BenchResult {
    avg_nanos_per_ray: f64,
}

fn benchmark(name: &str, chunk: &voxel_engine::VoxelChunk) -> BenchResult {
    let rays = generate_rays();

    // Warm up (branch predictor, cache) before the timed pass.
    for &(origin, dir) in &rays {
        std::hint::black_box(cast_ray(chunk, origin, dir, RAY_MAX_DIST));
    }

    let start = Instant::now();
    let mut hits = 0u32;
    for &(origin, dir) in &rays {
        if std::hint::black_box(cast_ray(chunk, origin, dir, RAY_MAX_DIST)).is_some() {
            hits += 1;
        }
    }
    let elapsed = start.elapsed();

    let avg_nanos_per_ray = elapsed.as_nanos() as f64 / rays.len() as f64;
    println!(
        "{name:>6}: {} rays in {elapsed:?}  ({avg_nanos_per_ray:.1} ns/ray avg, {:.1}% hit, {:.2}M rays/sec)",
        rays.len(),
        100.0 * hits as f32 / rays.len() as f32,
        1000.0 / avg_nanos_per_ray,
    );

    BenchResult { avg_nanos_per_ray }
}

/// A grid of parallel rays entering from outside the chunk on -Z, covering the whole X/Y
/// footprint -- deterministic and reproducible (not random), so results are directly comparable
/// run to run and across commits.
fn generate_rays() -> Vec<(Vec3, Vec3)> {
    let chunk_voxels = common::CHUNK_VOXELS as f32;
    let mut rays = Vec::with_capacity((RAYS_PER_AXIS * RAYS_PER_AXIS) as usize);
    for xi in 0..RAYS_PER_AXIS {
        for yi in 0..RAYS_PER_AXIS {
            let x = (xi as f32 + 0.5) / RAYS_PER_AXIS as f32 * chunk_voxels;
            let y = (yi as f32 + 0.5) / RAYS_PER_AXIS as f32 * chunk_voxels;
            rays.push((Vec3::new(x, y, -10.0), Vec3::Z));
        }
    }
    rays
}
