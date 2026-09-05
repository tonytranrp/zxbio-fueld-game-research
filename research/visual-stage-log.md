# Visual-richness stage log (docs/goals.md groups B–F, L, M)

Running before/after record. Every claim below was checked by actually viewing the referenced
capture, per the standing methodology note in `docs/goals.md` — never by the verify fraction alone.
Reference captures live in `research/captures/`; the full working set (all angles, numbered
sequences) stays in the session scratchpad.

## Baseline (goal 8, 2026-09-04, post-winding-fix, pre-Stage-1)

Three angles viewed (`captures/baseline_default.png`, ground-level walk view, straight-down):
- Terrain rendered as near-uniform pale gray (every solid voxel is Stone; Dirt exists in the
  palette but generation never assigns it). Flat white Lambert + 0.25 ambient floor.
- Voxel terracing reads as an artifact on every slope; no AO, so creases/valleys have no depth.
- Straight-down view shows the loaded region's water EDGE as a hard rectangle against sky — the
  "geometry just stops" pop that distance fog (goal 33) exists to hide.

## Stage 1 — AO, hemisphere ambient, sun color, albedo variation, richer palette (goals 10–20)

Changes: baked per-vertex concavity AO (`research/baked-ao-design.md`; the former pad byte of the
12B vertex, stride unchanged), two-color hemisphere ambient replacing the flat floor, warm golden
sun color, two-octave world-XZ value-noise albedo mottling (±10%), richer base palette.

After-captures viewed (`captures/stage1_default.png`, `captures/stage1_ground.png`), same angles
as baseline:
- The scene went from clinical gray to warm, sun-lit tan/sand tones; up-facing surfaces read
  warmer/brighter than side/down-facing ones (hemisphere term visibly working).
- AO darkens exactly the concave features: terrace inside-corners, valley creases, pit corners —
  the terracing now reads as sculpted sediment layers with depth rather than a flat artifact.
- Mottling is visible as gentle dappling at two scales, no visible noise-grid.
- Honest gap: with every solid voxel still Stone, the warm light makes mountains read as "sand
  dunes"; material identity (grass tops, shore sand — Group M / goal 81) is the missing half of
  "colorful", not more lighting.

Numbers:
- Tests 70/70 → **71/71** (new `[ao]` case asserts the design table's exact levels on
  hand-constructed flat/crease/pit/ridge geometry).
