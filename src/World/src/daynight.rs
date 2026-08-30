//! A real day/night cycle -- the "natural future feed for CropGrowth's
//! own light input (time-of-day/sun-angle instead of a fixed 1.0/0.35)"
//! `session.rs`'s own DirectionalLight spawn comment named as unbuilt
//! since this game's very first crop iteration. Sweeps a single
//! `DirectionalLight` ("sun") across the sky, scales its illuminance and
//! the world camera's own clear (sky) color together, and feeds the
//! resulting brightness into `crop.rs`'s own Liebig's-law `light` term --
//! real crops don't photosynthesize in the dark, so growth genuinely
//! slows (not halts, see `NIGHT_AMBIENT_LIGHT` below) at night, the same
//! kind of real, not-smoothed-over consequence this game's other systems
//! already hold themselves to.
//!
//! The day/night SHAPE is a standard, well-known simplification (solar
//! elevation roughly following a sine curve from sunrise to sunset, zero
//! net direct sunlight at night) -- not a "surprising research finding"
//! requiring its own citation the way e.g. hydrogen.rs's grid-emissions
//! figure did, more a geometric modeling convention shared by countless
//! games/sims. `DAY_LENGTH_SECONDS` is, like every other pacing constant
//! in this game (corn.rs's own ~20s grow cycle, solar.rs's own 45s
//! payback period), a deliberate game-legible choice, not a literal
//! real-time-of-day conversion.
//!
//! `NIGHT_AMBIENT_LIGHT` (a small nonzero floor rather than a hard zero)
//! is explicitly a GAME-LEGIBILITY choice, not a physically rigorous one
//! -- real C3 crop photosynthesis from moonlight/starlight alone is
//! negligible, but a true zero would mean crop growth completely stalls
//! for half of every cycle, which reads as "the game stopped working"
//! rather than "it's night" during actual play. Said plainly here rather
//! than implied as more rigorous than it is.
#![forbid(unsafe_code)]

use crate::session::{CLEAR_COLOR, DeltaSeconds, WorldCamera};
use bevy::app::{App, Update};
use bevy::camera::{Camera, ClearColorConfig};
use bevy::color::Color;
use bevy::light::DirectionalLight;
use bevy::math::Vec3;
use bevy::prelude::{Query, Res, ResMut, Resource, Transform, With};

#[derive(Resource, Default)]
pub(crate) struct DayNightCycle {
    elapsed_seconds: f32,
}

impl DayNightCycle {
    pub(crate) fn light_level(&self) -> f32 {
        light_level(self.elapsed_seconds, DAY_LENGTH_SECONDS)
    }

    // A direct, unambiguous day/night check on the RAW phase -- not
    // derived from light_level() itself, since light_level() clamps its
    // own near-edge daytime values (just after sunrise, just before
    // sunset) up to the same NIGHT_AMBIENT_LIGHT floor genuine nighttime
    // also reads at, making the two indistinguishable from that value
    // alone. hud.rs's own Day/Night label needs the real answer, not an
    // approximation of it.
    pub(crate) fn is_daytime(&self) -> bool {
        is_daytime(self.elapsed_seconds, DAY_LENGTH_SECONDS)
    }
}

fn is_daytime(elapsed_seconds: f32, day_length_seconds: f32) -> bool {
    let cycle_length = day_length_seconds * 2.0;
    elapsed_seconds.rem_euclid(cycle_length) < day_length_seconds
}

// See this module's own doc comment: a game-pacing choice, not a literal
// real-day conversion. Day and night are equal length (a full cycle is
// twice this).
const DAY_LENGTH_SECONDS: f32 = 30.0;

// See this module's own doc comment on NIGHT_AMBIENT_LIGHT's own honesty
// caveat.
const NIGHT_AMBIENT_LIGHT: f32 = 0.05;

const NOON_ILLUMINANCE: f32 = 6000.0; // this game's own prior fixed value, now the daytime PEAK instead of a constant
const NIGHT_SKY_COLOR: Color = Color::srgb(0.02, 0.03, 0.08);

// Fraction of "sun brightness," 0.0 (full night) .. 1.0 (solar noon).
// Pulled out as a pure function so the curve shape is directly
// unit-testable without a Bevy World, the same convention this
// codebase's own crop.rs (sway_angle) and fuel.rs (puff_scale) already
// establish.
fn light_level(elapsed_seconds: f32, day_length_seconds: f32) -> f32 {
    let cycle_length = day_length_seconds * 2.0;
    let t = elapsed_seconds.rem_euclid(cycle_length);
    if t >= day_length_seconds {
        return NIGHT_AMBIENT_LIGHT;
    }
    let day_phase = t / day_length_seconds; // 0.0 (sunrise) .. 1.0 (sunset)
    (day_phase * std::f32::consts::PI).sin().max(NIGHT_AMBIENT_LIGHT)
}

