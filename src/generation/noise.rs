//! 3D Perlin noise (Ken Perlin's 2002 "Improved Noise" formulation: 12 cube-edge gradients, the
//! `6t^5 - 15t^4 + 10t^3` fade curve).
//!
//! Hand-rolled rather than depending on the `noise` crate — deliberately, so this stays a plain
//! scalar function this engine fully controls the shape of. The engine's own SIMD research
//! identified procedural noise evaluation as the one clearly-proven SIMD win in a voxel engine's
//! CPU-side pipeline (`pulp`-based, once this scalar version is validated) — a generic external
//! crate's implementation isn't shaped for that swap, and pulling one in now only to replace it
//! later is a real dependency cost for zero lasting benefit (see the library-creation skill's own
//! "every dependency is a cost every consumer pays" rule).

/// A seeded 3D Perlin noise generator. Cheap to construct (one 256-entry shuffle); clone freely
/// if you need one per thread.
#[derive(Debug, Clone)]
pub struct PerlinNoise {
    /// 512 entries: the classic 256-entry permutation table, duplicated once, so every index
    /// used during sampling (`p[i]`, `p[i + 1]` for `i` up to 255) stays in bounds without extra
    /// wraparound arithmetic at each use site.
    permutation: [u8; 512],
}

impl PerlinNoise {
    /// Builds a noise generator from `seed` — the same seed always produces the same noise field
    /// (see the `deterministic_for_a_given_seed` test), and different seeds produce visibly
    /// different fields (see `different_seeds_produce_different_noise`).
    pub fn new(seed: u64) -> Self {
        let mut table: [u8; 256] = [0; 256];
        for (i, slot) in table.iter_mut().enumerate() {
            *slot = i as u8;
        }

        let mut rng = SplitMix64::new(seed);
        for i in (1..256).rev() {
            let j = (rng.next() % (i as u64 + 1)) as usize;
            table.swap(i, j);
        }

        let mut permutation = [0u8; 512];
        for (i, slot) in permutation.iter_mut().enumerate() {
            *slot = table[i % 256];
        }

        Self { permutation }
    }

    /// Samples the noise field at `(x, y, z)`. Bounded within roughly `[-1, 1]` (the exact bound
    /// depends on which of the 12 gradient directions land at the sample point; see the
    /// `stays_within_a_safe_bound` test for the empirically-checked range this implementation
    /// actually produces). Exactly `0.0` at every integer lattice point, by construction — a
    /// well-known, provable property of this algorithm (see `zero_at_every_integer_lattice_point`),
    /// not a coincidence of this specific implementation.
    pub fn sample(&self, x: f32, y: f32, z: f32) -> f32 {
        let xi = x.floor();
        let yi = y.floor();
        let zi = z.floor();

        let xf = x - xi;
        let yf = y - yi;
        let zf = z - zi;

        let u = fade(xf);
        let v = fade(yf);
        let w = fade(zf);

        let xi = (xi as i32 & 255) as usize;
        let yi = (yi as i32 & 255) as usize;
        let zi = (zi as i32 & 255) as usize;

        let p = &self.permutation;
        let a = p[xi] as usize + yi;
        let aa = p[a] as usize + zi;
        let ab = p[a + 1] as usize + zi;
        let b = p[xi + 1] as usize + yi;
        let ba = p[b] as usize + zi;
        let bb = p[b + 1] as usize + zi;

        lerp(
            w,
            lerp(
                v,
                lerp(
                    u,
                    grad(p[aa], xf, yf, zf),
                    grad(p[ba], xf - 1.0, yf, zf),
                ),
                lerp(
                    u,
                    grad(p[ab], xf, yf - 1.0, zf),
                    grad(p[bb], xf - 1.0, yf - 1.0, zf),
                ),
            ),
            lerp(
                v,
                lerp(
                    u,
                    grad(p[aa + 1], xf, yf, zf - 1.0),
                    grad(p[ba + 1], xf - 1.0, yf, zf - 1.0),
                ),
                lerp(
                    u,
                    grad(p[ab + 1], xf, yf - 1.0, zf - 1.0),
                    grad(p[bb + 1], xf - 1.0, yf - 1.0, zf - 1.0),
                ),
            ),
        )
    }

    /// Fractal Brownian motion: sums `octaves` layers of this noise field at doubling frequency
    /// and halving amplitude each octave (the standard fBm construction), normalized back to
    /// roughly the same `[-1, 1]` range a single [`Self::sample`] call has. More octaves add
    /// finer detail on top of the same broad shape, at a proportional cost in samples taken.
    pub fn sample_fbm(&self, x: f32, y: f32, z: f32, octaves: u32) -> f32 {
        let mut total = 0.0;
        let mut amplitude = 1.0;
        let mut frequency = 1.0;
        let mut amplitude_sum = 0.0;

        for _ in 0..octaves.max(1) {
            total += self.sample(x * frequency, y * frequency, z * frequency) * amplitude;
            amplitude_sum += amplitude;
            amplitude *= 0.5;
            frequency *= 2.0;
        }

        total / amplitude_sum
    }
}

