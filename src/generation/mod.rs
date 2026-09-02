//! Procedural content generation: noise fields and terrain built from them, replacing
//! hand-authored/hand-stamped test scenes with something that can actually approach the scale
//! this engine targets.

mod noise;
mod terrain;

pub use noise::PerlinNoise;
pub use terrain::{fill_heightmap_terrain, HeightmapParams};
