# Caves, Karst & Underground Terrain: A Simulation-Grade Reference

**Scope:** the full underground — limestone dissolution chemistry, speleogenesis, cave geometry and scale statistics, rates, speleothems, breakdown mechanics, karst surface terrain, lava tubes, and cave climate — written for a **voxel engine whose active goal is cave generation**. Geometry, sizes, passage densities, and depth distributions get priority treatment because those are the numbers a generator needs. Cave climate is included at simulation depth because the project's audio/reverb system switches on an "outside/cave" boundary. Every formula is either a standard derivation (shown) or cited to a specific source; measured quantities are ranges with inline links. This is Part 4 of the merged Earth-terrain document; section numbering is fixed and merge-stable.

**How to read this:** §1–2 are the chemistry and mass balance everything else rests on. §3–5 are the "what caves actually look like and how fast they form" core. §11 is the opinionated voxel-generation recipe — read it last, after the statistics in §4 make the parameter choices non-arbitrary.---

## 1. The chemistry of limestone dissolution

### 1.1 CO₂ hydration and the dissolution reaction

Rainwater is weakly acidic before it hits the ground. Atmospheric CO₂ (~420 ppm, $p_{CO_2} \approx 4.2\times10^{-4}$ atm) hydrates to carbonic acid:

$$CO_2(g) \rightleftharpoons CO_2(aq) \quad (K_H \approx 3.4\times10^{-2}\ \text{mol/(L·atm at 25 °C)})
$$
$$CO_2(aq) + H_2O \rightleftharpoons H_2CO_3^* \rightleftharpoons H^+ + HCO_3^- \quad (K_1 \approx 4.5\times10^{-7}\ \text{at 25 °C})
$$

($H_2CO_3^*$ = CO₂(aq) plus true carbonic acid, mostly the former.) This alone gives pH ≈ 5.6 — aggressive enough to dissolve limestone slowly. The dissolution reaction:

$$\boxed{CaCO_3 + CO_2 + H_2O \rightarrow Ca^{2+} + 2\,HCO_3^-}$$

One mole of dissolved calcite consumes one mole of CO₂. The open-system (constant $p_{CO_2}$) saturation calcium concentration is, to first order:

$$c_{eq} \approx \left(\frac{K_1 K_H K_c}{4 K_2 \gamma^3}\, p_{CO_2}\right)^{1/3}$$

