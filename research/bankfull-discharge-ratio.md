# Bankfull discharge for small humid-temperate catchments

Web research performed 2026-09-07 for Prompt 006 goal 309, because the project's own research corpus
(`earth-terrain-geomorphology-research.md` Part 2 §1.3) gives the bankfull regime equation
`W = a·Q^0.5` but not the discharge to put in it for a world whose largest basin is ~5 km².

Claims are labelled **CONFIRMED** (read from the cited source) or **INFERENCE** (derived here from
cited quantities, arithmetic shown). Every numeric claim carries its URL.

---

## 1. The question, and why it was the wrong one

The question asked was: what is `Q_bankfull / Q_mean_annual` for catchments of 1–50 km²?

**That ratio is not a published statistic.** The literature relates bankfull discharge to *drainage
area* or to *flood recurrence interval*, essentially never to mean annual flow. The useful answer is
a relation that skips the conversion entirely.

## 2. What to use instead — CONFIRMED

**Petit & Pauquet (1997)**, ~30 gravel-bed rivers / ~40 gauging stations in the Belgian Ardennes
(Cfb oceanic climate), **catchments 4 km² to 2,700 km²**:

> Q_b = 0.087 · A^1.044   (r = 0.989)

- CONFIRMED: the equation, the r-value, and the 4–2,700 km² range.
- INFERENCE: units are m³/s and km². Not stated in the retrieved text but forced by consistency —
  at A = 1,607 km² (Ourthe at Tabreux) it gives 193 m³/s; at 4 km², 0.37 m³/s.
- Source: <https://orbi.uliege.be/bitstream/2268/21780/1/ESP%26L%201997%20.pdf>

**This is the right anchor for this project**: the catchment size range, the climate (humid oceanic),
and the bed type all match the world the climate stage describes, and it is valid *down into* our
range where the regime equations are not (see §5).

### The implied ratio, for reference — INFERENCE

Ardenne mean annual runoff is 460–500 mm/yr (Ourthe@Tabreux 460, Ourthe Orientale 480, Ourthe
Occidentale 500) — <https://doi.org/10.5194/hess-21-423-2017>. At 470 mm/yr = 0.0149 m³/s per km²:

    Q_bf / Q_mean = 0.087·A^1.044 / (0.0149·A) = 5.84 · A^0.044

| A (km²) | 1 | 5 | 10 | 50 |
|---|---|---|---|---|
| Q_bf/Q_mean | 5.8 | 6.3 | 6.5 | 6.9 |

So **k ≈ 6–8, defensible range 4–12**, nearly flat with area. The value of 10 this project used
before the research is inside that band but at its top.

## 3. What moves it

- **Area** — weak and region-dependent. The ratio goes as `A^(b−1)` where b is the bankfull-discharge
  exponent: Ardennes b = 1.044 (flat); typical US regional curves b ≈ 0.78 (falls as A^−0.22, a
  factor ~2.4 across 1→50 km²). *INFERENCE for the b ≈ 0.78 figure, derived from width–area exponent
  β = 0.39 ± 0.21 with W ∝ Q^0.5.*
- **Climate** — CONFIRMED and larger than the area effect. Erikson, Renshaw & Magilligan (2024):
  discharge scales linearly with area across most of North America, but **low-runoff-efficiency
  (arid) regions scale non-linearly and their bankfull dimensions rise faster with area**; the
  channel-forming discharge has a longer recurrence there.
  <https://par.nsf.gov/biblio/10512013-spatial-variation-drainage-area-runoff-relationships-implications-bankfull-geometry-scaling>
- **Flashiness** — CONFIRMED, and it moves *opposite* to recurrence interval. Petit & Pauquet:
  impermeable flashy Ardenne pebble-beds have bankfull RI **< 0.7 yr**; baseflow-dominated gravel and
  sandy/silty rivers **> 2, even > 3 yr**; Roberts (1989) permeable > 2 yr vs impermeable 4–8 months.
  **INFERENCE, and important:** a groundwater-fed chalk stream has a damped hydrograph, so its whole
  flood-frequency curve compresses toward the mean — *high* bankfull RI but *low* Q_bf/Q_mean. **Do
  not infer the multiplier from a recurrence interval.**

## 4. The 1.5-year recurrence interval — correct, but the spread is far larger than usually stated

The classic figure is 1.5 yr (Leopold/Wolman; Dury 1977's 1.58 is the modal annual flood of a Gumbel
EV1). Measured spread — all CONFIRMED from the cited sources:

