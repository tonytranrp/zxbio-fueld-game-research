# Frame time — decision log

Prompt 004, Group AK (goals 244–275). **In progress**: AK-A (244–248) is done. Written the way this
repo's logs are: every measurement, every "decided against", and the things that turned out to be
wrong — including the ones I got wrong.

---

## 1. The first measurement changed the question (goal 244)

**Vsync was hardcoded.** `RenderContext::present()` called `swapchain->Present(1)` with the comment
"vsync on -- correctness over speed is M1.4's own done-when". With that in place, `present` time is
the *panel's*, not the renderer's, and every frame-time percentile downstream measures the display:
a 6 ms frame reads as 6 ms and a 7 ms frame reads as 12, which is a step function, not a signal.
It is a setting now (`--vsync` / `--no-vsync`, on by default because that is the shipping
behaviour), and every number below is measured with it **off**.

### And the headline number is not what the brief expected

The brief's baseline table says **76 fps / 13.15 ms**, and its instruction was: *"76 fps at 13.15 ms
with only 3.2–6.3 ms of GPU march+resolve means roughly half the frame is not the marcher. Before
you optimise the marcher, find out what the other 7–10 ms is."*

Measured on `stress_pose` today, vsync off, RelWithDebInfo, vk:

| | value |
|---|---|
| frame ms mean / median | 5.36 / **5.21** |
| frame p95 / p99 / max | 7.09 / 10.23 / 13.68 |
| GPU march (median) | **4.93** |
| GPU whole-frame range | 5.19 |
| phase coverage | 96.6% |

**The median frame is 5.21 ms — 192 fps — and 4.93 ms of it is the marcher.** There is no missing
7–10 ms. The gap the brief asked me to hunt does not exist on this build: the frame is
**GPU-bound on the march**, and the 76 fps baseline predates Prompt 002 (which turned the ImGui
overlay off for scenarios) and Prompt 003. Vsync on vs off at the same pose is 5.46 vs 5.21 ms
median — so vsync was *not* manufacturing the old number either, at this pose.

**Verdict, in one sentence: the frame is GPU-bound on the primary+secondary march, the average is
already comfortably past the 150 fps target, and the owner's complaint is entirely about variance.**
That reframes the whole prompt, and it is why AK-A comes first.

---

## 2. Secondary rays are half the marcher (goal 246)

Six traversals per shaded pixel (1 primary + 1 shadow + 4 AO). `stress_pose`, march GPU ms:

| configuration | vk | d3d12 |
|---|---|---|
| shadows + AO (shipping) | 4.90 | 5.12 |
| `--no-ao` | 3.09 | 3.70 |
| `--no-shadows` | 4.41 | 4.83 |
| both off (primary only) | **2.50** | **3.18** |

- **AO costs 1.81 ms — 37% of the marcher** — for four rays, so ~0.45 ms per AO ray class.
- **Shadows cost 0.49 ms — 10%** — for one ray.
- **Secondary rays together are 49% (vk) / 38% (d3d12) of the march.**
- The primary ray alone is 2.50 ms, i.e. 400 fps. **Even the primary is not the bottleneck at this
  pose**, which is the reframing goal 246 asked for and it should be said plainly: any plan that
  optimises primary traversal is optimising the smaller half.

---

## 3. The rebuild storm is real, and it owns the p99 (goal 247)

`fly_transect`, vsync off, with and without world rebuilds (`--no-rebuild` freezes the tree, which
makes the world go stale — that is the point):

| | rebuild ON | rebuild OFF |
|---|---|---|
| mean | 4.97 | 4.42 |
| median | 4.64 | 4.01 |
| p95 | 7.17 | 5.96 |
| **p99** | **13.16** | **6.89** |
| trees built | 3 | 1 (the initial one) |
| slow frames (>20 ms) | 7 of 2014 | 3 of 2265 |
| ...caused by upload or build | **5** | **0** |

**p99 halves, and every upload- and build-caused stall disappears.** The diagnosis in the brief's §0
is confirmed. What is left with rebuilds off is three frames, two of which are the harness's own PNG
capture (below) and one a 35 ms `present`.

### The 180 ms frames were my own instrument, and finding that out was the useful part

Both conditions showed a **max near 180 ms**, unchanged by suppressing rebuilds — which looked like
a second, larger problem hiding behind the first. The slow-frame line printed seven phases summing
to **0.7 ms of 180**, so the frame was 99.6% unaccounted.

It is the **`capture` phase**: a staging copy, a `WaitForIdle` and libpng, which `CLAUDE.md` already
documents at 200+ ms and which Prompt 002 added as an eighth phase. The *exit summary* included it;
the *live slow-frame line* still printed the original seven. A breakdown that does not add up to its
own total is not a breakdown, and this one cost a hypothesis before I noticed the exit summary
disagreed with it. `capture` is in that line now.

**So there is no mystery stall.** Excluding capture frames, the worst real frames are: with rebuilds
on, 39.0 ms (upload), 24.8 (post, while building), 22.7 (upload), 21.1, 20.0 — all rebuild-related;
with rebuilds off, 35.4 / 15.2 / 11.3 ms, all `present`, no upload or build cause at all.

### A vacuous measurement I caught before reporting it

The first attempt at this comparison produced *identical* numbers for both conditions, including
"3 trees built" with rebuilds supposedly off. The cause: my scenario-copy step used
`sed 's|^option --no-taa$|...|'` to inject flags, and **`fly_transect.scn` has no `option` lines at
all**, so the substitution matched nothing and both runs were the default configuration. The
`stress_pose` measurements in §1 and §2 are unaffected (that file does have the line), and the
fly_transect numbers above were re-taken by inserting after `backend`, which every scenario has.

That is the sixth vacuous instrument in this arc, and the first I caught by checking the instrument
rather than by disbelieving the result — the check was "does the flag appear in the file I actually
ran", which took one grep.
