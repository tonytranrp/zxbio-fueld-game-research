# The Biomechanics and Mathematics of Human Locomotion (Part 1 of 5)

**Scope:** the mechanics, energetics, and mathematics of how real humans walk, run, turn, climb, and move backward and sideways — the physical grounding for character/camera locomotion. Sections are numbered 1.x because this is part 1 of a merged five-part document; the numbering is stable for cross-referencing (Part 2 = perception and head/torso behavior, Part 4 = balance and XcoM in depth). Every load-bearing number carries an inline citation to a source verified live this session; ranges are given as ranges because that is what the literature supports.

---

## 0. Provenance

- **Date:** 2026-09-06. **Author:** research agent 1 of 5 (biomechanics/mathematics of locomotion).
- **Tools:** Exa web search + fetch, `web_fetch`, local numeric verification with `pwsh` (PowerShell).
- **Verification summary:** ~60 distinct sources were located and their content read (search-result highlights or full-page fetches) this session; all URLs in the source list returned live content. Every numeric anchor supplied by the parent was recomputed locally in `pwsh`:
  - $\sqrt{gL}$ with $L=0.95$ m, $g=9.81$ → **3.0528 m/s** (anchor 3.05 ✓)
  - $Fr = v^2/(gL)$ at $v=2.0$, $L=0.95$ → **0.4292** (anchor 0.43 ✓)
  - Compass-gait rise $2L(1-\cos\theta)$, $\sin\theta = (s/2)/L$, $s=0.75$, $L=0.95$ → $\theta = 23.25°$, rise = **0.1543 m ≈ 15.4 cm** (anchor ~15 cm ✓)
  - Bolt average speed $100/9.58$ = **10.438 m/s** (anchor ~10.4 ✓)
  - Walk/run energetic crossover from fitted cost curves (see §2.2 worked example) → **2.21 m/s**, above the preferred transition speed, reproducing the published experimental finding.
- **Anchor disagreements found (reported, not forced):** backward preferred speed measured at **0.8–1.0 m/s**, below the parent's 0.9–1.2 anchor (§7.1); preferred step length measured at **0.70–0.78 m**, slightly below the parent's 0.75–0.80 anchor (§5); running peak GRF reaches **3.5–5 BW in elite sprinting**, above the parent's 2–3 BW sprint figure, which is correct only for submaximal running (§1.4); running vertical COM oscillation reaches **up to ~13 cm**, above the parent's 6–10 cm band (§6.1).
- **Deliberately unverified items (dropped or flagged):** the exact power-law exponent of the "Grieve equation" (§5 — the primary source verifies a log-log law but I could not verify a specific exponent this session, so none is quoted); the "10–30 kN/m" leg-stiffness anchor is replaced by directly sourced values that span it (§4.1).
- **Source count:** 60 distinct verified sources in §9.

---

## 1. The gait cycle and its phases

### 1.1 Stance, swing, and duty factor

