//! Headless, GPU-free benchmark comparing `cast_ray` throughput on the sparse vs. dense test
//! scenes (the same ones `voxel_scene.rs` renders). Run with
//! `cargo run --release --example benchmark_raymarch` (release mode matters a great deal here --
//! this is exactly the kind of tight numeric loop debug builds make look far slower than it
//! actually is).
//!
//! **Real measured result, worth understanding rather than assuming**: the dense scene is
//! roughly 2x *faster* than the sparse one (0.51x on one measured run: 536 ns/ray sparse vs.
//! 271 ns/ray dense), the opposite of what "the occupancy skip should help the sparse scene most"
//! naively predicts. Why: these particular scenes differ enormously in hit RATE, and hit/miss
//! resolution cost dominates this measurement far more than occupancy-skip effectiveness does.
//! The sparse scene's rays mostly MISS entirely (few small spheres in a big empty volume) --
//! a miss means marching the full chunk depth through every hierarchy level before concluding
//! "nothing here," the single most expensive outcome for any DDA marcher. The dense scene's rays
//! almost all HIT within the first few voxels (it's mostly solid, entered from outside) -- an
//! early hit is cheap almost regardless of how the interior is structured. This benchmark is a
//! realistic end-to-end comparison, not a controlled isolation of the occupancy-skip optimization
//! specifically -- that would need either a same-hit-rate pair of scenes, or a direct
//! `cast_ray` vs. brute-force-per-voxel comparison on the SAME scene (not implemented here --
//! see the engine's own mip-hierarchy memory/notes for why that's a real duplication-risk
//! tradeoff against reusing the DDA internals, not just an oversight).

#[path = "common/mod.rs"]
mod common;

use std::time::Instant;

use bevy::math::Vec3;
use voxel_engine::cast_ray;

const RAYS_PER_AXIS: u32 = 128;
const RAY_MAX_DIST: f32 = 300.0;

fn main() {
    let sparse = common::build_sparse_chunk();
    let dense = common::build_dense_chunk();

    let sparse_result = benchmark("sparse", &sparse);
    let dense_result = benchmark("dense", &dense);

    println!();
    println!(
        "dense/sparse: {:.2}x slower on the dense scene (same {} rays, same chunk size)",
        dense_result.avg_nanos_per_ray / sparse_result.avg_nanos_per_ray,
        RAYS_PER_AXIS * RAYS_PER_AXIS,
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
