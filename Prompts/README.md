# Prompts — the side-session → main-session handoff folder

This repo has two working sessions:

- **Side session (DeepSeek Harness, this folder's author):** reviews the repo, runs web
  research, decides what should be built next, and writes the prompt files here. It does NOT
  write engine code by default.
- **Main session (Claude Code, `[CC]`):** the coding session. It reads a prompt file from this
  folder and executes it against the real repo.

## Conventions

1. **One prompt = one `.md` file in this folder**, named `NNN-<date>-<slug>.md`
   (`001-2026-09-07-gameplay-physics-world-life.md`). `NNN` is sequential, never reused.
2. Every prompt file is **self-contained**: it must boot [CC]'s context (what to read first),
   state the standing rules, define task groups with concrete **Check** criteria (the repo's
   "a goal is not done without its check" standard), and list what is explicitly out of scope.
3. Prompts reference **repo-internal paths** (`research/*.md`, `docs/goals.md`), not files
   outside the repo, so [CC] never depends on Downloads paths.
4. Research the side session produces or collects is saved into `research/` first, then cited
   from the prompt. Evidence lives in the repo, not in chat history.
5. When a prompt is completed by [CC], it updates `docs/goals.md` (new groups, marked `[x]`
   in place) and `docs/progress.md` (findings folded in) — the prompt says so explicitly each
   time; this README only tracks the prompts themselves.

## Index

| # | File | Status | Scope |
|---|------|--------|-------|
| 001 | `001-2026-09-07-gameplay-physics-world-life.md` | **partial (A, B, C7, E done; C1-C6, D not started)** | Player physics (walking/jump/swim, fixed timestep, micro-step smoothing), crosshair + aim UX, global wind system, trees v2 (space-colonization skeletons + pipe model + sway), grass (voxel blades + wind, overlay stretch), water surface dynamics (Gerstner visual layer) |

## How to request the next prompt (for the human)

Tell the side session, in any wording, what the next feature arc is. It will: re-read whatever
parts of the repo changed since the last prompt (it does its own context gathering — subagents
are used only for web research, never for repo summarization), run any web research the topic
needs, and drop `NNN-….md` here with the same structure.

## 001 — completion note (main session, 2026-09-07)

Groups **A** (player physics), **B** (wind), **E** (water motion) and **C7** (mesh-path wind
parity) are complete with their Checks recorded in `docs/goals.md` groups AD/AE/AH and goal 189.
Groups **C1-C6** (trees v2) and **D** (grass) were **not started** — the pass ran out of budget,
and `research/gameplay-pass-log.md` §8 records what they build on rather than leaving half a tree
system behind. Their goals are in `docs/goals.md` as AF (186-192) and AG (193-195), unchecked.

Two Checks are knowingly weaker than the prompt asked, both with the reason written down: A3 has no
capture sequence (a 4 cm effect at a 0.1 s time constant is not something a still shows honestly;
the unit test asserts the stronger claim), and E3 applies its shore fade only on the CPU (a shader
depth probe needs a water-skipping traversal variant the 7,000-ray oracle guards, and because this
implementation displaces normals rather than geometry, the artefact E3 prevents cannot occur).

164/164 tests, `--verify-frame` 34.7%/34.6% on both backends, `--autofly --walk` 0 violations.

