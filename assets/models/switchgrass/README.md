# switchgrass

Second crop model for the biofuel farm (`src/World/src/switchgrass.rs`'s
Liebig's-law growth system, shared with `crop.rs`'s corn). Switchgrass
(Panicum virgatum) is a real advanced-generation/cellulosic biofuel
feedstock -- planted deliberately beside the corn field as the first-
generation vs. advanced-generation biofuel narrative progression noted as a
follow-up in `corn_plant/README.md`. See `switchgrass.rs`'s own doc comment
for the cited real-research (Schmer et al. 2008 PNAS, Namoi et al. 2025,
Hamilton et al. 2015) behind its gameplay constants.

## Source

Generated with [Meshy AI](https://www.meshy.ai) text-to-3D (`meshy-6`
default model), preview + refine (PBR) pipeline, via the `openapi/v2/
text-to-3d` REST API.

- Preview task: `01a0521a-1af7-71fa-8c2a-faae6389a08f`
- Refine task: `01a0521c-bd74-765d-a682-151f64d0c4ea`
- Prompt: "a single clump of switchgrass (Panicum virgatum), tall perennial
  prairie bunch grass with narrow blade-like leaves and an airy open seed
  panicle at the top, low-poly game asset style, stylized but recognizable"
- Texture prompt (refine pass): "dry golden-green prairie grass blades,
  natural bunch grass coloring, subtle seasonal color variation"
- Settings: `art_style: realistic`, `should_remesh: true`, `enable_pbr: true`

## License

Generated via a paid Meshy API subscription; Meshy's terms grant the
subscriber commercial usage rights to generated assets. No third-party
reference asset was used as generation input (pure text-to-3D).

## Preprocessing notes

None yet -- used as exported (no Blender cleanup pass). Not rigged/animated
(a static prop). If Blender cleanup becomes necessary (topology, UVs, or
pivot/origin adjustment), record it here.