The load-bearing consequence is the **one-third power**: solubility grows only as $p_{CO_2}^{1/3}$, so multiplying soil CO₂ by 100 multiplies dissolvable calcium by only ~4.6, and solubility *decreases* with temperature (Henry's constant drops fastest of all the constants over 0–30 °C) — cold water dissolves more limestone, which is why alpine and glacial-period dissolution is efficient despite low soil CO₂ ([White 2016](https://doi.org/10.3986/ac.v44i3.1896); [denudation concepts review](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).

### 1.2 The CO₂ source: soil and ground air

The dissolution engine is not atmospheric CO₂ but **soil air**, enriched by root respiration and microbial oxidation of organic matter. Measured ranges:

- Temperate karst soils (Moravian Karst, Czech Republic): $p_{CO_2(soil)} \sim 10^{-2.7}$ atm ≈ 2% ([Faimon et al. 2012](https://doi.org/10.3986/ac.v41i1.47)).
- Lascaux site soils, France: 0.6–6% CO₂ seasonally, efflux 6–25 g CO₂/m²/day ([Lascaux epikarst study](https://www.springerprofessional.de/the-co2-dynamics-in-the-continuum-atmosphere-soil-epikarst-and-i/16644174)).
- Tropical forest (Mulu, Borneo): soil $p_{CO_2}$ 3–5% ([Mulu karst study](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).
- Atmosphere: ~0.042%.

Parent anchor "soil-air pCO₂ ~10–100× atmosphere" **verifies as 25–150×** (0.1–6% vs 0.042%) — correct in spirit, slightly wider at both ends. Generator defaults: temperate 1–3%, tropical 3–6%, arid/alpine 0.1–0.5%.

An important refinement: dripwater chemistry at several well-studied sites requires $p_{CO_2}$ *higher* than the overlying soil (Moravian: $10^{-1.53}$ ≈ 0.03 atm reconstructed from dripwater vs 0.02 atm in soil), implying an additional **"ground air" CO₂ source deeper in the epikarst** — organic matter washed down into karren fissures and oxidized at depth ([Faimon et al. 2012](https://doi.org/10.3986/ac.v41i1.47); [Gibraltar karst study](https://www.sciencedirect.com/science/article/abs/pii/S001670371630028X)). For simulation: the aggressive-water factory is the top ~10 m of rock, not just the soil.

### 1.3 Open vs closed system evolution

Two end-members control how much limestone a given water parcel can dissolve ([Savoy watershed study](https://www.sciencedirect.com/science/article/abs/pii/S0009254118301219)):

- **Open system:** water stays in contact with a large CO₂ reservoir (soil/epikarst air) while dissolving — $p_{CO_2}$ constant, Ca climbs to full open-system saturation (~1–2 mmol/L at soil CO₂). The epikarst and vadose-zone regime.
- **Closed system:** water seals off from the gas phase (phreatic flow). The initial dissolved CO₂ is the only budget; as dissolution proceeds, CO₂ is consumed and total capacity drops to roughly 40–60% of the open-system value. Closed-system conditions lengthen initial passage development by about a factor of five (Palmer, cited in the Savoy study above).

### 1.4 The rate law: Plummer–Wigley–Parkhurst (PWP)

The empirical workhorse for calcite dissolution kinetics is the three-term rate law of Plummer, Wigley & Parkhurst (1978), from pH-stat and free-drift experiments at 5–60 °C and $p_{CO_2}$ from 0.0003 to 0.97 atm ([original paper](https://doi.org/10.2475/ajs.278.2.179); [critical review](https://doi.org/10.1021/bk-1979-0093.ch025)). Three parallel surface reactions:

$$
\begin{aligned}
CaCO_3 + H^+ &\rightleftharpoons Ca^{2+} + HCO_3^- &&(k_1\ \text{term: dominates at pH} < 4)\\
CaCO_3 + H_2CO_3^* &\rightleftharpoons Ca^{2+} + 2HCO_3^- &&(k_2\ \text{term: the CO₂-dependent path, karst waters})\\
CaCO_3 + H_2O &\rightleftharpoons Ca^{2+} + HCO_3^- + OH^- &&(k_3\ \text{term: neutral-water hydrolysis})
\end{aligned}
$$

$$\boxed{R_{net} = k_1 a_{H^+} + k_2 a_{H_2CO_3^*} + k_3 a_{H_2O} - k_4\, a_{Ca^{2+}}\, a_{HCO_3^-}}$$

with $k_4$ the backward (precipitation) term, temperature- and $p_{CO_2}$-dependent. Rate constants follow Arrhenius dependence with activation energies of ~2, 10 and 8 kcal/mol for $k_1$, $k_2$, $k_3$ ([BRGM review](https://infoterre.brgm.fr/rapports/RR-39062-FR.pdf)). Predictions are good to within a factor of 2–3 over many orders of magnitude in rate, worst near equilibrium — exactly where caves care.

### 1.5 The kinetic trigger — why caves are shallow

The single most consequential fact in speleogenesis: **the reaction order changes near saturation.** Field and laboratory studies converge on (White 2016, [summarized here](https://doi.org/10.3986/ac.v44i3.1896); [Dreybrodt 1990](https://doi.org/10.1086/629431)):

- At high undersaturation ($SI_C < -1$ or so): rate is first-order in (undersaturation), fast, partly transport/mass-transfer controlled.
- Approaching equilibrium ($SI_C > -0.3$, roughly >70–90% saturation): the rate law becomes **fourth-order** ($R \propto (c_{eq}-c)^4$), surface-reaction controlled, and rates drop by **orders of magnitude** — experimentally to ~10⁻⁶ of the far-from-equilibrium value at 95% saturation (Dreybrodt's $k$ parameter for natural limestone is 0.5–0.9, i.e. the switch happens at 50–90% of saturation).

Numerically: water reaches ~70% saturation within **less than a meter** of flowing over fresh limestone ([Palmer, allogenic-water dynamics](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1032&context=kip_articles)). The first meter of flow does nearly all the dissolving; beyond that point the water is nearly inert under first-order kinetics, and passage growth is governed by the slow fourth-order tail: a fracture widens everywhere at a glacial rate set by how fast water is pushed through while staying undersaturated.

This is the "kinetic trigger" that decides **where caves form**:

1. **Epikarst (top ~3–15 m):** water constantly re-equilibrates with soil/ground-air CO₂ (open system), so it stays aggressive. Dissolution rates are highest here ([Klimchouk 2004](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=6334&context=kip_articles)).
2. **Water table / epiphreatic zone:** mixing of waters with different $p_{CO_2}$/Ca histories (Bögli mixing corrosion) plus short flow paths to springs keeps water undersaturated along the whole route — this is where trunk passages race ahead ([Dreybrodt & Gabrovšek 2003](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=5791&context=kip_articles): modeled maximal dissolution rates active close to the water table, dropping rapidly with depth).
3. **Deep phreatic zone:** water is saturated and closed-system; dissolution proceeds at the fourth-order crawl. Deep loops form only where fissures are sparse but large (Ford's State 1, §3.3) or where deep aggressiveness is injected (hypogenic systems).

**Generator consequence:** aggressive dissolution is a *shallow* phenomenon. A believable cave generator should concentrate horizontal passage growth at paleo-water-table levels and vertical shaft growth in the top 10–30 m, with deep phreatic loops as the exception (sparse fissure frequency), not the rule.

### 1.6 Gypsum: same game, different rules

Gypsum (CaSO₄·2H₂O) dissolves by simple hydration with no acid required, and its kinetics are near-linear to saturation — it never hits calcite's fourth-order wall. Measured dissolution rates:

- Gypsum in pure water: 0.03–0.05 g/m²/s; rock salt ~3 g/m²/s (~100× gypsum); calcite in CO₂-water: 0.3–3×10⁻⁴ g/m²/s. **Gypsum dissolves ~100–1000× faster than limestone** by mass flux ([INERIS gypsum guide](https://www.ineris.fr/sites/default/files/contribution/Documents/Ineris-Guide_Gypse_VA-20_11Nov-WEB.pdf)).
- Gypsum is also ~10–30× more soluble than limestone by equilibrium concentration ([Li & Einstein 2017](https://doi.org/10.1002/2017wr021776)), with dissolution largely transport-controlled.

Two generator consequences. First, gypsum karst forms on human timescales — cavities can grow perceptibly over decades ([evaporite karstification modeling](https://mdpi-res.com/d_attachment/energies/energies-15-00761/article_deploy/energies-15-00761-v2.pdf?version=1642738727)) — with modeled denudation ~0.4 mm/yr (400 mm/kyr, ~10× typical limestone). Second, because dissolution stays fast near saturation, epigene gypsum caves are **laterally limited**: water saturates within tens of meters of the insurgence, so you get large sinkholes feeding small, short conduits rather than long branchwork systems ([Stafford et al. 2008](https://doi.org/10.5038/1827-806x.37.2.1)). Hypogenic gypsum mazes (the giant Ukrainian mazes) form instead by rising flow and free convection through fractured gypsum — geometry from the fracture field, not the surface.

---

## 2. Mass balance and denudation rates — parent-anchor verification

### 2.1 The mass balance

A drainage basin exports dissolved CaCO₃ at rate = runoff × concentration. The equivalent surface lowering (denudation):

$$\boxed{D = \frac{I \cdot c}{\rho_{rock}} \qquad \left[\frac{\text{mm}}{\text{kyr}}\right] = \frac{I\,[\text{mm/yr}] \times c\,[\text{mg/L}]}{\rho\,[\text{g/cm}^3]}}$$

with $I$ = specific runoff/infiltration, $c$ = dissolved CaCO₃ equivalent, $\rho \approx 2.6–2.7$ g/cm³. This is White's (1984) maximal-denudation form; Corbel's (1959) $D = 4IH/100$ (I in dm/yr, H in mg/L, ρ = 2.5) is the same equation in disguise ([concepts and methods review](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).

**Parent's anchor, re-derived and checked:** with open-system soil pCO₂ of ~0.02–0.03 atm, calcite saturation gives Ca ≈ 1.2–1.8 mmol/L ≈ 120–180 mg/L CaCO₃ — the parent's 1.5 mmol/L is dead center. With I = 0.5 m/yr (500 mm/yr, a wet temperate-to-subtropical catchment; Slovenia's Kras gets ~1400 mm precipitation, Borneo's Mulu >4000 mm):

$$D = \frac{500 \times 150}{2.7} \approx 28\ \text{mm/kyr}$$

The arithmetic verifies. **But the parent's "spread over 1–5 m of epikarst → 5.6–28 mm/kyr" framing is wrong, and this is the documented disagreement:** denudation rate is a *catchment-averaged surface-lowering equivalent* — the depth over which dissolution physically occurs does not divide the catchment-average number. Spreading the same mass flux over a deeper zone changes the *local* porosity gain, not the equivalent lowering. The correct comparison is against measured catchment denudation:

### 2.2 Measured denudation rates

- **Humid-climate global compilation (cosmogenic ³⁶Cl, China climate gradient):** arid-zone rates climb with precipitation up to ~700 mm/yr MAP, then humid rates **converge to 34.1 ± 11.7 mm/kyr**, kinetically limited ([cosmogenic study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).
- **Mass-balance (solute load) rates, temperate Europe:** Slovenia 77–80, Derbyshire 55–100, Yorkshire 42–51 mm/kyr ([Banks, UK compilation](https://nora.nerc.ac.uk/id/eprint/503972/1/Dissolution%20rates%20in%20limestone%20v4.pdf)).
- **Direct surface measurements are systematically LOWER:** micro-erosion meter and tablet studies give 10–48 μm/yr — e.g. 28 mm/kyr on the Trieste Karst, 35 mm/kyr in a 129-year railway cutting in the Pennines, 11–48 μm/yr in the Austrian Alps vs a catchment mass balance of 95 μm/yr at Kläffer Spring ([Austrian tablet study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X04003101)). The gap is physical: only ~30% of the dissolution potential is spent on the exposed surface; the rest works on fractures and in the epikarst ([concepts review](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133)).
- **Subarctic (Svartisen, Norway, 2600 mm runoff):** autogenic denudation 32.5 ± 10.2 mm/kyr ([Lauritzen 1990](https://doi.org/10.1002/esp.3290150206)).
- **Tropical (Florida estimate, White 1988):** ~1–2 inches per thousand years ≈ 25–50 mm/kyr ([FDOT](https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0)).
- **Gypsum:** ~400 mm/kyr (§1.6) — an order of magnitude above limestone.

**Verdict on the parent's anchor:** the mass-balance arithmetic and the Ca concentration verify; the "5.6–28 mm/kyr" low end is an artifact of the epikarst-spreading misframing and is **below** the measured humid-climate band (roughly 20–100 mm/kyr; best-constrained central value ~34 mm/kyr). Where the parent's balance *is* conservative relative to reality: (a) allogenic runoff sinking from adjacent non-carbonate catchments can raise local denudation well above the autogenic balance; (b) conduit (point-recharge) flow delivers undersaturated water deeper than diffuse flow, so real cave-forming dissolution is not limited to the surface term; (c) tropical runoff often exceeds 1 m/yr, pushing D toward 50–80 mm/kyr. Where it is generous: arid and boreal karsts run 5–20 mm/kyr.

**Generator default:** 10–50 mm/kyr surface lowering in any humid limestone terrain, with karst landscape evolution timescales of 10⁵–10⁷ yr (§5.3, tower karst).

---

## 3. Speleogenesis: how cave systems form

### 3.1 The two genetic end-members

**Epigenic caves** (the great majority, ~80–85% of explored systems) form by descending meteoric water that acquires aggressiveness from the surface — soil CO₂, plus mixing/dilution effects where allogenic streams sink directly into karst. The diagnostic pattern is the **branchwork**: tributary passages join downstream like a surface drainage net, discharging at one or few springs. Passage cross-sections are graded (fairly uniform along a flow path), with scallops and clastic sediment recording turbulent flow ([Palmer 1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2)).

**Hypogenic caves** (~15–20%) form by waters rising from below, with aggressiveness generated at depth. Branchwork patterns **never** form in hypogenic settings ([Klimchouk 2007](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004108)). Instead:

- **Network mazes** — tall narrow fissure passages intersecting in roughly orthogonal patterns, following the joint set;
- **Spongework** — coalesced intergranular pores, blobby 3D voids;
- **Ramiform** rooms — chambers with irregular galleries branching at several levels;
- The diagnostic **morphological suite of rising flow**: floor slots (inlet fissures), rising wall channels, ceiling cupolas (buoyant upwelling), interchange structures.

**Sulfuric acid speleogenesis (SAS) — the Lechuguilla/Carlsbad mechanism, verified:** these Guadalupe Mountains caves were dissolved not by carbonic acid but by **sulfuric acid generated at (and above) the water table**. H₂S, produced at depth by sulfate-reducing microbes reacting hydrocarbons with sulfate from dissolved Castile anhydrite, rose along joints into the incipient caves. Where H₂S-bearing water met oxygenated groundwater/atmosphere — at the water table and in the subaerial cave above — it oxidized ($H_2S + 2O_2 \rightarrow H_2SO_4$, microbially mediated, with native sulfur as an intermediate). The sulfuric acid attacked the limestone ($H_2SO_4 + CaCO_3 \rightarrow CaSO_4 + H_2O + CO_2$), dissolving the wall **and precipitating gypsum** (floor deposits up to 10 m thick in Carlsbad), with the released CO₂ forming carbonic acid for a second dissolution round. Evidence: patterns indicating in-situ acid generation, low-pH alteration minerals (alunite, endellite), deep solution rills, and δ³⁴S down to −25.8‰ in cave gypsum pointing to microbial fractionation ([Hill 1990](https://doi.org/10.1306/0c9b2565-1710-11d7-8645000102c1865d); [Palmer, SAS support](https://nmgs.nmt.edu/publications/guidebooks/downloads/57/57_p0195_p0202.pdf); [NCKRI special paper](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1017&context=kip_monographs)). SAS at each cave level lasted tens to hundreds of thousands of years, ending by mid-Pliocene; the active analogs are Movile Cave (Romania) and the Wyoming H₂S caves; 84 SAS areas are known globally.

### 3.2 Maze formation — the Q/L rule

Palmer's key quantitative result ([1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2); [epigenic vs hypogenic mazes](https://www.sciencedirect.com/science/article/abs/pii/S0169555X11001498)): enlargement rate scales with the ratio of discharge to flow distance, **Q/L**. Maze caves form wherever *many alternate routes* all have high Q/L simultaneously — via (1) diffuse recharge through an insoluble but permeable caprock (every fracture under the cap gets equally aggressive water; usually < 0.5 km² areal extent), (2) floodwater pulses from sinking streams (angular networks in fractured rock, anastomotic mazes along low-angle partings, spongework in porous rock), or (3) hypogenic uniform injection from below. Epigenic mazes typically reach traversable size within ~500 ka; hypogenic transverse mazes in limestone within 1 Ma require permeable adjacent beds and ≤90% calcite saturation, which is rare — deep aggressiveness sources are usually needed ([Palmer 2011](https://www.sciencedirect.com/science/article/abs/pii/S0169555X11001498)).

### 3.3 The water-table vs deep-phreatic debate, resolved: the Ford–Ewers model

The century-long argument — vadose, deep-phreatic, or water-table origin for trunk passages — was resolved by Ford and Ewers (1978) with a model that contains all three as states of one variable: **the frequency of fissures penetrable by groundwater** ([Ford & Ewers 1978](https://doi.org/10.5038/1827-806x.10.3.1); [Ford's retrospective](https://digitalcommons.usf.edu/kip_articles/6893)):

- **State 1 (sparse fissures):** water is forced into deep loops below spring elevation — *deep phreatic* caves, high-amplitude looping passages. Requires few but large fractures.
- **State 2:** intermediate fissure frequency — loops with decreasing amplitude.
- **State 3:** high fissure frequency — passages track the water table closely (*water-table caves*), multiple contemporaneous levels.
- **State 4:** fissure frequency/matrix porosity so high the cave never concentrates into enterable passages.
- Later amplified with **State 0** (fissures too sparse for any genesis) and **State 5** (porosity too high for enterable scale) — so the parent's "five-state model" is right: Ford's amplification is 0–5, the 1978 paper has 4 ([Ford 2003 perspective](https://www.academia.edu/35559784/Perspectives%5Fin%5Fkarst%5Fhydrogeology%5Fand%5Fcavern%5Fgenesis)).

The water table does not precede the cave — the evolving plumbing *creates* it, with the master conduit propagating headward from the spring (Rhoades & Sinacori's refinement). Numerical modeling since confirms the sequence: karst aquifers evolve from distributed flow to a few breakthrough conduits that then drain their neighbors ([Dreybrodt & Gabrovšek 2003](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=5791&context=kip_articles); [Perne et al. 2014](https://doi.org/10.5194/hess-18-4617-2014)).

### 3.4 Breakthrough: the feedback that makes caves

The early evolution of a single fracture under constant head is dominated by the fourth-order kinetics: slow widening everywhere (water saturates in <1 m). But widening → more flow → fresh aggressive water penetrates deeper → the first-order zone extends → a positive feedback. At **breakthrough**, flow and widening rate jump by several orders of magnitude within a geologically instant interval, after which widening is roughly uniform along the whole conduit at ~10⁻² cm/yr (Dreybrodt 1990, [The role of dissolution kinetics](https://doi.org/10.1086/629431); [Bakalowicz analysis](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/96WR01332)). Breakthrough times range from 10⁴ yr (short, steep, well-fed fractures) to >10⁶ yr (long, low-gradient). The conduit that breaks through first captures the flow of its competitors — this *competition* is what turns a uniform fracture mesh into a branchwork with a few big trunks and many starving tributaries.

### 3.5 Joint and fracture control on passage density and orientation

- **Orientation:** epigenic passages follow the two or three dominant joint/bedding orientations of the massif; the map of a branchwork cave is effectively a map of the fracture tensor. Carlsbad/Lechuguilla networks are "roughly orthogonal" because the joint sets are ([Palmer, SAS support](https://nmgs.nmt.edu/publications/guidebooks/downloads/57/57_p0195_p0202.pdf)).
- **Density:** passage density scales with penetrable-fissure frequency (Ford States 1→3→4). Sparse, wide fissures → few large deep passages; dense fissures → many small passages at the water table; extremely dense → no enterable caves at all.
- **Stratigraphy:** bedding-plane partings host anastomotic tubes; shale/sandstone interbeds act as aquitards that perch water and localize dissolution at their contacts ("inception horizons"). Mammoth's passages are stratigraphically pinned to specific Mississippian members ([NPS karst summary](https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf)).

**Generator consequence:** generate the joint field first (2–3 orientation sets with regionally consistent strikes), then bias passage alignment to it. Passage count per unit volume is a direct function of the fissure-frequency parameter, and the Ford state (deep loops vs water-table levels) should be a single slider.

---

## 4. Cave geometry and scale statistics

The numbers a voxel generator needs, with sources.

### 4.1 The longest systems

| System | Length | Notes |
|---|---|---|
| Mammoth Cave (Kentucky, USA) | **685.6 km** (2022–2026) | Longest known; parent's ">670 km" verifies, update to ~686 km ([Wikipedia, 2026](https://en.wikipedia.org/wiki/List_of_longest_caves); [NPS](https://www.nps.gov/articles/000/exploring-the-worlds-longest-known-cave.htm)) |
| Sistema Ox Bel Ha (Quintana Roo, Mexico) | **541.7 km** (Feb 2026), depth 57.3 m | Longest *underwater* system, 160+ cenote entrances ([Wikipedia](https://en.wikipedia.org/wiki/Sistema_Ox_Bel_Ha)) |
| Sistema Sac Actun (Mexico) | ~377–452 km | Second underwater ([showcaves stats](https://showcaves.com/english/explain/Statistics/Longest.html)) |
| Mulu systems (Borneo) | >355 km mapped | Includes Clearwater, Deer, etc. ([caves of Malaysia](https://cavesofmalaysia.wordpress.com/cave-statistics/)) |

**Parent anchor "Mammoth >670 km": verified, current figure 685.6 km.** The Yucatán flooded systems are the deep insight for a generator: an *epikarst-at-sea-level* platform with no surface drainage can host 500+ km of cave in a horizontal sheet only ~10–60 m thick — horizontal passage density can be enormous when the water table is flat and recharge diffuse.

### 4.2 The deepest

- **Veryovkina Cave (Arabika Massif, Western Caucasus, Abkhazia/Georgia): 2,209 m** (2024 GNSS-traced revision; earlier figures 2,212–2,223 m from siphon surveys). Entrance at 2,285 m asl; length 17.5 km; includes the 155-m Babatunda shaft. Second-deepest: **Krubera Cave**, same massif, the only other known cave past 2,000 m ([Wikipedia, Veryovkina](https://en.wikipedia.org/wiki/Veryovkina_Cave)).
- **Parent anchor "Veryovkina ~2.2 km": verified** (2,209 m current best).
- The >2 km club exists only in high alpine mountains with glacially-recharged, steeply-fissured massifs. Below −2,212 m the lower siphons defeat even cave divers; the 6,000+ m of subhorizontal passages found below −2,100 m in Veryovkina (an ancient aquifer collector) show deep systems do exist where recharge is huge ([exploration history, same source](https://en.wikipedia.org/wiki/Veryovkina_Cave)).

### 4.3 Chambers

- **Sarawak Chamber (Gua Nasib Bagus, Mulu, Borneo):** laser-scanned 2011 at **600 m long × 435 m wide × up to 115 m high; area 164,459 m²; volume 9.58×10⁶ m³** — largest by area, second by volume ([Wikipedia](https://en.wikipedia.org/wiki/Sarawak_Chamber); [Mulu Caves Project](https://mulucaves.org.uk/articles/sarawak-chamber); [caves of Malaysia](https://cavesofmalaysia.wordpress.com/cave-statistics/)). **Parent anchor "600×415×80 m": partially wrong — width is 435 m and max height 115 m** (the 80 m likely reflects an early estimate; the expedition's own conservative tape-and-compass numbers were 700×400×100, refined by laser to 600×435×115).
- **Miao Room (Gebihe system, China):** volume 10.78×10⁶ m³ — largest by volume, smaller by area (140,900 m²) ([caves of Malaysia](https://cavesofmalaysia.wordpress.com/cave-statistics/)).
- **Son Doong (Vietnam):** ~4.5–5 km long, passage up to **150–250 m high and ~90–200 m wide**; a 2010 survey found a 2-km chamber 250 m high and 200 m wide; internal jungle with 40-m trees beneath collapsed skylights. **Parent anchor "5 km long, 200 m high": verified** ([caves of Malaysia](https://cavesofmalaysia.wordpress.com/cave-statistics/)).
- **Deer Cave (Mulu):** 4.1 km long, max width 168.7 m, average ceiling >120 m, highest roof (Antler Passage) 226 m, entrance 146 m wide ([same source](https://cavesofmalaysia.wordpress.com/cave-statistics/)).
- **Big Room, Carlsbad:** ~355×255 m footprint, ~78 m high — for scale on the ~3× smaller tier.

### 4.4 Passage cross-section shapes — the water-history decoder

The single most useful morphological fact for a generator: **cross-section shape encodes the flow regime that carved it** ([Mulu morphology study](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf); [Palmer, Mammoth geology](https://digitalcommons.usf.edu/kip_articles/8226/)):

- **Phreatic tube:** circular/elliptical full-pipe cross-section, scalloped walls, gradient following the hydraulic grade (can go uphill in the flow direction). Formed below the water table. Diameter 1–10 m typical for trunks.
- **Vadose canyon:** tall, narrow (width:height 1:3–1:10), meandering, cut by a free-surface stream; gradient follows gravity. Formed above the water table. Mulu's vadose canyons average 3.4 m wide × 14.8 m high ([same source](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).
- **Keyhole (composite):** phreatic tube above + vadose trench below — the classic "water table dropped mid-history" signature.
- **Breakdown chamber:** angular, block-floored, ceiling height >> passage width; Mulu mean 42.6 × 31.2 m ([same source](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)) — this class contains all the giant chambers.
- **Basal slots / fissures:** 0.3–3 m wide, tens of meters tall.

Mulu's surveyed length split is a good generator prior for a mixed tropical system: phreatic tubes 31%, vadose canyons 23%, breakdown chambers 17%, mazes 13%, composite 9%, active streamways 6.5% ([same source](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).

**Scallops** (asymmetrical dissolution flutes) record paleo-flow velocity; mean scallop length inversely tracks velocity — 10 cm scallops ≈ dm/s flow, 1 cm scallops ≈ m/s. Use as a texture-direction hint keyed to the generated paleo-flow field.

### 4.5 Vertical organization

The canonical vadose-to-phreatic stack ([Klimchouk 2004](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=6334&context=kip_articles); [Williams 2008](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1173&context=ijs)):

1. **Soil (0–0.5 m)** and **epikarst (typically 3–15 m; up to 30+ m alpine):** high-porosity weathered zone, 2–3 orders of magnitude more permeable than the bulk rock, functioning as a perched aquifer that stores storm water and delivers it *concentrated* to the few penetrating fissures (>50% of recharge arrives in the vadose zone already concentrated, per Kiraly).
2. **Vadose transmission zone:** largely unweathered bedrock (porosity <2%), vertical percolation. Hosts **shafts** (vertical wells, commonly several tens of m deep, fed by epikarst drainage — "the most common feature among explored vertical caves" per Klimchouk) and **meandering canyon passages** on bedding planes. Shafts cluster under doline axes and karren fields, following the epikarst drainage pattern.
3. **Epiphreatic (flood) zone:** the band around the water table that floods seasonally or per-storm — passages oscillate between air-filled and pipe-full, and dissolution is maximal (§1.5).
4. **Phreatic zone:** below the water table — saturated, low-gradient flow in tubes and loops, siphons at the downstream end.

Passage levels stack downward through time as base level drops: oldest levels highest, active drain deepest ([Mammoth NPS](https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf)). Multi-level systems with 3–5 levels spaced tens of meters vertically over a few hundred meters of total relief are the classic configuration (Mammoth; Swildon's Hole with four major levels, [Ford's Mendip work](https://legacy.caves.org/pub/journal/JCKS/PDF/V27/v27n4-Ford.htm)).

---

## 5. Rates and ages

### 5.1 Passage enlargement rates

Palmer's synthesis, confirmed by field measurements and modeling ([Palmer 1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2); [allogenic-water dynamics](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1032&context=kip_articles)):

- Wall retreat in active conduits: **maximum ~0.01–0.1 cm/yr (0.1–1 mm/yr)**, set by kinetics and nearly independent of discharge beyond a threshold. Mean-annual rates ~0.01 cm/yr (0.1 mm/yr) typical.
- The rate depends on **Q/L** before saturating: early-stage conduits grow at wildly varying rates; only those that gain discharge reach the kinetic maximum.
- Floodwater caves: adjacent fissures with initial widths ≥0.01 cm can reach traversable size within **10,000 years** (Palmer, allogenic paper above).
- Local measured wall retreat (MEM in active streamways): 0.087 mm/yr (Mulu active) declining to 0.021 mm/yr in relict passages ([Mulu study](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).

**Parent anchor "wall retreat ~0.01–1 mm/yr": verified** — kinetic ceiling 0.1–1 mm/yr, typical active conduits ~0.1 mm/yr.

**Time for a 10-m trunk:** a 10-m tube grown from a ~0.1–1 cm initial fracture needs ~5 m of radial growth — ~50 kyr at the typical 0.1 mm/yr, ~5 kyr at the kinetic maximum, plus 10–100 kyr of slow pre-breakthrough phase. Palmer's summary: **most caves require 10⁴–10⁵ yr to reach traversable size** ([Palmer 1991](https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2)). Full system integration (sink-to-spring): 10⁵–10⁶ yr. Landscape maturity: 10⁶–10⁷ yr.

### 5.2 Denudation by region (summary table)

| Setting | Denudation (mm/kyr) | Source |
|---|---|---|
| Arid karst (MAP < 700 mm) | < 5–15 | [cosmogenic gradient study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133) |
| Humid temperate (Europe) | 20–100 (mass balance); 10–50 (surface) | [Banks UK compilation](https://nora.nerc.ac.uk/id/eprint/503972/1/Dissolution%20rates%20in%20limestone%20v4.pdf) |
| Humid global best-fit | 34 ± 12 | [cosmogenic study](https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133) |
| Subarctic, high runoff | ~32 | [Lauritzen 1990](https://doi.org/10.1002/esp.3290150206) |
| Tropical (Florida) | ~25–50 | [FDOT](https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0) |
| Gypsum karst | ~300–400 | [evaporite modeling](https://mdpi-res.com/d_attachment/energies/energies-15-00761/article_deploy/energies-15-00761-v2.pdf?version=1642738727) |

### 5.3 How old are the big systems?

- **Mammoth Cave:** the rocks are Mississippian (~330 Ma) but the cave is young — groundwater began interacting with the Girkin Limestone ~10 Ma ago; **upper levels fully developed by 3.2 Ma** (cosmogenic ²⁶Al/¹⁰Be dating of cave quartz pebbles); lower levels cut during the Pleistocene as the Green River incised, with the active level still forming. The passage-level stack is a Plio-Pleistocene record of base-level history ([NPS cave & karst summary](https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf), citing Granger et al. 2001). **Parent anchor "Mammoth Plio-Pleistocene vs older precursors": verified** — ~10 Ma inception, main levels 3.2 Ma to present; regional karstification began late Tertiary/early Quaternary per several independent lines ([AIPG field guide](https://www.uky.edu/KGS/geoky/fieldtrip/2005%20AIPG%20Guidebooks/MammothCave.pdf)).
- **Carlsbad/Lechuguilla:** SAS speleogenesis over ~8 Ma of episodic uplift, ending mid-Pliocene; vadose overprinting in the Pleistocene ([NCKRI special paper](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1017&context=kip_monographs)).
- **Mulu:** U-series stalagmite tiers at 412 ka / 187 ka / 34 ka record stepwise base-level lowering ([Mulu study](https://www.geojournal.net/uploads/archives/8-4-227-593.pdf)).
- **Tower karst:** >10 Myr of evolution for fengcong→fenglin transformation ([Guangxi chapter](https://link.springer.com/chapter/10.1007/978-90-481-3055-9_30)).

---

## 6. Speleothems

### 6.1 The precipitation mechanism (degassing)

Drip water arrives at the cave carrying Ca²⁺ + 2HCO₃⁻ in equilibrium with the *epikarst's* high pCO₂ (0.02–0.05 atm). Cave air holds far less CO₂ (600–6,000 ppm typical, §10). The disequilibrium drives CO₂ **outgassing** from the thin water film, raising pH, shifting HCO₃⁻ → CO₃²⁻, and forcing calcite to precipitate:

$$Ca^{2+} + 2HCO_3^- \rightarrow CaCO_3\downarrow + CO_2\uparrow + H_2O$$

Precipitation rate in thin films follows $R = \alpha\,(c - c_{eq})$ with $\alpha$ a temperature-dependent kinetic constant (≈ 1.3×10⁻⁵ cm/s at 10 °C, fitted 0–30 °C in [Dreybrodt & Romanov](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1049&context=kip_articles)). Rate depends on drip-water Ca, supersaturation, film thickness, temperature, and **cave-air CO₂** — doubling ambient CO₂ from 390 to 2,500 ppm roughly halves growth rate, and deposition ceases above ~5,000 ppm in several Texas caves ([James et al. 2015](https://doi.org/10.1002/2014gc005658)).

### 6.2 Measured growth rates — parent anchor checked

- Modeled rates validated against 31 European drip sites: good agreement (R² = 0.69), growth correlating with Ca²⁺ (R² = 0.61) and temperature, *not* drip rate (R² = 0.09) across the investigated drip-rate range ([Baker et al.](https://www.sciencedirect.com/science/article/abs/pii/S0009254100003995)).
- Site means: ~0.1–0.5 mm/yr for actively growing temperate stalagmites; one pair modeled at 0.18–0.32 mm/yr; seasonal bias 55–99% of annual growth in the fast season ([StalGrowth](https://www.mdpi.com/2076-3263/11/5/187)).
- Slow end (arid/alpine/dead caves): <0.01 mm/yr, effectively zero; hiatuses common.
- Fast end (tropical, high-Ca drip water): up to ~1 mm/yr sustained; >1 mm/yr is rare and short-lived.

**Verdict on parent anchor "stalagmites typically ~0.01–3 mm/yr": the top end is too generous.** Verified range: **~0.01–1 mm/yr, bulk of measured sites between 0.05 and 0.5 mm/yr.** Controlling variables: drip-water calcium (hence soil CO₂ and temperature) and cave-air CO₂; drip *rate* controls shape (next) more than vertical growth speed, except at very slow drip (interval > ~100 s, where supply genuinely limits growth and produces candle-shaped forms) ([Sofular modeling](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2022.969211/full)).

### 6.3 Shape: the equilibrium-radius law

Under constant conditions a stalagmite converges on an invariant shape that translates upward without changing form (Franke 1965; proven and modeled in [Dreybrodt & Romanov](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1049&context=kip_articles)). The key result — a simple mass balance on the apex:

$$\boxed{R_{eq} = \sqrt{\frac{V_{drop}}{\pi\,\tau\,\alpha}} \quad \text{(equilibrium radius; } V_{drop} \approx 0.1\ \text{cm}^3, \tau = \text{drip interval, } \alpha \approx 1.3\times10^{-5}\ \text{cm/s)}}$$

Notably $R_{eq}$ is independent of the supersaturation. Worked example from the source: τ = 30 s, α at 10 °C → R = 9 cm — very realistic. The scaling is verified over three orders of magnitude (5 cm to 20 m diameter; the Cuban giant at Cueva San Martín Infierno is ~70 m tall, 20 m diameter, and *shape-similar* to a 4.5-cm specimen). Recent analytic work extends this to a Damköhler-number classification with three shapes — flat-top (Da>1), columnar (Da=1), conical (Da<1) — all observed in nature ([PNAS, ideal stalagmites](https://pmc.ncbi.nlm.nih.gov/articles/PMC12557760/)).

Generator recipe: sample a drip interval per site (log-uniform 5–1000 s maps to radii ~3–40 cm); heights then follow age × growth rate (0.05–0.5 mm/yr), capped by passage ceiling. Columns form where stalactite + stalagmite meet — mostly under long-lived fracture drips.

### 6.4 Forms and why some caves are dead

- **Stalactites:** straw forms (thin tubes, active feed) to massive tapering cones; growth self-limiting as the feed tube clogs.
- **Flowstone:** sheet flow over walls/slopes; thickness cm–m over 10⁴–10⁵ yr.
- **Rimstone dams (gours):** precipitation lips at pool edges, terracing downhill; can grow to meter-scale walls.
- **Helictites:** ignore gravity, driven by capillary feed through a central canal; direction wanders.
- **Dead caves:** drip water that has already equilibrated (long residence in big conduits, or prior calcite precipitation upstream in the epikarst during drought) deposits nothing. Also: high cave-air CO₂ (>2,000–5,000 ppm) suppresses or stops deposition ([James et al. 2015](https://doi.org/10.1002/2014gc005658)). Arid-region and hypogenic caves are typically bare. Generator: speleothem density keys to (a) distance below an active epikarst drip field, (b) cave-air ventilation (near entrances → more degassing → more deposition, until dust/drying dominates very close to the entrance), (c) CO₂ state.

---

## 7. Breakdown: the mechanics of underground voids

### 7.1 What holds a ceiling up

The roof of a bedded limestone passage behaves as **voussoir beams**: a cracked "beam" of rock between vertical joints, arching under compression between abutments. Classic beam theory *underestimates* their stability because the cracked beam re-arches; failure modes are midspan snap-through buckling (thin beams, span/thickness >10), abutment crushing, and shear slip along cross-joints ([Diederichs & Kaiser, voussoir analogue](https://www.sciencedirect.com/science/article/abs/pii/S0148906298001806)). Field extensometer data put the linear-behavior limit at ~10% of bedding thickness of midspan deflection.

Empirical stability envelopes:

- **Barton's Q-system unsupported-span envelope** (man-made openings): Span = 2·Q^0.66 — the design curve for permanent unsupported excavations ([Barton 1976, in Jordá-Bordehore](https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf)).
- **Jordá-Bordehore's natural-cave fit (137 large-span caves):** the stable/unstable boundary is **Span = 5.4·Q^0.73** — natural caves sit systematically above the mining envelope because they are unlined, unblasted, dome-arched, and have had 10⁴–10⁶ yr of natural proof-testing ([same source](https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf)).
- Practical spans: temperate-karst passage sections usually <10 m wide; optimal sites reach 50 m; tropical chambers exceed 100 m; the record is Sarawak's ~300–435 m. US room-and-pillar limestone mines average 13.5 m rooms and stay naturally stable in good roof ([NIOSH survey](https://stacks.cdc.gov/view/cdc/226788)).
- Long-term: cave roof beams creep. Compressive failure by subcritical crack propagation over 10⁴+ yr produces the huge observed deflections, with failure by splitting/delamination ([Tharp & Holdrege](https://doi.org/10.1201/9781003761365-140)). The 1,400-year-old Heidong quarry caverns hold an **81-m unsupported span** (dome roof, massive tuff, Q≈51) — beyond all engineering expectations, proving arch geometry and intact rock matter more than conservative design lines ([Heidong study](https://doi.org/10.18814/epiiugs/2013/v36i1/006)).

**How 100-m chambers survive:** (1) thick, massive, strong beds (Sarawak's strata are separated by 15–20 m; [Jordá-Bordehore](https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf)); (2) dome/arch geometry rather than flat spans; (3) deep burial giving confining stress; (4) natural selection — the spans that didn't work already fell, and their breakdown now supports the remaining roof.

### 7.2 Breakdown initiation and the pile

Breakdown is not only "the ceiling got too wide." Documented initiators: bedding-plane separation above passages, lateral stream undercutting of walls, loss of buoyant support when the water table drops (buoyancy contributes ~40% of ceiling support in flooded limestone — [Andrejchuk & Klimchouk, gypsum breakdown](https://doi.org/10.5038/1827-806x.31.1.4)), and **vadose weathering**: oxidation of sulfides and clay hydration along veins and impure beds wedges blocks loose over time; fallen blocks are typically bounded by *pre-existing* discontinuities, with coated (not fresh) faces ([Osborne 2002, breakdown by vadose weathering](https://doi.org/10.5038/1827-806x.31.1.3)). In Mammoth's dry passages, **gypsum-crystal wedging** — gypsum crystallizing in wall fractures plus replacement of limestone by gypsum — produces characteristic shard-and-rock-flour breakdown with curved ceiling plates hanging at steep angles ([gypsum wedging in Mammoth](https://caves.org/wp-content/uploads/Publications/JCKS/v65/v65n1-White.pdf)).

**Pile geometry and the self-arrest law.** When a ceiling fails, a breakout dome stopses upward while breakdown accumulates below. The bulk volume of broken rock exceeds the solid volume by the **coefficient of loosening** $K_{loos}$ ≈ 1.1–1.3 (clays) to 1.5–2+ (hard rock). The dome stops migrating upward when the pile chokes it:

$$h_c = \frac{h_0}{K_{loos}-1}$$

the "height of closure" above a receptacle cavity of initial height $h_0$. For $h_0$ = 5 m and $K_{loos}$ = 1.4, closure occurs 17.5 m up; with $K_{loos}$ = 1.5–2, 5–10 m. This is why big chambers under 45–60+ m of overburden almost never break through to the surface — the pile catches up first ([Andrejchuk & Klimchouk 2002, Kungurskaya Cave](https://doi.org/10.5038/1827-806x.31.1.5)). Kungurskaya recorded 64 failure events in 23 years, predominantly slab breakdown, with breakout domes 20–24 m high whose floors stay under 10 m of clearance because the talus fills them.

**What a breakdown pile looks like inside:** a cone or ridge of angular blocks (slab to multi-meter size) grading from coarse irregular blocks at the base through cobbles to rock flour at the top of gypsum-wedged piles; slope at roughly the angle of repose (~35–40°) but *mobile* — piles creep and re-sort without new collapses ([Osborne 2002](https://doi.org/10.5038/1827-806x.31.1.3)). Ceiling above the pile shows a fresh breakout dome (cupola) with exposed bedding edges. In maze caves, breakdown talus is the main exploration obstacle.

---

## 8. Karst surface terrain

### 8.1 Sinkholes (dolines): types, mechanics, sizes

Four mechanisms, after the Florida classification ([USGS WRI 85-4126](https://pubs.usgs.gov/wri/1985/4126/report.pdf); [FL FGS](https://inspectapedia.com/vision/Florida-Sinkhole-Report.pdf)):

1. **Solution sinkhole:** bare/thinly-covered limestone dissolves and subsides at the same rate; funnel-shaped, gentle; 5–100+ m diameter, depth up to ~10 m. Dominant on exposed karst.
2. **Cover-subsidence sinkhole:** sand cover ravels slowly downward into widening fissures in buried limestone; shallow broad depression, meters to tens of m across, forming over months to millennia.
3. **Cover-collapse sinkhole:** cohesive clay layer bridges a growing void; when the bridge fails, sudden collapse — steep-walled, 1–100 m across, hours from trigger to hole ([Marion County FL stats](https://pmc.ncbi.nlm.nih.gov/articles/PMC6509126/); [FDOT](https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0)). Triggers: water-table decline (pumping, drought), heavy rain, vibration.
4. **Rock collapse (rare):** the cave roof itself fails to the surface — the terminal-breakdown mechanism of §7.2, mostly self-arrested unless overburden is thin.

**Densities (parent anchor "hundreds per km²" — verified):** Slovenia's national lidar census found **471,192 dolines**; average solution doline: 9 m deep, 42 m diameter, 14,000 m³; density on level surfaces **up to 500/km²** (covering up to 60–80% of the ground in extreme patches); the Classical Kras plateau averages ~60/km²; Postojna area ~300/km²; collapse dolines are rare (314 in the country, mean depth 49 m, mean volume 1.2×10⁶ m³, largest 11.6×10⁶ m³) ([Mihevc & Mihevc 2021](https://doi.org/10.3986/ac.v50i1.9462); [UNESCO Classical Karst](https://whc.unesco.org/fr/listesindicatives/6072/)). Pinellas County, FL: ~2.2/km² identified in 1926 ([USF thesis](https://digitalcommons.usf.edu/etd/1306)). Yucatán state: 6,717 depressions over 454 km² — 4,620 dolines, 2,021 uvalas, 76 poljes ([Aguilar et al.](https://doi.org/10.4311/2015es0124)).

### 8.2 Cenotes and the Chicxulub ring

Cenotes are collapse dolines that intersect the water table in the flat Yucatán platform — the ceiling openings of the flooded Ox Bel Ha/Sac Actun systems. The **Ring of Cenotes** is a 165–180-km-diameter semicircular band of high cenote density tracing the buried rim of the 66-Ma Chicxulub impact crater beneath ~1 km of post-impact carbonate: fracturing and slumping along the crater's outer slump zone localized groundwater flow, dissolution, and collapse, marking the boundary between unfractured limestone inside the ring and fractured limestone outside ([Perry et al. 1995](https://doi.org/10.1130/0091-7613(1995)023%3c0173:ROCSNW%3e2.3.CO;2); [Connors et al. 1996](https://doi.org/10.1111/j.1365-246x.1996.tb04066.x); [surficial geology of Chicxulub](https://link.springer.com/article/10.1007/BF00575099)). The cenotes themselves are young — likely <130 ka, formed during the last interglacial highstand — but their alignment is inherited from impact-age structures ([Northwestern explainer](https://sites.northwestern.edu/monroyrios/ring-of-cenotes/); [EGU 2024 analogue modeling](https://doi.org/10.5194/egusphere-egu24-3188): ~6,500 cenote outlines mapped, elongation E-W regionally, deviations above the crater margin attributed to impact-induced stress/isostatic relaxation). Crater diameter estimates from the ring and gravity: ~180 km (cenote-ring school) to ~240 km (gravity school).

**Generator gold:** an ancient buried structure (impact, graben, reef front) can express on the surface millions of years later purely through the karst drainage-density field. Ring-shaped sinkhole swarms are a legitimate, real-Earth pattern.

### 8.3 Poljes, karren, and the rest of the surface vocabulary

- **Poljes:** giant flat-floored closed basins with perennial or seasonal flooding through ponors (swallow holes) and estavelles (openings that alternate between sinking and rising with flood stage). Slovenia's Cerknica polje: 38 km² floor, intermittent lake up to 26 km²; Planina polje: 6×2 km, floods lasting months ([UNESCO nomination](https://whc.unesco.org/fr/listesindicatives/6072/)). Yucatán's 76 poljes occupy ~half the depression area ([Aguilar et al.](https://doi.org/10.4311/2015es0124)).
- **Karren / karrenfeld:** cm-to-meter solution sculpting of bare limestone — rillenkarren (solution flutes on steep faces), clints and grikes (pavement blocks and their 0.1–3 m slots), kamenitzas (solution pans). The surface texture of every bare karst.
- **Uvalas:** compound depressions from coalesced dolines — the intermediate step between doline field and polje.

### 8.4 Tower karst vs cockpit karst — the Guilin question

Two interlocked tropical end-members ([Day & Tang, Guilin slope survey](https://doi.org/10.1002/1096-9837(200010)25:11%3C1221::AID-ESP133%3E3.0.CO;2-D); [Waltham, karst lands of southern China](https://www.researchgate.net/publication/263374259_The_karst_lands_of_southern_China)):

- **Fengcong (peak cluster / cockpit):** clustered conical hills sharing a common base, enclosed doline/cockpit depressions between them; hillslope angles 60–75°; formed under *rapid uplift* with thick vadose zones and vertical flow. Dominates the uplifted Guizhou plateau.
- **Fenglin (peak forest / tower karst):** isolated steep towers rising from an alluvial karst plain; mean slope 62.4° at Guilin, commonly near-vertical walls >100 m tall (Yangshuo; Lijiang plain towers average 130 m). Formed where base level is *stable or slowly falling*: aggressive allogenic water at the plain's water table undercuts tower feet with dissolution notches and foot caves, undercut-collapse converts cones to towers, and lateral planation proceeds around the residuals.

Requirements for mature fenglin: pure strong horizontally-bedded limestone in great thickness; an alluviated plain maintained by allogenic sediment; water table pinned at the plain; hot wet climate; slow uplift matched to denudation. Timescale >10 Myr ([Guangxi chapter](https://link.springer.com/chapter/10.1007/978-90-481-3055-9_30)). Tower edges follow sub-vertical fault planes; tower summits preserve a log-normal height distribution echoing an old base-level surface ([Lijiang structural study](https://www.schweizerbart.de/papers/zfg/detail/36/98327/Structural_and_hydrogeological_origin_of_tower_karst_in_southern_China_Lijiang_plain_in_the_Guilin_region)). Field evidence at Guilin supports *parallel* development of fengcong and fenglin by different mechanisms rather than a strict sequence ([Day & Tang](https://doi.org/10.1002/1096-9837(200010)25:11%3C1221::AID-ESP133%3E3.0.CO;2-D)).

### 8.5 Hydrologic surface expressions

- **Losing streams / sinking rivers (ponors):** allogenic surface streams that sink at the karst boundary, leaving **blind valleys** terminated by swallow holes.
- **Resurgences (springs):** the outlets. Karst springs carry the conduit system's discharge; the Slovenian Ljubljanica drains a ~1,100+ km² catchment almost entirely underground through the polje chain ([UNESCO nomination](https://whc.unesco.org/fr/listesindicatives/6072/)).
- **Estavelles:** openings that function as ponor in low stage and spring in high stage — the polje's overflow plumbing.
- **Mature karst has *no* surface drainage** — the Kras plateau is "a bleak, waterless place" (the original meaning of *kras*), with everything underground ([USGS](https://pubs.usgs.gov/wri/1985/4126/report.pdf); [UNESCO](https://whc.unesco.org/fr/listesindicatives/6072/)).

**Generator consequence:** karst is detectable from the surface hydrology alone — sinking streams, blind valleys, dry crossings of expected valleys, resurgent springs at stratigraphic/lithologic boundaries, and flood pulses.

---

## 9. Lava tubes and other cave types

### 9.1 Lava tube formation mechanics

A lava tube is the drained conduit of a channelized basalt flow. Four roofing mechanisms are observed ([Greeley, USGS PP1350 ch. 59](https://pubs.usgs.gov/pp/1987/1350/pdf/chapters/pp1350_ch59.pdf); [Valerio et al. 2008](https://doi.org/10.1029/2007jb005435); [Dragoni et al. 1995](https://doi.org/10.1029/94jb03263)):

1. **Channel crust-over:** on sluggish-moderate channel flow (<1–3 m/s), crust grows inward from the levees and closes "like a zipper" along the medial bright zone;
2. **Rafted-crust jamming:** crustal plates torn from levees raft downflow and weld together at constrictions (the majority of Mauna Ulu channel surfaces were >50% crusted);
3. **Levee overgrowth:** overflow splashes build levees upward and inward until they arch over (2–5 m/s flows);
4. **Lava-toe budding / sheet-flow lobation:** pahoehoe toes extend beneath a solidified crust with no open-channel phase at all — many tubes never see daylight.

Physics of the transition: a stationary roof becomes possible where the shear stress at the crust base falls below the crust's yield strength — favored by *wide, thin* channels (reduced basal shear), *gentle slopes* (the Etna model finds tube formation below ~6°), and *low-to-moderate effusion rates* (<~10 m³/s) ([Valerio et al. 2008](https://doi.org/10.1029/2007jb005435)). Once roofed, the tube insulates the flow spectacularly: the Ai-laau flow feeding Kazumura lost only ~4 °C over its 39-km run ([Allred & Allred](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)). Inside, active tubes deepen by **thermal erosion** (melting + sweeping of the floor), producing stacked multi-level systems as the master tube incises below its own earlier floors; measured downcutting reached 10 cm/day at Kilauea and ~15 m of incision in 18 months at Mauna Ulu ([same source](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).

### 9.2 Sizes — parent anchor checked

- **Typical diameters: 1–15 m**, exceptional tubes to ~21 m wide × 18 m high (Kazumura's largest cross-sections). **Parent anchor "up to ~30 m": slightly generous — ~21×18 m is the documented Kazumura maximum** ([Kazumura study](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- **Kazumura Cave, Hawai'i: 59.3 km surveyed (1995 expedition), vertical extent 1,098 m, average slope 1.9° over 32 km linear, mean cross-section 20.3 m², ~1.2×10⁶ m³ volume, 82 entrances** — the world's longest lava tube. **Parent anchor "65 km": disagreement — the published survey figure is 59.3 km** (some later popular lists cite ~65 km; I found no survey source beyond the 59.3 km paper, so use 59.3–65 km with the survey number primary) ([Allred & Allred](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- Kazumura's age is only 350–500 years BP — lava tubes are geologically *instant* caves compared to limestone's 10⁴–10⁶ yr.

### 9.3 Interior features, drainage, and survival

- **Cooling lips / floor levees:** crust welded along the flow margins at the level of the last active lava surface, forming raised benches along walls — near entrances (where cooling air circulated) they grow into "tube-in-tube" profiles ([Kazumura study](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- **Rafted floor:** the frozen final flow surface — pahoehoe ropes, shove-pressure ridges; walls show horizontal flow ridges and glaze; ceilings near entrances frothy/popcorn-textured from degassing.
- **Skylights and pit craters:** roof collapse windows; Kazumura retains 50+ pre-drainage skylights ([same source](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).
- **Relict survival:** after drain-out, tubes persist as caves; subsequent roof collapse segments them (each entrance = a collapse), and ceiling accretion linings spall. Because the host is young basalt with no subsequent speleogenesis, lava tubes are pristine snapshots — no speleothems beyond tiny secondary deposits, no sediment fill beyond entrance wash.

**Generator contrast (limestone vs lava):** lava tubes are *single-trunk, downdip-following, low-sinuosity-meandering, mostly single- or few-level* conduits whose cross-section is set by the flow's thermal budget, not by fracture statistics; they begin at a vent and end at a flow-front delta, with braided-distributary complexity near the front.

### 9.4 Other cave types (brief)

- **Talus caves:** voids in rockfall boulders — no genesis of their own; geometry = boulder packing.
- **Tafoni:** cavernous weathering hollows in cliff faces (salt/hydration weathering + wind), dm-to-room scale, shallow.
- **Glacier caves:** meltwater channels in/under ice — meters to tens of m, fast-forming (seasons), fast-dying.
- **Sea caves:** wave-cut notches along weak zones (joints, dykes) in cliffs — typically 5–50 m deep, tall narrow mouths, tidally flooded floors.
- **Piping / pseudokarst:** suffosion pipes in badlands loess and colluvium — unconsolidated material washed out along subvertical pipes, producing sinkhole-like depressions without bedrock dissolution (cover-subsidence minus the limestone).
- **Quartzite/silicate karst:** exists (Venezuelan tepuis) via extreme residence times — slow, but the same geometry vocabulary.

---

## 10. Cave climate and environment

### 10.1 Temperature

**Cave temperature ≈ local mean annual surface temperature (anchor verified).** The rock mass acts as a low-pass thermal filter; the cave asymptotically approaches the flow-weighted mean temperature of infiltrating water, set by the local annual mean ([Badino 2004](https://doi.org/10.5038/1827-806x.33.1.10)). Refinements that matter:

- Annual amplitude inside deep cave zones: **0.1–8.8 °C** measured across a global cave sample — Planinska (Slovenia) 0.1 °C vs Balcões (Azores) 8.8 °C ([comparative study](https://pmc.ncbi.nlm.nih.gov/articles/PMC10676404/)); equilibrium is typically reached within meters to a few hundred meters of an entrance depending on airflow.
- Lapse rate inside deep caves is not the atmospheric 6.5 °C/km but between the water-adiabatic (~2.3 °C/km) and moist-air (~5 °C/km) values ([Badino 2006](https://doi.org/10.3986/ac.v34i2.261)).
- Caves lag climate change by centuries (mountain-scale equilibration times).
- Lava tubes track the host rock and local microclimate — Kazumura runs 15 °C near the crater to 22 °C at the coast ([Kazumura study](https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf)).

### 10.2 Humidity and CO₂

- Deep-cave RH is **95–100%** (near-condensation); it varies little with surface RH ([James et al. 2015](https://doi.org/10.1002/2014gc005658)).
- **Cave-air CO₂**: 420 ppm (atmosphere) at entrances → **~600–6,000 ppm in typical ventilated cave interiors** → 1.1–3.7% at Lascaux's confined galleries ([Lascaux aerology study](https://www.springerprofessional.de/the-co2-dynamics-in-the-continuum-atmosphere-soil-epikarst-and-i/16644174)) → >5% in poorly-ventilated deep systems, where it is the growth-limiting variable for speleothems (§6). CO₂ is the main caving hazard gas; it stratifies and accumulates in poorly-ventilated lower galleries during summer stagnation, flushed in winter.### 10.3 Ventilation regimes: chimney vs barometric

Two airflow engines, different signatures ([Pflitsch et al. 2010](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=2599&context=kip_articles); [Gomell & Pflitsch 2022](https://doi.org/10.5038/1827-806x.51.3.2437)):

- **Chimney-effect (convective) caves** — the common case, any cave with two entrances at different elevations: winter = cold air sinks in, warm cave air out the top; summer reverses. Flow <0.5 m/s typically, strongly seasonal. Driver: density contrast between cave air (≈annual mean) and outside air; in the tropics, diurnal density cycling ventilates caves daily ([James et al. 2015](https://doi.org/10.1002/2014gc005658)).
- **Barometric caves** — one effective entrance and a huge volume-to-opening ratio: the cave behaves as a **low-pass pressure filter**. Falling outside pressure → the cave exhales; rising → it inhales — through the same openings, in the *same* direction at all openings, reversing on timescales of seconds to days as cyclones pass. **Wind Cave (South Dakota) is the classic example** (found by its audible cave wind; Conn 1966 proved the barometric mechanism), Jewel Cave second. Airflow of **several m/s at the openings**. Measured dynamics: internal pressure signals are displaced (altitude), delayed (travel time), smoothed (semi-diurnal atmospheric tides damped by 0.06–0.19 hPa), and damped (multi-day cyclonic variation reduced up to ~1.6 hPa at Jewel's Deep Camp); pressure response explains >99% of internal pressure at most sites ([Gomell et al. 2021](https://doi.org/10.5038/1827-806x.50.3.2393); [Gomell & Pflitsch 2022](https://doi.org/10.5038/1827-806x.51.3.2437)). Pressure-to-temperature coupling: ~2–14×10⁻³ °C/hPa via adiabatic compression, plus up to 1 °C/hPa barometric-wind advection near entrances ([French cave APV study](https://www.sciencedirect.com/science/article/pii/S2772883822001200)).

### 10.4 Entrance zones

From outside in: **entrance zone** (variable T/RH, daily and seasonal swing, green plants, twilight) → **twilight zone** (dim light, mosses, temperature still tracks surface with lag) → **transition/mist zone** (constant T near the cave mean, RH near 100%, condensation mist when warm humid air enters a cool cave — the drip-forming belt) → **deep cave** (total dark, T constant to <1 °C, RH 95–100%, air still unless convective/barometric flow). Zone lengths scale with entrance size and airflow: meters for a tight crawl, hundreds of meters for a large walk-in entrance ([Pflitsch et al. 2010](https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=2599&context=kip_articles)).

### 10.5 Acoustics — the one-paragraph cross-reference

For the project's audio/reverb system, the physically-derived switch variables are: **(1) reverberation time rises steeply past the twilight zone** — a cave passage is a hard-walled, low-absorption waveguide, so RT60 grows from open-air ~0 s to seconds in chambers (Sarawak-class volumes toward 10+ s); model it as a function of local passage volume/wall area (Sabine: $T_{60} = 0.161\,V/(A\,\bar\alpha)$) rather than a binary flip. **(2) The "outside/cave" boundary is also a humidity/temperature/density boundary** — the mist zone's condensation and the temperature step are what a player *feels* and hears (air density changes sound speed ~0.6 m/s per °C). **(3) Barometric and chimney airflow is audible wind** — a few m/s at openings of large systems, reversing on weather timescales; same noise-budget category as surface wind but keyed to a different clock. **(4) Water** — drip rate (which the speleothem density model of §6 predicts) and cave-stream noise replace surface ambience; see the project's existing treatment in `research/natural-sound-generation-physics.md` §3.4 for the flow-noise synthesis side, which applies unchanged to underground streams.

---

## 11. Practical simulation synthesis: a cave-generation recipe for a voxel engine

Opinionated, numbers-first, targeting our engine's actual architecture (SVO octree at 2⁻⁷ m finest voxels near the camera, 512-m root regions, FastNoise2 for fields) — but architecture-agnostic where possible. The one-line philosophy: **generate the water first, then carve what the water carves.** Every real pattern in §§1–8 follows from flow + chemistry + time; faking the patterns directly (random worm tunnels) gets the statistics wrong in ways players feel.

### 11.1 The generation stack

**Step 0 — the joint/fracture field.** Generate 2–3 regionally-consistent orientation sets (strike sampled per-biome, dip 80–90°) plus near-horizontal bedding (1–3° regional dip) — domain-warped ridged 3D noise thresholded into planes, or fracture intensity sampled per column from 2D noise. This field is *the* skeleton: passage orientation (§3.5), maze density (§3.2), and doline alignment (§8.2) all key to it. Fracture spacing: 1–10 m (dense, Ford State 3–4) to 20–100 m (sparse, State 1–2).

**Step 1 — water table and base level.** One 2D field $z_{WT}(x,y)$: the heightfield low-pass filtered toward river/sea level, with aquifer drawdown near resurgences. Passage levels = paleo-water-table surfaces: 3–5 discrete levels spaced 20–60 m vertically, each older-and-higher (§4.5, Mammoth geometry). A per-region base-level history (100–300 m incision over ~3 Myr) generates the level stack for free.

**Step 2 — the flow graph.** A directed graph on the region's coarse 2D grid: inputs (doline swarms, sinking streams, diffuse epikarst recharge) → conduits → springs at valley floors/lithologic contacts. Grow headward along steepest water-table descent (the Rhoades–Sinacori direction, §3.3), branching with Horton-like statistics (the surface-drainage numbers from Part B apply underground too). Each edge carries discharge Q and flow length L — keep the **Q/L** ratio (§3.2) as the growth-rate proxy: top-decile edges become trunk passages, the rest crawlways or sub-voxel.

**Step 3 — carve the density field.** Per graph edge, carve a cross-section into the voxel density field:

- **Horizontal trunk at water-table levels:** phreatic tube, diameter 2–15 m (Mammoth trunks to 20 m; Mulu tubes mean 8.7×6.3 m; Deer Cave is the 100-m-class outlier, §4.3). Elliptical, slightly wider than tall.
- **Vadose input passages:** vertical shafts (cylindrical wells 2–10 m diameter, 10–100+ m deep; Veryovkina's Babatunda is 155 m) at doline axes and sinking-stream points; meandering canyons (3–15 m deep, 1–3 m wide, §4.4) along bedding routes from shafts to the water table.
- **Keyholes:** wherever a level was phreatic then drained — tube above, 1–2 m trench below. Two extrusions, instantly reads as "real cave."
- **Chambers:** from the size distribution, not uniform placement — lognormal-ish, median ~10–20 m span, 1% tail to 50 m, ~0.01% to 100+ m. Place at passage confluences (Q jumps), the water-table/spring margin, and under thin roof beds; bias survival to "massive bed" stratigraphic units (a per-region bed-competence field), capping spans at ~10–30 m elsewhere (§7.1).
- **Mazes:** where fracture intensity is high *and* uniform aggressiveness holds (under sandstone caprock; floodwater backflooding above trunk confluences; hypogenic settings), fill 0.1–0.5 km² footprints with network/spongework — orthogonal fissure passages 1–4 m wide, 5–15 m tall.

**Step 4 — breakdown.** Post-process every carved span with a stability check (span vs bed competence, §7.1): spans over threshold get a breakout dome above plus a talus cone below at 35–38° repose, block sizes 0.3–3 m, dome height capped by the self-arrest law ($h_c = h_0/(K_{loos}-1)$, $K_{loos}$ 1.4–1.8). Terminal breakdowns where passages meet valley walls = entrances.

**Step 5 — decoration.** Speleothem density per §6.4: high where the ceiling is under an active drip field, moderate near entrances (ventilated → degassing), zero in flood zones, dead caves where a prior-precipitation or high-CO₂ flag is set. Stalagmite radius from the $R_{eq}$ law with drip interval log-uniform 5–1000 s → 3–40 cm radius; straws/flowstone/rimstone as tiling variants; helictites as rare wander-noise forms. Scale check: 0.05–0.5 mm/yr vertical growth against the cave's simulated age gives believable heights without simulating growth.

**Step 6 — fill with water/climate.** Below $z_{WT}$: fill to the water table (siphons at level downstream ends). Tag each carved cell with climate state: distance-to-entrance → temperature blend from surface MAT to cave-constant, RH ramp to ~100%, CO₂ field (entrances 420 ppm → interior 1,000–6,000, higher in dead-end lowers), and ventilation regime (chimney if two entrances straddle elevation; barometric if volume/opening ratio is huge — then modulate an audible wind with a slow weather-timescale oscillator, §10.3). These tags feed the audio system's outside/cave switch as continuous parameters rather than a boolean.

### 11.2 Parameter table (all values sourced above)

| Parameter | Range | Default | Source § |
|---|---|---|---|
| Soil/epikarst pCO₂ (temperate/tropical) | 0.001–0.06 atm | 0.02 | §1.2 |
| Open-system saturation Ca | 1–2 mmol/L | 1.5 | §2.1 |
| Denudation rate (humid) | 10–100 mm/kyr | 30 | §2.2 |
| Conduit wall retreat (active) | 0.01–1 mm/yr | 0.1 | §5.1 |
| Time to traversable passage | 10⁴–10⁵ yr | — | §5.1 |
| Epikarst thickness | 3–15 m (30+ alpine) | 8 | §4.5 |
| Vadose shaft depth | 10–155 m | 30–80 | §4.5 |
| Water-table level spacing (multi-level) | 20–60 m | 40 | §4.5 |
| Trunk passage diameter | 2–20 m | 6 | §4.4 |
| Canyon width:height | 1:3–1:10 | 1:5 | §4.4 |
| Chamber span distribution | 10–435 m, lognormal tail | median 15 m | §4.3 |
| Chamber height:span | 0.1–0.35 | 0.2 | §4.3 |
| Cave-system areal footprint | 0.5 km² (epigenic maze) – 200+ km² (Mammoth-class) | 5–50 km² | §4.1 |
| Passage density (map length per area) | 1–10 km/km² typical; up to ~30 in maze patches | 4 | §4.1/§4.4 |
| Doline density (mature karst) | 60–500 /km² | 100–200 | §8.1 |
| Doline diameter (solution) | 14–80 m (5th–95th pct, Slovenia) | 40 | §8.1 |
| Collapse doline depth | 20–100+ m | 50 | §8.1 |
| Polje size | 2–40 km² | — | §8.3 |
| Tower karst: tower height/width | 50–300 m / 50–150 m | — | §8.4 |
| Fengcong hillslope | 60–75° | — | §8.4 |
| Lava tube diameter | 1–21 m | 3–10 | §9.2 |
| Lava tube length (master) | 1–59 km | — | §9.2 |
| Lava tube gradient | 1–6° (mean Kazumura 1.9°) | 2–4° | §9.2 |
| Cave interior temperature | local MAT ± 0.1–8.8 °C annual swing | MAT | §10.1 |
| Cave RH (deep) | 95–100% | 99% | §10.2 |
| Cave CO₂ | 600–60,000 ppm | 1,000–6,000 | §10.2 |
| Barometric airflow (openings) | 0.5–10 m/s, weather-paced | — | §10.3 |
| Stalagmite radius | 3–40 cm (giant 10 m) | 9 cm @ τ=30s | §6.3 |
| Stalagmite vertical growth | 0.01–1 mm/yr | 0.1 | §6.2 |
| Gypsum denudation | ~400 mm/kyr | — | §1.6 |
| Breakdown loosening K | 1.1–2.0 | 1.5 | §7.2 |
| Breakdown repose angle | 35–40° | 37° | §7.2 |### 11.3 Coupling to the surface heightfield

Karst must deform the surface, or it's invisible. Cheapest-first:

1. **Doline swarms:** where (limestone AND low slope AND humid) flags coincide, subtract dolines from the heightfield: point process at 60–500/km², diameter lognormal centered ~40 m, depth ≈ diameter/4 for solution forms, steeper (≈ diameter/2, walls 60–90°) for the rare collapse forms. Align elongation with the joint field. Dolines are the surface expression of shafts — place the Step-3 shafts at doline centers, and drain them.
2. **Sinking streams and dry valleys:** route the surface hydrology; where a stream crosses onto karst, terminate it at a ponor (blind valley) and hand its discharge to the cave graph as an input edge. Leave "expected" valleys dry across karst plateaus. Resurgences: re-add the discharge as springs at valley contacts, with flood-pulse behavior (the epiphreatic concept).
3. **Sinkhole plains vs caprock plateaus:** the Mammoth configuration — a resistant cap over the limestone preserves plateau ridges with caves beneath, while adjacent uncapped terrain becomes a sinkhole plain lowering 10–100 mm/kyr. Implement as a caprock mask gating doline density and surface lowering.
4. **Uvalas/poljes:** coalescence in the doline point process — after N Myr of landscape age, merge overlapping dolines into compound depressions and, at the largest scales, flood-stage poljes with estavelle behavior.
5. **Tower karst / cockpit (tropical flag):** where "fenglin" conditions hold (stable base level, allogenic input), flatten plains aggressively while leaving fracture-bounded residual towers (edges from the joint field, 60–75° slopes, 50–300 m tall); under "fengcong" conditions (rapid uplift), steepen and deepen instead.
6. **The Chicxulub trick:** if the world has any buried ancient structure (impact crater, rift, reef front), modulate the fracture-intensity field with it — surface karst density will inherit the pattern with geological delay, a wonderful piece of world-lore made physical.

### 11.4 What to fake (blunt calls)

- **Do not** simulate the PWP kinetics per water parcel — the kinetics matter only through their *consequences* (where caves form, §1.5), which the graph+Q/L approach already encodes.
- **Do not** grow caves incrementally in simulated time — place them at their end-state from the statistics; reserve a "passage age" attribute for decoration choices (older = more breakdown, more sediment fill, more speleothems if drip-fed).
- **Do** keep a continuous water-table field rather than per-chunk decisions — the #1 artifact risk in voxel caves is discontinuity at chunk boundaries; the graph is the seam contract.
- **Do** make the audio boundary (outside/cave) a function of measured local state (enclosure from the density field, distance-to-entrance, climate tags) so reverb, wind, and drip ambience all agree — this is the payoff the climate section exists for.
- **Voxel-scale honesty check:** at 2⁻⁷ m (7.8 mm) finest voxels with ~4 m full-res radius, a 6-m trunk passage near the player renders beautifully, and the 15-m chambers hold up at one LOD step back; the 100–400 m chamber class needs the SVO's sparse-brick LOD to be affordable at all — which is exactly what it's for. Cave *volumes* are tiny relative to rock: even Mammoth's ~1–2% passage-per-rock volume means the octree stays overwhelmingly solid — carve, don't rebuild.

## Provenance

Written 2026-09 by the terrain-research subagent (Agent 4, caves/karst domain) as Part 4 of the merged Earth-terrain document, following the house style of `research/water-physics-and-wave-simulation.md`. Questions 40–50 (Section D) of `research/_terrain_question_bank.md` are answered in §§1–10; section numbering is fixed for the merge. All parent numeric anchors were independently verified against sources; disagreements are reported inline and consolidated below. Web research: 9 search batches / ~28 queries via Exa against primary literature (Palmer, Ford & Ewers, Dreybrodt, Klimchouk, Plummer–Wigley–Parkhurst, Badino, Faimon), agency reports (USGS, NPS, NIOSH, FL FGS/FDOT, INERIS), and the national Slovene doline lidar census. URLs were captured from live search results; a few claim-adjacent numbers (e.g. post-1995 Kazumura resurvey) could not be pinned to a primary source and are flagged rather than asserted.

**Consolidated anchor-disagreement ledger (parent value vs source value):**

| # | Anchor | Parent | Source | Verdict |
|---|---|---|---|---|
| 1 | Epikarst-spreading denudation | 5.6–28 mm/kyr (spread over 1–5 m epikarst) | 20–100 mm/kyr measured humid; ~34 mm/kyr best-fit | **Framing error** — denudation is catchment-average lowering; depth-spreading doesn't divide it. Arithmetic and Ca verify. |
| 2 | Soil pCO₂ 10–100× atmosphere | 10–100× | 25–150× (0.1–6%) | Verified in spirit; range slightly wider. |
| 3 | Mammoth Cave length | >670 km | 685.6 km | Verified, update. |
| 4 | Veryovkina depth | ~2.2 km | 2,209 m | Verified. |
| 5 | Sarawak Chamber | 600×415×80 m | 600×435×115 m (laser 2011) | **Width and height wrong** (415→435, 80→115). |
| 6 | Son Doong | 5 km, 200 m high | ~4.5–5 km, 150–250 m high | Verified. |
| 7 | Wall retreat | 0.01–1 mm/yr | 0.01–1 mm/yr (max 0.1–1; typical 0.1) | Verified. |
| 8 | Stalagmite growth | 0.01–3 mm/yr | 0.01–1 mm/yr (bulk 0.05–0.5) | **Top end too generous** (3 → 1). |
| 9 | Kazumura length | 65 km | 59.3 km (survey paper) | **Disagreement**; popular ~65 km unsourced in survey literature. |
| 10 | Lava tube max diameter | up to ~30 m | up to ~21 m (Kazumura) | Slightly generous. |
| 11 | Sinkhole density hundreds/km² | "up to hundreds" | up to 500/km² (Slovenia) | Verified. |
| 12 | Cave temp ≈ local MAT | — | confirmed (Badino 2004; global comparative study) | Verified. |

## Sources

1. Plummer, Wigley & Parkhurst (1978), kinetics of calcite dissolution — https://doi.org/10.2475/ajs.278.2.179
2. Plummer, Parkhurst & Wigley (1979), critical review of calcite kinetics — https://doi.org/10.1021/bk-1979-0093.ch025
3. White, W. (2016), Chemistry and karst — https://doi.org/10.3986/ac.v44i3.1896
4. BRGM review of carbonate kinetic data — https://infoterre.brgm.fr/rapports/RR-39062-FR.pdf
5. Faimon et al. (2012), epikarstic pCO₂ from drip hydrochemistry — https://doi.org/10.3986/ac.v41i1.47
6. Gibraltar karst CO₂ / ground air — https://www.sciencedirect.com/science/article/abs/pii/S001670371630028X
7. Savoy watershed CO₂ and dissolution dynamics — https://www.sciencedirect.com/science/article/abs/pii/S0009254118301219
8. Lascaux CO₂ dynamics (soil–epikarst continuum) — https://www.springerprofessional.de/the-co2-dynamics-in-the-continuum-atmosphere-soil-epikarst-and-i/16644174
9. Denudation concepts and methods — https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004133
10. Lauritzen (1990), Svartisen autogenic/allogenic denudation — https://doi.org/10.1002/esp.3290150206
11. Banks, UK limestone dissolution rates — https://nora.nerc.ac.uk/id/eprint/503972/1/Dissolution%20rates%20in%20limestone%20v4.pdf
12. Austrian Alps carbonate tablet field test — https://www.sciencedirect.com/science/article/abs/pii/S0169555X04003101
13. Palmer (1991), Origin and morphology of limestone caves — https://doi.org/10.1130/0016-7606(1991)103%3c0001:oamolc%3e2.3.co;2
14. Palmer (2011), Distinction between epigenic and hypogenic maze caves — https://www.sciencedirect.com/science/article/abs/pii/S0169555X11001498
15. Hill (1990), sulfuric acid speleogenesis, Carlsbad — https://doi.org/10.1306/0c9b2565-1710-11d7-8645000102c1865d
16. Palmer, support for sulfuric acid origin, Guadalupes — https://nmgs.nmt.edu/publications/guidebooks/downloads/57/57_p0195_p0202.pdf
17. NCKRI Special Paper 2, H₂S in Guadalupe caves — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1017&context=kip_monographs
18. Klimchouk (2007), Morphogenesis of hypogenic caves — https://www.sciencedirect.com/science/article/abs/pii/S0169555X08004108
19. Ford & Ewers (1978), four-state model — https://doi.org/10.5038/1827-806x.10.3.1
20. Ford (2014), four-state model retrospective — https://digitalcommons.usf.edu/kip_articles/6893
21. Ford (2003), perspectives in karst hydrogeology — https://www.academia.edu/35559784/Perspectives%5Fin%5Fkarst%5Fhydrogeology%5Fand%5Fcavern%5Fgenesis
22. Dreybrodt (1990), dissolution kinetics in karst aquifer development — https://doi.org/10.1086/629431
23. Dreybrodt & Gabrovšek (2003), basic processes of karst evolution — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=5791&context=kip_articles
24. Bakalowicz (1997), early karst conduit development — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/96WR01332
25. Perne, Covington & Gabrovšek (2014), pressurized-to-free-surface conduit evolution — https://doi.org/10.5194/hess-18-4617-2014
26. Palmer, dynamics of cave development by allogenic water — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1032&context=kip_articles
27. List of longest caves (Wikipedia, 2026) — https://en.wikipedia.org/wiki/List_of_longest_caves
28. Sistema Ox Bel Ha — https://en.wikipedia.org/wiki/Sistema_Ox_Bel_Ha
29. Showcaves longest-cave statistics — https://showcaves.com/english/explain/Statistics/Longest.html
30. Veryovkina Cave — https://en.wikipedia.org/wiki/Veryovkina_Cave
31. Sarawak Chamber — https://en.wikipedia.org/wiki/Sarawak_Chamber
32. Mulu Caves Project, Sarawak Chamber — https://mulucaves.org.uk/articles/sarawak-chamber
33. Caves of Malaysia, statistics (Son Doong, Deer Cave, Miao Room) — https://cavesofmalaysia.wordpress.com/cave-statistics/
34. Mulu speleogenesis & morphology study — https://www.geojournal.net/uploads/archives/8-4-227-593.pdf
35. Jordá-Bordehore (2017), stability of natural caves, 137 spans — https://digital.csic.es/bitstream/10261/277147/1/stability_assessment_natural_2017.pdf
36. Diederichs & Kaiser (1999), voussoir beam analogue — https://www.sciencedirect.com/science/article/abs/pii/S0148906298001806
37. Tharp & Holdrege, very long-term loading of cave roof beams — https://doi.org/10.1201/9781003761365-140
38. Heidong quarry long-term span study — https://doi.org/10.18814/epiiugs/2013/v36i1/006
39. NIOSH roof stability in US limestone mines — https://stacks.cdc.gov/view/cdc/226788
40. Osborne (2002), cave breakdown by vadose weathering — https://doi.org/10.5038/1827-806x.31.1.3
41. Andrejchuk & Klimchouk (2002), Kungurskaya breakdown mechanisms — https://doi.org/10.5038/1827-806x.31.1.5
42. Klimchouk & Andrejchuk (2002), Western Ukraine gypsum breakdown — https://doi.org/10.5038/1827-806x.31.1.4
43. Pisarowicz/White, gypsum wedging in Mammoth Cave — https://caves.org/wp-content/uploads/Publications/JCKS/v65/v65n1-White.pdf
44. USGS WRI 85-4126, west-central Florida sinkholes — https://pubs.usgs.gov/wri/1985/4126/report.pdf
45. Florida FGS, sinkhole favorability — https://inspectapedia.com/vision/Florida-Sinkhole-Report.pdf
46. FDOT, central Florida sinkhole evaluation — https://fdotwww.blob.core.windows.net/sitefinity/docs/default-source/geotechincal/geotechnical/documents/cfsinkholeevaluation.pdf?sfvrsn=201e9fef_0
47. Pinellas County sinkhole modification thesis — https://digitalcommons.usf.edu/etd/1306
48. Marion County FL sinkhole susceptibility — https://pmc.ncbi.nlm.nih.gov/articles/PMC6509126/
49. Perry et al. (1995), Ring of Cenotes / Chicxulub — https://doi.org/10.1130/0091-7613(1995)023%3c0173:ROCSNW%3e2.3.CO;2
50. Connors et al. (1996), Yucatán karst and Chicxulub size — https://doi.org/10.1111/j.1365-246x.1996.tb04066.x
51. Surficial geology of the Chicxulub crater — https://link.springer.com/article/10.1007/BF00575099
52. Ring of Cenotes explainer (Northwestern) — https://sites.northwestern.edu/monroyrios/ring-of-cenotes/
53. EGU 2024, cenote formation above Chicxulub — https://doi.org/10.5194/egusphere-egu24-3188
54. Aguilar et al., Yucatán karst depression density — https://doi.org/10.4311/2015es0124
55. Mihevc & Mihevc (2021), Slovenian lidar doline census — https://doi.org/10.3986/ac.v50i1.9462
56. UNESCO tentative list, Classical Karst — https://whc.unesco.org/fr/listesindicatives/6072/
57. Day & Tang (2000), Guilin tower-karst slopes — https://doi.org/10.1002/1096-9837(200010)25:11%3C1221::AID-ESP133%3E3.0.CO;2-D
58. Waltham, karst lands of southern China (fengcong/fenglin) — https://www.researchgate.net/publication/263374259_The_karst_lands_of_southern_China
59. Guangxi fenglin/fengcong chapter — https://link.springer.com/chapter/10.1007/978-90-481-3055-9_30
60. Lijiang plain tower karst structure — https://www.schweizerbart.de/papers/zfg/detail/36/98327/Structural_and_hydrogeological_origin_of_tower_karst_in_southern_China_Lijiang_plain_in_the_Guilin_region
61. Klimchouk (2004), epikarst origin and classification — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=6334&context=kip_articles
62. Williams (2008), epikarst review — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1173&context=ijs
63. Jones (2013), physical structure of epikarst — https://doi.org/10.3986/ac.v42i2-3.672
64. Greeley, lava tube formation (USGS PP1350 ch. 59) — https://pubs.usgs.gov/pp/1987/1350/pdf/chapters/pp1350_ch59.pdf
65. Valerio, Tallarico & Dragoni (2008), lava tube formation mechanisms — https://doi.org/10.1029/2007jb005435
66. Dragoni, Piombo & Tallarico (1995), tube roofing model — https://doi.org/10.1029/94jb03263
67. Allred & Allred, Kazumura Cave development and morphology — https://legacy.caves.org/pub/journal/PDF/V59/V59N2-Allred.pdf
68. Badino (2004), cave temperatures and global climatic change — https://doi.org/10.5038/1827-806x.33.1.10
69. Badino (2006), underground drainage and geothermal flux — https://doi.org/10.3986/ac.v34i2.261
70. James, Banner & Hardt (2015), global cave ventilation model — https://doi.org/10.1002/2014gc005658
71. Pflitsch, Wiles & Horrocks (2010), barometric caves (Jewel/Wind) — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=2599&context=kip_articles
72. Gomell et al. (2021), air pressure propagation in barometric caves — https://doi.org/10.5038/1827-806x.50.3.2393
73. Gomell & Pflitsch (2022), airflow dynamics in Wind and Jewel Cave — https://doi.org/10.5038/1827-806x.51.3.2437
74. French caves, APV-induced temperature variations — https://www.sciencedirect.com/science/article/pii/S2772883822001200
75. Global comparative cave temperature study — https://pmc.ncbi.nlm.nih.gov/articles/PMC10676404/
76. Dreybrodt & Romanov, regular stalagmites — https://digitalcommons.usf.edu/cgi/viewcontent.cgi?article=1049&context=kip_articles
77. PNAS, shapes of ideal stalagmites (Damköhler classification) — https://pmc.ncbi.nlm.nih.gov/articles/PMC12557760/
78. Sofular Cave stalagmite shape modeling — https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2022.969211/full
79. Baker et al., intra/inter-annual stalagmite growth — https://www.sciencedirect.com/science/article/abs/pii/S0009254100003995
80. StalGrowth program — https://www.mdpi.com/2076-3263/11/5/187
81. NPS, Mammoth Cave karst summary — https://npshistory.com/publications/maca/cave-karst-summary-2016.pdf
82. NPS, exploring the world's longest known cave — https://www.nps.gov/articles/000/exploring-the-worlds-longest-known-cave.htm
83. AIPG field guide, Mammoth Cave geology — https://www.uky.edu/KGS/geoky/fieldtrip/2005%20AIPG%20Guidebooks/MammothCave.pdf
84. Palmer, geology of Mammoth Cave — https://digitalcommons.usf.edu/kip_articles/8226/
85. Gypsum reactive-transport karstification (Energies) — https://mdpi-res.com/d_attachment/energies/energies-15-00761/article_deploy/energies-15-00761-v2.pdf?version=1642738727
86. Stafford et al. (2008), Castile Formation gypsum karst — https://doi.org/10.5038/1827-806x.37.2.1
87. INERIS gypsum dissolution guide — https://www.ineris.fr/sites/default/files/contribution/Documents/Ineris-Guide_Gypse_VA-20_11Nov-WEB.pdf
88. Li & Einstein (2017), gypsum cavity evolution — https://doi.org/10.1002/2017wr021776

---

## Appendix A — Forty further questions

My own questions, beyond the parent's 40–50, each answered or explicitly dispositioned.

1. **How fast does an epikarst develop on fresh rock?** Decades–centuries for the fissure network (stress-release driven), 10³–10⁴ yr for full hydrologic function — slower under soil (cushioned), faster with biotic CO₂. Dispositioned: quantitative rates per lithology not pinned to a single source; see [61].
2. **What is the diameter distribution of epikarst storage voids?** Millimeter–decimeter fissures, a few % bulk porosity vs <0.5% in the bulk rock. Dispositioned from [61]/[62] qualitative ranges.
3. **Does dissolution deepen or flatten karst drainage divides?** Both: divides lower by diffuse denudation while conduits incise below — net effect is drainage concentration and divide migration toward the weaker aquifer. Dispositioned: not sourced quantitatively here; flagged for Part B cross-reference.
4. **What controls spring density (springs per km of strike)?** Lithologic boundaries and stratal dips; contact springs line the karst/non-karst boundary. Dispositioned qualitatively via [56].
5. **How much of a karst aquifer's flow is conduit vs diffuse at maturity?** >50% of recharge reaches the vadose zone already concentrated ([61], citing Kiraly); mature aquifers transmit the majority of storm discharge through conduits. Answered.
6. **What is the depth of the "no-cave" floor — below which karstification shuts off?** Set by base level and the 4th-order kinetics: practically, caves stop where flow paths are long and closed-system; no universal depth. Answered via §1.5/§3.3.
7. **How do scallop sizes distribute along a real passage?** Lognormal around a flow-velocity-set mean; coarser near inlets. Dispositioned: scallop–velocity calibration (Curl's equation) not independently sourced here; texture hint only.
8. **What is the temperature of hypogenic vs epigenic cave air?** Epigenic ≈ MAT; hypogenic can be warm (thermal input) — hydrothermal caves exceed MAT. Answered via [69] framework.
9. **How deep can siphons go and still be dived?** The deepest cave siphons push past 100 m in the Caucasus systems; Veryovkina's terminal siphon is 26 m at −2,209 m. Answered from [30].
10. **What is the world's deepest single shaft?** ~450-m class (some Mexican pits); Veryovkina's Babatunda (155 m) is cited for that cave. Dispositioned: global superlative not pinned to a primary source in this research.
11. **How does seawater mixing-zone speleogenesis (halocline) differ?** Dissolution at the fresh/salt interface by mixing corrosion; produces flank-margin caves — horizontally extensive, low-density mazes in coastal platforms (Bahamas model per Mylroie & Carew, cited in [16]). Answered.
12. **Can caves form below the sea?** Yes — blue holes and drowned flank-margin caves; the Yucatán systems' deepest point is 57.3 m. Answered from [28].
13. **What is the CO₂ production rate of a tropical soil?** 6–25 g CO₂/m²/day measured at Lascaux's temperate site; tropical higher. Answered from [8].
14. **How do drip intervals distribute through a cave?** Broadly lognormal, site means from seconds to hours; the shape-defining range is 5–1000 s. Dispositioned from the modeling literature [76]/[78].
15. **What is the survival time of a straw stalactite before clogging?** Variable; straws that keep feeding grow for 10³–10⁴ yr. Dispositioned; no hard number sourced.
16. **Do caves have weather?** Yes — barometric pressure fronts, fog formation at entrance zones, and "cave clouds" in big chambers. Answered via [71]–[74].
17. **What is the fastest recorded passage growth?** Floodwater-cave fissures reaching traversable size in 10 kyr ([26]); gypsum cavities in decades ([85]). Answered.
18. **How often do catastrophic chamber collapses happen?** Kungurskaya: 64 documented failure events in 23 years — small ones are continuous; chamber-scale ones are millennial. Answered from [41].
19. **What is the largest known breakdown pile?** Chamber-filling piles of 10⁵–10⁶ m³ in the Ukrainian gypsum mazes (Zoloushka-scale). Dispositioned from [42] qualitative.
20. **How do denudation rates vary with relief, holding climate fixed?** Higher relief → deeper flow paths → more closed-system → *lower* specific rates; but allogenic inputs raise local rates. Partially answered via [9]'s e-folding model; flagged for Part B synthesis.
21. **What is the aspect ratio of dolines globally?** Depth ≈ diameter/4 to /10 for solution forms; steeper for collapse. Answered from [55] statistics (mean 9 m deep / 42 m diameter ≈ /4.7).
22. **How do uvalas form — coalescence or single mega-dissolution?** Mostly coalescence of dolines with joint control; the Yucatán census separates them statistically ([54]). Answered.
23. **What is the flood periodicity of a polje?** Seasonal (Cerknica floods months each year) to multi-annual. Answered from [56].
24. **How big can an estavelle be?** Meter-scale openings up to canyon-scale overflow conduits. Dispositioned qualitatively.
25. **Does karst drainage obey Horton's laws?** Yes — conduit networks are drainage networks; bifurcation ratios near surface-river values. Dispositioned; cross-ref Part B rather than independently sourced.
26. **What is the matrix porosity of a telogenetic vs eogenetic carbonate?** Telogenetic (uplifted, cemented): <2%, 0.005–0.5% in the bulk mass; eogenetic (young): 20–40%, caves rare at enterable scale (Ford State 5). Answered from [61]/[62].
27. **How quickly does a lava tube cool after drain-out?** Years–decades for a 10-m-class tube to reach ambient; Kazumura now tracks a local MAT gradient of 15–22 °C. Dispositioned from [67] temperature data; explicit cooling model not sourced.
28. **Can lava tubes re-activate?** Yes — reoccupation by later flows is common and produces multi-level stacking with cross-cutting. Answered from [67].
29. **What is the maximum slope at which lava tubes form?** Model says <~6°; Kazumura mean 1.9°. Answered from [65]/[67].
30. **How many entrances does a long lava tube have?** Kazumura: 82, mostly roof collapses. Answered from [67].
31. **What lives in caves (for ecology-driven generation)?** Troglobites, troglophiles, bats; guano deposits drive entire food webs. Dispositioned: out of scope for terrain; flagged for the ecology part.
32. **How do cave winds sound at different passage scales?** Barometric entrance winds reach several m/s (audible, weather-paced); interior flows are cm/s (silent). Answered from [71]–[73].
33. **What is the pH range of cave drip water?** ~6.4–8.2 measured at Mulu. Answered from [34].
34. **How does agriculture/land-use change karstification rates?** Enhanced soil CO₂ and focused recharge can accelerate dissolution and conduit development irrespective of seasonality ([epikarst land-use study](https://doi.org/10.1002/esp.4768)). Answered.
35. **What is the characteristic spacing of inception horizons?** Bedding-controlled, meters–tens of meters in a thick limestone sequence. Dispositioned from [83]'s stratigraphic treatment.
36. **How much sediment do caves store?** Entire valley-fill sequences — Mammoth's passages store quartz-pebble caches datable by cosmogenics. Answered from [81]/[83].
37. **What is the relict survival rate of cave levels vs surface destruction?** Caprock preservation is the dominant control — uncapped karst destroys upper levels as fast as lower ones form. Answered from [82].
38. **How do you detect karst from orbit (validation target for generated worlds)?** Dolines on DEMs, absence of surface drainage, collapse-density anomalies — the Yucatán and Slovenia censuses both used remote sensing at scale. Answered from [54]/[55].
39. **What is the geometry of the water table inside a karst massif?** A gently sloping surface with steep draws toward conduits; gradients orders of magnitude below the surface topography. Answered from [19]/[21].
40. **What is the single best one-number summary of cave "believability" for validation?** Passage-length per unit area (km/km² of karst), checked against 1–10 for typical systems and ~30 in maze patches — everything else (chambers, levels, shapes) hangs off the same generation graph. My call; grounded in §4.1/§4.4 statistics.