- Verify fraction unchanged: 39.4% Vulkan at the standard pose (goal 17: the 25% threshold stays
  meaningful — the metric counts differing pixels, and lighting changes don't shift it).
- Goal 18 shader-cost A/B, measured (not assumed), same 700-frame run, settled scene, RTX 4070
  laptop, debug build: old PS math 164.9 fps ↔ Stage-1 PS math 165.1 fps — the added ALU
  (AO multiply + hemisphere lerp + 8 sin-hash noise samples) is below run-to-run noise. The PS
  A/B was done by swapping shader SOURCE at runtime (same varyings both sides), no rebuild.

## Stage 2 — Bloom & tone mapping (goals 21–27)

Wiring reality vs the research: at the pinned DiligentFX commit, `Bloom::Execute` gates on
`PostFXContext::IsPSOsReady()`, and that flag is set ONLY inside `PostFXContext::Execute` — which
demands depth/motion-vector/camera inputs Bloom never consumes and runs four full-res helper
passes per call. Found empirically (bloom stayed PENDING forever; the first A/B diff measured
bloom's contribution at literally zero once overlay text was masked). Resolution: ONE warm-up
`PostFXContext::Execute` at startup with 2×2 dummy inputs flips the sticky readiness flag; per
frame only `PrepareResources` runs. Scene now renders into an RGBA16F offscreen target
(terrain PSO format follows it; construction order PostProcessor→TerrainRenderer is load-bearing);
composite = soft-knee tonemap (identity below 0.75, tanh shoulder above — ACES was tried first
and VISIBLY washed out mids ~30% in the viewed capture; the knee curve preserves the authored
look and only rolls off bloom overshoot).

Tuning by viewed captures + masked pixel-diffs (`stage2_default.png`):
- First tune (Threshold 0.60): whole-frame haze, mean +8% everywhere — the SKY (max channel ~0.9)
  is brighter than any terrain (~0.8), so a low threshold blooms 77% of the frame. Re-tuned to
  0.80/0.12: soft silhouette glow only (max delta 11/255), no clipped whites. Stage 3's water
  sun-glint (first real >1.0 emitter) is the intended true bloom source (goal 32 re-tunes).
- Goal 26's predicted metric break happened EXACTLY: bloom's sky gradient made 97.7% of pixels
  differ bytewise from the verify reference. Fixed by tolerance comparison (>16/channel — bloom
  deltas ≤11, terrain-vs-sky 50+): verify returns 39.4% Vulkan / 39.2% D3D12, same as
  pre-post-processing, restoring the metric's "terrain visible" meaning.
- Goal 25 cost, measured settled at 700 frames: 165.0 fps with the full chain ↔ 165.0 fps
  --no-post (worst-frame 7.2 vs 7.3 ms) — unmeasurable at this scene scale.
- Goal 27 combined view: warm Stage-1 terrain + subtle Stage-2 silhouette glow reads as
  atmosphere, not effect-soup; the remaining flatness is material identity (Group M), not light.

## Group M — material palette expansion (goals 81, 93–96)

Target list (goal 93, written before touching the frozen asserts): exactly TWO new materials, each
tied to a real feature — **Sand** (goal 81's shoreline band: columns with surface ≤ sea+1.75) and
**Grass** (the missing "green ground" reading; surface voxel of gentle above-sea columns, slope ≤
1.9 via a seam-exact 34×34 margin-grid central difference). Appended AFTER Wood/Leaves so every
baked ID stays valid; both frozen counts updated 6→8 together (goal 94). Dirt — in the palette
since Phase 1 but never once assigned — now actually exists as the soil band (depth 1–3).
Mesher refinement: surface cells pick their HIGHEST solid corner's material (ties break away from
water), so grass/sand skins read cleanly instead of whichever soil corner edge iteration hit first.

Viewed (`groupM_shore.png`, `groupM_default.png`): green grass caps, dirt terrace bands, exposed
rock on steep faces, a sand ring around every shore with the bright shallow-water band over it —
the single biggest step toward "colorful" of the whole arc; the goal-95 mottle interaction reads
as natural stylized variation, not chaos. Verify fraction rose 12.3%→14.2% (more material-boundary
contrast), same 6% threshold. 71/71 tests.

Goal 96 (per-material shading tweak) — written decision: NO further per-material specular now.
Water already has the one genuinely different path (fresnel/ripple/glint); sand/grass/dirt/stone
under shared Lambert+AO+hemisphere read correctly at this art style, and a per-material
roughness constant would be a real material system's first slice — that belongs with textures,
not before them.

## Groups H + J — code-quality audits & deferred-item cleanup (2026-09-04)

- Goal 53/78 benchmarks, re-run Release core-only, new baseline
  `benchmarks/baselines/2026-09-04-visual-pass.json`: extract_mesh 3.07 ms (vs 3.97 ms in the
  hardening baseline -- no regression from AO + water-depth + material-pick; single-run numbers,
  the compare.py U-test convention applies to any future regression CLAIM). Map suite unchanged
  in shape: boost_flat still wins build (16.4 µs) and stays competitive on find; unordered_dense
  still loses lookups.
- Goal 54 (mesh_extractor deep read): read in full three times this pass (AO, water depth,
  material pick); the padded-sampling pass now serves FOUR concerns, each with its own boundary
  test ([ao], [water], coverage, continuity). The floating-sliver issue is documented with full
  repro for a RenderDoc session (goal 73's tool) -- see water-foliage-design.md.
- Goal 55 (throw audit): 32 `throw std::runtime_error` sites, all creation/contract failures that
  flow to main()'s single catch -- consistent by design; each message names its subsystem, which
  is what a faster diagnosis actually needs. No change warranted.
- Goal 56 (generation-time re-check): extract_mesh 3.07 ms Release includes trees+AO+depth; the
  debug streaming smoke (verify settle ~10 s, autofly bounds green) matches pre-Stage tuning.
- Goal 57 (CoordMap consistency): grep found ZERO raw std::unordered_map/std::map in engine/world/
  render/app/tools outside the alias's own comment. New code (tree_counts_) uses CoordMap.
- Goal 58 (dispatcher convention): no new cross-system notifications were introduced by this
  pass -- post-process and aim query are direct in-frame calls, which is the correct shape for
  synchronous per-frame work; nothing bypassed the event bus.
- Goal 59 (new-code boundary tests): AO levels [ao], water-depth encoding [water] (found a real
  off-by-one -- the scan skipped the anchor voxel), swim settle [swim], aim query 3 sections
  [aim], sliver guards [sliver] x2. 76/76 total.
- Goal 60 (.clang-format): did not exist; added one CALIBRATED to the codebase (4-space, 110-col,
  left pointers). 51/99 files drifted from mechanical formatting; applied the one-time tree-wide
  pass (whitespace-only) -- post-pass drift 0, tests 76/76. Enforced going forward.
- Goal 61 (clang-tidy): real headless run (VOXEL_CLANG_TIDY=ON, LLVM 22.1.4) -- caught 2 REAL
  findings in the new mesh_dump (exception-escape from main, atoi), both fixed; re-run clean.
  Test-dir exclusions still correct (Catch2 macro noise, not product code).
- Goal 74 (mesh_dump .obj): implemented (terrain by material groups, v//vn); validated
  structurally (869 verts matching the in-engine chunk, 0 bad face refs) -- honest partial on the
  external-viewer check: Blender's MCP addon wasn't running; the file is standard .obj.
- Goal 75 (RG16 normals): viewed `captures/banding_slope.png` -- slope striping is geometric
  terracing + material banding; smooth grass shades continuously with NO quantization banding
  under the new lighting. Answer: no; the A/B stays unrun per the goal's own conditional.
- Goal 76 (backward-cpp): the premise was false -- it was never declared in Dependencies.cmake
  (CLAUDE.md's note is research documentation, not a dependency). Nothing to remove.
- Goal 77 (unordered_dense): KEEP, harness-only -- it just earned its keep again as the
  comparison column in today's baseline re-run; benchmark-gated fetch costs normal builds nothing.