| Study | Region | Bankfull RI |
|---|---|---|
| Williams (1978) | 36 sites, global | **1.01–32 yr**; only ~⅓ in the 1–2 yr band |
| Castro & Jackson (2001) | Pacific NW | mean 1.4; humid W OR/WA 1.2 |
| Sweet & Geratz (2003) | NC Coastal Plain | **below 1 yr**; partial-duration mean 0.19 yr |
| Powell et al. (2006) | Ohio large rivers | 0.3–1.4 yr |
| Petit & Pauquet (1997) | Belgian Ardennes | < 0.7 small; 1.1–1.5 above 250 km² |
| Lawlor (2004) | W. Montana | 1.0–4.4 yr, median 1.5 |
| Smith (1979) | Alberta, ice-scoured | mean **16.7 yr** (2.4–45) |

Hey (1998): use of the 1.5-year flood as the design discharge **"is not supported by field data"**
(<https://doi.org/10.1061/40382(1998)163>).

Two traps: an **annual-maximum series cannot report an RI below 1 year**, which is why humid-temperate
studies so often report "inconclusive"; and Lawlor found bankfull / 2-year-peak ranging **0.21 to 3.7**
(median 0.84) across 41 sites in one state — an 18× spread on a single well-defined quantity.
<https://doi.org/10.3133/sir20045263>

## 5. The caution that matters most here — CONFIRMED

**This project's rivers are below the datasets that produced the regime equations it cites.**
NEH Part 654 Ch. 9 Table 9-5 data ranges (<https://irrigationtoolbox.com/NEH/Part%20654/CHAPTERS/Chapter-09.pdf>;
flagged by the researcher as a single PDF extraction, worth re-verifying if it becomes load-bearing):

| Source | Bankfull discharge range |
|---|---|
| **Nixon (1959)** | 19.8–510 m³/s |
| Hey & Thorne (1986) | 3.9–425 m³/s |
| Charlton et al. (1978) | 2.7–156 m³/s |

A river of a few m³/s is **5–10× below the entire Nixon dataset**, and the same chapter states the
generalized sand-bed width predictors **should not be applied below 17 m³/s**. This world's largest
river is ~0.5 m³/s bankfull, so `W = a·Q^0.5` is being extrapolated roughly two orders of magnitude
below its calibration. **INFERENCE on why it degrades:** at these scales bank strength is dominated
by root reinforcement and woody debris, for which the regime equations contain no term.

## 6. The honest size of the uncertainty — CONFIRMED

An independent route from area straight to width, Sofia & Nikolopoulos (2020),
<https://www.nature.com/articles/s41598-020-61533-x>:

> W_bkf = α·A^β,  α = 3.6 ± 2.3,  β = 0.39 ± 0.21  (m, km²); literature 1.0 < α < 5.83, 0.1 < β < 0.6

**The two routes disagree by ~2.6× at A = 10 km²** (route 1 + `W = 3.5·Q^0.5` gives 3.4 m; route 2 at
α = 3.6 gives 8.8 m). That gap is a real region effect — humid maritime versus the semi-arid montane
Colorado Front Range the α = 3.6 was fitted on — not arithmetic error, and it is the honest size of
the uncertainty on any river width in this project.

Reassuring counterpoint: since **W ∝ √Q**, a 2× error in discharge is only a 1.41× error in width.

## 7. What this project did with it

`world/generation/include/world/generation/field/rivers.hpp` uses **Petit & Pauquet directly** —
`Q_bf = 0.087·A_eff^1.044` — with `A_eff` the **precipitation-weighted** contributing area, so goal
302's orographic field decides which basins carry water. Because the precipitation plane is
normalised to a mean of 1.0, `A_eff` averages to the true area and the relation is applied at the
scale it was calibrated at; a wet windward basin gets a larger effective area and a dry lee one a
smaller.

The `bankfull_multiple` parameter this research was requested for **was removed**, because the
relation it was needed for is no longer in the path.

`terrain_dump` reports both width routes side by side so the 2.6× disagreement is visible rather
than hidden behind whichever one shipped.

## 8. Leads not taken

- **Bieger et al. (2015)** notes Leopold & Maddock's (1953) original downstream hydraulic geometry
  was built on *average annual* discharge, not bankfull — so a mean-annual hydraulic-geometry
  relation would remove the conversion entirely. No modern well-calibrated one was found.
  <https://digitalcommons.unl.edu/cgi/viewcontent.cgi?article=2520&context=usdaarsfacpub>
- The **FluvialGeomorph `RegionalCurve` R package** (CC0, USACE) holds 163 regional curves with
  intercept, slope, drainage-area range and median recurrence in one table. It would settle the US
  cross-check in a single query if R is ever available here.
- No US regional-curve report (Chaplin 2005, Cinotto 2003, Mulvihill 2009, Bieger 2015) could be read
  past its front matter, so **the US cross-check of the ratio remains unverified**.