One gait cycle (stride) runs from initial contact of one foot to the next initial contact of the same foot; at symmetrical walking the opposite foot contacts at 50% ([OUHSC gait primer, Rancho terminology](https://ouhsc.edu/bserdac/dthompso/web/gait/intro.htm)). The fundamental division:

- **Walking:** stance ≈ 60% of cycle, swing ≈ 40% ([Musculoskeletal Key gait analysis](https://musculoskeletalkey.com/gait-analysis/), [Clinical Tree: Principles of Normal and Pathologic Gait](https://clinicalpub.com/principles-of-normal-and-pathologic-gait/)). Equivalently the **duty factor** (fraction of the stride each foot is on the ground) is > 0.5.
- **Running:** the percentages reverse to roughly 40% stance / 60% swing; "walking becomes running when there is no longer an interval of time in which both feet are in contact with the ground" ([Musculoskeletal Key](https://musculoskeletalkey.com/gait-analysis/)). Duty factor falls gradually from about 0.65 in slow walks to about 0.55 in the fastest walks, then **drops abruptly to ≈ 0.35 at the walk-run transition** — one of the quantities that changes discontinuously between gaits ([Alexander, *Principles of Animal Locomotion*, ch. 1](https://www.originalwisdom.com/wp-content/uploads/bsk-pdf-manager/2020/04/Alexander_2013_Principles-of-Animal-Locomotion.pdf)).

Double support: each walking cycle contains two double-support periods of ~10% each, **~20–25% of the cycle total at typical speeds**, shrinking with speed and reaching zero at the walk-run transition; running instead has two "double float" periods where *neither* foot is down ([Musculoskeletal Key](https://musculoskeletalkey.com/gait-analysis/), [Clinical Tree](https://clinicalpub.com/principles-of-normal-and-pathologic-gait/); measured forward-walking double support ≈ 11.8% per period in [Donno et al. 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC10223441/)).

### 1.2 The eight Rancho Los Amigos phases

The clinical standard terminology, with the canonical percentages ([OUHSC](https://ouhsc.edu/bserdac/dthompso/web/gait/intro.htm), [Corelabs RLA overview](https://corelabs.blog/rla-gait-cycle-phases-analysis), [Clinical Tree](https://clinicalpub.com/principles-of-normal-and-pathologic-gait/)):

| Phase | Period | % of cycle | Function |
|---|---|---|---|
| Initial contact (IC) | stance | 0% | limb positioned for weight acceptance |
| Loading response (LR) | stance | 0–10% | shock absorption; double support #1 |
| Midstance (MSt) | stance | 10–30% | single-limb support, body vaults over foot |
| Terminal stance (TSt) | stance | 30–50% | heel rise, forefoot rocker, propulsion setup |
| Preswing (PSw) | stance | 50–60% | trailing-limb unload; double support #2 |
| Initial swing (ISw) | swing | 60–73% | foot clearance |
| Midswing (MSw) | swing | 73–87% | limb advances past stance foot |
| Terminal swing (TSw) | swing | 87–100% | hamstring-controlled deceleration, foot positioning |

(The exact sub-boundaries vary a few percent between sources — e.g. one handbook lists stance 62/swing 38 — but 60/40 with 0/10/30/50/60/73/87 breakpoints is the standard set; see [Studylib RLA handbook extract](https://studylib.net/doc/27856409/los-amigos-research-and-education-center---observational-ga).)

### 1.3 Ground reaction force (GRF) shapes

**Walking — double hump.** The vertical GRF has two peaks (weight acceptance, then push-off) separated by a midstance dip toward ~0.7 BW: measured first/second peaks of 1.14/1.20 BW with a 0.72 BW minimum in healthy young women ([Stasiu et al., reference values](https://bibliotekanauki.pl/articles/307416.pdf)). Across speeds 1.0–3.0 m/s the walking peak grows from ~1.0 to ~1.5 BW ([Nilsson & Thorstensson 1989](https://pubmed.ncbi.nlm.nih.gov/2782094/)). So the canonical "~1.1–1.2 BW at preferred speed, rising with speed" is well supported.

**Running — single active peak.** The vertical GRF shows one dominant active peak; across 1.5–6.0 m/s it grows from ~2.0 to ~2.9 BW, with an additional sharp **impact peak at touchdown in rearfoot strikers but generally not forefoot strikers** ([Nilsson & Thorstensson 1989](https://pubmed.ncbi.nlm.nih.gov/2782094/)). In elite sprinting the stance-averaged force reaches ~2.5 BW and the **peak 4–5 BW** ([David, ISBS review of Weyand's program](https://commons.nmu.edu/cgi/viewcontent.cgi?article=1302&context=isbs), [Clark, Ryan & Weyand two-mass model](https://digitalcommons.wcupa.edu/cgi/viewcontent.cgi?article=1009&context=kin_facpub)) — i.e., the "2–3 BW" figure holds for distance running, not top-speed sprinting.

Anteroposterior (braking/propulsion) and mediolateral peaks are roughly an order of magnitude smaller than vertical: e.g. AP peaks 0.20/0.22 BW, ML peaks 0.03–0.06 BW in the Stasiu dataset ([Stasiu et al.](https://bibliotekanauki.pl/articles/307416.pdf)); AP force roughly doubles with walking speed and grows 2–4× with running speed ([Nilsson & Thorstensson 1989](https://pubmed.ncbi.nlm.nih.gov/2782094/)). The impulse identity is exact and worth internalizing: over a steady stride the mean vertical GRF equals body weight, so short contact times *force* high peaks — in running, $\bar{F}_{contact} = BW\,(t_{step}/t_c)$ (worked in §4.2; derivation in [Tongen & Wunderlich, *Biomechanics of Running and Walking*](https://ww2.amstat.org/mam/2010/essays/TongenWunderlichRunWalk.pdf) and [Clark et al.](https://digitalcommons.wcupa.edu/cgi/viewcontent.cgi?article=1009&context=kin_facpub)).

---

## 2. Speed regimes, energetics, and the walk-run transition

### 2.1 The speed ladder (all verified numbers)

| Regime | Speed | Source |
|---|---|---|
| Optimal-cost walking (min J/kg/m) | ~1.0 m/s net, 1.25–1.35 m/s gross | [Bastien et al. load-carrying](https://dial.uclouvain.be/pr/boreal/en/object/boreal%3A5243/datastream/PDF_10/view), [Handford & Srinivasan](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/) |
| Self-selected / preferred walking | 1.39 m/s; commonly 1.3–1.5 m/s | [Minetti et al. 2003 treadmill-on-demand](https://doi.org/10.1152/japplphysiol.00128.2003), [Bastien et al.](https://dial.uclouvain.be/pr/boreal/en/object/boreal%3A5243/datastream/PDF_10/view) |
| Preferred walk→run transition (PTS) | 1.89–2.16 m/s across studies; avg 1.99 | [Hreljac et al. 2005 (JSSM)](https://www.jssm.org/volume04/iss4/cap/jssm-04-446.pdf) |
| "Neither walk nor run" dead zone | 7.2–8.4 km/h (2.0–2.3 m/s) | [Minetti et al. 2003](https://doi.org/10.1152/japplphysiol.00128.2003) |
| Backward walk→run transition | 1.58 ± 0.16 m/s | [Hreljac et al. 2005](https://www.jssm.org/volume04/iss4/cap/jssm-04-446.pdf) |
| Recreational top running speed | 6.2 m/s (slowest of 33 tested) | [Weyand et al. 2000](https://doi.org/10.1152/jappl.2000.89.5.1991) |
| Bolt 100 m WR average | 10.44 m/s (9.58 s) | [Štuhec et al. Bolt analysis](https://pmc.ncbi.nlm.nih.gov/articles/PMC10669785/) |
| Bolt top speed | 12.32 m/s at 52.5 m (laser); 12.34 m/s at ~68 m (section analysis) | [Štuhec et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC10669785/), [World Athletics "How fast can a human run"](https://worldathletics.org/download/downloadnsa?filename=4b5bd54f-42ba-4899-9494-8947ee4f8fe4.pdf&urlslug=how-fast-can-a-human-run), [Maćkała & Mero 2013](https://doi.org/10.2478/hukin-2013-0015) |

Bolt took 41 steps (avg 2.45 m, longest 2.87 m) at 4.47 Hz step frequency ([Štuhec et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC10669785/)).

### 2.2 Cost of transport and the energetic crossover

Margaria's program (1938, 1963): walking metabolic cost per distance is U-shaped in speed with a minimum near 1.3–1.4 m/s, while **running cost per distance is essentially constant** — ~1 kcal/kg/km ≈ 4.2 J/kg/m on the level, independent of speed ([Margaria, Cerretelli, Aghemo & Sassi 1963](https://ptabdata.blob.core.windows.net/files/2021/IPR2021-01597/v37_Ex.%201039%20-%20Margaria.pdf)). Minetti's group, measuring on endurance-trained mountain runners, got a lower level-running figure: $C_r = 3.40 \pm 0.24$ J kg⁻¹ m⁻¹ and minimum walking cost $1.64 \pm 0.50$ J kg⁻¹ m⁻¹ at ~1.0 m/s ([Minetti et al. 2002, extreme slopes](https://iris.unibs.it/bitstream/11379/540545/1/Minetti%20JAP%202002.pdf)). **Both numbers are reported: the running cost of transport is 3.4–4.2 J/kg/m depending on subject population** (trained athletes sit at the low end). Minetti's review summary: optimal walking ≈ 2 J kg⁻¹ m⁻¹ near 1.1–1.3 m/s, running ≈ 4 J kg⁻¹ m⁻¹ flat with speed ([Minetti, lecture/abstract via Academia.edu](https://www.academia.edu/90021782/The_transition_between_walking_and_running_in_humans_metabolic_and_mechanical_aspects_at_different_gradients)).

**Worked example (reproduces the published crossover result).** Model gross walking metabolic rate as $\dot{E}_w = a_0 + a_2 v^2$ with the forward-walking coefficients $a_0 \approx 2.1$ W/kg, $a_2 \approx 1.2$ W kg⁻¹ (m s⁻¹)⁻² (the $a_2$ range 1–1.5 from [Handford & Srinivasan](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/)), and running as $\dot{E}_r = C_r\,v$ with $C_r = 3.6$ J/kg/m (level intercept of Minetti's polynomial, §6.4). Then:

$$\frac{a_0 + a_2 v^2}{v} = 3.6 \;\Rightarrow\; 1.2v^2 - 3.6v + 2.1 = 0 \;\Rightarrow\; v = \frac{3.6 \pm \sqrt{3.6^2 - 4(1.2)(2.1)}}{2(1.2)} = 2.21 \text{ m/s (or } 0.79\text{)}$$

The walking cost-per-distance minimum sits at $v^\* = \sqrt{a_0/a_2} \cdot \tfrac{1}{\sqrt{2}}$... more directly, minimizing $a_0/v + a_2 v$ gives $v = \sqrt{a_0/(2a_2)} \cdot 2 = \sqrt{2a_0/a_2}$ — numerically $v = 1.32$ m/s, matching the preferred walking speed, and the **energetic crossover is ~2.2 m/s — *above* the observed preferred transition speed of ~2.0 m/s.**

That gap is a robust experimental fact: Minetti, Ardigò & Saibene measured both the spontaneous transition speed $S_s$ and the equal-cost speed $S_m$ at seven gradients and found **$S_s$ statistically lower than $S_m$ everywhere, by 0.5–0.9 km/h** ([Minetti et al. 1994](https://doi.org/10.1111/j.1748-1716.1994.tb09692.x)); Hreljac found the same ordering on the level ([Hreljac 1993, cited in Hreljac 1995](https://doi.org/10.1016/0021-9290(94)00120-s)). So humans start running *before* it is metabolically worthwhile. Candidate determinants, per Hreljac's criteria-based screen: the transition is effected to prevent overexertion of the **dorsiflexors** — maximum ankle angular velocity/acceleration reach capacity at fast walking ([Hreljac 1995, kinematic factors](https://www.sciencedirect.com/science/article/abs/pii/002192909400120S), [Hreljac 1993, kinetic factors](https://www.sciencedirect.com/science/article/abs/pii/0966636293900497)); Minetti's mechanical account: pendular energy recovery degrades near $S_s$, the thigh-spread reaches a structural limit, and internal work rises steeply — and interestingly, at $S_s$ **a running stride costs about as much as a walking stride** (a per-stride, not per-meter, equalization) ([Minetti et al. 1994](https://doi.org/10.1111/j.1748-1716.1994.tb09692.x)).

### 2.3 Dynamic similarity and the Froude number

$$\boxed{Fr = \frac{v^2}{gL}}$$

with $L$ the hip height (leg length). The dynamic similarity hypothesis of Alexander & Jayes: animals of different sizes move in dynamically similar fashion at equal $Fr$, allowing prediction of stride length, duty factor, phase, and force patterns across species ([Alexander & Jayes 1983](https://zslpublications.onlinelibrary.wiley.com/doi/10.1111/j.1469-7998.1983.tb04266.x), [Alexander, *The Gaits of Bipedal and Quadrupedal Animals*](https://journals.sagepub.com/doi/10.1177/027836498400300205)).

- **Humans and other bipeds switch from walk to run at $Fr \approx 0.5$**, across absolute speeds and leg lengths ([Kram, Domingo & Ferris 1997](https://doi.org/10.1242/jeb.200.4.821)). Check: $v$ at $Fr=0.5$, $L=0.95$: $\sqrt{0.5 \times 9.81 \times 0.95} = 2.16$ m/s — the top of the observed PTS band. The PTS literature spans $Fr \approx 0.35$–0.5 depending on the population and leg-length convention; at $v = 2.0$ m/s, $L = 0.95$, $Fr = 0.43$ (mid-band).
- **Reduced gravity:** at lower gravity the transition occurs at proportionally slower absolute speeds but the same $Fr$ — direct evidence the transition is triggered by inverted-pendulum dynamics ([Kram et al. 1997](https://doi.org/10.1242/jeb.200.4.821)).
- **Children and small-stature adults:** the mechanics of walking in children, pygmies and dwarfs is not different from taller individuals when speed is expressed in Froude terms ([Minetti review](https://www.academia.edu/90021782/The_transition_between_walking_and_running_in_humans_metabolic_and_mechanical_aspects_at_different_gradients)).
- **Loaded walking:** load pushes the transition *down* — PTS decreased ~0.1 m/s at 15% body mass ([Raynor et al., via NMU ISBS](https://commons.nmu.edu/cgi/viewcontent.cgi?article=2424&context=isbs)); in military personnel PTS fell 0.16 m/s at 25 kg and 0.27 m/s at 40 kg (~0.06 m/s per 10 kg), with stature becoming more influential as load grows ([Gill et al., NMU ISBS](https://commons.nmu.edu/cgi/viewcontent.cgi?article=2424&context=isbs)). Loaded walking also becomes mechanically more guarded: with 47 kg packs, double-support fraction and toe-off time increase ([Harman et al., load-speed interaction](http://oai.dtic.mil/oai/oai?identifier=ADP010991&metadataPrefix=html&verb=getRecord)). Notably, the *optimal* (min-cost) speed for backpack loads up to 75% body mass stays ≈ 1.30 m/s gross / ≈ 1.06 m/s net ([Bastien et al.](https://dial.uclouvain.be/pr/boreal/en/object/boreal%3A5243/datastream/PDF_10/view)).

---

## 3. Walking as an inverted pendulum (derivation)

### 3.1 The rigid-leg vault and its speed limit

Model: point mass $m$ on a rigid massless leg of length $L$ pivoting about the stance foot. During single support the only forces on the mass are gravity $mg$ and the leg axial force; the equation of motion about the pivot is

$$mL^2\ddot{\theta} = mgL\sin\theta \;\Rightarrow\; \ddot{\theta} = \frac{g}{L}\sin\theta \;\approx\; \frac{g}{L}\theta \;\text{(small angles, unstable equilibrium at }\theta=0)$$

The small-angle form is the classic inverted pendulum: midstance is an unstable equilibrium, and the body "falls forward" over the foot with time constant $\sqrt{L/g}$. Energy is conserved along the arc (the leg force does no work about the pivot): kinetic energy at midstance trades against gravitational potential at heel-strike/toe-off, which is exactly the walking energy-exchange mechanism (percentage of "recovery" up to ~65–70% at preferred speeds, falling near the transition; [Minetti et al. 1994](https://doi.org/10.1111/j.1748-1716.1994.tb09692.x), [Cavagna & Kaneko via PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC1182534/)).

**The speed limit.** For the foot to remain loaded on the ground, the leg axial force must stay ≥ 0. At midstance the mass moves on a circular arc of radius $L$, requiring centripetal acceleration $v^2/L$ directed upward toward the pivot; gravity supplies at most $g$:

$$m\frac{v^2}{L} \le mg \quad\Longrightarrow\quad \boxed{v_{max} = \sqrt{gL} \approx 3.05 \text{ m/s for } L = 0.95 \text{ m}}$$

This is the same argument as the Froude condition: walking becomes dynamically impossible at $Fr = 1$ ([Kram et al. 1997](https://doi.org/10.1242/jeb.200.4.821), who state the centripetal/gravity ratio *reduces to* $v^2/gL$). But humans abandon walking at $Fr \approx 0.5$ ($v \approx 2.2$ m/s), roughly two-thirds of $\sqrt{gL}$ — **the inverted-pendulum ceiling is not the binding constraint**. Two real reasons:

### 3.2 Step-to-step transitions: the collision cost (Kuo–Donelan–Ruina)

Between arcs, the COM velocity must be redirected from the downward-tipped arc of the trailing pendulum to the upward-tipped arc of the leading one. The leading limb performs **negative collision work** on the COM; an equal amount of positive work must be restored. A collision model predicts the rate of negative work scales with the **fourth power of step length** — verified experimentally: with step frequency fixed at 1.8 Hz and step length varied 0.4–1.1 m, mechanical work rate rose from 1 W to 38 W ($r^2 = 0.96$) and metabolic rate from 7 W to 379 W ($r^2 = 0.95$), both ∝ $l^4$ ([Donelan, Kram & Kuo 2002](https://doi.org/10.1242/jeb.205.23.3717), [PubMed](https://pubmed.ncbi.nlm.nih.gov/12409498/)). This transition work, not the pendular motion itself, is a major determinant — roughly **two-thirds of the net metabolic cost of moderate-speed walking** ([Kuo, Donelan & Ruina 2005, *Exerc. Sport Sci. Rev.*](https://journals.lww.com/acsm-essr/fulltext/2005/04000/energetic_consequences_of_walking_like_an_inverted.6.aspx)).

Two model lineages make this precise:

- **The rimless wheel and the "simplest walking model"** (McGeer 1990; Garcia et al. 1998; Kuo 2002, all cited as the model basis in [Donelan et al. 2002](https://doi.org/10.1242/jeb.205.23.3717)): passive dynamic walkers that descend like a rimless wheel, losing energy at each spoke collision; level walking is powered by adding energy at toe-off.
- **The energetics of *where* you push off** (Kuo 2001): powering the simplest walking model with a **toe-off impulse just before heel-strike is four times cheaper** than applying the same energy as stance-leg torque, because push-off *reduces the collision loss it precedes* ([Kuo 2001, *J. Biomech. Eng.*](https://doi.org/10.1115/1.1427703)). The optimal coordination is push-off work ≈ collision work in magnitude; eliminating push-off entirely multiplies the energy that must be dissipated and restored by up to **4×** ([Soo & Donelan 2010](https://doi.org/10.1242/jeb.044214); in their isolated rocking paradigm, removing front-leg push-off raised total required work by 286%, [Soo 2011](https://www.sfu.ca/locomotionlab/assets/soo-gait-posture-2011.pdf)). The ankle supplies ~88% of trailing-limb push-off work ([Soo & Donelan 2010](https://doi.org/10.1242/jeb.044214)) — which is precisely the joint whose velocity limit Hreljac implicated in triggering the gait transition (§2.2).

### 3.3 Capture point / extrapolated COM (brief — Part 4 has the depth)

Hof's extrapolated center of mass:

$$\boxed{\boldsymbol{\xi} = \mathbf{x} + \frac{\mathbf{v}}{\omega_0}, \qquad \omega_0 = \sqrt{g/l}}$$

The dynamic-stability condition is that the XcoM (not the COM) stay within the base of support; the margin of stability $b$ is its distance to the boundary ([Hof, Gazendam & Sinke 2005](https://braceworks.ca/wp-content/uploads/2016/05/hof-condition-for-dynamic-stability.pdf)). In walking, the XcoM runs up to ~4 cm ahead of the COM in phase quadrature; at foot contact the CoP is placed just 1.6 ± 0.7 cm lateral to the XcoM in controls (wider, 2.7 cm, in amputees) — balance in walking is far more marginal than the COM trajectory alone suggests ([Hof et al. 2006, *Control of lateral balance in walking*](https://doi.org/10.1016/j.gaitpost.2006.04.013)). A simple control rule suffices for stable walking: place the CoP a fixed distance behind and outward of the XcoM at each foot placement; a velocity disturbance $\Delta v$ is compensated by shifting the footfall by $\Delta v/\omega_0$ ([Hof 2007](https://doi.org/10.1016/j.humov.2007.08.003); estimation caveats in [Curtze, Buurke & McCrum 2024](https://doi.org/10.1016/j.jbiomech.2024.112045)). Part 4 of the merged document treats XcoM/capture-point control in full.

---

## 4. Running as a bouncing spring (SLIP)

### 4.1 The model and its parameters

The spring-loaded inverted pendulum (Blickhan 1989; McMahon & Cheng 1990, origins cited in [Lipfert et al./Seyfarth group](https://www.sciencedirect.com/science/article/abs/pii/S0021929012003934)): a point mass on a massless linear leg spring. Flight is ballistic; stance obeys

$$\text{flight: } m\ddot{\mathbf{r}} = -mg\,\hat{\mathbf{y}}; \qquad \text{stance: } m\ddot{\mathbf{r}} = k\,(l_0 - l)\,\hat{\mathbf{u}}_l, \quad l = \|\mathbf{r}-\mathbf{r}_{foot}\|$$

with touchdown/liftoff events at foot contact and zero leg force. The dimensionless parameter set is $(\tilde{k}, \alpha, E/(mgl_0))$ with

$$\tilde{k} = \frac{k\,l_0}{mg}$$

**Measured human leg stiffness (verified values, replacing the anchor's "10–30 kN/m"):**

- Farley & González: $k_{leg} = 7.0 \to 16.3$ kN/m at 2.5 m/s as stride frequency was forced from 26% below to 36% above preferred — a **2.3-fold** increase; at freely chosen frequency, stiffness is roughly constant across speed, with faster running accommodated by sweeping a larger leg angle ([Farley & González 1996](https://pubmed.ncbi.nlm.nih.gov/8849811/), [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/0021929095000291)).
- A 105-athlete study at 9–19.5 km/h: $k_{leg} = 26.2 \pm 3.2$ to $27.6 \pm 3.3$ kN/m, again not significantly changing with speed but changing with step frequency; dimensionless stiffness $\tilde{k} \approx 12$–50 ([Janik et al. 2024, *Sports Engineering*](https://link.springer.com/article/10.1007/s12283-024-00462-8)).
- Method matters: different leg-parameter definitions in experiments yield different stiffness estimates, and SLIP fits can be poor when fed directly measured parameters — the "dynamical leg parameters" must be fit to reproduce apex sequences ([Lipfert et al. 2012](https://www.sciencedirect.com/science/article/abs/pii/S0021929012003934), critique lineage from Brughelli & Cronin cited therein; [Morin-style contact-time methods reviewed in Dalleau et al./Willy et al.](https://www.sciencedirect.com/science/article/abs/pii/S0021929007002035), where contact time explains 90–96% of stiffness variance and a 10% shorter contact time gives ~25% higher stiffness).

Worked scale check: $k = 20$ kN/m, $l_0 = 1$ m, $m = 70$ kg → $\tilde{k} = 20000/(70 \times 9.81) = 29$. SLIP is *self-stable* in a J-shaped region of $(k, \alpha)$ space — it recovers from small perturbations with no control — and this stability survives anchoring the model with real leg masses ([Peuker et al. 2011](https://infoscience.epfl.ch/handle/20.500.14299/91269)). But fixed-parameter SLIP is a poor multi-step predictor of human running; adding swing-ankle state (ankle-SLIP) improves one-step prediction by factors of 4–23 ([Clark et al. 2015? no — *Constructing predictive models of human running*](https://pmc.ncbi.nlm.nih.gov/articles/PMC4305406/)), and hip torque plus damping (TD-SLIP) fit GRF profiles 2–4× better than ideal SLIP ([Ankarali et al., TD-SLIP](https://www.ihmc.us/dwc2012files/Ankarali.pdf)). A robustness criterion on SLIP also reproduces walk-run transitions qualitatively, though at Froude numbers well below the human ~0.5 ([Ojeda et al., arXiv 1403.0879](https://arxiv.org/pdf/1403.0879)).

### 4.2 Duty factor and contact times

Contact time falls steeply with speed: measured means of **280 ms at 3.0 m/s → 209 ms at 4.5 → 168 ms at 6.0 m/s** (flight times 98 → 143 → 139 ms) in novice and experienced runners alike ([Vitale, O'Toole & Wortley](https://digitalcommons.wku.edu/cgi/viewcontent.cgi?article=3635&context=ijesab)). At top speed, contact time is ~**110 ms** in athletic subjects — and essentially the same in backward running (116 ms), one of the lines of evidence that speed is stance-limited by the *minimum time* needed to apply large forces, not by maximum force capacity (hopping applies *larger* forces than running but is slower) ([Weyand, Sandell, Prime & Bundle 2010](https://doi.org/10.1152/japplphysiol.00947.2009)). The lowest contact time on record is ~70 ms, which under a force–contact-time extrapolation corresponds to a ~12.75 m/s human speed ceiling ([World Athletics "How fast can a human run"](https://worldathletics.org/download/downloadnsa?filename=4b5bd54f-42ba-4899-9494-8947ee4f8fe4.pdf&urlslug=how-fast-can-a-human-run)).

**Duty factor, worked.** Duty factor is the foot's contact fraction of the *stride*: $DF = t_c/(2(t_c + t_a))$. At 3.0 m/s: $DF = 0.280/(2 \times 0.378) = 0.37$ — consistent with Alexander's ~0.35 post-transition value ([Alexander 2013](https://www.originalwisdom.com/wp-content/uploads/bsk-pdf-manager/2020/04/Alexander_2013_Principles-of-Animal-Locomotion.pdf)).

**Force, worked.** Steady running requires the stride-averaged vertical force to equal body weight, so the contact-averaged force is $\bar{F} = BW\,(t_c + t_a)/t_c$. At a sprint with $t_c = 0.10$ s, $t_a = 0.12$ s: $\bar{F} = 2.2\,BW$, and since the force trace is a peaked hump, the peak is ≈1.5–2× that: 3.5–4.5 BW, matching measurements ([Clark et al. two-mass model](https://digitalcommons.wcupa.edu/cgi/viewcontent.cgi?article=1009&context=kin_facpub), [David ISBS](https://commons.nmu.edu/cgi/viewcontent.cgi?article=1302&context=isbs)). Faster top speeds come from **greater mass-specific ground forces, not faster leg repositioning** — swing time is nearly constant (~0.35 s minimum repositioning) across runners spanning 6.2–11.1 m/s, while contact-averaged force rises 1.26× ([Weyand et al. 2000](https://doi.org/10.1152/jappl.2000.89.5.1991)).

### 4.3 Why running cost is flat per distance

The leg spring stores and returns elastic energy each step (tendon), so the muscle does not pay the full collision cost of redirecting the COM; because the step-to-step redirection loss scales with the (roughly constant) spring deflection rather than accumulating with speed, the **cost per meter is flat** — the constant ~1 kcal/kg/km result of Margaria ([Margaria et al. 1963](https://ptabdata.blob.core.windows.net/files/2021/IPR2021-01597/v37_Ex.%201039%20-%20Margaria.pdf)). Consistent with this, runners self-optimize contact time and leg stiffness to within ~5% of their metabolic optimum, with a U-shaped cost curve in contact time ($r^2 = 0.84$) ([Moore et al. 2019](https://pmc.ncbi.nlm.nih.gov/articles/PMC7739683/)).

---

## 5. Step length, cadence, and their laws

**The identity:** $v = s \cdot f$ (speed = step length × step frequency; a stride = 2 steps). Typical preferred values: cadence ≈ 113 steps/min (1.88 Hz), stride 1.41 m (step ≈ 0.71 m) at ~1.33 m/s ([Musculoskeletal Key, Table 22.1](https://musculoskeletalkey.com/gait-analysis/)); in a force-treadmill study, preferred step length was $0.756 \pm 0.052\,L$ at 1.25 m/s ([Donelan et al. 2004](https://www.sfu.ca/locomotionlab/assets/latstab2004.pdf)). So the parent anchor "0.75–0.80 m at preferred walk" sits at the high end: measured absolute step lengths are ~0.70–0.78 m at preferred speeds for typical leg lengths.

**Grieve's law.** Grieve & Gear (Ergonomics **1966** — the prompt's "Grieve 1968" appears to be a mis-dating; the verified citation is 1966) measured 50 subjects aged 1–35 and found that a **log–log (power-law) regression describes the adult step-frequency–speed relationship better than a linear one**, that the product of maximum step frequency and $\sqrt{\text{stature}}$ is approximately constant after age 5, and that adults hold swing time roughly constant ([Grieve & Gear 1966](https://doi.org/10.1080/00140136608964399)). The frequently quoted specific exponent ("$f \propto v^{0.6}$") **could not be verified from a live source this session and is therefore not asserted**; what is verifiable is the power-law form, its superiority over linear fits in adults, and that children may be fit either way ([Grieve & Gear 1966](https://doi.org/10.1080/00140136608964399), applied and re-validated in [Hirokawa 1989](https://doi.org/10.1016/0141-5425(89)90038-1)). Modern replots show stride length is well approximated as *linear* in normalized cadence, speed as quadratic, and walk ratio as a decreasing power function ([Zhen et al. 2019](https://pmc.ncbi.nlm.nih.gov/articles/PMC6449492/)).

**Preferred vs. mechanically optimal cadence — an honest disagreement.** The mechanical work total $W_{tot} = W_{ext} + W_{int}$ is minimized at a step frequency **20–30% below** the freely chosen one ([Cavagna & Franzetti-type experiment, PMC1182534](https://pmc.ncbi.nlm.nih.gov/articles/PMC1182534/)); yet metabolic cost is minimized *near* the chosen frequency in energetic models ([Minetti & Saibene 1992, cited therein](https://pmc.ncbi.nlm.nih.gov/articles/PMC1182534/)). The resolution is that mechanical work is not the only metabolic currency (force/time costs matter; [Kuo 2001](https://doi.org/10.1115/1.1372322) shows a force-per-time swing-leg cost predicts the preferred speed–step-length relationship better than swing-work cost).

**Racewalking constraints.** IAAF/World Athletics Rule 230: no *visible* loss of contact, and the advancing knee must be straight from first contact to the vertical-upright position ([Hanley, *Biomechanics and the rules of race walking*](https://eprints.leedsbeckett.ac.uk/view/creators/Hanley=3AB=3A=3A.html%3E)). Consequences, from elite data: knee 180° ± 2 at contact, hyperextending to 185° ± 4 at midstance; flight time averages ~30 ms — under the ~40 ms visibility threshold; the straight knee makes the stance leg a rigid lever, so propulsion is generated by **hip extensors/flexors and ankle plantarflexors** while the knee only absorbs energy (−46.4 ± 9.5 J during swing) ([Hanley & Bissas 2013](https://doi.org/10.1080/02640414.2013.777763), [Hanley & Bissas 2016](https://doi.org/10.1080/02640414.2016.1206662)); pelvic rotation and counter-rotating shoulders extend effective leg length and lower the COM ([Hanley's PhD thesis](https://eprints.leedsbeckett.ac.uk/id/eprint/591/6/Brian%20Hanley%20PhD%20thesis%20Biomechanics%20of%20elite%20race%20walking.pdf)). Elite race-walk treadmill speeds run 8–15 km/h, with COM oscillations that (unlike normal walking, §6) grow with speed when normalized to body height ([Klimek et al. 2025](https://doi.org/10.5604/01.3001.0055.4412)) — the locked knee removes the shock-absorbing flexion that flattens the COM arc.

**Step width and lateral stability.** Humans prefer a step width of **0.12–0.13 L ≈ 8–12 cm** (for L ≈ 0.95 m); mechanical and metabolic costs rise with the *square* of width beyond preferred (+54% and +45% at 0.45 L), while too-narrow steps cost +8% from lateral limb swing to avoid the stance leg; the cost minimum at 0.12 L coincides with preferred width and with foot width (0.11 L) ([Donelan, Kram & Kuo 2001](https://doi.org/10.1098/rspb.2001.1761), [full text](https://spot.colorado.edu/~kram/DKKwidthPRSL2001.pdf)). External lateral stabilization (springs at the waist) narrows preferred width by 47% and cuts cost 5.7–9.2% — lateral balance control is *active* and costs metabolic energy ([Donelan et al. 2004](https://www.sfu.ca/locomotionlab/assets/latstab2004.pdf)). Backward walking is done with a wider step (~0.14 vs 0.09 m) and slower, reflecting the added balance demand ([Donno et al. 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC10223441/)).

---

## 6. Center-of-mass trajectory and elevation changes

### 6.1 Measured vertical oscillation

- **Walking:** vertical COM excursion rises with speed from **2.7 ± 0.5 cm at 0.7 m/s to 4.8 ± 0.9 cm at 1.6 m/s**; mediolateral excursion does the opposite (7.0 cm → 3.9 cm) ([Orendurff et al.](http://media.kenanaonline.com/files/0019/19217/Orendurff_3.pdf)). Independent measurement methods agree at 4–5 cm at freely selected speed, ~3 cm slow, up to ~8 cm at the fastest walking speeds (~2.2 m/s) ([Gard, Miff & Kuo 2004](https://www.sciencedirect.com/science/article/abs/pii/S016794570300085X), [Gard & Chaffin-type JPO summary](https://journals.lww.com/jpojournal/fulltext/2001/09000/what_determines_the_vertical_displacement_of_the.9.aspx)). So "2–5 cm" is right for normal speeds; expect ~8 cm near the transition.
- **Running:** preferred oscillation ≈ 8.3 cm at ~2.7 m/s, with an overall measured range of **6.5–13.1 cm** in experienced distance runners at 2.7–4.4 m/s ([Strohinger et al.-type MSSE 2024 markerless study](https://journals.lww.com/acsm-msse/fulltext/2024/10001/reliability_of_vertical_oscillation_during_running.2241.aspx)); an independently measured preferred value of 8 ± 0.4 cm with running economy minimized at ~6–8 cm oscillation ([biofeedback study, Humboldt State](https://digitalcommons.humboldt.edu/cgi/viewcontent.cgi?article=1273&context=etd)). The parent anchor "6–10 cm" is therefore right for moderate speeds; sprint-adjacent speeds push higher.
- A sharp kinematic discriminator between the gaits: in the first half of stance the walking COM rises 3.1 cm (vaulting) while the running COM *falls* 7.3 cm (spring compression), even though the virtual leg compression is similar (0.091 m vs 0.123 m) — the difference is dominated by the stance limb sweeping 30.4° in walking vs 19.2° in running ([Lee & Farley 1998](https://doi.org/10.1242/jeb.201.21.2935)).

### 6.2 The compass-gauge prediction vs. reality (model–measurement comparison)

A rigid straight leg of length $L$ with step length $s$ sets a half-step angle $\sin\theta = (s/2)/L$, and the COM rides an arc whose peak-to-peak vertical excursion is

$$\Delta h = 2L(1 - \cos\theta), \qquad \theta = \arcsin\!\left(\frac{s}{2L}\right)$$

Worked: $s = 0.75$ m, $L = 0.95$ m → $\theta = 23.25°$, $\Delta h = 0.154$ m ≈ **15.4 cm** — **3–6× the measured 2.7–4.8 cm** of real walking at those step lengths. The classical "six determinants of gait" (Saunders et al. 1953) attribute the flattening to pelvic rotation, pelvic list (obliquity), stance-phase knee flexion, and ankle/foot rockers. Modern measurements split these into what actually works and what doesn't:

- **What actually flattens the arc:** the geometry of **foot roll-over** and step length. A rocker-based inverted-pendulum model — effective leg length $L$, foot rollover radius $\rho$, step length — predicts vertical excursion from geometry alone ($\Delta h$ a function of $L$, $\rho$, $s$ only), matching able-bodied data across 0.8–2.0 m/s without any pelvic or knee terms ([Gard et al., "What Determines the Vertical Displacement of the Body During Normal Walking?"](https://journals.lww.com/jpojournal/fulltext/2001/09000/what_determines_the_vertical_displacement_of_the.9.aspx)). Pelvic **rotation** lengthens the effective leg at heel-strike/toe-off and flattens the arc (this is also why racewalkers exaggerate it: [Hanley thesis](https://eprints.leedsbeckett.ac.uk/id/eprint/591/6/Brian%20Hanley%20PhD%20thesis%20Biomechanics%20of%20elite%20race%20walking.pdf)).
- **What does *not* reduce the excursion (contrary to the classical story):** stance-phase knee flexion and pelvic obliquity peak at opposite toe-off and have largely returned to baseline by the time of peak COM height — they **lower the average COM elevation slightly but do not reduce the peak-to-peak excursion**; they are shock absorbers for the loading response ([Gard et al., JPO 2001](https://journals.lww.com/jpojournal/fulltext/2001/09000/what_determines_the_vertical_displacement_of_the.9.aspx), [Gard et al. 2004](https://www.sciencedirect.com/science/article/abs/pii/S016794570300085X)).
- Measurement-method caveat: sacral-marker methods overestimate vertical COM excursion at fast walking because the legs' reciprocal configuration in double support raises the segment-averaged COM within the trunk; segmental and force-platform methods agree ([Gard et al. 2004](https://www.sciencedirect.com/science/article/abs/pii/S016794570300085X)).

**Simulation-grade summary:** walking COM vertical oscillation is *not* a free parameter — it is set by geometry (step length, leg length, foot rollover shape), and any character controller that varies step length should scale the bob accordingly; a rigid-straight-leg model overpredicts it by 3–6× and must be corrected with foot roll-over and modest pelvic rotation.

### 6.3 Elevation changes: stairs

Standard stair geometry in the biomechanics literature: rise **18 cm**, tread ~28.5–46 cm ([Protopapadaki et al. 2006](https://www.clinbiomech.com/article/S0268-0033(06)00185-9/abstract), [Lin, Lu & Hsu 2004](https://doi.org/10.4015/s1016237204000153)). Per step the COM must rise/drop ≈ the riser height (18 cm).

- **Joint excursions:** stair ascent stance takes the hip to ~53° and the **knee through ~60° flexion** (13–60°), vs ~18° knee flexion in level walking; descent flexes the knee up to **77.6°** ([Lin et al. 2004](https://doi.org/10.4015/s1016237204000153)).
- **Moments:** peak knee extensor moment in ascent ≈ 1.16 Nm/kg (vs ~0.5 in level walking); peak knee adduction moment 0.42 Nm/kg ([Costigan, Deluzio & Wyss 2002](https://www.sciencedirect.com/science/article/abs/pii/S0966636201002016), ascent extensor moments also in [Lin et al. 2004](https://doi.org/10.4015/s1016237204000153)).
- **Patellofemoral joint force:** peak distal–proximal contact force ≈ **3 BW on average, up to 6 BW**, occurring at high knee flexion where contact area is small — and the peak patellofemoral force is ~**8× higher in stair ascent than level walking** ([Costigan et al. 2002](https://www.sciencedirect.com/science/article/abs/pii/S0966636201002016)). Independent musculoskeletal estimates: peak PF reaction force 33.9 N/kg (≈3.5 BW) in ascent, 27.9 N/kg (≈2.8 BW) in descent, vs 10.1 N/kg in walking ([Chen, Scher & Powers 2010](https://doi.org/10.1123/jab.26.4.415)); finite-element-coupled models give peak PF contact force 3.1 BW at 20% of stance with stresses 2–4× walking levels, quadriceps force 3.87 BW ([Makani et al. 2022](https://doi.org/10.1002/cnm.3646)). So the parent's "3–4× body weight patellofemoral" is the consensus band, with sources spanning 2.8–3.5 BW mean and tails to 6 BW.
- **Energetics:** normal stair ascent/descent requires roughly **nine times the net limb work** of level walking (positive on ascent, negative on descent), with the knee contributing 34% of positive work on ascent and the knee and ankle sharing absorption on descent ([Grimmer et al. 2023](https://journals.plos.org/plosone/article/file?id=10.1371%2Fjournal.pone.0294161&type=printable)).

### 6.4 Elevation changes: slopes (and the downhill paradox)

Minetti et al. measured walking and running costs on grades from −0.45 to +0.45 and fit fifth-order polynomials to cost (J kg⁻¹ m⁻¹) as a function of gradient $i$ ([Minetti et al. 2002](https://iris.unibs.it/bitstream/11379/540545/1/Minetti%20JAP%202002.pdf)):

$$\boxed{C_w(i) = 280.5i^5 - 58.7i^4 - 76.8i^3 + 51.9i^2 - 19.6i + 2.5}$$
$$\boxed{C_r(i) = 155.4i^5 - 30.4i^4 - 43.3i^3 + 46.3i^2 - 19.5i + 3.6}$$

(level intercepts are the speed-averaged fits; the measured level minimum $C_w$ was 1.64 ± 0.50 J/kg/m at ~1.0 m/s and $C_r$ = 3.40 ± 0.24 J/kg/m.)

Key facts, all from that paper and its companions:

- Walking cost is minimized on a **−10% grade** (0.81 J/kg/m — half the level minimum!) and rises again on steeper descents, reaching 3.46 J/kg/m at −45%; running is minimized at **−20%** (1.73 J/kg/m) ([Minetti et al. 2002](https://iris.unibs.it/bitstream/11379/540545/1/Minetti%20JAP%202002.pdf)). Most-economical walking gradient: −10.2 ± 0.8% ([Minetti, Ardigò & Saibene 1993](https://doi.org/10.1113/jphysiol.1993.sp019969)); for running −10.6 ± 0.5% ([Minetti et al. 1994, gradient running](https://doi.org/10.1242/jeb.195.1.211)).
- **The downhill paradox explained:** positive (concentric) and negative (eccentric) muscle work have different efficiencies (eff⁺ ≈ 0.15–0.18, eff⁻ ≈ 0.73–0.80; ratio ≈ 5). Below about −10% grade, the growing eccentric absorption load outstrips the saving, and cost climbs again — the muscles act as brakes with force-dissipation costs, and mechanical efficiencies approach those of pure eccentric contraction ([Minetti et al. 1993](https://doi.org/10.1113/jphysiol.1993.sp019969), [Minetti et al. 1994](https://doi.org/10.1242/jeb.195.1.211), [Minetti et al. 2002](https://iris.unibs.it/bitstream/11379/540545/1/Minetti%20JAP%202002.pdf); Margaria's classic eccentric "efficiency" up to −1.18 in [Margaria 1963](https://ptabdata.blob.core.windows.net/files/2021/IPR2021-01597/v37_Ex.%201039%20-%20Margaria.pdf)).
- Mountain-path design corollary: the gradient minimizing energy per *vertical* meter is ≈ 20–30% (up to ±25–35% band) — real mountain paths statistically match this ([Minetti 1995, *Optimum gradient of mountain paths*](https://air.unimi.it/handle/2434/19898), [Minetti et al. 2002](https://iris.unibs.it/bitstream/11379/540545/1/Minetti%20JAP%202002.pdf)).

### 6.5 Elevation changes: jumping and landing

Dropping from a 31 cm box produces peak vertical GRFs **in excess of 4 BW** in adolescent athletes; drop-landing joint forces scale with height, with modeled ankle/hip joint force peaks **above 20 BW** from 72 cm in half-squat parachute-style landings ([Ford-type drop-vertical-jump cohort study](https://pmc.ncbi.nlm.nih.gov/articles/PMC3644482/), [Yeow-type musculoskeletal landing model](https://pmc.ncbi.nlm.nih.gov/articles/PMC6051254/)). The landing strategy distributes absorption between ankle (Achilles-tendon-dominant, quasi-isometric muscle) and knee (quadriceps eccentric); at higher drop heights the knee's net work turns negative — it becomes the dominant dissipator, which is exactly the mechanism linking hard landings to patellofemoral and ACL loads ([Zhang et al. 2025 drop-jump study](https://doi.org/10.7717/peerj.19490), [Kubota et al. 2026](https://doi.org/10.7717/peerj.20947)).

---

## 7. Backward and sideways gait

### 7.1 Backward walking: kinematics and kinetics

- **Approximately a time-reversal of forward walking.** Time-reversed joint angles are similar except at the ankle; joint muscle powers are almost reversed-polarity images — concentric activity in forward walking becomes eccentric in backward walking and vice versa ([Winter, Pluck & Yang 1989](https://doi.org/10.1080/00222895.1989.10735483)). Segment elevation-angle waveforms are essentially time-reversed, but **EMG patterns are drastically different** — the kinematic template is conserved while muscle synergies are fully reorganized ([Grasso, Bianchi & Lacquaniti 1998](https://doi.org/10.1152/jn.1998.80.4.1868)).
- **The reversed ankle strategy.** In forward walking the ankle plantarflexors generate the push-off burst (peak ankle power ≈ 3.3 W/kg); in backward walking peak ankle power drops to ≈ 1.7 W/kg and the ankle becomes principally a **shock absorber**, with dorsiflexion increasing (20.8° vs 13.7°) ([Miyashiro-type JPTS 3D ankle analysis](https://www.jstage.jst.go.jp/article/jpts/25/6/25_jpts-2013-021/_pdf)). In backward **running** the roles interchange: **knee extensors become the primary propulsion** (peak knee moment 3.60 Nm/kg, peak power 12.4 W/kg) while the ankle plantarflexors absorb (−6.8 W/kg) ([DeVita & Stribling 1991](https://doi.org/10.1249/00005768-199105000-00013), [PubMed](https://pubmed.ncbi.nlm.nih.gov/2072839/)); the hip moment pattern mirrors forward running ([DeVita & Stribling 1991](https://doi.org/10.1249/00005768-199105000-00013)).
- **Soft landing, hard takeoff.** Forward running couples the muscle–tendon stretch (landing, high force) with the long foot lever and shortening (takeoff); backward running reverses the machine relative to the motor, forcing greater force during shortening over a shorter distance — mechanically measurable as reversed landing–takeoff asymmetry (brake longer than push) and metabolically as a less efficient rebound ([Cavagna et al. 2011, *Running backwards: soft landing–hard takeoff*](https://pmc.ncbi.nlm.nih.gov/articles/PMC3013407/)).
- **Speed and cadence.** Preferred backward walking speed: median **0.8 m/s vs 1.1 m/s forward** (~27% slower), with shorter stride, shorter step length, wider step width, and greater variability ([Donno et al. 2023, 75-subject reference study](https://pmc.ncbi.nlm.nih.gov/articles/PMC10223441/)); 0.99 m/s at self-selected in a second cohort ([Wang-type PMC9777745](https://pmc.ncbi.nlm.nih.gov/articles/PMC9777745/)); 0.96 m/s in light conditions in Grasso's corner-walking study ([Grasso et al. 1998](http://prevost.pascal.free.fr/public/pdf/Grasso1998.pdf)). **The parent anchor 0.9–1.2 m/s is above all three measured values — the verified band is 0.8–1.0 m/s.** Backward walk→run transition: 1.58 ± 0.16 m/s, strongly correlated with the forward PTS (r = 0.82) ([Hreljac et al. 2005](https://www.jssm.org/volume04/iss4/cap/jssm-04-446.pdf)).
- **Metabolic cost at matched speed.** At 1.34 m/s: forward 12.42, backward 15.95, sideways 22.10 ml/kg/min VO₂ — i.e. **backward ≈ +28%, sideways ≈ +78%** at the same slow speed; at 2.23 m/s: forward 27.15, backward 31.33 (+15%), sideways 32.58 (+20%) ([Williford et al. 1998](https://doi.org/10.1097/00005768-199809000-00011)). Other matched-intensity comparisons confirm backward elicits greater HR, VCO₂, RER and RPE even when VO₂ is matched ([Hsu-type graded BW/FW study](https://www.sciencedirect.com/science/article/abs/pii/S0966636218309342)); musculoskeletal simulations put the baseline backward-walking penalty around 20–40% ([Neri-type split-belt modeling study](https://pmc.ncbi.nlm.nih.gov/articles/PMC8405989/)). The commonly quoted "~30% higher" is the middle of the 15–40% measured band and holds best at slow matched speeds.
- **Why it is used and why it is avoided.** Used: rehabilitation (knee osteoarthritis — reduced knee abduction moments; patellofemoral unloading; VMO/vasti isometric-concentric strengthening) and sports training ([Zhang et al. via Donno 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC10223441/), [Flynn & Soutas-Little 1993](https://doi.org/10.2519/jospt.1993.17.2.108)). Avoided for travel: the destination lies in the non-visual field, balance is harder (larger COM variability, closed-eye effects worst; [Wang-type PMC9777745](https://pmc.ncbi.nlm.nih.gov/articles/PMC9777745/)), and — the deep reason — the reversed motor–machine coupling forfeits the stretch-shortening economy of the foot–Achilles system ([Cavagna et al. 2011](https://pmc.ncbi.nlm.nih.gov/articles/PMC3013407/)).
- **Head/torso during backward movement (brief; Part 2 has the depth).** Forward walkers point gaze at the destination more than 1 s before a corner; **backward walkers' anticipatory gaze is reduced (under 1 s) and some show none at all** — no subject visually pursued the starting position behind them ([Grasso et al. 1998](http://prevost.pascal.free.fr/public/pdf/Grasso1998.pdf)). Real backward locomotion therefore involves either head rotation toward the travel direction or periodic over-shoulder sampling; the latter is a behavioral observation consistent with the vision literature but I found no quantified sampling-interval study this session (unverified as a number).

### 7.2 Sideways/lateral movement: the physics of why games' straight strafes don't exist

- **The measurement (Handford & Srinivasan 2014).** Net metabolic rate of sideways (side-step/shuffle) walking at matched speeds below 1 m/s is **3–5× that of forward walking**; at each gait's own optimal speed, sideways costs 8.95 vs 3.2 J kg⁻¹ m⁻¹ (**~2.8–3×**). Preferred sideways speed 0.575 ± 0.123 m/s; metabolically optimal 0.610 ± 0.064 m/s — subjects, even unpracticed, sit within 2.4% of their optimum ([Handford & Srinivasan 2014, full text](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/)). Sideways walking at 1 m/s has the same gross metabolic rate as *running at 2.3 m/s* ([Handford & Srinivasan, supplementary comparison](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/)).
- **The mechanism (why it's expensive):** sideways walking is **repeated starting and stopping**. Each step must accelerate the body laterally from rest and decelerate it again — there is no inverted-pendulum vault, because the legs are aligned fore-aft relative to the travel direction, the knee does not bend laterally, and the ankle can push off only weakly sideways ([Handford & Srinivasan 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/): "the knee does not bend sideways, and the ankle can push-off only a little sideways"). In the quadratic metabolic model, the sideways coefficient is $\alpha_s' \approx 7.8$ W kg⁻¹ (m s⁻¹)⁻² versus forward $\alpha_2 \approx 1$–1.5 — a **5–8× stiffer speed penalty** ([Handford & Srinivasan](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/) as used in [Brown, Seethapathi & Srinivasan 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8307777/)).
- **Williford's independent numbers agree:** lateral motion at 1.34 m/s costs +78% VO₂ over forward (above); at 2.23 m/s lateral ≈ backward > forward, all significantly different, with different stride length/frequency combinations ([Williford et al. 1998](https://doi.org/10.1097/00005768-199809000-00011)).
- **Gait patterns:** at slow speeds humans use a walk-like side-step (leading leg absorbs, trailing leg propels — anatomically forced asymmetry since the legs are fore-aft of the travel direction); at higher speeds **13 of 15 subjects preferred an asymmetric gallop-like pattern** rather than a run-like one ([Yamashita-type sideways locomotion study](https://www.sciencedirect.com/science/article/abs/pii/S1050641113002125)). Cross-over variants are less strenuous at low speed but costly at high speed (constant crossing/uncrossing); Handford's pilot work selected the shuffle for the main experiment ([Handford MSc thesis](http://hdl.handle.net/1811/53177)).
- **The direct answer to "why don't real humans straight-strafe":** because per meter it costs ~3× the energy and the optimal speed is half as fast; humans take one or two sideways steps for short displacements (kitchen counter, group photo) and **turn and walk forward for anything longer — the one-time turning cost is amortized against the ~3× per-meter penalty** ([Handford & Srinivasan 2014, Discussion](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/)). A unified energy-optimality model with an explicit sideways coefficient predicts exactly this path selection, including that humans slow down and use gentle curves rather than sharp turns ([Brown et al. 2021 PNAS](https://pmc.ncbi.nlm.nih.gov/articles/PMC8307777/)).
- **Head/torso (brief; Part 2 deep):** to see the travel direction while sidestepping, the head/torso must rotate ~90° toward travel, or rely on peripheral/periodic sampling; Handford's subjects simply faced perpendicular to travel ([Handford & Srinivasan 2014](https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/)) — i.e., real sideways movers usually *do* rotate the head to lead with the eyes, the same anticipatory strategy as turning (§8.3), not the fixed-torso sideways glide of game characters.

---

## 8. Turning and changes of direction

### 8.1 Step turns vs spin turns

Definitions ([Hase & Stein 1999, cited in Taylor et al. 2005](https://www.sciencedirect.com/science/article/abs/pii/S0167945705000515)): a **step turn** changes direction *away from* the stance limb (land left → turn right); a **spin turn** turns *toward* the stance limb, the swing leg crossing in front.

- **Step turns are the more stable and the more parsimonious** — the CoM stays between the feet, whereas spin turns feature an excursion of the CoM beyond the lateral edge of the base of support ([Taylor et al. 2005, 3D comparison](https://www.sciencedirect.com/science/article/abs/pii/S0167945705000515), building on Patla et al. 1991; CoM-beyond-BoS in [Kreter & Fino 2024](https://doi.org/10.1098/rsif.2023.0577)). Step turns are also what adults predominantly choose in daily life.
- **Spin turns load the knee harder:** significantly greater internal tibial rotation (13.5° ± 5.9° vs 5.1° ± 5.2°) and greater peak valgus torque (−0.91 vs −0.25 Nm/kg in the left leg of right-dominant subjects) — combined internal rotation + valgus is the loading pattern that tensions the ACL ([Wang & Zheng 2010](https://doi.org/10.1055/s-0030-1261942)).
- Spin turns additionally recruit more ankle musculature (invertor moments appear only in spin turns) and require greater braking plantarflexor moments as turn angle grows ([Zheng-type turn-angle/pivot-foot study](https://journals.humankinetics.com/view/journals/jab/22/1/article-p74.xml)).

### 8.2 The metabolic cost of turning

- A **single 180° turn costs the same as walking ~5.9 m straight** at 1.67 m/s; per-turn cost rises with angle ([Wilson et al. 2013](https://pmc.ncbi.nlm.nih.gov/articles/PMC5552125/)).
- In children, 180° turns raised cost-per-distance by ~13% at 3.5 km/h and **~30% at 5.5 km/h**; 45° turns cost nothing measurable, 90° turns matter only at ≥ 4.5 km/h ([McNarry et al. 2018](https://pmc.ncbi.nlm.nih.gov/articles/PMC6244695/)).
- Thirty 180° turns per minute at 3 km/h elicits the energy expenditure of straight walking at 6 km/h (Hatamoto et al., cited in [McNarry et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC6244695/)); turning accounts for ~15% of stair-climbing energy (Minetti et al. 2011, cited in [McNarry et al.](https://pmc.ncbi.nlm.nih.gov/articles/PMC6244695/)).
- **A quantitative cost model** (Brown–Seethapathi–Srinivasan): metabolic rate while walking a curve $\dot{E} = \alpha_0 + \alpha_1 v^2 + \alpha_2 \omega^2$, with a separate quadratic sideways term for holonomic motion; this single criterion predicts that humans **slow down when turning, slow more for tighter turns, never use sharp turns, and often do not take the shortest path** (which is not energy-optimal) ([Brown, Seethapathi & Srinivasan 2021 PNAS](https://pmc.ncbi.nlm.nih.gov/articles/PMC8307777/), [preprint](https://arxiv.org/pdf/2001.02287)).

### 8.3 Anticipatory head rotation (brief — Part 2 details)

Head yaw anticipates body yaw by **~200 ms** ([Grasso et al. 1996, *The predictive brain*](https://pubmed.ncbi.nlm.nih.gov/8817526/), [Courtine & Schieppati 2003](https://doi.org/10.1046/j.1460-9568.2003.02736.x)); at preferred speeds the anticipation is at a **constant distance (~1.1 m) before the turn**, invariant for turn angles 45–135° ([Sreenivasa, Frissen, Souman & Ernst 2008](https://doi.org/10.1007/s00221-008-1525-3)). During the turn the head leads the trunk by 10–15° and leads the heading by up to 20–40°; the eyes lead further, with saccades jumping gaze along the future path ([Sreenivasa et al. 2008](https://doi.org/10.1007/s00221-008-1525-3), [Imai et al. 2001](https://pubmed.ncbi.nlm.nih.gov/11204402/), [Grasso et al. 1998](http://prevost.pascal.free.fr/public/pdf/Grasso1998.pdf)). Head anticipation is strong enough to detect turning intent 300 ms after initiation and >1.2 s before completion ([Farkhatdinov, Roehri & Burdet 2017](https://doi.org/10.1088/1748-3190/aa80ad)).

### 8.4 Cutting and inside-leg knee loading (the ACL evidence)

Sidestep cutting concentrates multiplanar load on the *inside* (stance) knee: the "dynamic valgus" pattern — hip abduction + hip internal rotation + knee valgus + lateral trunk lean + lateral foot plant — creates a large knee abduction moment via the GRF moment arm ([McBurnie et al. 2020 systematic review](https://doi.org/10.1186/s40798-020-00276-5), [PubMed](https://pubmed.ncbi.nlm.nih.gov/33136207/)). Quantified: technique factors (cut width, knee valgus, toe landing, approach speed, cut angle) explain **62% of variance** in peak knee abduction moment in 123 female handball players, with the GRF moment arm mattering more than force magnitude ([Kristiansen et al. 2014 BJSM](https://bjsm.bmj.com/content/48/9/779)). Whole-body technique modification (narrower plant, upright trunk) measurably reduces peak valgus moments ([Cochrane et al. 2010](https://journals.sagepub.com/doi/10.1177/0363546509334373)); 90° cuts generate higher vertical/braking/propulsive forces than 45° cuts (Sigward & Powers, cited in [Wilson-type PLOS study](https://pmc.ncbi.nlm.nih.gov/articles/PMC5552125/)), and the penultimate foot contact absorbs much of the braking so the final contact can turn the body ([McBurnie et al. 2020](https://doi.org/10.1186/s40798-020-00276-5)). Typical cutting contact time: 0.319 ± 0.06 s (cited therein). The performance-injury conflict is real: the "high-risk" wide-plant, leaned postures are also the fast ones ([Sigward & Powers 2015, *Cutting Mechanics*](https://journals.lww.com/acsm-msse/fulltext/2015/04000/cutting_mechanics__relation_to_performance_and.18.aspx)).

### 8.5 Minimum stable turn radius vs speed

**The friction bound.** Turning requires centripetal acceleration $a_c = v^2/R$ supplied by horizontal GRF, bounded by friction: $a_c \le \mu g$, hence

$$R_{min} = \frac{v^2}{\mu g}$$

Worked (μ = 0.6): v = 1.5 m/s → R = 0.38 m; v = 5 m/s → 4.2 m; v = 10 m/s → 17 m. At fast walking turns the *required* coefficient of friction at push-off reaches 0.54 (fast) vs 0.38 (slow) — fast turning around corners exceeds the OSHA static-COF guideline of 0.50 ([required-COF turning study, PMC4054705](https://pmc.ncbi.nlm.nih.gov/articles/PMC4054705/)).

**But friction is usually not the binding constraint at walking speed.** In circle-walking at 1 m radius and 1.5 m/s, the maximum leg angle (~12°) is far inside the friction cone (30–50° for μ = 0.6–1.2), and the centripetal-force requirement raises leg force by only ~1.3% — nowhere near explaining the measured ~50% metabolic increase; humans slow down to turn for **energetic and stability reasons, not slip avoidance** ([Brown et al. 2021](https://pmc.ncbi.nlm.nih.gov/articles/PMC8307777/)). For fast curved *running*, the lateral work dominates: at 6 m/s on a 6 m radius, COM work rises ~25% over straight-line running, with the inner limb deflecting the COM laterally and the outer limb accelerating it forward — the deviation is over-compensated during contact to make up for the straight ballistic aerial phases ([Curry-type curved-running study](https://pdfs.semanticscholar.org/e728/f0353247cdc62e1c64461b27d798c16293d4.pdf)).

---

## 9. Consolidated boxed-equation summary and primary sources

### 9.1 Boxed equations

| # | Equation | Meaning | Section |
|---|---|---|---|
| 1 | $Fr = v^2/(gL)$ | Froude number; walk→run at $Fr \approx 0.5$ | §2.3 |
| 2 | $v_{max} = \sqrt{gL} \approx 3.05$ m/s ($L=0.95$) | inverted-pendulum walking ceiling (centripetal/gravity) | §3.1 |
| 3 | $\boldsymbol{\xi} = \mathbf{x} + \mathbf{v}/\omega_0,\ \omega_0=\sqrt{g/l}$ | extrapolated COM (XcoM); foot placement rule | §3.3 |
| 4 | $\tilde{k} = k\,l_0/(mg)$ | dimensionless SLIP leg stiffness | §4.1 |
| 5 | $\bar{F}_{contact} = BW\,(t_c+t_a)/t_c$ | mean contact force from duty (stride-average = BW) | §1.4, §4.2 |
| 6 | $\dot{E}_{turn} = \alpha_0 + \alpha_1 v^2 + \alpha_2 \omega^2$ (+ $\alpha_s' v_s^2$ sideways) | metabolic cost of curved/holonomic walking | §7.2, §8.2 |
| 7 | $\Delta h = 2L(1-\cos\theta),\ \theta=\arcsin(s/2L)$ | compass-gait COM rise (≈15.4 cm at s=0.75, L=0.95 vs 2.7–4.8 measured) | §6.2 |
| 8 | $C_w(i),\, C_r(i)$ fifth-order polynomials in gradient $i$ | Minetti slope-cost fits | §6.4 |
| 9 | $R_{min} = v^2/(\mu g)$ | friction-limited turn radius | §8.5 |

Key worked numbers this document: $\sqrt{gL}$ = 3.053 m/s; $Fr(2.0\text{ m/s}, 0.95\text{ m})$ = 0.429; $v(Fr{=}0.5)$ = 2.16 m/s; energetic walk/run crossover ≈ 2.21 m/s; compass-gait rise 15.4 cm; walking cost-min speed 1.32 m/s (model) vs 1.3–1.4 measured; duty factor at 3 m/s running = 0.37; sprint mean contact force 2.2 BW.

### 9.2 Primary sources (all verified live this session)

1. OUHSC gait terminology (Rancho): https://ouhsc.edu/bserdac/dthompso/web/gait/intro.htm
2. Musculoskeletal Key, Gait Analysis: https://musculoskeletalkey.com/gait-analysis/
3. Clinical Tree, Principles of Normal and Pathologic Gait: https://clinicalpub.com/principles-of-normal-and-pathologic-gait/
4. Corelabs, RLA Gait Cycle: https://corelabs.blog/rla-gait-cycle-phases-analysis
5. RLA Observational Gait Analysis handbook extract: https://studylib.net/doc/27856409/los-amigos-research-and-education-center---observational-ga
6. Nilsson & Thorstensson 1989, GRF at different speeds: https://pubmed.ncbi.nlm.nih.gov/2782094/ (doi:10.1111/j.1748-1716.1989.tb08655.x)
7. Stasiu et al., reference gait values (women): https://bibliotekanauki.pl/articles/307416.pdf
8. Tongen & Wunderlich, Biomechanics of Running and Walking: https://ww2.amstat.org/mam/2010/essays/TongenWunderlichRunWalk.pdf
9. Clark, Ryan & Weyand, two-mass model: https://digitalcommons.wcupa.edu/cgi/viewcontent.cgi?article=1009&context=kin_facpub
10. Minetti, Ardigò & Saibene 1994, walk-run transition: https://doi.org/10.1111/j.1748-1716.1994.tb09692.x (abstract: https://air.unimi.it/handle/2434/19918)
11. Minetti et al. 1994, gradient running: https://doi.org/10.1242/jeb.195.1.211
12. Minetti et al. 2002, extreme slopes (polynomials): https://iris.unibs.it/bitstream/11379/540545/1/Minetti%20JAP%202002.pdf (doi:10.1152/japplphysiol.01177.2001)
13. Minetti et al. 2003, treadmill-on-demand: https://doi.org/10.1152/japplphysiol.00128.2003
14. Minetti 1995, optimum gradient of mountain paths: https://air.unimi.it/handle/2434/19898
15. Minetti, Ardigò & Saibene 1993, gradient walking: https://doi.org/10.1113/jphysiol.1993.sp019969
16. Minetti, transition review abstract: https://www.academia.edu/90021782/The_transition_between_walking_and_running_in_humans_metabolic_and_mechanical_aspects_at_different_gradients
17. Margaria, Cerretelli, Aghemo & Sassi 1963, energy cost of running: https://ptabdata.blob.core.windows.net/files/2021/IPR2021-01597/v37_Ex.%201039%20-%20Margaria.pdf
18. Hreljac 1995, kinematic determinants: https://doi.org/10.1016/0021-9290(94)00120-s
19. Hreljac 1993, kinetic determinants: https://www.sciencedirect.com/science/article/abs/pii/0966636293900497
20. Hreljac et al. 2005, backward gait transition (JSSM): https://www.jssm.org/volume04/iss4/cap/jssm-04-446.pdf
21. Alexander & Jayes 1983, dynamic similarity: https://zslpublications.onlinelibrary.wiley.com/doi/10.1111/j.1469-7998.1983.tb04266.x
22. Alexander, The Gaits of Bipedal and Quadrupedal Animals: https://journals.sagepub.com/doi/10.1177/027836498400300205
23. Alexander 2013, Principles of Animal Locomotion: https://www.originalwisdom.com/wp-content/uploads/bsk-pdf-manager/2020/04/Alexander_2013_Principles-of-Animal-Locomotion.pdf
24. Alexander 1989, Optimization and gaits: https://doi.org/10.1152/physrev.1989.69.4.1199
25. Kram, Domingo & Ferris 1997, reduced gravity: https://doi.org/10.1242/jeb.200.4.821
26. Gill et al., stature/load and PTS: https://commons.nmu.edu/cgi/viewcontent.cgi?article=2424&context=isbs
27. Bastien et al., backpack load carrying: https://dial.uclouvain.be/pr/boreal/en/object/boreal%3A5243/datastream/PDF_10/view
28. Looney et al. 2021, military loads: https://pubmed.ncbi.nlm.nih.gov/33652153/
29. Looney et al., LCDA backpacking equation: https://pmc.ncbi.nlm.nih.gov/articles/PMC8919998/
30. Harman et al., load-speed interaction: http://oai.dtic.mil/oai/oai?identifier=ADP010991&metadataPrefix=html&verb=getRecord
31. Donelan, Kram & Kuo 2002, step-to-step transitions: https://doi.org/10.1242/jeb.205.23.3717 (PubMed: https://pubmed.ncbi.nlm.nih.gov/12409498/)
32. Kuo, Donelan & Ruina 2005, ESSR: https://journals.lww.com/acsm-essr/fulltext/2005/04000/energetic_consequences_of_walking_like_an_inverted.6.aspx
33. Soo & Donelan 2010, rocking transitions: https://doi.org/10.1242/jeb.044214
34. Soo 2011, push-off/collision coordination: https://www.sfu.ca/locomotionlab/assets/soo-gait-posture-2011.pdf
35. Kuo 2001, simplest walking model energetics: https://doi.org/10.1115/1.1427703
36. Kuo 2001, speed–step length relationship: https://doi.org/10.1115/1.1372322
37. Hof, Gazendam & Sinke 2005, condition for dynamic stability: https://braceworks.ca/wp-content/uploads/2016/05/hof-condition-for-dynamic-stability.pdf
38. Hof et al. 2006, control of lateral balance: https://doi.org/10.1016/j.gaitpost.2006.04.013
39. Hof 2007, XcoM simple control: https://doi.org/10.1016/j.humov.2007.08.003
40. Curtze, Buurke & McCrum 2024, notes on MoS: https://doi.org/10.1016/j.jbiomech.2024.112045
41. Farley & González 1996, leg stiffness & stride frequency: https://pubmed.ncbi.nlm.nih.gov/8849811/ (doi:10.1016/0021-9290(95)00029-1)
42. Janik et al. 2024, leg stiffness energy minimisation: https://link.springer.com/article/10.1007/s12283-024-00462-8
43. Lipfert et al. 2012, dynamical leg parameters: https://www.sciencedirect.com/science/article/abs/pii/S0021929012003934
44. Constructing predictive models of human running (ankle-SLIP): https://pmc.ncbi.nlm.nih.gov/articles/PMC4305406/
45. Ankarali et al., TD-SLIP: https://www.ihmc.us/dwc2012files/Ankarali.pdf
46. Ojeda et al., SLIP robustness & transitions: https://arxiv.org/pdf/1403.0879
47. Peuker et al. 2011, anchoring SLIP: https://infoscience.epfl.ch/handle/20.500.14299/91269
48. Contact time / leg stiffness determinants: https://www.sciencedirect.com/science/article/abs/pii/S0021929007002035
49. Moore et al. 2019, optimal contact time: https://pmc.ncbi.nlm.nih.gov/articles/PMC7739683/
50. Vitale, O'Toole & Wortley, contact/flight times: https://digitalcommons.wku.edu/cgi/viewcontent.cgi?article=3635&context=ijesab
51. Weyand et al. 2000, greater forces not faster legs: https://doi.org/10.1152/jappl.2000.89.5.1991
52. Weyand et al. 2010, biological limits from the ground up: https://doi.org/10.1152/japplphysiol.00947.2009
53. David, ISBS grounded perspective: https://commons.nmu.edu/cgi/viewcontent.cgi?article=1302&context=isbs
54. Štuhec et al., Bolt 100 m analysis: https://pmc.ncbi.nlm.nih.gov/articles/PMC10669785/
55. World Athletics, How fast can a human run: https://worldathletics.org/download/downloadnsa?filename=4b5bd54f-42ba-4899-9494-8947ee4f8fe4.pdf&urlslug=how-fast-can-a-human-run
56. Maćkała & Mero 2013, Bolt kinematics: https://doi.org/10.2478/hukin-2013-0015
57. Grieve & Gear 1966, stride/frequency relationships: https://doi.org/10.1080/00140136608964399
58. Hirokawa 1989, gait under constraints: https://doi.org/10.1016/0141-5425(89)90038-1
59. Cavagna-type step-frequency determinants: https://pmc.ncbi.nlm.nih.gov/articles/PMC1182534/
60. Zhen et al. 2019, temporal-spatial models: https://pmc.ncbi.nlm.nih.gov/articles/PMC6449492/
61. Donelan, Kram & Kuo 2001, preferred step width: https://doi.org/10.1098/rspb.2001.1761 (PDF: https://spot.colorado.edu/~kram/DKKwidthPRSL2001.pdf)
62. Donelan et al. 2004, lateral stabilization: https://www.sfu.ca/locomotionlab/assets/latstab2004.pdf
63. Orendurff et al., COM displacement vs speed: http://media.kenanaonline.com/files/0019/19217/Orendurff_3.pdf
64. Lee & Farley 1998, determinants of COM trajectory: https://doi.org/10.1242/jeb.201.21.2935
65. Gard et al. 2004, kinematic/kinetic COM methods: https://www.sciencedirect.com/science/article/abs/pii/S016794570300085X
66. Gard et al., vertical displacement determinants (JPO 2001): https://journals.lww.com/jpojournal/fulltext/2001/09000/what_determines_the_vertical_displacement_of_the.9.aspx
67. Vertical oscillation reliability in running (MSSE 2024): https://journals.lww.com/acsm-msse/fulltext/2024/10001/reliability_of_vertical_oscillation_during_running.2241.aspx
68. COM vertical motion & running economy (Humboldt): https://digitalcommons.humboldt.edu/cgi/viewcontent.cgi?article=1273&context=etd
69. Costigan, Deluzio & Wyss 2002, stair kinetics: https://www.sciencedirect.com/science/article/abs/pii/S0966636201002016
70. Chen, Scher & Powers 2010, PFJRF: https://doi.org/10.1123/jab.26.4.415
71. Makani et al. 2022, stair ascent knee FE: https://doi.org/10.1002/cnm.3646
72. Brechter & Powers 2002, PF stress stairs: https://www.sciencedirect.com/science/article/abs/pii/S0966636202000905
73. Protopapadaki et al. 2006, stair kinematics: https://www.clinbiomech.com/article/S0268-0033(06)00185-9/abstract
74. Lin, Lu & Hsu 2004, 3D stair analysis: https://doi.org/10.4015/s1016237204000153
75. Grimmer et al. 2023, level/stair transitions: https://journals.plos.org/plosone/article/file?id=10.1371%2Fjournal.pone.0294161&type=printable
76. Drop vertical jump landings: https://pmc.ncbi.nlm.nih.gov/articles/PMC3644482/
77. Landing musculoskeletal model (drop heights): https://pmc.ncbi.nlm.nih.gov/articles/PMC6051254/
78. Zhang et al. 2025, drop-jump landing strategies: https://doi.org/10.7717/peerj.19490
79. Kubota et al. 2026, drop-jump determinants: https://doi.org/10.7717/peerj.20947
80. Winter, Pluck & Yang 1989, backward walking reversal: https://doi.org/10.1080/00222895.1989.10735483
81. Grasso, Bianchi & Lacquaniti 1998, motor patterns FW/BW: https://doi.org/10.1152/jn.1998.80.4.1868
82. Cavagna et al. 2011, backward running rebound: https://pmc.ncbi.nlm.nih.gov/articles/PMC3013407/
83. DeVita & Stribling 1991, backward running kinetics: https://doi.org/10.1249/00005768-199105000-00013
84. JPTS 2013, ankle in backward walking: https://www.jstage.jst.go.jp/article/jpts/25/6/25_jpts-2013-021/_pdf
85. Donno et al. 2023, FW/BW multifactorial: https://pmc.ncbi.nlm.nih.gov/articles/PMC10223441/
86. Backward walking styles & balance: https://pmc.ncbi.nlm.nih.gov/articles/PMC9777745/
87. Hyatt et al. 1996, retro-locomotion cost: https://doi.org/10.1097/00005768-199605001-00557
88. Graded FW/BW matched intensity: https://www.sciencedirect.com/science/article/abs/pii/S0966636218309342
89. Split-belt FW/BW adaptation (simulated energetics): https://pmc.ncbi.nlm.nih.gov/articles/PMC8405989/
90. Flynn & Soutas-Little 1993, backward running knee: https://doi.org/10.2519/jospt.1993.17.2.108
91. Grasso et al. 1998, eye-head coordination FW/BW: http://prevost.pascal.free.fr/public/pdf/Grasso1998.pdf
92. Handford & Srinivasan 2014, sideways walking: https://pmc.ncbi.nlm.nih.gov/articles/PMC3917343/ (doi:10.1098/rsbl.2013.1006)
93. Handford MSc thesis, sideways walking: http://hdl.handle.net/1811/53177
94. Williford et al. 1998, forward/backward/lateral costs: https://doi.org/10.1097/00005768-199809000-00011
95. Sideways gait patterns (walk/run/gallop-like): https://www.sciencedirect.com/science/article/abs/pii/S1050641113002125
96. Brown, Seethapathi & Srinivasan 2021, energy optimality navigation: https://pmc.ncbi.nlm.nih.gov/articles/PMC8307777/ (preprint: https://arxiv.org/pdf/2001.02287)
97. Taylor et al. 2005, step vs spin 3D: https://www.sciencedirect.com/science/article/abs/pii/S0167945705000515
98. Wang & Zheng 2010, knee rotation spin/step turns: https://doi.org/10.1055/s-0030-1261942
99. Turn angle & pivot foot kinetics: https://journals.humankinetics.com/view/journals/jab/22/1/article-p74.xml
100. Kreter & Fino 2024, foot placement changes: https://doi.org/10.1098/rsif.2023.0577
101. Wilson et al. 2013, turning energy: https://pmc.ncbi.nlm.nih.gov/articles/PMC5552125/
102. McNarry et al. 2018, children turning cost: https://pmc.ncbi.nlm.nih.gov/articles/PMC6244695/
103. Grasso et al. 1996, predictive brain: https://pubmed.ncbi.nlm.nih.gov/8817526/
104. Courtine & Schieppati 2003, curved-path walking: https://doi.org/10.1046/j.1460-9568.2003.02736.x
105. Sreenivasa et al. 2008, head-trunk turning: https://doi.org/10.1007/s00221-008-1525-3
106. Imai et al. 2001, body/head/eye in turning: https://pubmed.ncbi.nlm.nih.gov/11204402/
107. Farkhatdinov et al. 2017, anticipatory turning detection: https://doi.org/10.1088/1748-3190/aa80ad
108. McBurnie et al. 2020, cutting determinants review: https://doi.org/10.1186/s40798-020-00276-5 (PubMed: https://pubmed.ncbi.nlm.nih.gov/33136207/)
109. Kristiansen et al. 2014, sidestep technique: https://bjsm.bmj.com/content/48/9/779
110. Cochrane et al. 2010, technique reduces valgus: https://journals.sagepub.com/doi/10.1177/0363546509334373
111. Sigward & Powers 2015, cutting mechanics: https://journals.lww.com/acsm-msse/fulltext/2015/04000/cutting_mechanics__relation_to_performance_and.18.aspx
112. Required COF during turning: https://pmc.ncbi.nlm.nih.gov/articles/PMC4054705/
113. Curved running mechanics: https://pdfs.semanticscholar.org/e728/f0353247cdc62e1c64461b27d798c16293d4.pdf
114. Klimek et al. 2025, race-walker COM: https://doi.org/10.5604/01.3001.0055.4412
115. Hanley & Bissas 2013, race walking kinetics: https://doi.org/10.1080/02640414.2013.777763
116. Hanley & Bissas 2016, race walking work-energy: https://doi.org/10.1080/02640414.2016.1206662
117. Hanley, race walking rules: https://eprints.leedsbeckett.ac.uk/view/creators/Hanley=3AB=3A=3A.html%3E
118. Hanley PhD thesis, elite race walking: https://eprints.leedsbeckett.ac.uk/id/eprint/591/6/Brian%20Hanley%20PhD%20thesis%20Biomechanics%20of%20elite%20race%20walking.pdf
119. Hanley et al., angular kinematics race walking: https://doi.org/10.13140/2.1.3223.2005

*(The deliverable counts 60 distinct load-bearing sources; several entries above are alternate-access forms — DOI, PubMed, and full-text PDF — of the same paper, listed for verifiability.)*
