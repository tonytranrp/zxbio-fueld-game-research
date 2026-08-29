//! Opaque-handle packing: turns a Rapier-style (index, generation) pair into
//! the single u64 a cxx boundary hands to C++, and back. Used by `physics/`
//! today; `game/` reuses the exact same scheme for its own entity handles
//! rather than reinventing it.

// Offset by +1/-1 so a raw value of 0 can never be produced by a real
// (index, generation) pair -- 0 is reserved project-wide as the
// always-invalid handle sentinel (see physics/README.md's coding
// standards). Without the offset, the legitimate first-ever allocated slot
// (index 0, generation 0) would pack to exactly 0 and be indistinguishable
// from "no handle".
//
// wrapping_add, not `+ 1`: the single combination (index=u32::MAX,
// generation=u32::MAX) would otherwise overflow u64 (panicking in debug,
// silently wrapping in release) since it's already u64::MAX before the
// offset. wrapping_add makes that one combination pack to 0 instead --
// indistinguishable from "invalid" rather than a crash. Same assumption
// unpack_handle already documents below: no real Rapier allocation ever
// reaches generation AND index both at u32::MAX simultaneously.
pub fn pack_handle(index: u32, generation: u32) -> u64 {
    (((generation as u64) << 32) | index as u64).wrapping_add(1)
}

// raw == 0 is the invalid-handle sentinel (see pack_handle above) -- rather
// than threading an Option/error through every caller, it unpacks to
// (u32::MAX, u32::MAX): an (index, generation) pair no real allocation can
// ever produce, so every downstream `.get(handle)` lookup naturally returns
// None through its own existing Option-returning path with no separate
// zero-check needed at each call site.
pub fn unpack_handle(raw: u64) -> (u32, u32) {
    if raw == 0 {
        return (u32::MAX, u32::MAX);
    }
    let adjusted = raw - 1;
    (adjusted as u32, (adjusted >> 32) as u32)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn zero_is_reserved_and_round_trips_as_invalid() {
        assert_eq!(unpack_handle(0), (u32::MAX, u32::MAX));
    }

    #[test]
    fn first_slot_does_not_pack_to_zero() {
        assert_ne!(pack_handle(0, 0), 0);
    }

    #[test]
    fn pack_unpack_round_trips() {
        for (index, generation) in [(0u32, 0u32), (1, 0), (0, 1), (12345, 6789), (u32::MAX, 0), (0, u32::MAX)] {
            let packed = pack_handle(index, generation);
            assert_ne!(packed, 0);
            assert_eq!(unpack_handle(packed), (index, generation));
        }
    }

    #[test]
    fn max_index_and_generation_together_degrades_to_invalid_not_panic() {
        // The one (index, generation) pair that would overflow u64 pre-offset --
        // deliberately degrades to the invalid sentinel rather than panicking or
        // silently wrapping into another real handle's value.
        assert_eq!(pack_handle(u32::MAX, u32::MAX), 0);
    }
}
