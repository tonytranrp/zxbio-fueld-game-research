# Stage 3 design notes — water shading & foliage variety (goals 28, 29, 31, 32, 36, 38, 39)

Written BEFORE implementation, per each goal's own check.

## Goal 28 — fresnel water term

- `F = F0 + (1 - F0) * (1 - dot(V, N))^5` (Schlick), `F0 = 0.02` (water's normal-incidence
  reflectance).
- Reflection color = `SkyRadiance(reflect(-V, N))` from `sky_common.fxh` — the cheap "fixed
  sky-tint" the goal names, upgraded for free: since the shared sky model contains the analytic
  sun disc, sun glints along the reflection vector fall out automatically, and goal 32's explicit
  Blinn term below sharpens them.
- Refraction/body color = depth-tinted: `lerp(shallowTint, deepTint, depth)` where depth is the
  water-column depth baked per-vertex (below).
- Final water color = `lerp(bodyColor, reflection, F) + specular`.

**Depth rule** (goal 28's "deeper more opaque, shallow more transparent"): water-column depth is
computed AT MESH TIME by scanning down from each water-surface cell through water voxels until
solid ground, capped at 8 voxels, normalized to [0,1], and carried in the vertex's AO attribute —
water's concavity-AO is meaningless (a flat open surface is always ao=1), so the byte is free for
water-material vertices, with zero format/stride change. Shallow shores get a warm green-blue
"you can see the bottom" tint; ≥8-voxel water is full deep blue. The cap is also honest about
data availability: the mesher's neighbor halo only extends one chunk, so an unbounded scan could
not be seam-correct anyway.

## Goal 29 — how water actually renders: opaque single-pass, deliberately

Neither a blend-state second PSO nor a forward-transparent second pass — and that is the decision,
with this reasoning: **this engine does not mesh underwater terrain at all** (only the water top
surface and above-water land produce geometry; documented since the terrain-fixes pass). True
alpha blending would therefore composite water against the SKY/fog behind it, not against a
seabed — objectively wrong, and worse than opaque. The correct v1 is opaque water whose
transparency is EMULATED analytically: the depth-tinted body color above literally encodes "what
you would see looking into this much water". Revisit with a real transparent pass if and when
goal 80's cave/3D-density decision makes submerged geometry exist; noted there.

## Goals 31/32 — ripple normal + sun glint

- Ripple: perturb N = (0,1,0) with two scrolling derivative waves,
  `dN.xz = A1*cos(dot(p.xz, d1)*f1 + t*s1)*d1 + A2*cos(dot(p.xz, d2)*f2 + t*s2)*d2`
  (amplitudes ~0.04/0.025, non-parallel directions, different speeds so the pattern never
  visibly loops). Time reaches the PS through the fog constants' free w lane.
- Glint: Blinn half-vector `pow(saturate(dot(N, H)), 256)` scaled by kSunColor and HDR-bright
  (>1) so Stage 2's bloom picks it up — the intended "real bloom source" flagged in the Stage-2
  log.

## Goal 36 — foliage silhouettes & selection

Three deterministic variants from the existing `placement_key` hash (no new placement system):
selector = `(key >> 8) & 0xFF`.

| selector | shape | construction (same primitive kit as today) |
|----------|-------|--------------------------------------------|
| < 96     | round tree (existing) | box trunk + single octahedron canopy |
| < 192    | conifer | taller/thinner trunk + 3 stacked, shrinking octahedra |
| else     | shrub | no trunk; one squashed octahedron sitting on the ground |

## Goal 38 — per-tree color jitter

Rides the AO attribute (tree geometry is unoccluded by construction, so the byte is free, same
argument as water depth): per-tree factor in [0.80, 1.0] from `(key >> 16) & 0xFF`, multiplied
into every vertex of that tree. AO multiplies the final lit color in the PS, so this IS a
brightness jitter — done with zero format change. (A hue shift would need a real per-instance
color; deliberately out of scope for the byte we have.)

## Goal 39 — wind sway

Vertex-shader time offset applied to LEAVES-material vertices only (`Material` is already a VS
input): `offset.xz = sway * (sin(t*1.4 + wp.x*0.37) , cos(t*1.1 + wp.z*0.29))`, sway ≈ 0.15
voxels. Trunks (Wood) and terrain get zero offset — trunks stay still by material test, not by a
height ramp (height-within-object is not in the vertex, and the material split gives the goal's
requested look without new data).

## Goal 40 — grass/flower ground-cover: DECIDED NO for this pass (written decision)

Instanced billboard/cross-quad ground-cover is a genuinely different rendering system from
everything this engine has (per-instance data, a second draw path or instancing layer, its own
density/streaming story, alpha-tested foliage textures where the project deliberately has no
textures at all). Trees + shaped water + sky/fog is already the substantial visual jump this arc
promised, and Group M's material expansion (grass/sand SURFACE COLOR) delivers most of the
remaining "green ground" reading at zero architectural cost. Ground-cover geometry is the natural
NEXT visual arc after a texture/instancing story exists -- deferred deliberately, not skipped
silently.

## Stage-3 verification record (2026-09-04)

- Goal 30 viewed (`water_glint.png`, `sliver_closeup.png`): shallow shore band visibly lighter/
  cyan, deep water rich blue, fresnel brightening toward grazing -- reads as real water.
- Goal 31 mechanically A/B'd (frames ~5 s apart, same pose): 11,867 water-region pixels changed
  (>6/255) between captures -- the ripple pattern moves.
- Goal 39 same A/B: 202 canopy pixels changed (max delta 166 -- geometry motion), trunk region
  ZERO changed pixels (max 2 = noise). Canopies sway, trunks still, exactly as specified.
- Goal 32 with a caveat, honestly: the Blinn glint term is live, and the bright streaky sun-region
  reflections in `water_fixed.png` come from the reflection vector hitting the sky model's sun.
  The classic "sun behind camera over water" composition is geometrically impossible with this
  near-zenith sun direction; a lower golden-hour sun (future lighting work) would showcase the
  narrow lobe properly.
- Goal 37 viewed (`water_fixed.png`, `sliver_closeup.png`): round trees, stacked-octahedra
  conifers, and ground shrubs mixed in one view; goal 38's per-tree brightness jitter visible as
  canopy-to-canopy variation.
- Shader cost: 165.0 fps settled with fog+water+sway+sky (identical to every earlier waypoint --
  still free at this scale).

## NEW ISSUE found by this pass's captures (pre-existing, filed for goal 54's mesh review)

Thin vertical unlit-stone sliver "curtains" hang in the air near chunk-corner world positions
around (100-130, 32-64, 60-90), visible only from grazing angles (repro:
`--pos 60,26,150 --pitch -18 --yaw -35`, dashes top-center; `research/captures/water_fixed.png`).
Bisected mechanically: present with culling disabled, with the sky pass off, at radius 2 AND 5
(fixed world position, not a streamed-edge effect), with previous-commit geometry (predates the
Stage-3 mesher/tree changes), and in chunk layer y=1 alone. Color-matched to ambient-lit Stone.
Almost certainly a mesh_extractor boundary-layer edge case -- exactly the file goal 54 already
schedules for a deep read; the repro pose makes that review concrete.