// The sun's own look-at target for a given point in the DAYTIME portion
// of the cycle -- azimuth sweeps east to west across the whole day,
// elevation follows the same low-at-sunrise/sunset, high-at-noon shape
// light_level() itself already uses. Not called at all at night (see
// update_day_night below) -- direction is visually irrelevant once
// illuminance has already dropped to NIGHT_AMBIENT_LIGHT's own
// near-nothing level, so there's nothing to gain from computing it.
fn sun_look_target(day_phase: f32) -> Vec3 {
    let azimuth = std::f32::consts::PI * day_phase;
    let elevation = (day_phase * std::f32::consts::PI).sin().max(0.05);
    Vec3::new(azimuth.cos(), -elevation, azimuth.sin())
}

pub(crate) fn setup(app: &mut App) {
    app.world_mut().spawn((
        DirectionalLight {
            illuminance: NOON_ILLUMINANCE,
            ..Default::default()
        },
        Transform::from_xyz(0.0, 10.0, 0.0).looking_at(Vec3::new(0.3, 0.0, 0.5), Vec3::Y),
    ));

    app.insert_resource(DayNightCycle::default());
    app.add_systems(Update, update_day_night);
}

fn update_day_night(
    dt: Res<DeltaSeconds>,
    mut cycle: ResMut<DayNightCycle>,
    mut sun: Query<(&mut DirectionalLight, &mut Transform)>,
    mut world_camera: Query<&mut Camera, With<WorldCamera>>,
) {
    cycle.elapsed_seconds += dt.0;
    let level = light_level(cycle.elapsed_seconds, DAY_LENGTH_SECONDS);

    if let Ok((mut light, mut transform)) = sun.single_mut() {
        light.illuminance = NOON_ILLUMINANCE * level;
        let cycle_length = DAY_LENGTH_SECONDS * 2.0;
        let t = cycle.elapsed_seconds.rem_euclid(cycle_length);
        if t < DAY_LENGTH_SECONDS {
            let day_phase = t / DAY_LENGTH_SECONDS;
            *transform = Transform::from_xyz(0.0, 10.0, 0.0).looking_at(sun_look_target(day_phase), Vec3::Y);
        }
    }

    if let Ok(mut camera) = world_camera.single_mut() {
        let Color::Srgba(day) = CLEAR_COLOR else {
            return;
        };
        let Color::Srgba(night) = NIGHT_SKY_COLOR else {
            return;
        };
        let day_vec = Vec3::new(day.red, day.green, day.blue);
        let night_vec = Vec3::new(night.red, night.green, night.blue);
        let mixed = night_vec.lerp(day_vec, level);
        camera.clear_color = ClearColorConfig::Custom(Color::srgb(mixed.x, mixed.y, mixed.z));
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn light_peaks_at_solar_noon_and_drops_to_the_ambient_floor_at_night() {
        let day_length = 30.0;
        let sunrise = light_level(0.0, day_length);
        let noon = light_level(day_length / 2.0, day_length);
        let midnight = light_level(day_length * 1.5, day_length);

        assert!((sunrise - NIGHT_AMBIENT_LIGHT).abs() < 1.0e-5, "sunrise itself should sit at (or just above) the ambient floor, not full brightness");
        assert!((noon - 1.0).abs() < 1.0e-5, "solar noon should reach exactly full brightness");
        assert!((midnight - NIGHT_AMBIENT_LIGHT).abs() < 1.0e-6, "the middle of the night should sit exactly at the ambient floor, not drop below it");
    }

    #[test]
    fn light_level_repeats_every_full_cycle() {
        let day_length = 30.0;
        let cycle_length = day_length * 2.0;
        let early = light_level(5.0, day_length);
        let one_cycle_later = light_level(5.0 + cycle_length, day_length);
        let two_cycles_later = light_level(5.0 + cycle_length * 2.0, day_length);

        assert!((early - one_cycle_later).abs() < 1.0e-4, "the exact same point in a later cycle should reproduce the same light level");
        assert!((early - two_cycles_later).abs() < 1.0e-4, "the cycle should keep repeating indefinitely, not drift");
    }

    #[test]
    fn is_daytime_distinguishes_dim_dawn_from_genuine_night_even_though_light_level_cannot() {
        let day_length = 30.0;
        // A moment just after sunrise: light_level() itself clamps this
        // near-zero sin() value up to the same NIGHT_AMBIENT_LIGHT floor
        // genuine night reads at (see this module's own doc comment on
        // is_daytime()) -- the whole reason this separate check exists.
        let just_after_sunrise = 0.01;
        let deep_night = day_length * 1.5;

        assert!(is_daytime(just_after_sunrise, day_length), "a moment just after sunrise is still daytime, even if its own light_level() reads at the same floor value night does");
        assert!(!is_daytime(deep_night, day_length), "the middle of the night should read as night");
    }
}
