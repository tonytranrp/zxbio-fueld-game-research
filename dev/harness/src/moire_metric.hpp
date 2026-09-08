#pragma once

// Prompt 005 goal 276: a number that separates WANTED texture from UNWANTED aliasing.
//
// WHY THE EXISTING METRIC CANNOT DO THIS JOB. `--verify-frame`'s local-contrast metric answers
// "is there a scene at all" by counting pixels whose neighbour delta exceeds 4/255. It reads 34.7%
// on the current svo path. But a fine deliberate stipple and a field of ring moire BOTH raise that
// number -- one is the look the owner asked for and the other is the artefact he complained about,
// and a metric that cannot tell them apart cannot judge this pass.
//
// WHAT THIS MEASURES, and how it was arrived at. Two hypotheses were tried and the first was
// REFUTED before this one was written (research/fine-grain-look-log.md has the printouts):
//
//   REFUTED -- "ring moire is a low-frequency ENVELOPE modulating a high-frequency carrier, so
//   measure the envelope's structure." The two radial spectra are nearly identical: the target
//   capture and the ringed frame both put ~24% of their envelope energy at 64-256 px periods, and
//   the ringed frame has LESS mid-band envelope energy, not more. An apparent 3.9x separation in an
//   intermediate version turned out to be a peak-finder locking onto the analysis band's own edge.
//
//   WHAT THE SAME PRINTOUT SHOWED INSTEAD, which is the textbook signature rather than an invention:
//
//       radial period    target capture    ringed frame
//       2-4 px               54.3%            91.5%
//       4-8 px               41.9%             7.7%
//
//   The target's high-frequency energy is SPREAD across 2-8 px. The aliased frame's is 91.5%
//   crammed against the pixel Nyquist limit. That is what aliasing IS -- signal energy folded up at
//   the sampling rate -- while a deliberate stipple sits at its OWN frequency with a roll-off either
//   side. So:
//
//       moire ratio = E(period 2-4 px) / E(period 4-8 px), over textured pixels only
//
// NO FFT. The two bands are separated with a difference-of-box filter bank rather than a transform:
// `L - box3(L)` passes roughly 2-4 px and `box3(L) - box7(L)` roughly 4-8 px. Validated against the
// FFT form on eleven images -- it reproduces the separation (9.5x vs 9.2x) without the transform,
// and a box blur is a prefix sum, so this costs a few passes over the frame.
//
// VALIDATED, not asserted. The numbers are in the log and reproduced in the unit test:
//   * known-good `lin_water_checkerboard_after.png` (the owner's target) -> 1.14
//   * known-bad `svo_ground_hilltop.png` (the frame he complained about)  -> 10.88
//   * SEPARATION 9.5x
//   * synthetics whose answer is known by construction: band-limited noise centred on a 6 px radial
//     period (a wanted stipple) -> 1.19, on 4 px -> 5.84, on 2.3 px (aliasing) -> 34.88, white noise
//     -> 9.71. The target capture's 1.14 lands on the 6 px synthetic's 1.19, which independently
//     says the target's stipple has a ~6 px radial period.
//   * it responds to supersampling: the same CPU render at 1 spp scores 4.72 and at 9 spp 3.52.
//     Only 25% for 9x the samples -- which is itself a confirmed finding, matching the Luanti
//     report that SSAA does not fix voxel-lattice moire. This needs pre-filtering, not more rays.
//
// THE LIMITATION, stated because it changes how the number may be used: the ratio is meaningless
// where there is no carrier. A textureless synthetic scores 4.57 on pure numerical noise, which
// would read as "aliased". `MoireResult::valid` is false below `kMinCarrierRms` and a caller must
// check it rather than treating 0 as good.

#include <cstdint>
#include <string>

#include "image_compare.hpp"

namespace dev::harness {

struct MoireResult {
    bool valid = false;             ///< false when the frame carries too little texture to judge
    double ratio = 0.0;             ///< E(2-4 px) / E(4-8 px) over textured pixels -- the metric
    double carrier_rms = 0.0;       ///< RMS of the 2-4 px band over the mask, 0..1
    double textured_fraction = 0.0; ///< share of pixels the mask kept
    std::string note;
};

/// Below this masked carrier RMS the ratio is numerical noise, not a measurement.
inline constexpr double kMinCarrierRms = 0.004;

/// Pixels are kept when their local carrier amplitude exceeds this fraction of the frame's own
/// mean. Relative rather than absolute so it adapts to exposure; sky, fog and still water carry no
/// carrier at all and how much of them is in frame is a framing property, not a rendering one.
inline constexpr double kTextureMaskFraction = 0.25;

/// Rows to skip from the top, for a capture with an overlay baked into it (svo_ground_hilltop's
/// stats panel is 160 px tall; the reference numbers above use 300 to clear the sky as well).
[[nodiscard]] MoireResult moire_ratio(const Image& image, std::uint32_t crop_top = 0);

} // namespace dev::harness
