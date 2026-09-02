//! Voxel material identifiers and the palette mapping them to render info.

use bevy::color::LinearRgba;

/// A single voxel's material identifier. `0` (see [`VoxelId::AIR`]) means empty space.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct VoxelId(pub u8);

impl VoxelId {
    /// The empty/air voxel — the default value of every newly-created [`VoxelChunk`] cell.
    ///
    /// [`VoxelChunk`]: crate::storage::VoxelChunk
    pub const AIR: VoxelId = VoxelId(0);

    /// Creates a voxel ID for the given material index. `0` is reserved for [`VoxelId::AIR`].
    pub const fn new(material: u8) -> Self {
        VoxelId(material)
    }

    /// Returns `true` if this is [`VoxelId::AIR`].
    pub const fn is_air(self) -> bool {
        self.0 == 0
    }
}

/// Render info for one voxel material — currently just a color; roughness/emissive are natural
/// additions once the shading model grows past flat Lambertian.
#[derive(Debug, Clone, Copy)]
pub struct VoxelMaterialInfo {
    pub color: LinearRgba,
}

impl Default for VoxelMaterialInfo {
    fn default() -> Self {
        Self {
            color: LinearRgba::WHITE,
        }
    }
}

/// Maps every possible [`VoxelId`] (0-255) to its [`VoxelMaterialInfo`]. Unset entries default
/// to white, so an un-palette-mapped material ID still renders as visible geometry rather than
/// silently invisible/black.
#[derive(Debug, Clone)]
pub struct VoxelPalette {
    entries: [VoxelMaterialInfo; 256],
}

impl VoxelPalette {
    /// Creates a palette where every material ID defaults to [`VoxelMaterialInfo::default`].
    pub fn new() -> Self {
        Self {
            entries: [VoxelMaterialInfo::default(); 256],
        }
    }

    /// Sets the render info for `id`.
    pub fn set(&mut self, id: VoxelId, info: VoxelMaterialInfo) {
        self.entries[id.0 as usize] = info;
    }

    /// Returns the render info for `id`.
    pub fn get(&self, id: VoxelId) -> VoxelMaterialInfo {
        self.entries[id.0 as usize]
    }
}

impl Default for VoxelPalette {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn air_is_zero_and_reports_as_air() {
        assert_eq!(VoxelId::AIR, VoxelId::new(0));
        assert_eq!(VoxelId::AIR, VoxelId::default());
        assert!(VoxelId::AIR.is_air());
        assert!(!VoxelId::new(1).is_air());
    }

    #[test]
    fn palette_defaults_every_id_to_white_and_stores_overrides() {
        let mut palette = VoxelPalette::new();
        assert_eq!(palette.get(VoxelId::new(200)).color, LinearRgba::WHITE);

        let red = LinearRgba::new(1.0, 0.0, 0.0, 1.0);
        palette.set(VoxelId::new(1), VoxelMaterialInfo { color: red });
        assert_eq!(palette.get(VoxelId::new(1)).color, red);
        // Unrelated entries stay untouched.
        assert_eq!(palette.get(VoxelId::new(2)).color, LinearRgba::WHITE);
    }
}
