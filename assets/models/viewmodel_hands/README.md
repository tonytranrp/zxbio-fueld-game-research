# viewmodel_hands

## Attribution
- Source: Meshy AI text-to-3D (Smart Topology), generated via direct HTTP API call (not a Blender plugin).
- License: placeholder -- confirm against the Meshy account/plan's asset license terms before shipping.
- Generated: 2026-08-25.

## Meshy prompt used
"forearms and hands only, no torso/shoulders/face... reaching forward, palms open, fingers loosely
curled... rolled-up dark olive sleeve with stitched cuff" -- refined via Meshy's Smart Topology at a
~9000 target polycount.

## Rigging note
Meshy's auto-rig endpoint was tried first and rejected this asset with `422 Unprocessable Entity:
"Pose estimation failed, please provide a valid model URL"` -- it requires a full body to
pose-estimate against and cannot rig a forearms-only mesh. The armature below was built and skinned
manually in Blender instead.

## Preprocessing (Blender 5.2.0 LTS)
- Imported raw Meshy `refined.glb` (single mesh object "Mesh_0", 7467 verts / 9702 tris / one
  material with 4 embedded 2048x2048 PBR textures, no armature, no animation, as expected for an
  unrigged asset).
- Mesh cleanup: merge-by-distance (removed 2607 duplicate vertices -- a Meshy topology artifact that
  initially made ~63% of vertices non-manifold and caused automatic weight painting to fail
  outright), recalculated normals outside, deleted loose geometry. Triangle count unchanged at 9702
  after cleanup (duplicates were welded, not extra geometry).
- Built a hand-placed (not Rigify) armature from the actual mesh geometry: bone coordinates were
  derived from the real vertex data (per-Z-band cross-sectional centroids for the forearm/wrist
  centerline, a 2D height-grid over the hand region to separate the thumb cluster from the four-finger
  mass) and cross-checked against viewport screenshots. Mixamo/UE-style convention: no metacarpal
  bone, no fingertip end-bone.
- Skinned via Blender's automatic weights (`ARMATURE_AUTO`) after the manifold fix above; vertex
  weight influences limited to 4 per vertex and renormalized (raylib/cgltf only reads
  `JOINTS_0`/`WEIGHTS_0`, i.e. 4 influences). Known limitation: ~0.9% of vertices (44/4860), all
  within ~4mm of the exact left/right midline where the two hands visually touch, carry a small
  (<0.8) weight from the opposite hand's bones -- this reads as reasonable blending at the contact
  seam, not a defect, but is worth a visual check if that seam is ever posed far apart.
- Two animation clips authored by hand (Meshy delivered rig-only, no motion -- expected for an
  auto-rig-style request): see Clip order contract below. Amplitudes are intentionally small; a
  separate procedural head-bob/sway is layered on top by engine code.
- Scaled uniformly so the long axis (elbow-cut to longest fingertip, Blender Z after import) is
  0.47m, then rest-posed and applied (`transform_apply`) so both objects ship at unit scale.
- Exported as a single embedded `.glb` (GLB format always embeds textures): Y-up conversion,
  modifiers applied, materials exported, skinning with <=4 influences/vertex, rest-position armature,
  animations exported as separate actions, force-sampled (required because the idle clip uses SINE
  easing, which glTF cannot represent natively -- the exporter bakes it to sampled keyframes
  automatically). Draco/meshopt/gltfpack compression deliberately left disabled: raylib's cgltf-based
  loader cannot decode those extensions.
- Round-trip verified: re-imported the exported file into a scratch collection and confirmed 9702
  triangles, 34 bones (names below), and animation clip order, then discarded the scratch import.

## Bone list (34, exact strings -- used verbatim as C++ rig-binding string constants)
Per side (`.L` = mesh +X side, `.R` = mesh -X side, as authored -- this is a labeling convention only,
not a verified anatomical left/right, since the original Meshy generation's facing direction is
unknown):
`forearm`, `hand`, `thumb1`, `thumb2`, `thumb3`, `index1`, `index2`, `index3`, `middle1`, `middle2`,
`middle3`, `ring1`, `ring2`, `ring3`, `pinky1`, `pinky2`, `pinky3` (17 x 2 sides = 34 total).

Full list: forearm.L, forearm.R, hand.L, hand.R, thumb1.L, thumb1.R, thumb2.L, thumb2.R, thumb3.L,
thumb3.R, index1.L, index1.R, index2.L, index2.R, index3.L, index3.R, middle1.L, middle1.R,
middle2.L, middle2.R, middle3.L, middle3.R, ring1.L, ring1.R, ring2.L, ring2.R, ring3.L, ring3.R,
pinky1.L, pinky1.R, pinky2.L, pinky2.R, pinky3.L, pinky3.R.

## Clip-order contract (index, not name, is what the engine's `LoadModelAnimations` uses)
- Clip index 0 = `idle` (60 frames @ 30fps = 2.0s, loops, SINE-eased, subtle forearm sway +
  finger-relax settle).
- Clip index 1 = `walk` (20 frames @ 30fps = ~0.67s, loops, BEZIER, faster/snappier forearm + hand
  secondary-motion bounce).
- Verified via round-trip re-import immediately after export, not assumed from authoring order.
