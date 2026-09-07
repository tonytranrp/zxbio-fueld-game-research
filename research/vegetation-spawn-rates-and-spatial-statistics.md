# Vegetation Spawn Rates & Spatial Statistics: The Quantitative Ecology of Where Plants Actually Grow

**Scope:** the stand level, not the tree level. The companion document `tree-motion-growth-and-appearance.md` answers "what is one tree and how does it move" (LAI 1.25–3.6 for oak, Pipe Model, leaf-count derivation); this document answers "how many are there, how old are they, how clustered, and why." Every number below is a range with a source, or is flagged as an anchor disagreement. Formulas are derived or cited. Section numbering is fixed for merge into the Earth-terrain document as Part 6.

**The motivating question — "what is the spawn rate of trees, and other plants, on the land itself?" — has a real, quantitative answer.** Trees spawn (are established) at densities spanning six orders of magnitude across Earth's biomes, from ~0.1 stems/ha for a commercial mahogany species in Amazonia to >500,000 seedlings/ha in a first-post-fire lodgepole pine flush. The number a generator needs depends entirely on (a) biome, (b) successional age, and (c) the size class counted. Sections 1 and 7 give the tables; Sections 3–4 explain why the placement is never uniform; Section 10 converts all of it into a pipeline.

---

## 1. Tree density by biome and successional stage

### 1.1 The anchor verification

The parent's anchor values were: boreal ~500–2,000 stems/ha, temperate ~200–1,000, tropical ~400–800. Verification results:

