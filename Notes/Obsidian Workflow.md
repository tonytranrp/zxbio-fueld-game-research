# Obsidian Workflow

Use this repo folder as the Obsidian vault. Obsidian will read the Markdown files in place, so edits to notes are normal Git changes.

## How To Open

1. In Obsidian, choose `Open folder as vault`.
2. Select `C:\Users\Tonyt\Documents\GitHub\zxbio-fueld-game-research`.
3. Open [[Project Hub]].

## How We Use It

- Put durable project notes under `Notes/`.
- Keep source-local standards in each folder `README.md`.
- Use wiki links for project knowledge, such as `[[src/engine/physics/README]]`.
- Use tags for broad filtering: `#area/engine`, `#area/game`, `#area/research`, `#area/bugs`.
- Keep temporary windows, layout, and plugin cache out of Git through `.gitignore`.

## Good Note Types

- Architecture decisions: why a system moved in a specific direction.
- Bug research: verified behavior, cause, and affected files.
- Implementation journal: what changed during a larger refactor.
- Build notes: commands, toolchain quirks, and verification history.

## Boundaries

Obsidian is for Markdown knowledge. Build outputs, generated files, local UI state, and package caches should stay out of the vault graph.