fn fade(t: f32) -> f32 {
    t * t * t * (t * (t * 6.0 - 15.0) + 10.0)
}

fn lerp(t: f32, a: f32, b: f32) -> f32 {
    a + t * (b - a)
}

/// One of the 12 cube-edge-midpoint gradient directions, selected by the low 4 bits of `hash`
/// (Perlin's own "improved noise" gradient set — chosen because it avoids the directional bias
/// of naive random gradients while being cheap to evaluate: each direction is `(+-1, +-1, 0)` in
/// some axis permutation, so `grad` is a couple of sign-selected additions, no actual dot-product
/// multiplication needed).
fn grad(hash: u8, x: f32, y: f32, z: f32) -> f32 {
    let h = hash & 15;
    let u = if h < 8 { x } else { y };
    let v = if h < 4 {
        y
    } else if h == 12 || h == 14 {
        x
    } else {
        z
    };
    (if h & 1 == 0 { u } else { -u }) + (if h & 2 == 0 { v } else { -v })
}

/// SplitMix64 (Sebastiano Vigna, public domain) — a small, fast, well-distributed PRNG used only
/// to shuffle the permutation table once at construction; not used anywhere performance-
/// sensitive, so simplicity was preferred over speed here.
struct SplitMix64(u64);

impl SplitMix64 {
    fn new(seed: u64) -> Self {
        Self(seed)
    }

    fn next(&mut self) -> u64 {
        self.0 = self.0.wrapping_add(0x9E3779B97F4A7C15);
        let mut z = self.0;
        z = (z ^ (z >> 30)).wrapping_mul(0xBF58476D1CE4E5B9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94D049BB133111EB);
        z ^ (z >> 31)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn deterministic_for_a_given_seed() {
        let a = PerlinNoise::new(42);
        let b = PerlinNoise::new(42);
        for i in 0..20 {
            let x = i as f32 * 0.37;
            assert_eq!(a.sample(x, x * 1.3, x * 0.7), b.sample(x, x * 1.3, x * 0.7));
        }
    }

    #[test]
    fn different_seeds_produce_different_noise() {
        let a = PerlinNoise::new(1);
        let b = PerlinNoise::new(2);
        let mut any_different = false;
        for i in 0..20 {
            let x = i as f32 * 0.53;
            if (a.sample(x, 1.0, 2.0) - b.sample(x, 1.0, 2.0)).abs() > 1e-6 {
                any_different = true;
                break;
            }
        }
        assert!(any_different, "different seeds should not produce identical noise fields");
    }

    #[test]
    fn zero_at_every_integer_lattice_point() {
        // A well-known, provable property of this exact algorithm (fade(0)=0 collapses every
        // interpolation to the corner gradient's own dot product, and grad(_, 0, 0, 0) is always
        // exactly 0 regardless of which gradient direction the hash selects) -- not something
        // that could pass by coincidence if the implementation were subtly wrong.
        let noise = PerlinNoise::new(7);
        for x in -3..=3 {
            for y in -3..=3 {
                for z in -3..=3 {
                    let value = noise.sample(x as f32, y as f32, z as f32);
                    assert!(
                        value.abs() < 1e-5,
                        "expected ~0 at integer lattice point ({x},{y},{z}), got {value}"
                    );
                }
            }
        }
    }

    #[test]
    fn stays_within_a_safe_bound() {
        let noise = PerlinNoise::new(123);
        let mut min = f32::INFINITY;
        let mut max = f32::NEG_INFINITY;
        let mut i = 0.0f32;
        while i < 40.0 {
            let mut j = 0.0f32;
            while j < 40.0 {
                let value = noise.sample(i * 0.13, 5.0, j * 0.17);
                min = min.min(value);
                max = max.max(value);
                j += 0.7;
            }
            i += 0.7;
        }
        assert!(min > -1.5 && max < 1.5, "noise range [{min}, {max}] outside the expected safe bound");
    }

    #[test]
    fn nearby_samples_are_close_not_discontinuous() {
        let noise = PerlinNoise::new(99);
        let base = noise.sample(3.14, 2.71, 1.41);
        let nudged = noise.sample(3.14 + 0.001, 2.71, 1.41);
        assert!(
            (base - nudged).abs() < 0.05,
            "a tiny input change should not produce a large output jump: {base} vs {nudged}"
        );
    }

    #[test]
    fn fbm_with_one_octave_matches_plain_sample() {
        let noise = PerlinNoise::new(5);
        let (x, y, z) = (1.5, 2.5, 3.5);
        assert_eq!(noise.sample_fbm(x, y, z, 1), noise.sample(x, y, z));
    }

    #[test]
    fn fbm_stays_within_a_safe_bound_too() {
        let noise = PerlinNoise::new(321);
        let mut min = f32::INFINITY;
        let mut max = f32::NEG_INFINITY;
        let mut i = 0.0f32;
        while i < 20.0 {
            let value = noise.sample_fbm(i * 0.1, 0.0, i * 0.05, 4);
            min = min.min(value);
            max = max.max(value);
            i += 0.3;
        }
        assert!(min > -1.5 && max < 1.5, "fBm range [{min}, {max}] outside the expected safe bound");
    }
}