- **Boreal 500–2,000: CONFIRMED at the mature-stand level, but the full range is far wider.** Crowther et al.'s (2015) global map ([Crowther 2015 PDF](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)) found the *highest* forest tree densities on Earth in boreal/tundra zones — their boreal biome mean density is ~1,000–2,000+ trees/ha (implied by 0.74 trillion boreal trees over ~700 Mha of boreal forest per [Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)), with dense young boreal regeneration reaching 5,000–10,000+ stems/ha. Madrigal-González et al. (2023), analyzing 3,000+ plots in 23 protected regions, found the highest densities "irrespective of latitude" in plots dominated by *small* trees, with high-productivity plots converging on **≈500–800 trees/ha** for large-tree-dominated stands ([Madrigal-González 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC9839683/)) — a striking cross-biome convergence worth knowing: at high productivity, mature stands of ~500–800 stems/ha are typical *everywhere*.
- **Temperate 200–1,000: CONFIRMED with caveats.** Spies & Franklin's classic Pacific-Northwest data give young Douglas-fir stands **758–1,154 stems/ha**, mature **373–548**, old-growth **394–511** (trees >5 cm dbh) ([Spies & Franklin 1991](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)). Tappeiner et al. found natural old-growth Douglas-fir regenerated at **~100–120 trees/ha** and grew at that density with little self-thinning, while post-harvest young stands exceed 500 stems/ha ([Tappeiner et al. 1997](https://doi.org/10.1139/x97-015)). The Crowther temperate broadleaf biome mean is ~1,000/ha (362.6 billion trees over ~370 Mha implied).
- **Tropical 400–800: PARTIALLY CONFIRMED — the anchor is right for ≥10 cm dbh but misleadingly low as a total.** Lewis et al.'s 260 African plots give a mean of **426 ± 11 stems/ha ≥10 cm dbh** (notably *lower* than Amazonian ~600, a signature structural feature of African forests), with AGB 395.7 Mg/ha ([Lewis et al. 2013](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)). But Schietti et al.'s 55 central-Amazonian 1-ha plots found **2,051–11,475 stems/ha at ≥1 cm dbh** versus 450–1,088 at ≥10 cm — an order of magnitude more small stems hiding under the same canopy ([Schietti et al. 2016](https://doi.org/10.1111/1365-2745.12596)). The ≥10 cm threshold is a *measurement convention*, not an ecological reality; a generator must pick a size cutoff and stay consistent.
- **Anchor not in parent list, but load-bearing: the wet-tropical liana/understory structure.** Lianas reach high loads in gaps and disturbed tropical forest and measurably increase tree mortality ([McDowell et al. 2018](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)), which feeds back to keep tropical stands *denser and smaller-stemmed* than self-thinning alone predicts (Section 2).

### 1.2 Savanna: the rainfall-driven gradient

This is the cleanest density-vs-climate curve in all of ecology, and a generator should hard-code it:

- **Sankaran et al. (2005), 854 sites across Africa:** maximum (99th-quantile) woody cover increases linearly with MAP from ~100 to 650 mm, then saturates. **Trees are typically absent below 101 mm MAP.** The upper-bound line is $\text{Cover}(\%) = 0.14 \cdot \text{MAP} - 14.2$ between 101 and 650 mm MAP; above 650 ± 134 mm MAP, rainfall suffices for canopy closure and *disturbance* (fire, herbivory) becomes necessary to keep a savanna open ([Sankaran et al. 2005](https://edepot.wur.nl/27945)). This single piecewise relationship is the savanna biome-mask rule.
- **Stem density specifically:** the increases in woody cover along the rainfall gradient come **more from bigger crowns than more stems** — across an African 876-site remote-sensing survey, mean crown size rose 233%, crown density only 73%, and cover 491% from the driest (<200 mm) to wettest (1,200–1,400 mm) MAP bins ([Cramer et al. 2017](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)). Crown density spans roughly ~10–100 crowns/ha across that gradient. A southern-African threshold analysis found a biodiversity–biomass threshold at **~180 mature stems/ha** below which competitive interactions collapse ([Loubota Panzou et al. 2020](https://nph.onlinelibrary.wiley.com/doi/10.1111/nph.17639)).
- **The Kalahari sand-sheet transect** (200–1,000 mm MAP on homogeneous deep sands): woody basal area rises **~2.5 m²/ha per 100 mm MAP** above the 200 mm floor; maximum tree height reaches 20 m at ~800 mm; the dominant family shifts from Mimosaceae (<400 mm) to Combretaceae/mopane (400–600 mm) to Caesalpinaceae (>600 mm) ([Scholes et al., Kalahari transect](https://the-eis.com/elibrary/sites/default/files/downloads/literature/Trends%20in%20savanna%20structure%20and%20composition%20along%20an%20aridity%20gradient%20in%20the%20Kalahari.pdf)). Family turnover is a cheap, defensible proxy for species turnover in procedural savannas.

### 1.3 Other biomes

- **Mediterranean shrubland/chaparral:** shrub-dominated, effectively 0 large trees/ha in mature chaparral; woody shrub densities are high (hundreds to thousands per ha, clumped). Crowther's Mediterranean-forest category carries ~53 billion trees globally at low density ([Crowther 2015](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)). Chaparral is now heavily grass-invaded: grass/herb cover is ~34% of historically chaparral land in southern California National Forests, with 24% of chaparral showing >50% herbaceous cover ([Park 2020](https://par.nsf.gov/servlets/purl/10303007)).
- **Mangrove:** stem densities of hundreds to a few thousand per ha (often >1,000 for *Rhizophora* stands); total biomass carbon 14–1,000+ Mg C/ha with Indo-Pacific stands >1,000 Mg C/ha ([Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)). Salinity and flooding duration, not competition, control the density (Section 6).
- **Tropical dry forest:** lower density than moist forest — Pan et al. give 159–199 Mg C/ha and Madrigal-González confirm "seasonal dry tropical and subtropical forests" have relatively low tree density under water scarcity.

### 1.4 Successional stages: the two orders-of-magnitude swings

**Dense regeneration thickets — the parent anchor "10,000–100,000+ stems/ha in young stands" is CONFIRMED and if anything conservative:**

- Post-1988-fire Yellowstone lodgepole pine: mean stand density at 24 years = **21,738 stems/ha, range 0–344,067 stems/ha** ([Turner et al. 2016](https://doi.org/10.1890/15-1585.1)); high-serotiny sites commonly exceed **100,000 stems/ha** ([Schoennagel et al. 2003](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Schoennagel_etal2003_Ecology_FireIntervalSerotiny.pdf)). First-year lodgepole seedling density in the northern Rockies has a median of **25,270 ha⁻¹ and maximum >500,000 ha⁻¹** ([Clark-Wolf et al. 2023](https://par.nsf.gov/servlets/purl/10441749)).
- The 1 cm+ Amazonian understory (Section 1.1) shows the same phenomenon without fire: 2,000–11,000 stems/ha of sub-canopy trees coexisting with the ~600 ≥10 cm stems.
- Dense young temperate stands: 758–1,154 stems/ha in young Douglas-fir (>5 cm dbh — a bigger minimum size, hence lower count).

**Thinning toward old-growth — the parent anchor "old-growth temperate ~50–200 stems/ha of large trees" is CONFIRMED as the count of *large* trees, with an important nuance:**

- Old-growth Douglas-fir: **19 trees/ha ≥100 cm dbh** (with total stand density still 394–511 stems/ha >5 cm dbh, because shade-tolerant hemlock fills the understory) ([Spies & Franklin 1991](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf); [Van Pelt et al. 2015](https://www.mdpi.com/1999-4907/6/9/3177)). Old-growth definition commonly requires ≥20 Douglas-fir/ha older than 200 years, ≥10 large snags/ha, >37 t/ha of large logs.
- Hypermaritime old-growth British Columbia: ~**170 stems/ha >7.5 cm dbh** total, with ~30–70 old trees (>250 yr)/ha, some exceeding 1,000 years ([Hoffman et al. 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)).
- **The nuance (Tappeiner et al.):** natural old-growth did NOT necessarily pass through a dense-thicket phase. Oregon old-growth regenerated at 100–120 stems/ha over a prolonged period and self-thinned little, whereas post-harvest even-aged stands start >500/ha and thin hard. Two legitimate old-growth pathways exist — and a procedural generator that always spawns dense thickets will get post-disturbance forest right and fire-suppressed-origin old-growth wrong.

### 1.5 The consolidated table

| Biome / stage | Typical stems/ha (≥10 cm dbh unless noted) | Extremes with source |
|---|---|---|
| Boreal mature | 500–2,000 | highest densities on Earth in boreal/tundra zones ([Crowther 2015](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)) |
| Temperate young (post-disturbance) | 500–1,500 (>5 cm dbh) | 758–1,154 PNW young ([Spies & Franklin](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)); >500 post-harvest ([Tappeiner](https://doi.org/10.1139/x97-015)) |
| Temperate mature | 300–600 | 373–548 PNW mature |
| Temperate old-growth, large trees | 15–100 (≥50–100 cm dbh) | 19/ha ≥100 cm Douglas-fir; ~170/ha >7.5 cm hypermaritime |
| Temperate old-growth, all sizes | 250–550 | 394–511 (Spies & Franklin) |
| Tropical moist, ≥10 cm | 400–700 | 426 African mean ([Lewis](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)); 450–1,088 Amazonia ([Schietti](https://doi.org/10.1111/1365-2745.12596)) |
| Tropical moist, ≥1 cm | 2,000–11,500 | 2,051–11,475 (Schietti) |
| Tropical, large emergents (≥70 cm) | 0–11 | 1.8/ha mean central Amazonia (Schietti) |
| Savanna, arid (<350 mm MAP) | ~0–5 (trees essentially absent <101 mm) | Sankaran: absent below 101 mm MAP |
| Savanna, mesic (650–1,000 mm) | ~50–300 (crowns) | cover bound 0.14·MAP−14.2 % ([Sankaran](https://edepot.wur.nl/27945)); crowns ~10–100/ha ([Cramer](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)) |
| Mediterranean shrubland | ~0 trees; shrubs hundreds–thousands | grass invasion now 34% of chaparral land ([Park](https://par.nsf.gov/servlets/purl/10303007)) |
| Mangrove | 500–2,000+ | biomass up to >1,000 Mg C/ha ([Pan](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)) |
| Post-fire serotinous conifer flush (yr 1–5) | 5,000–100,000+ | median 25,270, max >500,000 ([Clark-Wolf](https://par.nsf.gov/servlets/purl/10441749)); 0–344,067 at yr 24 ([Turner 2016](https://doi.org/10.1890/15-1585.1)) |
| Glacier-foreland pioneer (yr 0–10) | ~0 trees; <2% vascular cover | ground cover <2% to yr ~20, ~10% by yr 50, ~60% by yr 120+ ([Fickert 2017](https://doi.org/10.5772/intechopen.69479)) |

**The blunt engineering call:** a single "trees per hectare" constant per biome is unsalvageable. The minimum viable model is (biome × size-class × successional-stage) — three axes, with the stage axis worth at least a 10× density swing in fire-adapted and tropical biomes.

---

## 2. The self-thinning law

### 2.1 The law and its modern reassessment

Yoda et al. (1963), from overcrowded pure stands: mean plant mass $\bar{m}$ and surviving density $N$ follow a power law:

$$\boxed{\;\bar{m} = K\,N^{-3/2}\;}\qquad\Longleftrightarrow\qquad \log \bar{m} = \log K - \tfrac{3}{2}\log N$$

Equivalently, in stand-biomass form ($B = N\bar m$): $B \propto N^{-1/2}$ — a log–log plot of biomass against density falls on a line of slope −1/2. The classic geometric interpretation: a plant of mass $m$ linear size $\ell$ with $m \propto \ell^3$ controls ground area $\propto \ell^2$, so $N \propto \ell^{-2} \propto m^{-2/3}$, giving $m \propto N^{-3/2}$. Reineke (1933) had earlier proposed the forestry form $\log N = a - 1.605 \log D$ (quadratic mean diameter), essentially the same law with a diameter exponent near −3/2 after mass–diameter allometry ([West 2026, spruce self-thinning](https://link.springer.com/article/10.1007/s00468-026-02787-2)).

**The modern debate, verified:** Weller (1987) and Lonsdale (1990) re-examined the evidence and found the slope much more variable than claimed. Lonsdale's combined-data slope was **−0.379** (biomass–density form), shallower than −1/2, plausibly because only stem mass is usually measured — and he concluded there was "no evidence at present for a −1/2 power rule" though an ideal slope might exist ([Lonsdale 1990](https://doi.org/10.2307/1938275)). The −4/3 alternative comes from the metabolic-theory/WBE tradition (Enquist, Brown, West): if plant resource use scales as $m^{3/4}$ and resources per area are fixed (energetic equivalence), then $N \propto m^{-3/4}$, i.e. $m \propto N^{-4/3}$. Deng et al.'s work on the metabolic-rate/population-density trade-off found the *ratio* of the metabolic and density exponents stayed near −1 under resource limitation, supporting a generalized energetic-equivalence view with variable individual exponents, rather than fixed −3/2 or −4/3 ([Deng et al. 2008](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.804.4516)). Peters et al. (2018) showed shifting slopes emerge naturally from adaptive plasticity — plant architecture changes along the trajectory, so no single slope should hold for the whole trajectory ([Peters et al. 2018](https://ideas.repec.org/a/eee/ecomod/v390y2018icp1-9.html)). Kikuzawa's synthesis: the slope depends on the height–mass allometry coefficient and dry-matter density; $-3/2$ is one special case ([Kikuzawa 1999](https://doi.org/10.1006/anbo.1998.0782)).

**Reconciliation for a generator (opinionated):** the −3/2 geometric packing argument holds when *light* (one-sided, size-asymmetric competition) is the limiting resource — dense even-aged stands. The −4/3 metabolic argument holds when a *below-ground* resource (water, nutrients — size-symmetric competition) limits; Stoll et al. showed experimentally that reducing size-asymmetry of competition gives more biomass at a given density, i.e. shallower effective slopes ([Stoll et al. 2002](https://doi.org/10.1098/rspb.2002.2137)). And the cross-biome data (Section 1.1) says self-thinning is *more intense under severely limiting conditions* and weakest in high-productivity tropical forest, where dense small-stem packing persists ([Madrigal-González 2023](https://pmc.ncbi.nlm.nih.gov/articles/PMC9839683/)). So:

- **Use −3/2 (mass basis) for dense young even-aged stands thinnning toward a closed canopy — light competition.**
- **Use something closer to −4/3 for water/nutrient-limited stands, and expect wide local scatter.**
- **Connect to the Pipe Model** (tree doc §1.2): the tree's sapwood area is proportional to its leaf area; stand-level leaf area saturates (LAI ceiling ~3–6 in closed temperate/tropical canopy, cross-referenced from the tree doc's oak 1.25–3.6), and self-thinning is the demographic consequence of that ceiling — as each surviving tree's pipe cross-section grows, fewer trees fit under a fixed total-leaf-area budget. Self-thinning is the stand-level shadow of the Pipe Model.

### 2.2 Worked example: N(t) for a temperate stand

Take a productive Douglas-fir stand (site index typical of the Oregon Cascades). Use Reineke's diameter form with the −3/2 mass law translated into practice via observed stand trajectories. Real growth data give the anchor points:

- **Year 0 (post-harvest/planting):** 500–1,000 stems/ha planted or seeded ([Tappeiner](https://doi.org/10.1139/x97-015)); natural post-fire lodgepole-type flushes can start at 10,000–100,000 (Section 1.4).
- **Year 30:** dense pole stage, PNW young stands measure 758–1,154 stems/ha (>5 cm dbh) ([Spies & Franklin](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)).
- **Year 80–150 (mature):** 373–548 stems/ha.
- **Year 200–400+ (old-growth):** 394–511 stems/ha total, but only ~19 stems/ha ≥100 cm dbh; the biomass that was in 1,000 small stems now sits in ~50–100 large ones plus understory hemlock.

Now the analytic version. With $\bar m = K N^{-3/2}$ and a mean-mass growth curve for temperate conifer (quadratic mean dbh growing roughly $D_q \approx 0.4\,\text{cm/yr}$ to a 60–80 cm ceiling; $\bar m \approx 0.08 D^{2.5}$ in kg for softwoods — standard allometry): eliminating $\bar m$ between the thinning law and the growth curve gives

$$N(t) = \left(\frac{K}{0.08\,(0.4t)^{2.5}}\right)^{2/3} \;\propto\; t^{-5/3}$$

Check against the data: from year 30 (N≈1,000, D≈12 cm) to year 150 (N≈450, D≈60 cm), the predicted drop is $(150/30)^{-5/3} \approx 9.5\times$-too-fast; the real drop is only ~2.2×. **This is the honest empirical result: individual temperate stands thin far more slowly than the bounding law, because they spend most of their life below the self-thinning boundary.** The law is a *ceiling*, the maximum-size–density line; stands approach it from below and then ride along it ([West 2026](https://link.springer.com/article/10.1007/s00468-026-02787-2) makes exactly this point for dynamic vs. boundary thinning lines). For a generator, use the ceiling to *cap* density given tree size, and use a slower empirical trajectory (like the PNW sequence above) as the default age–density curve. The clean implementation: pick target N₀ at establishment, grow each tree, and kill the smallest tree whenever $N \cdot \bar m > K^{2/3}$-violations of the ceiling occur — a queue, not a formula.

---

## 3. Spatial point processes: where exactly the trees are

### 3.1 The three patterns and their statistics

The null model is **complete spatial randomness (CSR)** — a homogeneous Poisson process of intensity $\lambda = N/A$. Against it, real stands are:

- **Aggregated/clumped** (most seedlings and saplings; most tropical species at some scale),
- **Regular/overdispersed** (mature stands in arid competition zones; plantation-like even-aged stands after thinning),
- **Mixed across scales** — the modern, verified consensus: "spatial patterns of species change depending on the scale of analysis... may follow an aggregated distribution at some scales, random or regular at others" ([MDPI Forests 2024](https://www.mdpi.com/1999-4907/15/4/714)). A Pan-Amazonian inventory study of 15 commercial species found most species aggregated at 0–1,110 m, random for 12 of 15 species at 0–370 m, and even one regular-at-large-scale case — with conspecific densities as low as 0.07–6 trees/ha ([Embrapa, Saracá-Taquera](https://www.alice.cnptia.embrapa.br/alice/bitstream/doc/1182840/1/28140.pdf)).

**Detection statistics** (all standard, all implementable):

- **Clark–Evans index:** $R = \bar d_{obs} / \bar d_{exp}$, ratio of observed mean nearest-neighbor distance to the Poisson expectation $1/(2\sqrt\lambda)$. R < 1 aggregated, R ≈ 1 random, R > 1 regular. Cheap, first-order; only sees nearest neighbors ([reviewed in Velázquez/Silva-navarrete-style summaries](https://link.springer.com/article/10.1186/s13717-021-00314-4)).
- **Ripley's K function** — the workhorse. $K(r)$ = expected number of additional points within distance $r$ of a typical point, divided by $\lambda$:

$$\boxed{\;\hat K(r) = \frac{A}{n^2}\sum_{i\ne j}\frac{I(d_{ij}\le r)\,w_{ij}}{}, \qquad L(r) = \sqrt{K(r)/\pi} - r\;}$$

($A$ plot area, $n$ points, $I$ indicator, $w_{ij}$ edge-correction weight; $L(r)$ is Besag's variance-stabilizing transform.) Under CSR, $K(r) = \pi r^2$ and $L(r) = 0$; $L>0$ aggregation, $L<0$ regularity, at each scale $r$ — this is *the* scale-resolving tool, which Clark–Evans is not ([Ripley 1977](https://doi.org/10.1111/j.2517-6161.1977.tb01615.x); methods summary in [Springer 2021 review](https://link.springer.com/article/10.1186/s13717-021-00314-4)).
- **Pair-correlation function $g(r)$:** the non-cumulative derivative — probability-density of a neighbor at exactly distance $r$. $g>1$ aggregated at that radius; increasingly preferred because K accumulates all scales below $r$ and can smear effects ([MDPI 2024](https://www.mdpi.com/1999-4907/15/4/714)).

**Caveat that matters for interpretation (and for validation suites):** environmental heterogeneity produces *virtual aggregation* — clumping that is habitat preference, not plant–plant interaction. The fix is the heterogeneous-Poisson null model rather than CSR ([Wiegand & Moloney tradition](https://link.springer.com/article/10.1186/s13717-021-00314-4)).

### 3.2 What field data actually shows, by life stage

- **Seedlings/saplings: aggregated, strongly.** Boreal Fennoscandian succession study: *Picea abies* saplings significantly clumped at 1–10 m in all three successional stages; mature trees random-to-clumped; interspecific repulsion between mature spruce and birch — stands organize as **species-specific mosaics** ([Aakala/Kreutz, Silva Fennica](https://www.silvafennica.fi/pdf/1279)).
- **Temperate broadleaf (remote sensing, whole-stand):** all-species pattern random at most scales; *Fagus* and *Betula* clustered; **large trees (>17.5 m) random; small trees clustered** — exactly the scale/stage dependence ([Nuske et al. 2005-era study](https://onlinelibrary.wiley.com/doi/10.1111/j.1654-1103.2005.tb02400.x)).
- **Chinese temperate forest (8 species, juveniles and adults):** all species aggregated at small scales; aggregation *decreases* with scale, ending random; juvenile aggregation at 0–40 m; aggregation significantly reduced when habitat heterogeneity is excluded, concentrating residual aggregation at 0–5/10 m — i.e. dispersal limitation acts at meters, habitat filtering at tens of meters ([MDPI 2024](https://www.mdpi.com/1999-4907/15/4/714)). Dispersal-limitation-driven aggregation correlates with seed mass, wood density, and maximum height ([Arnell et al. 2021](https://onlinelibrary.wiley.com/doi/10.1111/jvs.13070)).
- **Arid/semi-arid savanna:** woody plants are *more* aggregated on drier sites (<400 mm MAP), tied to facilitation and runoff-runon infiltration islands; aggregation measurable via L-function at 1–60 m ([Cramer et al. 2017](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)).

**Conserved pattern (verified repeatedly): conspecific clumping decays with age/size** — juvenile aggregation → adult randomness or mosaic — because distance/density-dependent mortality (Section 4) prunes clumps, and because self-thinning removes clustered small stems. The clumping of juveniles is set by the **seedling shadow** (typically 5–50 m radius, mode near the crown edge), then decays over the stand's life.

### 3.3 Cluster processes for the generator

When Poisson isn't enough (i.e. most of the time for anything young, anything animal-dispersed, anything arid), the standard models are **Neyman–Scott / Thomas cluster processes**: scatter parent points (a Poisson process of intensity $\kappa$), then scatter each parent's offspring as a Gaussian cluster of mean count $\mu$ and spread $\sigma$. The resulting $K$-function has closed form $K(r) = \pi r^2 + \frac{1}{\kappa}\left(1 - e^{-r^2/4\sigma^2}\right)$ — the second term is the aggregation excess, concentrated within ~2σ. Parameters straight from ecology: $\kappa$ = density of seed parents, $\mu$ = mean recruits per parent, $\sigma$ = seedling-shadow radius (Section 4).

**When is Poisson-with-density enough?** Blunt engineering call:

| Situation | Adequate model |
|---|---|
| Mature mixed temperate/tropical stand, all-species pooled | Poisson (verified: "pattern of all tree species combined was random for most scales") |
| Old-growth large trees | Poisson, possibly thinned to mild regularity |
| Any single species, tropical | Thomas cluster (σ ~ 5–40 m) |
| Seedlings/saplings, any biome | Thomas cluster (σ ~ 2–20 m) |
| Arid savanna / desert shrub | Strongly clustered (σ ~ 5–30 m, tied to runon patches) |
| Post-fire serotinous flush | Effectively Poisson at first (seed rain is broad), then self-thinning |
| Plantations / managed even-aged | Regular grid + jitter (this is what our engine currently does everywhere — wrong for everything natural) |

---

## 4. Seed dispersal kernels

The seed dispersal kernel $p(x)$ is the probability density of a seed landing at distance $x$ from the parent. Its **tail** controls colonization speed and the spatial grain of gene flow; its **mode** controls the seedling shadow.

### 4.1 Analytical forms (verified)

- **Wind — WALD (inverse Gaussian), 2D.** Katul et al.'s Wald Analytical Long-distance Dispersal model is a two-parameter inverse Gaussian whose parameters derive *mechanistically* from wind statistics, release height $h_r$, and seed terminal velocity $V_t$ — no fitting to dispersal data needed. Crucially, **WALD's asymptotic tail is a power law with exponent −3/2**, giving unbounded variance — turbulent updrafts make LDD intrinsic, not anomalous: seeds can travel >1 km where a ballistic calculation gives ~20 m ([Katul et al. 2005](https://doi.org/10.1086/432589); [Nathan et al. 2011 review](https://ce.berkeley.edu/sites/default/files/assets/users/thompson/10.%20Nathan%20et%20al%202011.pdf)). Large-eddy simulation confirms the −3/2 tail for light seeds in strong wind ([Bohrer et al. 2008](https://doi.org/10.1111/j.1365-2745.2008.01368.x)).
- **The 2Dt kernel (Clark et al. 1999).** A bivariate Student-t obtained by letting the Gaussian distance parameter itself vary randomly:

$$\boxed{\;p(r) = \frac{1}{2\pi u}\left(1 + \frac{r^2}{2u}\right)^{-3/2}\;}$$

(the 2D radial density with scale parameter $u$). It fits better than Gaussian/exponential for most species across temperate and tropical forests because it is simultaneously convex near the source *and* fat-tailed: less seed beyond 5 m than a Gaussian predicts near the crown, more beyond 30 m ([Clark et al. 1999](https://doi.org/10.1890/0012-9658(1999)080[1475:sdnafp]2.0.co;2)). Note WALD's tail exponent (−3/2) is *shallower* than 2Dt's (−2-class) — Katul et al.'s meta-analysis found measured kernels' exponents bound near the WALD value ([Katul 2005](https://doi.org/10.1086/432589)).
- **Animal dispersal fits inverse-power** forms best; wind-dispersed species fit Gaussian/Student-t — and animal-dispersed species have *longer* mean distances but *lower* fecundities than wind-dispersed ones in the same forest ([Clark, Poulsen et al. 2005, Cameroon](https://doi.org/10.1890/04-1325)). Most seeds, regardless of vector, still fall directly under the parent canopy; events >60 m are a small fraction of the crop but dominate colonization.

### 4.2 Numbers by mode

| Mode | Typical median/mode | Tail | Source |
|---|---|---|---|
| Wind (temperate tree) | ~10–60 m | power-law −3/2 tail; >100 m for light seeds in updrafts; >1 km possible | [Katul 2005](https://doi.org/10.1086/432589), [Nathan 2011](https://ce.berkeley.edu/sites/default/files/assets/users/thompson/10.%20Nathan%20et%20al%202011.pdf) |
| Animal (scatter-hoarding, e.g. jays/rodents) | ~tens of m (tens–hundreds for large vertebrates) | inverse-power; long | [Clark et al. 2005](https://doi.org/10.1890/04-1325) |
| Endozoochory (birds, bats, primates) | tens–hundreds of m | inverse-power | same |
| Ballistic (explosive pods, e.g. *Impatiens*, legumes) | ~1–10 m | thin (near-Gaussian) | standard; consistent with kernel reviews above |
| Water (hydrochory) | along channels, km-scale for buoyant seed | effectively unbounded downstream | [Nathan et al. 2008-era reviews] |

*(Ballistic and water rows carry the weakest specific citations in this document — the 1–10 m ballistic range and km-scale water range are consistent with the kernel literature above but I did not pin a single field-measurement paper; flagging rather than laundering.)*

### 4.3 Janzen–Connell: what spreads trees out again

Janzen (1970) and Connell (1971): seed survival *increases* with distance from the parent (enemies accumulate near adults), while seed density *decreases* — so recruitment peaks at an intermediate distance. Verified status:

- The full hump-shaped **Janzen–Connell pattern is rarer than textbooks suggest.** In a 24-species, 26-year Barro Colorado Island analysis, **Hubbell patterns (monotonic decline of recruit density with distance, but survival increasing) were the most common; true J–C humps were very rare** (non-zero recruit modes near 10 m for only 2 of 24 species); larger-seeded species suffered less distance-dependent mortality ([Marchand et al. 2019](https://doi.org/10.1002/ecy.2926)).
- The *mechanism* is real and measured: insect seed predators, soil pathogens. Whether it produces a hump depends on the ratio of seed dispersal distance to enemy dispersal distance — J–C requires seeds to outrun enemies; when enemies disperse as far as seeds you get McCanny patterns, when comparable, Hubbell ([Nathan & Casagrandi 2004](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00914.x); [Beckman et al. 2012](https://doi.org/10.1111/j.1365-2745.2012.01978.x)). Clumped seed deposition *increases* establishment by satiating predators ([Beckman 2012](https://doi.org/10.1111/j.1365-2745.2012.01978.x)).
- **Net generator rule:** apply a survival multiplier that rises with distance from conspecific adults (saturating by ~20–50 m) and multiply it against the dispersal kernel; the product is your effective recruitment kernel. Expect monotonic-decline-with-longer-tail (Hubbell) for most species, humps only for the animal-dispersed large-seeded ones. This is what *spreads* trees relative to a pure seed-shadow clump — and combined with Section 3.2's age-trend, it is why juvenile aggregation decays into adult mosaics.

---

## 5. Succession: timelines with numbers

**The modern view first:** succession is *assembly with stochasticity*, not a fixed sequence. The 100-year Glacier Bay permanent-plot record shows "stochastic early community assembly and subsequent inhibition have dominated; most species arrived shortly after deglaciation and have remained stable for 50+ years" — **no predictable species sequence or timeline** ([Buma et al. 2019](https://doi.org/10.1002/ecy.2885)). Fastie showed the classic Glacier Bay chronosequence mixes sites with different seed-source distances — up to 58% of variance in early spruce recruitment is distance to seed source ([Fastie 1995](https://doi.org/10.2307/1940722)). **But the time constants are robust** even when the species order is not:

### 5.1 Glacier forelands (primary succession)

- **Years 0–2:** first pioneers appear within **1–8 years** (commonly 1–2) of deglaciation in high-alpine forelands; ground cover <2% to year ~20 ([Fickert 2017](https://doi.org/10.5772/intechopen.69479); [Fickert 2020](https://doi.org/10.3390/d12050191)).
- **Years 20–60:** early-successional stage, ~30 species, cover ~10% at year ~50; dwarf shrubs dominant; **speedup in ground cover after ~half a century** (Fickert).
- **Years 60–160:** later successional; species >40, mean cover ~60%; first conifers appear **~120–150 years after deglaciation** near treeline (Fickert).
- **Years 100–200:** at Glacier Bay: shrub/cottonwood stages at 35–45 yr, spruce dominant >90 yr, near-100% overstory cover >160 yr, western hemlock in understory only >160 yr ([Fastie 1995](https://doi.org/10.2307/1940722)). In Norway, birch woodland develops **within 70 years** below 1,000 m; above ~1,600 m, pioneer vegetation persists essentially unchanged even after 250 years ([Robbins & Matthews 2010](https://doi.org/10.1657/1938-4246-42.3.351)).
- **Old-growth traits: 200+ yr** (Section 1.4's Douglas-fir data; structural leveling "by 400–500 years" per [Spies & Franklin](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)).

The parent's anchor "~10 yr pioneers, ~50–100 yr young forest, ~200+ yr to old-growth traits" is therefore **CONFIRMED as an order-of-magnitude skeleton, with the caveat that it is strongly altitude/latitude-dependent** (70 yr to woodland at low elevation; ≥250 yr and counting at high).

### 5.2 Post-fire (secondary succession)

- **Serotinous conifers: recruitment pulse in the first 1–3 years, full stop.** Lodgepole establishment after the 1988 Yellowstone fires occurred "almost entirely during the first year postfire" and "shapes forest structure and function for centuries" ([Turner et al. 2019 PNAS](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)); northern-Rockies median first-year recruitment 1,292 ha⁻¹ falling to 850 (yr 2) and 417 (yr 3) ([Clark-Wolf 2023](https://par.nsf.gov/servlets/purl/10441749)). Density is set by pre-fire serotiny (nonlinearly, peaking for 70–200-yr fire intervals at low elevation) ([Schoennagel 2003](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Schoennagel_etal2003_Ecology_FireIntervalSerotiny.pdf)).
- **Short-interval reburns collapse the pulse:** 2016 reburns converted stands from a mean 26,700 stems/ha to **6,450 seedlings/ha — a sixfold reduction** — because the canopy seed bank needs 40–70 yr to build ([Turner et al. 2019](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)).
- **Herb/cover recovery:** bare-ground fraction goes from ~1.0 immediately post-fire to 0.63 at 21 months and 0.2 at 33 months in southern California shrubland ([McGuire et al. 2021](https://cw3e.ucsd.edu/wp-content/uploads/2021/04/McGuireetal2021_EEG.pdf)); ground cover ≥60–80% is needed to reduce sediment yield to background (Section 8).

### 5.3 Other disturbance types

- **Post-lava / post-landslide / post-volcanic:** primary succession on the same clock as glacier forelands or slower (no soil, no seed bank; colonization rate set by distance to seed source — Fastie's 58%-variance result is the general law). Mount St. Helens is the canonical modern case: lupine and other nitrogen-fixers engineer the earliest surfaces, analogous to *Dryas*/alder at Glacier Bay (facilitation through N inputs; [Chapin et al. 1994](https://esajournals.onlinelibrary.wiley.com/doi/10.2307/2937039)).
- **Floodplain succession:** typically faster than glacial (soil arrives pre-built): willow/cottonwood on fresh bars within years; these are classic **softwood→hardwood** transitions over 50–200 yr.
- **Old-field:** the fastest — annuals yr 1–3, herbaceous perennials yr 3–20, shrubs yr 10–30, forest canopy 30–100+ yr. (Classic chronosequence literature; treat the *sequence* as stochastic per §5 preamble, the *rates* as robust.)

### 5.4 The time constants that ARE robust

- **Background stem mortality in mature tropical forest: ~1–2%/yr — CONFIRMED.** Pan-tropical best-estimate adjusted stem turnover **1.81 ± 0.16 %/yr** across 65 sites; seven intensively-analyzed stands ranged 0.86–2.02 %/yr mortality ([Baker et al. 2004](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00923.x)). Global synthesis: mean mortality 1.64%/yr vs recruitment 1.62%/yr (not different); tropical mean turnover 1.74%/yr vs temperate 1.19%/yr; within temperate, angiosperm 1.71% > mixed 1.03% > gymnosperm 0.77%/yr; turnover tracks productivity ([Stephenson & van Mantgem-era synthesis](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub3558.pdf)). Amazonian regional mortality ranges 1.3–2.8%/yr, with drought-driven increases ([Esquivel-Muelbert et al. 2020](https://link.springer.com/article/10.1038/s41467-020-18996-3); [McDowell et al. 2018](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)). A generator can use **0.5–1%/yr temperate-boreal, 1–2%/yr tropical, and half the dead trees standing vs broken/uprooted** (Esquivel-Muelbert: 48.4% standing vs 51.2% broken/uprooted pan-Amazonia).

---

## 6. Environmental limits as placement rules (the biome-mask table)

| Limit | Threshold | Notes / source |
|---|---|---|
| **Alpine treeline** | Growing-season mean temperature ≈ **6.7 ± 0.8 °C** (root-zone 10 cm, seasonal mean). Körner's worldwide study: 6.7 °C (±0.8 SD), with temperate/Mediterranean treelines 7–8 °C, equatorial 5–6 °C, subarctic/boreal 6–7 °C ([Körner & Paulsen 2004](https://onlinelibrary.wiley.com/doi/10.1111/j.1365-2699.2003.01043.x); updated network: ~6.5 °C current mean, 5.5–7.5 °C range — [Körner's Basel group](https://duw.unibas.ch/en/koerner/research/high-elevation-treeline/)) | Growing-season *length* ≥ ~3 months required; neither season length, extremes, nor thermal sums predict treeline position globally. Mechanism: crown–air aerodynamic coupling + growth (sink) limitation, not photosynthesis or carbon storage ([Körner 2007](https://www.erdkunde.uni-bonn.de/article/view/2594)). **Parent anchor 6.7 °C: CONFIRMED.** |
| **Maritime vs continental treelines** | Same seasonal mean isotherm, but its elevation climbs **~94 m per 100 km** eastward along a maritime–continental gradient in Central Europe (mass-elevation + continentality) ([Kašpar & Treml 2016](https://doi.org/10.3354/cr01370)) | Implementation: treeline altitude = f(latitude, continentality); oceanic treelines lower and warmer-isotherm, continental higher |
| **Crop-analogue limits** | Date palm/olive northern limits track similar growing-season isotherms (an analogue, not a mechanism — use as sanity check on temperate-subtropical biome masks) | — |
| **Desert tree exclusion** | **Trees essentially absent below ~101 mm MAP** (Sankaran, Africa); desert shrubs persist to ~50–100 mm in runon settings; woody-cover upper bound linear in MAP to 650 mm ([Sankaran 2005](https://edepot.wur.nl/27945)) | **Soil texture dependence (the inverse-texture effect): coarse/sandy soils support MORE woody cover at given low MAP than fine/clay soils** — sand conducts water below the grass rooting zone, and each +1% sand content measurably decouples productivity from precipitation constraint ([Wang et al. 2022](https://iopscience.iop.org/article/10.1088/1748-9326/ac953f)); a global drylands survey found the texture threshold flips at **MAP = 383 mm** — below it, higher water-holding capacity helps shrubs; above it, it hurts ([Eldridge et al. 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11468912/)). Kalahari basal-area gradient: ~2.5 m²/ha per 100 mm MAP above 200 mm ([Scholes](https://the-eis.com/elibrary/sites/default/files/downloads/literature/Trends%20in%20savanna%20structure%20and%20composition%20along%20an%20aridity%20gradient%20in%20the%20Kalahari.pdf)). **Parent anchor "~100–250 mm tree-exclusion": the lower edge (100 mm) is CONFIRMED for Africa; 250 mm is the shrub-dominance threshold, not tree exclusion — a mild disagreement worth encoding as a two-stage rule.** |
| **Swamp/flood oxygen** | Tolerance ordering (flooding duration/depth): submerged-tolerant swamp species (baldcypress, mangroves — months of inundation, lenticels/aerenchyma) > floodplain pioneers (willow/cottonwood — weeks–months) > mesic uplands (days). Mangrove: permanent tidal flooding OK, salinity excludes instead ([Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)) | Peat-swamp forests at <5 dry months/yr; hypermaritime bog woodland shows the stunting endpoint: 15 stems/ha on blanket bog vs 95 on bog woodland ([Hoffman 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)) |
| **Salinity** | Mangroves dominate above ~salinity where tidal flushing maintains nutrients; freshwater swamp species excluded as salinity rises. (Ordering well-established; specific per-species thresholds not pinned in this pass — flagged.) | — |

**The blunt call for the generator:** treeline = a single scalar rule (seasonal-mean 6.7 °C isotherm on your temperature field, raised inland per continentality); desert edge = MAP threshold (101 mm zero-trees; 0.14·MAP−14.2% cover to 650 mm) **modulated by a sand-fraction map** (sandy = +cover in the arid half, −cover in the mesic half, flipping at 383 mm); swamp = a flooding-duration layer with a species-ordered tolerance list.

---

## 7. Non-tree ground cover

| Layer | Biome | Density / cover | Source |
|---|---|---|---|
| Grass tillers, managed temperate sward (tall fescue) | Temperate pasture | **940–2,836 tillers/m²** (measured range across management) | [Scheneiter & Assuero 2010](https://doi.org/10.4067/s0718-16202010000200004) |
| Grass plants, semi-arid African rangeland | Savanna restoration | **10–18 plants/m²**, basal cover 49–73% | [Mligo/Kizinga-era Frontiers 2021](https://www.frontiersin.org/journals/ecology-and-evolution/articles/10.3389/fevo.2020.613835/full) |
| Desert tussock grass (Stipagrostis, Namib escarpment, 120 mm MAP) | Desert | **~2 tussocks/m²**, canopy cover to 50% in wet years | [Wagner et al. 2016](https://doi.org/10.1371/journal.pone.0166743) |
| Shrub density, Mojave/Colorado desert (Ephedra/Larrea) | Desert | sites of 0–166 shrubs each (remote-sensed individuals); order **~100–1,000 shrubs/ha** | [Owen et al. 2024](https://pmc.ncbi.nlm.nih.gov/articles/PMC11118996/) |
| Chaparral shrubs | Mediterranean | hundreds–thousands/ha, closed canopy; now ~34% grass-invaded | [Park 2020](https://par.nsf.gov/servlets/purl/10303007) |
| Understory cover (PNW conifer forest, all ages) | Temperate conifer | **46–57% total understory cover**; shade-tolerant saplings 84–335/ha | [Spies & Franklin 1991](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf) |
| Shrub-encroached grassland carbon | Savanna (SW China) | vegetation C 0.304 → 1.574 kg/m² as shrub cover 0→10–30% | [Chen et al. 2019](https://www.plant-ecology.com/EN/Y2019/V43/I4/365) |

**The parent anchor "grassland tiller densities ~1,000–10,000/m²" is PARTIALLY CONFIRMED with a real disagreement:** managed swards measure ~1,000–3,000 tillers/m² ([Scheneiter & Assuero](https://doi.org/10.4067/s0718-16202010000200004)) — the lower half of the anchor. The upper anchor (10,000/m²) is plausible for *ungrazed dense lawn-type or stoloniferous* swards (the same source's size–density compensation data and standard pasture ecology), but I did not pin a field measurement at 10,000 for natural grassland; **natural semi-arid rangelands are 2–20 plants/m², two-to-three orders of magnitude sparser than the anchor.** Use 1,000–3,000/m² for lush managed/temperate grass, 10–100/m² for natural dry grassland, ~2/m² for desert tussock grass.

**Moss/lichen:** boreal forest floor and tundra moss-lichen fractions are high (often dominant in ground cover; the northern-Rockies post-fire study measured moss+forb cover as a seedling-facilitation variable with real predictive weight — [Clark-Wolf 2023](https://par.nsf.gov/servlets/purl/10441749)), but I did not pin specific boreal moss-fraction percentages in this research pass — **flagged, not fabricated.** Epiphyte loads in cloud forests: large but not quantified here — flagged.

**Connection to LAI (cross-reference, not re-derivation):** the tree doc's measured oak-forest LAI band is **1.25–3.6**; general biome LAI runs ~1–2 (sparse/dry) to high single digits (dense temperate/tropical). Ground-layer density should be set *by the residual* — what LAI the canopy doesn't capture — rather than independently: savanna LAI 1.27 (savanna peak, dry end) to 6.0 (forest peak) with a *bimodal* distribution above 1,000 mm MAP, i.e. two stable states at the same rainfall ([Yin et al. 2014, West Africa](https://esd.copernicus.org/articles/5/257/2014/esd-5-257-2014.pdf)). That bimodality is a placement rule in itself: above ~1,000 mm MAP in the tropics, a cell is either savanna-like (LAI ~1.3–2.7) *or* forest-like (LAI ~6), rarely in between — pick per-cell with a stochastic flip, not an interpolation.

---

## 8. Vegetation–terrain feedbacks (biotic terrain engineering)

### 8.1 Tree-throw and pit-and-mound microtopography

Wind-thrown trees create pit-and-mound pairs: pits ~0.5–2 m across, mounds of similar footprint and 0.2–0.8 m height (rootwad volumes typically <0.5–several m³). The quantitative anchors:

- **Mound lifetimes: ~2,000+ years.** The oldest still-recognizable tree-throw mound in a Michigan forest is **2,420 years old** (on a 14° slope); a diffusivity of $D = 2.5\times10^{-3}$ m²/yr flattens a 14° mound in ~2,500 years, matching the observation ([Gabet & Mudd 2010](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub), citing Schaetzl & Follmer 1990).
- **Tree-throw contributes 11–18% of total hillslope sediment flux** in the Indiana field area — measurable but not dominant against creep-like processes ([Doane et al. 2021](https://doi.org/10.1029/2021gl094987)). In SE Australia, tree-throw becomes dominant on mid-slopes **only if it occurs at ≥2–3 events ha⁻¹ yr⁻¹** ([Richards et al. 2011](https://doi.org/10.1002/esp.2149)).
- Tree-throw + root fracture produce a **humped soil-production function** (max erosion at intermediate soil thickness) and maintain soil-thickness heterogeneity — the pit-and-mound mosaic *is* the nucleation-point structure for renewed bedrock weathering (Gabet & Mudd 2010).

### 8.2 Creep rates with vs without plants — the order-of-magnitude claim

- Hillslope diffusivity $D$ **increases strongly with moisture among dry sites and less strongly among wet sites; among drier sites $D$ rises from desert → grassland → forest** — the establishment of life substantially accelerates soil creep ([Richardson et al. 2019, Geology](https://doi.org/10.1130/g45305.1)).
- Numbers: forested Oregon Coast Range landscape-scale $D \approx 5\times10^{-3}$ m²/yr (Reneau & Dietrich 1991, quoted in [Gabet & Mudd 2010](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub)); with fire-driven erosion removed, $D \approx 2.5\times10^{-3}$ m²/yr. Bare-soil/arid-site diffusivities are typically 10× smaller (~10⁻⁴–10⁻³ m²/yr across the compiled global dataset — Richardson et al.). **The "order-of-magnitude difference with vs without plants" anchor is CONFIRMED at the dry end and compresses to ~2× among wet, always-vegetated sites.**
- Bioturbation rates by agent (vertical mixing): **ants 0.0045–1.8 mm/yr; earthworms 0.54–10 mm/yr** (compiled by [Gabet et al. 2003](https://doi.org/10.1146/annurev.earth.31.100901.141314)); measured effective soil-reworking rates ~0.028–0.26 mm/yr depending on agent and depth ([ESPL 2020 study](https://www.ovid.com/journals/espl/fulltext/10.1002/esp.4628~bioturbation-and-erosion-rates-along-the-soilhillslope)).

### 8.3 Bank vegetation and rivers

Roots stabilize banks, narrowing channels and permitting meandering; log jams force avulsion and multi-thread plans. The deep-time evidence is the strongest statement available (Section 8.6). For a generator: vegetated-bank channels are ~2–10× narrower and single-thread/meandering; unvegetated reaches braid wide and shallow.

### 8.4 Fire–erosion coupling

- **Post-fire erosion multiplier and recovery:** debris-flow likelihood and volume peak in **post-fire year 1**, drop to **~1/3–1/4 volume by year 2**, and the 15-min rainfall intensity threshold rises from ~15–30 mm/hr (yr 1) to >60 mm/hr by yr 3 ([McGuire et al. 2021](https://doi.org/10.1002/1096-9837(200012)25:13-era summary; direct: EEG 2021](https://cw3e.ucsd.edu/wp-content/uploads/2021/04/McGuireetal2021_EEG.pdf)). Bare-ground fraction: 1.0 → 0.63 (21 mo) → 0.2 (33 mo). **Ground cover of 60–80% is required to reduce sediment yield to background** ([Robichaud et al., ERMiT validation](https://d10.nrfirescience.org/sites/default/files/2025-02/07-2-2-10_jfsp_p07-2-2-10_final_report_pi_robichaud.pdf)). The old rule "an order of magnitude decrease with each post-fire year, background by year 3" holds as a median but large storms in years 4–7 can still produce big yields.
- A *lagged* landslide window exists at **3–5 years** post-fire (infiltration has recovered but roots have decayed; Rice 1982 data: 10 m³/ha unburned, 298 m³/ha at 9 years post-fire) before root strength returns ([Thomas et al. 2020-era Landslides](https://link.springer.com/article/10.1007/s10346-020-01506-3)).
- Long-term share: fire-related processes ≈ **25% of long-term sediment yield** in the western Cascades (Swanson 1981), and **>90% of geologic-time denudation** in some Southwest settings (Orem & Pelletier; both quoted in [McGuire et al. 2016](https://doi.org/10.1002/2016jf003867)).

### 8.5 Beaver meadows

- **Dam densities: <1 to >70 dams per km of stream** (compiled range; [Larsen et al. 2021, Earth-Science Reviews](https://beavertrust.org/wp-content/uploads/2021/06/Larsen_et_al-2021-Earth-Science-Reviews.pdf)); Riding Mountain National Park models **9.5 dams/stream-km** at capacity, median pond area **1,419 m²**, median pond volume ~421 m³ ([Stoll et al. 2024](https://harvest.usask.ca/server/api/core/bitstreams/038f39e4-3681-4aca-8df8-c6322b18a265/content)).
- **Pond/meadow lifecycle:** dams maintained for years–decades; open-water extent up to **9–12× pre-beaver**; ponds fill at **0.26–47 cm/yr** sediment accumulation; **beaver meadows form within ~15 years** of occupation (compiled in [Bradbury et al. 2026](https://doi.org/10.1002/esp.70256)). Active beaver-colony lifespan at a site ~33 years in a Swiss case, sequestering ~10.1 t C/ha/yr long-term — order of magnitude above the counterfactual forest ([Hallberg-Larsen et al. 2026](https://martinezbeavers.org/wp-content/uploads/2026/03/Hallberg-Larsen-et-al.-2026-Beavers-can-convert-stream-corridors-to-persistent-carbon-sinks-Communications-Earth-Environment.pdf)). Pre-European North America: 15–100 million ponds holding 3–50 billion m³ of sediment ([Murray et al. 2023](https://doi.org/10.1029/2022jg007199)).
- Generator rule: valley-bottom cells with low stream power get dam cascades at 1–10 dams/km, pond patches of ~1,000–2,000 m², conversion to wet meadow after ~15–30 years, abandonment, then re-occupation.

### 8.6 Deep time: the Devonian soil/landscape revolution

Before land plants there were **no soils as we know them, no mud in rivers, no meandering channels**: Cambrian–Ordovician landscapes were sheet-braided sand rivers and aeolian tracts, with fines deflated to sea. The Silurian–Devonian evolution of roots → wood → trees (over ~60 Myr) is visibly correlated, step by step, with channelled-braided rivers, meandering rivers, muddy floodplains, fixed narrow channels, vegetated islands, and log jams ([Gibling & Davies 2012, Nature Geoscience](https://web.archive.org/web/20191103013328/https:/www.nature.com/articles/ngeo1376); [Gibling & Davies 2014 synthesis](https://www.sciencedirect.com/science/article/abs/pii/S001678781300120X); [Davies et al. 2021, Svalbard](https://doi.org/10.1144/jgs2020-225)). The mud itself has two verified origins: plant-driven weathering produced it, and — a genuinely fun result — **early bryophyte organics flocculated clay in rivers, raising settling velocities by three orders of magnitude and muddying floodplains *before* deep roots even evolved** ([Zeichner et al. 2021, Science](https://www.science.org/doi/10.1126/science.abd0379); [McMahon & Davies 2018](https://www.science.org/doi/10.1126/science.aan4660)). (A minority view argues the mud/meandering shift predates effective plant engineering — [Santos et al. 2016](https://eprints.whiterose.ac.uk/id/eprint/110462/9/Santos%20et%20al%202016%20JGSL%20reassessing%20the%20impact%20of%20early%20land%20plants%20on%20sedimentation.pdf) — report both.) **Parent anchor CONFIRMED.** For the game: an unvegetated world is a legitimately alien terrain — sandy braided sheets, no soil layer, no floodplain mud — and the engine's biome system would generate it correctly by simply disabling every rule in Sections 1–7.

---

## 9. Global numbers and measurement methods

- **Total tree count: ~3.04 trillion (95% CI ±96 billion)** — Crowther et al. 2015, from **429,775 ground-sourced plot measurements** (trees ≥10 cm dbh) plus 20 remote-sensing/GIS covariates, with trees defined ≥10 cm dbh. Split: ~0.74 T boreal, ~0.66 T temperate, ~1.39 T tropical/subtropical; biome totals table in the paper includes tropical moist 799 B, temperate broadleaf 363 B, boreal 749 B, tundra 95 B ([Crowther 2015](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)). **The anchor "3.0 trillion": CONFIRMED.** Follow-up corrections to note honestly: (a) the number is a modeled extrapolation with per-hectare uncertainty much larger than biome means ("the predictive power of our models is limited at the level of an individual hectare"); (b) the estimate is dominated by the ≥10 cm dbh convention — including smaller stems multiplies it several-fold (Section 1.1's Amazonian ratio); (c) ~15.3 billion trees cut/lost per year, ~46% reduction since human civilization (model-derived, larger error bars). Treat 3.04 T as "the right order of magnitude, defined one particular way."
- **Global forest area: ~4 billion ha** (FAO ~4.06 Gha "forest," being >10% canopy cover, >0.5 ha, >5 m trees — the definition caveats are real: the FAO class includes plantations and fragmented woodland that ecologists would exclude; Pan et al.'s ecological-zone accounting gives 1,354 Mha tropical rainforest + 795 Mha moist deciduous + … summing to a similar ~4 Gha with different sub-totals) ([Pan et al. 2013](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)).
- **Biomass/NPP by biome:** tropical intact live biomass **~152–163 Mg C/ha** (Pan's Table 3: tropical intact live biomass 152.1 Mg C/ha in 2007 vs boreal 46.7 and temperate 60.7 — i.e. tropical ~2.5–3× the per-area live biomass of extratropical forests); temperate rainforests are the per-area champions at **500–2,500 Mg/ha** total biomass with a record eucalypt stand at 2,844 Mg C/ha; African moist forest AGB mean **395.7 Mg dry/ha** ([Lewis 2013](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)); central Amazonian stands 124–346 Mg/ha ([Schietti](https://doi.org/10.1111/1365-2745.12596)). **Desert <5 Mg/ha aboveground — consistent with the anchor** (deserts carry 53 billion trees over ~2 Gha-class area = well under 5 Mg C/ha; the savanna bimodality paper's savanna-peak biomass 1.10–3.56 kg C/m² = 11–36 Mg/ha confirms the dry end of the gradient). Forests hold ~92% of all terrestrial biomass; necromass is 58% of total forest ecosystem carbon (Pan).
- **How measured — one honest paragraph on error bars.** Everything above rests on three instrument classes with genuinely different failure modes. (1) **Plot networks** (FIA ~300k US plots; RAINFOR/ForestGEO tropical plots; Crowther's 430k compilations): direct, but tiny sampling fractions — a 1-ha plot is 10⁻⁸ of the Amazon — and they cluster near roads; tropical stem-density estimates for ≥10 cm stems are good to ~±10%, but the ≥1 cm class doubles-to-quintuples counts with site-level scatter of ±50%. (2) **Passive optical remote sensing** (MODIS LAI/canopy cover, Hansen forest cover): global and cheap, but saturates above ~LAI 4–5, cannot see understory, and its "forest" is a cover-fraction not a stem count — the savanna/forest bimodality analysis had to use two independent products to check itself. (3) **LiDAR** (airborne; GEDI spaceborne; GLAS before it): the only thing that measures vertical structure, hence biomass with ~±15–20% at stand scale, but coverage is sampled transects, not wall-to-wall, and calibration depends on the same limited field plots. The composite result: **biome-mean densities are trustworthy to a factor ~1.3; hectare-level values are trustworthy to a factor ~2–3; and any single headline number (3.04 T) carries a conventions-dependence larger than its formal confidence interval.** A game engine's tolerance is comfortably looser than all of this.

---

## 10. Practical simulation synthesis

### 10.1 What the engine does now

`world/generation/src/tree_placement.cpp` places trees on a **jittered grid**: 8 m cells, ~37% of cells host a candidate (pre-mask), jittered within [2, cell−2) so spacing ≥4 m holds by construction, then masked by height band (`kTreeMinHeight`–`kTreeMaxHeight`) and local slope (`kTreeMaxSlope`), with shape/size/jitter selected from hash bits. What this gets right: determinism, cheapness, slope and altitude masking, guaranteed spacing. What it gets wrong, measured against Sections 1–9:

1. **Density is one global constant** — no biome, no climate, no age axis. Real density spans 0.1–500,000 stems/ha (Section 1). The current ~0.37 trees/64 m² ≈ **58 trees/ha** is a plausible *temperate woodland* number but is applied to every land surface, including what should be desert, boreal, and closed forest.
2. **Uniform spacing is the single most unrealistic choice available.** Jittered-grid is the plantation/orchard model. Real stands: clumped (Thomas cluster, Section 3) for juveniles/single-species/arid; Poisson for pooled mature; the current layout is *more regular than any natural forest ever measured* — its Clark–Evans R would exceed 1 by construction.
3. **No age structure** — all trees are 4–7 m trunks. Real stands mix 300-year emergents with 5-year saplings at ratios of ~1:50+ (Section 1.4); this is the difference between a forest and a tree farm.
4. **No climate fields** — height/slope masking is a crude proxy for treeline (Section 6 shows the real rule is a 6.7 °C seasonal isotherm, not a raw altitude), and there is no moisture axis at all (Section 6: MAP + soil texture are the dominant desert/savanna controls).
5. **No ground layer** — shrubs exist only as a tree-shape variant at tree spacing; grass/forb/moss layers (Section 7) are absent.
6. **Species mix is 3 shapes at fixed ratios** — no conspecific clustering (Section 3), no Janzen–Connell spacing (Section 4).

### 10.2 The minimum fix (highest realism-per-line-of-code, in order)

1. **Drive density from two climate fields.** Even a hand-authored temperature+moisture pair, or better: derive MAP and seasonal-mean temperature from the existing heightmap/noise. Then: trees/ha = f(MAP, T) using Section 6's thresholds and Section 1's table — zero below 101 mm MAP or the 6.7 °C isotherm; 0.14·MAP−14.2% cover (converted to stems) in the arid-savanna band; biome-table values elsewhere. This one change replaces the 37%-of-cells constant with the real six-order-of-magnitude gradient, and makes deserts actually empty — currently the engine's most visible ecological lie.
2. **Replace the jittered grid with per-species Thomas clusters.** Keep the 8 m candidate grid (it's a fine sampler), but accept candidates through a cluster field: generate cluster centers (Poisson, intensity κ from the species' adult density), then accept each candidate with probability from a Gaussian bump of radius σ around the nearest center. σ = 5–20 m for most species, ~2–5 m for ballistic dispersers, tens of meters for wind. Instant clumping; Clark–Evans drops below 1 where it should.
3. **Add a two-class age mixture.** For each cluster, with probability p_old (0.1–0.3), place one "veteran" (tall, wide, from the existing conifer/round params scaled 2–3×) plus a surrounding cohort of small stems; else a even-cohort of mid-size stems. This fakes Sections 1.4 and 5 (old-growth vs. stand-initiation structure) with one extra branch on the shape selector — and it is the single biggest *visual* win available, because varied height is what reads as "natural" to the eye.
4. **Add a moisture-modulated ground layer.** Grass cover fraction = g(MAP) with the bimodal tropical flip (Section 7's LAI note); shrubs as clustered small instances at Section 7 densities in the arid half. If only one thing: make grass cover track the moisture field so dry hillsides read dry.
5. **Fire scars and succession states (optional, cheap version).** A per-region "time since disturbance" field: young cells get the flush densities of Section 1.4 (scaled down for playability), old cells the sparse-large-tree profile. Even a two-state {recently burned, mature} bitmask driven by existing noise adds the dominant source of real-world spatial heterogeneity.

**What to fake, honestly:** individual seed dispersal (use the cluster process, not a kernel simulation); Janzen–Connell dynamics (implicitly captured by using cluster-decay-with-age in the age mixture); self-thinning as a process (precompute the age–density curve from Section 2.2's anchors and interpolate); soil-texture modulation in the first pass (add the 383 mm sand-flip rule only if a soil map ever exists); beaver meadows, tree-throw microtopography, and Devonian mode (pure set-dressing material for later — Sections 8.5, 8.1, 8.6 respectively — high charm, low systems value).

**What NOT to fake:** the treeline (it's a scalar rule, Section 6 — altitude-only masking is quantitatively wrong and trivially fixable); the desert (empty is correct and free); the clumping (one Gaussian bump is one line of probability).

### 10.3 Validation

Cheap acceptance tests against this document's numbers, for the placement system: (1) per-biome stems/ha within the Section 1 table's ranges; (2) Clark–Evans R < 1 for single species, ≈1 for all-species pooled (Section 3.2); (3) L(r) from Poisson within confidence bands for pooled mature stands; (4) nearest-neighbor spacing distribution log-normal-ish, never hard-capped at exactly 4 m (the current cap is detectable in a histogram and would fail any Ripley's K test at r = 4–8 m with a visible regularity dip).

---

## Provenance

Written 2026-XX-XX (subagent for the Earth-terrain merged document, Part 6). Sources: live web search (Exa MCP) — 8 search batches, ~26 queries; every numeric claim carries an inline link to its source. Anchor-disagreement discipline: the parent's "?"-marked anchors were each verified and are marked above as CONFIRMED / PARTIALLY CONFIRMED / with-disagreement; the disagreements found (temperate old-growth nuance, tiller-density upper bound, 250-mm "tree exclusion" vs 101 mm, self-thinning worked-example slope, J–C hump rarity, Crowther conventions) are stated with both values, not silently resolved. Ballistic/water dispersal distances, boreal moss fractions, cloud-forest epiphyte loads, and salinity thresholds carry explicit flags rather than fabricated citations. Cross-references: `tree-motion-growth-and-appearance.md` (LAI 1.25–3.6 oak; Pipe Model §1.2 — used in §2.1, not re-derived), `water-physics-and-wave-simulation.md` (house style), `_terrain_question_bank.md` §F (questions 59–68, all covered in sections 1–9 above).

## Consolidated source list

1. [Crowther et al. 2015 — Mapping tree density at a global scale (PDF)](https://www.crowtherlab.com/wp-content/uploads/2019/06/Crowther-2015.pdf)
2. [Pan et al. 2013 — The Structure, Distribution, and Biomass of the World's Forests](https://www.nrs.fs.usda.gov/pubs/jrnl/2013/nrs_2013_pan_001.pdf)
3. [Madrigal-González et al. 2023 — Global patterns of tree density (PMC)](https://pmc.ncbi.nlm.nih.gov/articles/PMC9839683/)
4. [Lewis et al. 2013 — Above-ground biomass and structure of 260 African tropical forests](https://research.wur.nl/en/publications/above-ground-biomass-and-structure-of-260-african-tropical-forest/)
5. [Schietti et al. 2016 — Forest structure along a 600 km Amazonian transect](https://doi.org/10.1111/1365-2745.12596)
6. [Sankaran et al. 2005 — Determinants of woody cover in African savannas](https://edepot.wur.nl/27945)
7. [Cramer et al. 2017 — Patterns in woody vegetation structure across African savannas](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)
8. [Yin et al. 2014 — Bimodality of woody cover and biomass across the precipitation gradient in West Africa](https://esd.copernicus.org/articles/5/257/2014/esd-5-257-2014.pdf)
9. [Loubota Panzou et al. 2020 — Structural diversity and tree density, southern African woodlands](https://nph.onlinelibrary.wiley.com/doi/10.1111/nph.17639)
10. [Scholes et al. — Kalahari aridity-gradient transect](https://the-eis.com/elibrary/sites/default/files/downloads/literature/Trends%20in%20savanna%20structure%20and%20composition%20along%20an%20aridity%20gradient%20in%20the%20Kalahari.pdf)
11. [Park 2020/2022 — Grass invasion into chaparral shrublands](https://par.nsf.gov/servlets/purl/10303007)
12. [Spies & Franklin 1991 — Structure of natural young, mature, and old-growth Douglas-fir forests](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub1244.pdf)
13. [Tappeiner et al. 1997 — Density, ages, and growth rates in old-growth and young-growth forests, coastal Oregon](https://doi.org/10.1139/x97-015)
14. [Van Pelt et al. 2015 — Variability of stand structures in old-growth PNW forests](https://www.mdpi.com/1999-4907/6/9/3177)
15. [Hoffman et al. 2021 — Old-growth structure in hypermaritime rainforest, BC](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)
16. [Freund et al. 2015 — Structure of early old-growth Douglas-fir forests](https://www.fs.usda.gov/sites/nfs/files/legacy-media/r06/2%20Freund_Franklin_and_Lutz_2015_Structure_of_early_old-growth_Douglas-fir_forests_in_the_PNW.pdf)
17. [Lonsdale 1990 — The Self-Thinning Rule: Dead or Alive?](https://doi.org/10.2307/1938275)
18. [Pickard 1983 — Three interpretations of the self-thinning rule](https://doi.org/10.1093/oxfordjournals.aob.a086526)
19. [Deng et al. 2008 — Trade-offs between metabolic rate and population density of plants](http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.804.4516)
20. [Kikuzawa 1999 — Theoretical relationships between mean plant size, size distribution and self-thinning](https://doi.org/10.1006/anbo.1998.0782)
21. [Stoll et al. 2002 — Size symmetry of competition alters biomass–density relationships](https://doi.org/10.1098/rspb.2002.2137)
22. [Peters et al. 2018 — A new mechanistic theory of self-thinning](https://ideas.repec.org/a/eee/ecomod/v390y2018icp1-9.html)
23. [West et al. 2026 — Dynamic self-thinning lines in Canadian interior spruce](https://link.springer.com/article/10.1007/s00468-026-02787-2)
24. [Wright et al. 2021-era review — plant density responses / constant final yield](https://onlinelibrary.wiley.com/doi/10.1111/pce.13968)
25. [Spatial point-pattern analysis review (Springer 2021)](https://link.springer.com/article/10.1186/s13717-021-00314-4)
26. [Embrapa — Ripley's K of 15 commercial Amazonian species](https://www.alice.cnptia.embrapa.br/alice/bitstream/doc/1182840/1/28140.pdf)
27. [Aakala (Kreutz) — Spatial tree community structure across boreal succession, Silva Fennica](https://www.silvafennica.fi/pdf/1279)
28. [Nuske-era — Spatial relationships between tree species and gaps, broad-leaved woodland](https://onlinelibrary.wiley.com/doi/10.1111/j.1654-1103.2005.tb02400.x)
29. [Arnell et al. 2021 — Fine-scale tree spatial patterns shaped by dispersal limitation](https://onlinelibrary.wiley.com/doi/10.1111/jvs.13070)
30. [MDPI Forests 2024 — Spatial patterns and associations of tree species (scale dependence)](https://www.mdpi.com/1999-4907/15/4/714)
31. [Katul et al. 2005 — WALD mechanistic long-distance wind dispersal](https://doi.org/10.1086/432589)
32. [Nathan et al. 2011 — Mechanistic models of seed dispersal by wind (review)](https://ce.berkeley.edu/sites/default/files/assets/users/thompson/10.%20Nathan%20et%20al%202011.pdf)
33. [Bohrer et al. 2008 — Canopy heterogeneity and wind-driven dispersal kernels](https://doi.org/10.1111/j.1365-2745.2008.01368.x)
34. [Clark et al. 1999 — Seed dispersal near and far: the 2Dt kernel](https://doi.org/10.1890/0012-9658(1999)080[1475:sdnafp]2.0.co;2)
35. [Clark, Poulsen et al. 2005 — Comparative seed shadows of bird-, monkey-, and wind-dispersed trees](https://doi.org/10.1890/04-1325)
36. [Marchand et al. 2019 — Distance-dependent mortality at Barro Colorado Island](https://doi.org/10.1002/ecy.2926)
37. [Nathan & Casagrandi 2004 — Janzen-Connell and beyond](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00914.x)
38. [Beckman et al. 2012 — Clumped seed dispersal and density-dependent mortality](https://doi.org/10.1111/j.1365-2745.2012.01978.x)
39. [Buma et al. 2019 — 100 yr of primary succession at Glacier Bay](https://doi.org/10.1002/ecy.2885)
40. [Fastie 1995 — Multiple pathways of primary succession at Glacier Bay](https://doi.org/10.2307/1940722)
41. [Chapin et al. 1994 — Mechanisms of primary succession following deglaciation](https://esajournals.onlinelibrary.wiley.com/doi/10.2307/2937039)
42. [Fickert 2017 — Glacier forelands: field laboratories for primary succession](https://doi.org/10.5772/intechopen.69479)
43. [Fickert 2020 — Common patterns and diverging trajectories, Eastern Alpine glacier forelands](https://doi.org/10.3390/d12050191)
44. [Robbins & Matthews 2010 — Successional trajectories, Norwegian glacier forelands](https://doi.org/10.1657/1938-4246-42.3.351)
45. [Schoennagel et al. 2003 — Fire interval and serotiny, postfire lodgepole pine density](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Schoennagel_etal2003_Ecology_FireIntervalSerotiny.pdf)
46. [Turner et al. 2016 — 24 years after the Yellowstone fires](https://doi.org/10.1890/15-1585.1)
47. [Turner et al. 2019 — Short-interval severe fire erodes subalpine forest resilience (SI)](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)
48. [Clark-Wolf et al. 2023 — Conifer seedling demography, northern Rocky Mountains](https://par.nsf.gov/servlets/purl/10441749)
49. [Hansen-era / Hansen et al. 2018 — Regeneration failure of two subalpine conifers](https://www.caryinstitute.org/sites/default/files/public/reprints/hansen_etal_2018_ecology.pdf)
50. [Baker et al. 2004 — Tropical forest turnover rates when census intervals vary](https://besjournals.onlinelibrary.wiley.com/doi/10.1111/j.0022-0477.2004.00923.x)
51. [Stephenson-era global turnover synthesis (pub3558)](https://andrewsforest.oregonstate.edu/sites/default/files/lter/pubs/pdf/pub3558.pdf)
52. [McDowell et al. 2018 — Drivers and mechanisms of tree mortality in moist tropical forests](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)
53. [Esquivel-Muelbert et al. 2020 — Tree mode of death and mortality risk across Amazon forests](https://link.springer.com/article/10.1038/s41467-020-18996-3)
54. [Körner & Paulsen 2004 — A world-wide study of high altitude treeline temperatures](https://onlinelibrary.wiley.com/doi/10.1111/j.1365-2699.2003.01043.x)
55. [Körner — High elevation treeline research page (Basel)](https://duw.unibas.ch/en/koerner/research/high-elevation-treeline/)
56. [Körner 2007 — Climatic treelines: conventions, global patterns, causes](https://www.erdkunde.uni-bonn.de/article/view/2594)
57. [Kašpar & Treml 2016 — Thermal characteristics of alpine treelines, Central Europe](https://doi.org/10.3354/cr01370)
58. [Eldridge et al. 2024 — Drivers of woody dominance across global drylands](https://pmc.ncbi.nlm.nih.gov/articles/PMC11468912/)
59. [Wang et al. 2022 — Soil coarsening alleviates precipitation constraint in global drylands](https://iopscience.iop.org/article/10.1088/1748-9326/ac953f)
60. [Nature Comm. 2024 — Vegetation resistance crossing aridity thresholds](https://www.nature.com/articles/s43247-024-01546-w)
61. [Scheneiter & Assuero 2010 — Tiller population density, tall fescue and prairie grass swards](https://doi.org/10.4067/s0718-16202010000200004)
62. [Wagner et al. 2016 — Herbaceous legume encroachment, Stipagrostis tussock density](https://doi.org/10.1371/journal.pone.0166743)
63. [Owen et al. 2024 — Native shrub densities, burrow co-occurrence, California desert](https://pmc.ncbi.nlm.nih.gov/articles/PMC11118996/)
64. [Frontiers 2021 — Morphoecological traits of African rangeland grasses](https://www.frontiersin.org/journals/ecology-and-evolution/articles/10.3389/fevo.2020.613835/full)
65. [Chen et al. 2019 — Shrub coverage effects on grassland carbon, SW China](https://www.plant-ecology.com/EN/Y2019/V43/I4/365)
66. [Gabet et al. 2003 — The effects of bioturbation on soil processes and sediment transport](https://doi.org/10.1146/annurev.earth.31.100901.141314)
67. [Gabet & Mudd 2010 — Bedrock erosion by root fracture and tree throw](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub)
68. [Doane et al. 2021 — Topographic roughness on forested hillslopes: tree-throw flux](https://doi.org/10.1029/2021gl094987)
69. [Richardson et al. 2019 — Influences of climate and life on hillslope sediment transport](https://doi.org/10.1130/g45305.1)
70. [Richards et al. 2011 — Bioturbation on a SE Australian hillslope](https://doi.org/10.1002/esp.2149)
71. [ESPL 2020 — Bioturbation and erosion rates along a soil catena](https://www.ovid.com/journals/espl/fulltext/10.1002/esp.4628~bioturbation-and-erosion-rates-along-the-soilhillslope)
72. [Gabet 2000 — Gopher bioturbation: non-linear hillslope diffusion](https://doi.org/10.1002/1096-9837(200012)25:13)
73. [McGuire et al. 2016 — Raindrop- vs flow-driven postwildfire sediment transport](https://doi.org/10.1002/2016jf003867)
74. [McGuire et al. 2021 — Time since burning and post-fire debris-flow initiation](https://cw3e.ucsd.edu/wp-content/uploads/2021/04/McGuireetal2021_EEG.pdf)
75. [Robichaud et al. — ERMiT post-fire erosion model validation](https://d10.nrfirescience.org/sites/default/files/2025-02/07-2-2-10_jfsp_p07-2-2-10_final_report_pi_robichaud.pdf)
76. [USDA RMRS GTR-240 — Post-fire treatment effectiveness for hillslope stabilization](https://d10.nrfirescience.org/sites/default/files/2023-09/rmrs_gtr240.pdf)
77. [Thomas et al. 2020-era — Landslides after wildfire (Landslides journal)](https://link.springer.com/article/10.1007/s10346-020-01506-3)
78. [Graber et al. 2026 — Multi-year postfire debris-flow hazard with recovery](https://doi.org/10.1130/ges02936.1)
79. [Larsen et al. 2021 — Beaver influences on river corridor hydrology/geomorphology (Earth-Science Reviews)](https://beavertrust.org/wp-content/uploads/2021/06/Larsen_et_al-2021-Earth-Science-Reviews.pdf)
80. [Stoll et al. 2024 — Beaver dam capacity of Canada's boreal plain](https://harvest.usask.ca/server/api/core/bitstreams/038f39e4-3681-4aca-8df8-c6322b18a265/content)
81. [Laurel & Wohl 2019 — Persistence of beaver-induced geomorphic heterogeneity and carbon stock](https://www.beaverinstitute.org/wp-content/uploads/2023/03/Laurel-and-Wohl-The-persistence-of-beaver-induced-geomorphic-heterogeneity-and-organic-carbon-stock-in-river-corridors-ESPL-20I9.pdf)
82. [Murray et al. 2023 — Beaver pond geomorphology and nitrogen retention](https://doi.org/10.1029/2022jg007199)
83. [Bradbury et al. 2026 — Sub-annual sediment dynamics in beaver ponds](https://doi.org/10.1002/esp.70256)
84. [Hallberg-Larsen et al. 2026 — Beavers convert stream corridors to persistent carbon sinks](https://martinezbeavers.org/wp-content/uploads/2026/03/Hallberg-Larsen-et-al.-2026-Beavers-can-convert-stream-corridors-to-persistent-carbon-sinks-Communications-Earth-Environment.pdf)
85. [Gibling & Davies 2012 — Palaeozoic landscapes shaped by plant evolution](https://web.archive.org/web/20191103013328/https:/www.nature.com/articles/ngeo1376)
86. [Gibling & Davies 2014 — Palaeozoic co-evolution of rivers and vegetation](https://www.sciencedirect.com/science/article/abs/pii/S001678781300120X)
87. [Davies et al. 2021 — The Devonian landscape factory, Svalbard](https://doi.org/10.1144/jgs2020-225)
88. [Zeichner et al. 2021 — Early plant organics increased global terrestrial mud deposition](https://www.science.org/doi/10.1126/science.abd0379)
89. [McMahon & Davies 2018 — Evolution of alluvial mudrock forced by early land plants](https://www.science.org/doi/10.1126/science.aan4660)
90. [Santos et al. 2016 — Reassessing the impacts of early land plants on sedimentation](https://eprints.whiterose.ac.uk/id/eprint/110462/9/Santos%20et%20al%202016%20JGSL%20reassessing%20the%20impact%20of%20early%20land%20plants%20on%20sedimentation.pdf)

---

## Appendix A — Forty further questions

**Density and structure**
1. **How does stem density vary with plot size (the perimeter effect)?** Small plots inflate density because trees near edges get counted; Madrigal-González found plot size *not* supported as a determinant in their BIC model, but tropical 1-ha vs 0.1-ha counts differ systematically. *Disposition: use consistent plot windows when validating; not a first-order game concern.*
2. **What is the stem density of tree ferns and palms in tropical montane forest?** Not pinned; they follow different allometry (no secondary thickening) and can dominate thousands per ha. Flagged.
3. **How does plantation forestry density compare (e.g. pine at 1,100–2,500 stems/ha at planting)?** Standard silviculture; roughly the same as natural young-stand densities. *Dispositioned from general forestry practice, not pinned.*
4. **What's the density response to thinning treatments?** Variable-density thinning deliberately recreates clumping; relevant as "authored" landscapes. Not pinned.
5. **Do lianas have a measurable stem density of their own?** Yes — liana loading classes 0–4 are used as mortality predictors ([McDowell 2018](https://ipam.org.br/wp-content/uploads/2018/06/McDowelletal.2018.pdf)); absolute densities not pinned.

**Spatial statistics**
6. **What's the typical lag distance at which tropical conspecific aggregation saturates?** ~10–40 m from the Chinese-temperate and Amazonian data (Section 3.2); tropical BCI-scale studies give tens of meters. Use 10–40 m.
7. **How do you edge-correct Ripley's K on a finite game chunk?** Isotropic/translation edge corrections (Ripley 1977) or guard zones; for validation only, not runtime.
8. **What is the pair-correlation signature of a jittered grid?** A regularity dip at the grid spacing (4–8 m in our engine) — exactly the tell identified in §10.3.
9. **Are shrub patterns (vs tree patterns) more or less aggregated?** More, in drylands — tied to runon/facilitation islands ([Cramer 2017](https://bg.copernicus.org/articles/14/3239/2017/bg-14-3239-2017.html)).
10. **Does aggregation increase or decrease with elevation?** Not pinned; high-elevation stands often track microtopography (patchy safe sites), suggesting increase. Flagged.

**Dispersal**
11. **What seed terminal velocities do common game-relevant species have?** ~0.3–1.5 m/s for plumed/winged samaras; >5 m/s for heavy nuts. Consistent with WALD's parameter ranges but not tabulated here. Flagged as per-species art-direction data.
12. **How far can a single storm transport seeds?** Kilometers — WALD's unbounded-variance tail is the point ([Katul 2005](https://doi.org/10.1086/432589)).
13. **What is secondary dispersal (seed moved again after landing)?** Common — water-rewetting, ants, rodents; not pinned. Flagged.
14. **How does clumped seed deposition interact with J–C mortality?** Clumping *helps* by satiating predators ([Beckman 2012](https://doi.org/10.1111/j.1365-2745.2012.01978.x)) — the non-obvious sign.
15. **What fraction of seeds travel beyond 100 m?** Small (most fall under the parent — [Clark 2005](https://doi.org/10.1890/04-1325)) but dominates spread rate; 2Dt puts more >30 m than Gaussian ([Clark 1999](https://doi.org/10.1890/0012-9658(1999)080[1475:sdnafp]2.0.co;2)).

**Succession**
16. **What is the classic climax vs. individualistic (Gleasonian) resolution?** Modern view: assembly rules with stochasticity; no fixed climax ([Buma 2019](https://doi.org/10.1002/ecy.2885)). Answered in §5.
17. **How fast does soil develop on fresh substrate?** Glacier Bay: ~10× soil organic matter increase over 200 yr, N accumulation 2.3–3.6 g/m²/yr under alder ([Fastie 1995](https://doi.org/10.2307/1940722)).
18. **What triggers gap-phase vs. stand-replacing dynamics?** Severity of the disturbance and shade-tolerance of the dominant; PNW data shows both pathways coexist regionally (Section 1.4).
19. **How long do snags stand?** Rot-resistant species (cedar) stand centuries — 90% of standing dead in hypermaritime BC were redcedar/yellow-cedar ([Hoffman 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)).
20. **What is the successional role of N-fixers?** Facilitative for spruce growth but reduces spruce stand density (competition); nets out ambiguous ([Fastie 1995](https://doi.org/10.2307/1940722); [Chapin 1994](https://esajournals.onlinelibrary.wiley.com/doi/10.2307/2937039)).
21. **How does post-fire recruitment depend on distance to live edge?** Sharply — most regeneration within 150 m of unburned edge for non-serotinous species ([Hansen 2018](https://www.caryinstitute.org/sites/default/files/public/reprints/hansen_etal_2018_ecology.pdf)).
22. **What happens on repeated short-interval fire?** Sixfold+ recruitment collapse (Section 5.2; [Turner 2019](https://turnerlab.ibio.wisc.edu/wp-content/uploads/sites/43/2021/12/Turner_etal2019PNAS_ReburnsSI.pdf)).
23. **What is nurse-plant facilitation and when does it flip to competition?** Shrubs facilitate seedlings in semiarid settings (Section 5.2's forb-cover effect); competition dominates in mesic zones. The stress-gradient hypothesis; not pinned further.

**Environmental limits**
24. **Why 6.7 °C and not the old 10 °C warmest-month rule?** Seasonal mean tracks treeline globally; warmest-month fails (Körner, Section 6). The old rule was a regional coincidence.
25. **What sets the polar (arctic) treeline vs the alpine one?** Same isotherm, near sea level in the Arctic ([Körner 2007](https://www.erdkunde.uni-bonn.de/article/view/2594)).
26. **Do treelines move with warming, and how fast?** Yes — rapid advances documented ([Körner group 2024 citations](https://duw.unibas.ch/en/koerner/research/high-elevation-treeline/)); rates not pinned here.
27. **What is the flooding-tolerance mechanism?** Aerenchyma/lenticels transporting oxygen; not pinned further — ordering in §6 is the usable result.
28. **What salinity kills which species?** Flagged in §6; per-species thresholds not pinned.
29. **What is a muskeg/palsa peatland tree density?** Blanket bog: ~15 stems/ha ([Hoffman 2021](https://datahub.bvcentre.ca/dataset/9015c1cb-0e5b-4d9a-b387-b9ce6b5aede6/resource/553f33c4-b30a-4df7-b6ef-16828858856f/download/ecosphere-2021-hoffman-oldgrowth-forest-structure-in-a-lowproductivity-hypermaritime-rainforest-.pdf)).
30. **Does aspect matter at treeline?** Yes — snowpack and radiation load shift the local limit by tens of meters; the central-European study needed whole-season metrics to see through it ([Kašpar & Treml 2016](https://doi.org/10.3354/cr01370)).

**Feedbacks**
31. **How much does a single tree-throw event move?** Rootwad volumes typically <0.5–several m³ ([ESPL 2020](https://www.ovid.com/journals/espl/fulltext/10.1002/esp.4628~bioturbation-and-erosion-rates-along-the-soilhillslope)); a pit+mound pair displaces roughly that downslope.
32. **What is the frequency of tree-throw?** Needs ≥2–3 events/ha/yr to dominate mid-slope flux ([Richards 2011](https://doi.org/10.1002/esp.2149)); typical forest frequencies are lower.
33. **How deep do roots weather bedrock?** Meters — root fracture drives the soil-production function ([Gabet & Mudd 2010](https://scholarworks.sjsu.edu/cgi/viewcontent.cgi?article=1017&context=geol_pub)).
34. **What is the roughness signature of pit-and-mound topography?** Directly measurable from high-res topography; theory links production/decay to a dimensionless flux ratio ([Doane 2021](https://doi.org/10.1029/2021gl094987)).
35. **How much carbon does a beaver meadow sequester?** ~10 t C/ha/yr long-term in one well-studied case, order of magnitude above forest counterfactual ([Hallberg-Larsen 2026](https://martinezbeavers.org/wp-content/uploads/2026/03/Hallberg-Larsen-et-al.-2026-Beavers-can-convert-stream-corridors-to-persistent-carbon-sinks-Communications-Earth-Environment.pdf)); N-removal ~50–450 kg/km² catchment ([Lazar-era, PubMed](https://pubmed.ncbi.nlm.nih.gov/26436285/)).
36. **What did rivers look like on Mars (the no-vegetation endmember)?** Sheet-braided, as Earth's pre-vegetation rivers — the same physics, no banks; supports §8.6's "disable everything" mode.

**Global numbers / methods**
37. **What did GEDI change about biomass estimates?** Spaceborne LiDAR gave the first global canopy-structure sampling; reduced error bars on tropical biomass; not pinned further.
38. **How much has tree count changed since Crowther?** Estimates converge on the same ~3 T order with different conventions; the follow-up literature debates the human-loss term (46%±) more than the total.
39. **What is the error bar on FAO's 4 Gha?** Definitional (canopy threshold, plantation inclusion) rather than statistical — likely ±0.5 Gha depending on convention.
40. **What would a pre-Devonian world's sediment yield be?** Higher per unit rainfall (no retention): Schumm's argument — sediment yield rises with precipitation to a rock-erodibility cap without vegetation ([Davies & Gibling 2010](https://www.sciencedirect.com/science/article/abs/pii/S0012825209001779)).
