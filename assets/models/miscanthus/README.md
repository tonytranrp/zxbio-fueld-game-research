# miscanthus

Third crop model for the biofuel farm (`src/World/src/miscanthus.rs`'s
Liebig's-law growth system, shared with `crop.rs`'s corn and
`switchgrass.rs`'s switchgrass). Giant miscanthus (Miscanthus x giganteus)
is a second cellulosic/advanced-generation biofuel feedstock, planted
alongside switchgrass to complete a real three-tier biofuel comparison:
corn (net-emitting, worst), switchgrass (net-emitting, far less),
miscanthus (genuinely net-negative). See `miscanthus.rs`'s own doc comment
for the cited real-research (Qin et al. 2016, a 2021 NC Piedmont water-use
field study, Univ. Maryland Extension FS-2024-0734) behind its gameplay
constants.

## Source

Generated with [Meshy AI](https://www.meshy.ai) text-to-3D (`meshy-6`
default model), preview + refine (PBR) pipeline, via the `openapi/v2/
text-to-3d` REST API.

- Preview task: `01a05238-9d2a-75cc-8dfe-5aa283432a67`
- Refine task: `01a0523a-3174-71bb-9c9c-4c3a39142791`
- Prompt: "a clump of giant miscanthus grass (Miscanthus x giganteus), tall
  bamboo-like perennial cane grass with long narrow arching leaves, dense
  upright cluster, low-poly game asset style, stylized but recognizable"
- Texture prompt (refine pass): "tall green-gold cane grass leaves,
  bamboo-like striped stems, natural miscanthus coloring"
- Settings: `art_style: realistic`, `should_remesh: true`, `enable_pbr: true`

## License

Generated via a paid Meshy API subscription; Meshy's terms grant the
subscriber commercial usage rights to generated assets. No third-party
reference asset was used as generation input (pure text-to-3D).

## Preprocessing notes

None yet -- used as exported (no Blender cleanup pass). Not rigged/animated
(a static prop). If Blender cleanup becomes necessary (topology, UVs, or
pivot/origin adjustment), record it here.
