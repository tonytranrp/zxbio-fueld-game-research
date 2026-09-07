# The Elements and the Public Chemistry Data Landscape

**Scope:** the PUBLIC DATA side of chemistry for a simulator — what a complete element dataset actually contains and where each field authoritatively lives, where the elements themselves came from (nucleosynthesis), the isotope/radioactivity databases, the compound databases (PubChem and friends), the reaction databases, the honest computational answer to "what happens if we mix X with Y," the medical/safety layer, and — opinionated — the minimum offline dataset a voxel game engine should actually ship. Every scale figure is dated because these databases grow weekly. Every URL was verified live during research (see Provenance).

**How to read this:** Sections 1–5 are "what data exists and who owns it." Section 6 is the payoff — the actual answer chain for mixing things — and Section 8 is the engineering distillation. Read 6 and 8 last.

---

## 1. The elements: fields, sources, and data quality

### 1.1 What a complete element record contains

A "complete" element dataset for simulation is roughly 40–60 fields per element across 118 rows:

- **Identity:** atomic number $Z$, symbol, name (IUPAC official), group/period/block, CAS registry number, discovery year/discoverer.
- **Mass:** standard atomic weight. Critically, this is NOT a single number for all elements — the IUPAC/CIAAW standard atomic weight of 14 elements is an *interval* (e.g. chlorine [35.446, 35.957], argon [39.792, 39.963], lead [206.14, 207.94]) because natural isotopic variation exceeds measurement uncertainty ([CIAAW Standard Atomic Weights](https://www.ciaaw.org/atomic-weights.htm); [Prohaska et al. 2021](https://doi.org/10.1515/pac-2019-0603)).
- **Electronic structure:** electron configuration (ground state, with known exceptions like Cr, Cu, and the actinides), term symbol, common oxidation states (measured/observed, not just periodic-trend guesses), electronegativity (Pauling; Allen and Mulliken scales exist and disagree for transition metals).
- **Energetics:** ionization energies IE₁–IE₃ (and up to IE₈ for the ambitious — NIST ASD has them all measured), electron affinity, Pauling electronegativity.
- **Sizes:** covalent radius, metallic radius, ionic radii (Shannon radii, which are coordination-number dependent — a single "ionic radius" is a simplification), van der Waals radius.
- **Bulk physics:** melting point, boiling point, density (at STP; allotrope-dependent for C, P, S, Sn), crystal structure at STP, electrical/thermal conductivity, magnetic ordering.
- **Abundance:** crustal (ppm), seawater, solar system, and universe — the worst-quality fields in the whole record (see 1.3).
- **Nuclear:** isotope count, stable-isotope list, natural isotopic composition, most-stable-isotope half-life for the radioactive elements ([CIAAW Isotopic Abundances](https://www.ciaaw.org/isotopic-abundances.htm)).

### 1.2 Authoritative sources, field by field

| Field | Primary authority | Notes |
|---|---|---|
| Atomic weights, isotopic abundances | [CIAAW](https://www.ciaaw.org/atomic-weights.htm) / IUPAC | Biennially revised; 2024 revisions to Gd, Lu, Zr already folded into the live tables |
| Ionization energies, electron configurations, spectral lines | [NIST Atomic Spectra Database](https://www.nist.gov/pml/atomic-spectra-database) | Measured, evaluated; the ground truth |
| Thermophysical (mp, bp, density, conductivity) | [NIST Chemistry WebBook](https://webbook.nist.gov/chemistry/), [WebElements](https://www.webelements.com/) | WebElements is a curated university site (Univ. of Sheffield, Mark Winter) — single-maintainer, excellent for per-element browsing, license permits quoting with attribution ([WebElements copyright](https://winter.group.shef.ac.uk/webelements/nexus/copyright.html)) but it is not a bulk-download API |
| Everything at once, machine-readable | [Wikidata](https://www.wikidata.org/wiki/Wikidata:WikiProject_Chemistry/Elements) via [SPARQL](https://query.wikidata.org/) | `?element wdt:P31 wd:Q11344` gives all elements with P1086 (atomic number), P246 (symbol), mass, config, and more; CSV/JSON export built into the endpoint. CC0 — the only truly embeddable bulk element table |
| Radioactive properties | IAEA/NUBASE (Section 3) | |

The engineering call: **build the element table from Wikidata SPARQL (CC0, one query, ~118 rows), then override mass with CIAAW and energetics with NIST.** PubChem also has an Element data collection (element pages exist, e.g. gold, iron, with boiling-point annotations per element) — useful as a cross-check, not as a primary.

### 1.3 Data quality — the honest part

- **Abundance estimates vary by orders of magnitude, sometimes literally.** Upper-continental-crust abundances for As, B, Be, Bi, Cd, In, Sb, Te, Tl, W and others vary by >50% between major studies, and for some elements only one or two estimates exist at all ([Hu & Gao 2008, Chemical Geology](https://www.sciencedirect.com/science/article/abs/pii/S000925410800185X)). The canonical modern compilation remains Rudnick & Gao (2003, Treatise on Geochemistry); Hu & Gao revised the rare ones. "Universe" and "solar system" abundances come from two entirely different methods (spectroscopy of the Sun vs. meteorite analysis) that disagree in known, partially-corrected ways. For a game: treat crustal abundance as one significant digit plus order of magnitude, no more.
- **Rare ≠ rare.** Total REE (lanthanides + Y + Sc) in the crust is ~130–240 µg/g — comparable to Cu (~60 ppm) and Zn (~70 ppm); Ce (~63 µg/g) is more abundant than copper ([USGS REE geology](https://geology.com/usgs/ree-geology/); [REE review 2025](https://www.mdpi.com/2075-163X/15/7/720)). They're "rare" because they don't concentrate into ore, not because they're scarce. A simulator that makes monazite dirt-common is right; one that makes elemental cerium precious is wrong.
- **Measured vs modeled:** ionization energies, melting points of the stable elements, and crystal structures are *measured*. Beyond bismuth everything is radioactive and half-lives are measured but the superheavy elements (Z ≥ 104) have half-lives from seconds down to microseconds and "crystal structure"/"density" fields in popular tables are *predictions*, not measurements — a single atom does not have a measurable density. If your table shows a density for oganesson, it's a model (and the model's error bars are wide).
- **Standard atomic weights are intervals, not constants** — 14 elements officially, and "normal material" footnotes (g, m, r in the CIAAW table) mark elements where specific geological samples fall outside the stated range. A game can just use the conventional single values, but a data pipeline should not be surprised by `[a, b]` entries.

---

## 2. How the elements are born: nucleosynthesis

The reference map is Jennifer Johnson's "origin of the elements" periodic table — the color-coded table showing each element's production route(s), maintained and revised since 2008 ([Johnson's SDSS post with the table and its reasoning](https://blog.sdss.org/2017/01/09/origin-of-the-elements-in-the-solar-system/)), rendered publicly by [NASA SVS](https://svs.gsfc.nasa.gov/13873/), and formalized with per-element fractional contributions in [Johnson, "Populating the periodic table: Nucleosynthesis of the elements," Science 363, 478 (2019)](https://www.science.org/doi/10.1126/science.aau9540) (data in its table S1 — machine-readable). The companion critical review is [Johnson, Fields & Thompson, "The origin of the elements: a century of progress," Phil. Trans. R. Soc. A 378 (2020)](https://royalsocietypublishing.org/doi/10.1098/rsta.2019.0301), which usefully grades each claim by confidence.

### 2.1 The processes

- **Big Bang nucleosynthesis (BBN), first ~15 minutes:** H, He (≈25% of mass as He-4), trace D, He-3, Li-7. Nothing heavier — the expanding Universe cooled below fusion temperatures and free neutrons (half-life ~10 min) decayed. By mass, ~98% of the Galaxy's H/He remains unprocessed to this day ([Science 2019](https://www.science.org/doi/10.1126/science.aau9540)).
- **Stellar fusion, up to iron:** H burning (pp chain in low-mass stars, CNO cycle once C exists — H burning lasts ~10 Gyr in a 1 M☉ star, ~25 Myr in 10 M☉, ~6 Myr in 30 M☉), then He burning (C, O), then carbon, neon, oxygen, silicon burning in massive stars. Fusion is exothermic only up to Fe-56/Ni-62 — the binding-energy-per-nucleon curve peaks there. Everything past iron requires processes that *consume* energy or sidestep Coulomb barriers.
- **The s-process (slow neutron capture):** in AGB stars (dying low-mass stars), neutrons captured slowly relative to β-decay rates, building heavy elements along the valley of stability. Makes most of the Sr–Ba region and the bulk of lead and bismuth. Johnson's key correction to popular charts: dying low-mass stars own the bottom-right of the table (Pb especially), and essentially all iron in the universe is from *explosive* nucleosynthesis, not the quiet core of a red giant (which gets locked in the remnant).
- **The r-process (rapid neutron capture):** neutron fluxes so intense that captures outpace β-decay, piling up nuclei far on the neutron-rich side before they decay back. Makes roughly half of all A > 90 nuclei, including the actinides (U, Th) and the third-peak elements (Pt, Au). Site: see 2.2.
- **The p-process / ν-process (proton-rich rare isotopes):** a few dozen proton-rich isotopes (lighter-than-typical r/s products) made partly by photodisintegration (γ-process) in supernova shocks and partly by neutrino-induced reactions; ~15% of meteoritic B-11 comes from ν-spallation in core-collapse supernovae ([Lemoine et al. 1998](https://doi.org/10.1086/305650)).
- **Cosmic-ray spallation for Li–Be–B:** Li, Be, B are *destroyed* in stellar interiors and almost never made there. They're made when fast cosmic-ray protons/α-particles break CNO nuclei in the interstellar medium apart (and, importantly, when fast C and O nuclei from supernovae/Wolf–Rayet winds fragment on ambient H and He). C-6... specifically: ⁶Li, ⁹Be, ¹⁰B are *pure* spallogenic products; the modern picture splits solar-system LiBeB roughly half GCR spallation and half fast-C/O fragmentation, with ν-process supernovae supplying the ¹¹B excess ([Lemoine, Vangioni-Flam & Cassé 1998, ApJ](https://doi.org/10.1086/305650); [Reeves, Rev. Mod. Phys. 66, 193 (1994)](https://doi.org/10.1103/RevModPhys.66.193)). This is *why* the abundance curve has its famous Li–Be–B canyon between He and C.

### 2.2 Supernovae vs neutron-star mergers for the r-process — the current split

Pre-2017 the default answer was "core-collapse supernovae, probably." **GW170817 settled the direction of the field:** the 2017 neutron-star merger, detected in gravitational waves and followed up across every band, produced a kilonova whose spectra and light curve required ~0.03–0.05 M☉ of freshly synthesized r-process material, with lanthanide-rich ejecta identified spectroscopically ([Pian et al., Nature 551, 67 (2017)](https://arxiv.org/abs/1710.05858); [Chornock et al., ApJL 848, L19 (2017)](https://iopscience.iop.org/article/10.3847/2041-8213/aa905c); [Cowperthwaite et al., Science 358, 1560 (2017)](https://www.science.org/doi/10.1126/science.aaq0049)). The two-component fit (a fast "blue" ~0.025 M☉ lanthanide-poor ejecta at ~0.3c plus a slower "red" ~0.04 M☉ lanthanide-rich one at ~0.1c) is in [Kasen et al., Nature 551, 80 (2017)](https://arxiv.org/pdf/1710.05463), whose conclusion — "neutron-star mergers may be the dominant contributors to r-process production in the Galaxy" — is now the standard view, with the caveat that direct Au/Pt spectral lines have *not* been identified (only upper limits of a few 10⁻³ M☉; [Perego et al. 2021](https://arxiv.org/abs/2101.08271)).

The refined consensus ([Johnson et al. 2020](https://royalsocietypublishing.org/doi/10.1098/rsta.2019.0301)): neutron-star mergers are *a* dominant site of the heavy r-process (A ≳ 140 — lanthanides, actinides, Au, Pt), but "unconvinced that they produce all of the elements past nickel" — the *light* r-process (A ≲ 140: Sr, Y, Zr) may have a significant contribution from rare core-collapse variants (magnetorotational supernovae, collapsars), and the exact split around Zr–Nb–Ag is actively disputed. For a simulator: **lanthanides and actinides = mergers; light r-process = mergers plus some rare supernovae; the boundary is fuzzy by row of the table.**

### 2.3 What this means for a simulator

Two concrete game-facing consequences:

1. **Elemental geography.** H/He everywhere (BBN, ~98% unprocessed); C/O/N in gas from stellar winds and the CNO cycle; Fe-peak (Fe, Ni, Co, Cr, Mn) concentrated where massive stars exploded — old populations still have it, but diluted; heavy elements (Ba, Au, U) trace r-process events, which are *rare* (~10⁻⁵/yr/Galaxy for NS mergers) — so r-process enrichment is lumpy and statistically late-arriving in a galaxy's history. The metallicity clock below formalizes this.
2. **Metallicity as a time axis.** Astronomers call everything past helium a "metal," and a star's metallicity
   $$[\mathrm{Fe/H}] = \log_{10}\left(\frac{N_{Fe}}{N_H}\right)_{star} - \log_{10}\left(\frac{N_{Fe}}{N_H}\right)_\odot$$
   is effectively a *birth-date*: gas gets progressively enriched by each generation of stars, so old stars formed from low-metallicity gas and have less of *everything* beyond H/He. Population III (first, zero-metallicity) stars had no planets worth simulating — planets are made of "metals." An old star/galaxy in a game should be H/He-dominated with trace C/O; a young one, rocky-planet-rich. This single scalar is the cheapest way to make a universe feel like it has a history.

---

## 3. Isotopes and radioactivity data

### 3.1 The databases

- **NUBASE** is the authoritative evaluated library of ground- and isomeric-state nuclear properties. NUBASE2020 contains recommended masses, half-lives, spins, decay modes and branching intensities for **3,340 nuclides in the ground state plus 1,938 isomers (T½ ≥ 100 ns), with 218 more unobserved nuclides estimated from systematic trends** ([NUBASE2020, Chinese Physics C 45, 030001 (2021)](https://www-nds.iaea.org/amdc/ame2020/NUBASE2020.pdf)). It ships as a fixed-width text table (`nubase_4.mas20`) — parseable in an afternoon, and it's the source you want offline. NUBASE2024 exists (the AMDC publishes on a 3-yr cycle); 2020 is the version with a stable canonical URL.
- **IAEA LiveChart of Nuclides** ([livechart](https://www-nds.iaea.org/relnsd/vcharthtml/VChartHTML.html)) — the interactive Z×N chart with an API and direct data download, updated from ENSDF. Good for browsing and for getting decay radiation energies (the γ lines).
- **NuDat 3** (BNL, [nndc.bnl.gov/nudat3](https://www.nndc.bnl.gov/nudat3/)) — levels, decay radiation, drawings; the same ENSDF evaluation underneath.

### 3.2 What the data looks like

Half-lives span 39 orders of magnitude across the table — from tellurium-128 (2.2×10²⁴ yr, effectively stable) down to superheavy isotopes at microseconds. Decay modes: α, β⁻, β⁺/electron capture, isomeric transition, spontaneous fission, plus β-delayed particle emission near the drip lines. Each nuclide row carries the mode(s) *and branching percentages* — essential, because real decay chains fork.

Decay chains as data: the four classical series (²³⁸U → ²⁰⁶Pb, ²³⁵U → ²⁰⁷Pb, ²³²Th → ²⁰⁸Pb, ²³⁷Np → ²⁰⁵Tl† (extinct in nature)) are just transitive closures of the NUBASE decay table — no separate database needed; derive them once at build time and bake the chains (each is 10–20 steps). For a game wanting radon hazards or uranium ore that "does something" over deep time, the chain + half-life + branch-fraction data from NUBASE is complete and public-domain-ish (IAEA/BNL data is freely usable; check per-site terms, they're permissive).

Engineering call: **NUBASE flat file offline, ~5,300 rows, one-time parse, done.** It's the nuclear counterpart of the Wikidata element table.

---

## 4. Compound databases: the public landscape

**Scale (dated):** PubChem contained **118.6 million compounds, 322.4 million substances, 295 million bioactivity data points from 1.67 million assays, and 1,006 contributing data sources as of 12 September 2024** ([PubChem 2025 update, Nucleic Acids Research](https://doi.org/10.1093/nar/gkae1059)); live counts at the [PubChem statistics page](https://pubchem.ncbi.nlm.nih.gov/docs/statistics). Substances are depositor descriptions (often redundant, sometimes structureless like extracts); compounds are the deduplicated, standardized structures.

| Database | Contents | Access | Bulk/offline? |
|---|---|---|---|
| **[PubChem](https://pubchem.ncbi.nlm.nih.gov/)** | 119M compounds, structures, computed properties, synonyms, links to everything | [PUG REST](https://pubchem.ncbi.nlm.nih.gov/docs/pug-rest) (structured properties, ~1M requests/day, 5 req/s limit, 30 s timeout) + [PUG View](https://pubchem.ncbi.nlm.nih.gov/docs/pug-view) (textual annotations: safety, pharmacology, toxicity; single-record, paginated) | **Yes** — official FTP monthly dumps of the full compound set; the only truly bulk-downloadable giant |
| **[ChemSpider](https://www.chemspider.com/)** (Royal Society of Chemistry) | >120 million aggregated structures with properties | Web search; limited API | **No** — free to search, *not* open data; full download only under license ([Caveat Usor comparison](https://pmc.ncbi.nlm.nih.gov/articles/PMC5900829/)) |
| **[ChEBI](https://www.ebi.ac.uk/chebi/)** (EBI) | ~60k–120k *curated* molecular entities with a rigorous ontology (roles: "toxin," "fuel," "buffer"...) | [FTP downloads](https://www.ebi.ac.uk/chebi/downloads): OWL/OBO/JSON ontology, SDF structures, PostgreSQL dump | **Yes** — CC BY 4.0, monthly releases; the best *small, semantically rich* compound set |
| **[NIST Chemistry WebBook](https://webbook.nist.gov/chemistry/)** | Thermochemical data (ΔfH° gas/liquid/solid, entropy, heat capacity, phase transitions) for **>7,000 organic and small inorganic compounds**, plus ion energetics, IR/mass/UV spectra, Henry's law constants | Web + predictable URL structure (`/cgi/cbook.cgi?ID=C<InChIKey-ish>`) | **Gray** — per-record web access is unrestricted, but there is no official full dump; NIST SRD products carry license terms. Scrape politely or accept per-compound lookups |
| **[Wikidata](https://www.wikidata.org/wiki/Wikidata:WikiProject_Chemistry/Elements) chemistry** | Structures (SMILES/InChI), formulae, IDs, select properties cross-linked to ~everything | [SPARQL](https://query.wikidata.org/), full dumps | **Yes** — CC0 |
| **[CAS Common Chemistry](https://commonchemistry.cas.org/)** | ~500,000 common/regulated substances with validated CAS Registry Numbers | Free [API](https://commonchemistry.cas.org/api-overview) (token, paginated) | **No bulk** — per-query; the full CAS REGISTRY (290M+ substances) is strictly commercial ([cas.org](https://www.cas.org/cas-data/cas-registry)) |

Blunt calls: **PubChem is the backbone** — free, bulk, gigantic, with real REST APIs ([PUG REST](https://pubchem.ncbi.nlm.nih.gov/docs/pug-rest), [PUG-View paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC6688265/)). **ChEBI is the brain** — its ontology (is this compound a "poison"? a "nutrient"? a "refrigerant"?) is exactly the layer a game needs for gameplay roles, and it's small enough to embed whole. **NIST WebBook is the thermometer** — the only public evaluated source of formation enthalpies, which Section 6 needs. ChemSpider and CAS Common Chemistry are lookup conveniences, not engine assets.

---

## 5. Reaction databases

- **The USPTO dataset** is the public anchor: **~1.8 million organic reactions text-mined from US patents (1976–Sept 2016)** by Daniel Lowe, released on figshare ([Lowe, figshare](https://figshare.com/articles/dataset/Chemical_reactions_from_US_patents_1976-Sep2016_/5104873)) in reaction-SMILES form (`reactants>reagents>product`). It is the training set behind essentially every open reaction-prediction model.
- **The Open Reaction Database (ORD)** ([open-reaction-database.org](https://open-reaction-database.org/about); [Kearnes et al., JACS 143, 18820 (2021)](https://doi.org/10.1021/jacs.1c09820)) is the schema + repository built to fix the field's data mess: a protobuf-based schema capturing inputs, amounts, conditions, outcomes, even analytical data; the repository has grown past **2 million reactions**, dominated by the USPTO import (~1.7M) plus ~90–100k curated/high-throughput academic and industrial submissions. CC BY-SA 4.0 data, Apache-2.0 code, everything on GitHub. The catch: ORD data needs cleaning before ML use — the [ORDerly](https://pmc.ncbi.nlm.nih.gov/articles/PMC11094788/) project documents how aggressively (dropping duplicates and implausible records cuts the USPTO set from 1.77M to ~0.7–0.9M usable examples depending on task).
- **PubChem's reaction layer**: PubChem has been assembling reaction annotations for compounds (its Patent and Pathways collections) — useful linkage, not a reactions-as-data API on the USPTO scale.
- **The proprietary giants, named so you don't waste time:** **Reaxys** (Elsevier, successor to Beilstein) and **SciFinder** (CAS) hold millions of carefully abstracted reactions with conditions and yields. They are subscription-only, API-restricted, and explicitly **not usable** for any game dataset ([ORDerly](https://pmc.ncbi.nlm.nih.gov/articles/PMC11094788/) says it plainly: commercial sets "are not freely available to ML practitioners, stymieing advances").
- **Biochemistry:** **BRENDA** (enzymes; **CC BY 4.0**, downloadable textfile ~72 MB with 100k+ EC-classed turnover/condition data; [brenda-enzymes.org](https://www.brenda-enzymes.org/), [license](https://www.brenda-enzymes.org/license.php)) and **Rhea** (EBI's non-redundant biochemical reaction set, cross-referenced to ChEBI participants) are the open biochemistry stack. (The task brief's "BKL" is, as far as I can verify, not a distinct public database — the biochemical reaction space is covered by BRENDA, Rhea, and MetaCyc; flagging rather than inventing.)

**Formats:** Reaction SMILES (`C=C.CBr>>CCBr` — reactants `>` agents `>` products over the arrow) is the de-facto standard, atom-mappable with `[CH3:1]...` annotations. **RInChI** (Reaction InChI, IUPAC) is the hashed, canonical identifier built from component InChIs — layers for reactants/products/agents, plus direction flags and a RInChIKey suitable for deduplication and database indexing ([RInChI v1.00 release](https://iupac.org/rinchi-version-1-00-software-release/); [format spec (PDF)](https://www.inchi-trust.org/wp/download/RInChI/RInChI%20V1-00-0.pdf); [C++/Python source](https://github.com/IUPAC-InChI/RInChI)). For an engine: store reactions as RXN/reaction-SMILES at authoring time, key them by RInChIKey.

---

## 6. "What happens if we mix X and Y?" — the honest answer chain

This is the question the whole document builds to, and the honest answer has three very different layers depending on whether X and Y are elements or compounds.

### 6.1 Two elements: mostly a solved lookup

Element + element = phase behavior, and this is where public data is genuinely good:

- **Phase diagrams (experimental):** for ~2,700 binary systems, experimental phase diagrams exist in handbooks (Massalski's *Binary Alloy Phase Diagrams*, 3+ volumes) — but not in a free bulk database. The public replacement is:
- **DFT-computed formation enthalpies and convex hulls.** The big three:
  - **Materials Project**: **178,627 materials across 51,298 chemical systems** as of the 2025 Nature Materials review ([Horton et al. 2025](https://perssongroup.lbl.gov/papers/Horton_et_al-2025-Nature_Materials.pdf)); LBL's 2026 summary puts it at **>200,000 materials plus >577,000 molecules** ([Berkeley Lab, Jan 2026](https://newscenter.lbl.gov/2026/01/13/accelerating-discovery-how-the-materials-project-is-helping-to-usher-in-the-ai-revolution-for-materials-science/)). REST API with a Python client (`mp-api`), free with registration. Crucially it exposes **`/materials/alloys` — suggested alloy pairs based on phase stability** — i.e., a machine-readable answer to "will these two mix?" ([MP API docs](https://docs.materialsproject.org/downloading-data/using-the-api/getting-started)).
  - **OQMD** (Northwestern): **1,407,395 DFT-computed materials** ([oqmd.org](https://oqmd.org/)), download and REST access; strong on hypothetical/prototype structures.
  - **AFLOW** (Duke): **>3.5 million material entries**, 1,100+ crystallographic prototypes, a REST API with its own AFLUX query language ([aflowlib.org](https://aflowlib.org/); [API docs](https://aflow.org/documentation/); [Esters & Curtarolo 2022](https://doi.org/10.1016/j.commatsci.2022.111808)).
  - Cross-database caveat: formation energies disagree between the three by up to ~0.1 eV/atom (median ~6%) for the same material — comparable to DFT-vs-experiment error; treat the hull as ±that ([Hegde et al., Phys. Rev. Materials 7, 053805 (2023)](https://doi.org/10.1103/PhysRevMaterials.7.053805)).
- **Hume-Rothery rules** for substitutional alloys (textbook, no database needed): solid solubility of B in A is favored when (1) atomic radii differ by <~15%, (2) same crystal structure, (3) similar electronegativities, (4) same valence. They're heuristics — maybe 70–80% predictive for the first-row transition metals, famously wrong for the Cu–Fe edge cases — but they're *cheap* and a game can run them on the element table alone with zero downloaded data.
- **Formation enthalpy lookup:** for the classic binaries (oxides, chlorides, sulfides), NIST WebBook and standard tables give experimental $\Delta_f H^\circ$. The reaction $\mathrm{Fe} + \tfrac{1}{2}\mathrm{O_2} \to \mathrm{FeO}$ with $\Delta_f H^\circ \approx -272$ kJ/mol is a lookup, not a computation.

**Verdict:** element + element is a *solved lookup* — pull MP's convex-hull entries for the binary system, get the stable intermetallic/compound at that composition, use Hume-Rothery as the cheap fallback for pair screening. This fits offline game data perfectly (see §8).

### 6.2 Two compounds: NOT a solved lookup

There is no public database that answers "mix A + B, get C" generally. What exists:

- **Reaction retrieval** (the solved half): if A + B → C is a *known* reaction, it's probably in USPTO/ORD as reaction SMILES — you can search by reactant structure. This is a dictionary lookup, exact-match, and only covers reactions someone bothered to patent or publish (~2M organic ones).
- **Reaction prediction** (the unsolved half): since 2018 the state of the art is sequence-to-sequence *transformers* treating reaction SMILES as a translation problem. The **Molecular Transformer** ([Schwaller et al., ACS Central Science 5, 1572 (2019)](https://pubs.acs.org/doi/full/10.1021/acscentsci.9b00576); [code](https://github.com/pschwllr/MolecularTransformer/)) trained on USPTO achieves ~90% top-1 accuracy on benchmark single-step organic reactions, and became the engine of **IBM RXN for Chemistry** ([rxn.res.ibm.com](https://rxn.res.ibm.com)), the free public platform. The open lineage matters for a game: the models are trained on **open USPTO data**, IBM open-sourced the model zoo ([rxn4chemistry/rxn-models](https://github.com/rxn4chemistry/rxn-models): Molecular Transformer, retrosynthesis on USPTO-50k, USPTO-1k-TPL reaction classification, yield models), and the whole stack runs on a laptop.
- **Where they fail, bluntly:** (1) training data is *organic synthesis from patents* — aqueous chemistry, inorganic precipitation, gas-phase reactions, polymerization, and anything at game-world temperatures are out-of-distribution; (2) no quantities, no concentrations, no times, no yields in the base input (a reactant>agent>product string is all the model sees — condition prediction is a separate, weaker model); (3) ~10% wrong on its *own* benchmark distribution, worse off it; (4) retrosynthesis planners built on it assume a chemist in the loop.

For a game wanting "mix rust + acid → get salt + water" gameplay, the correct architecture is not a neural net: it's a **curated reaction table keyed on functional groups/materials**, with the transformer models (if used at all) as an offline authoring tool to *suggest* reactions a designer then verifies. The honest engineering call: reaction prediction is a research demo, not a runtime system.

### 6.3 Thermodynamics vs kinetics — the ground truth a realism game needs

Whether a reaction *can* happen and whether it *will* happen in your timescale are different questions, and games that skip the distinction feel wrong.

**Thermodynamics — will it go, in principle?** From formation Gibbs energies (NIST WebBook for ΔfH°; entropy from the same tables):

$$\Delta G^\circ_{rxn} = \sum \Delta G^\circ_f(\text{products}) - \sum \Delta G^\circ_f(\text{reactants}), \qquad \Delta G = \Delta H - T\Delta S$$

$\Delta G < 0$ at the relevant temperature → thermodynamically favorable. This is pure table lookup plus arithmetic and is completely feasible at runtime.

**Kinetics — will it go at *game* timescale?** Rate via Arrhenius:

$$k = A\, e^{-E_a / RT}$$

and here is where realism lives or dies:

- **Graphite vs diamond:** at STP, diamond is *metastable* — thermodynamically graphite is the stable carbon phase by ~2.9 kJ/mol — yet the activation barrier for conversion is so high that diamond persists geologically forever. A game with only ΔG would silently turn every diamond into graphite. Metastability must be modeled as a first-class concept: ΔG tells you the destination, $E_a$ tells you whether the trip happens this eon.
- **Rust timescale:** iron + oxygen is strongly favorable (ΔG ≈ −370 kJ/mol for hematite) but kinetically slow at room temperature — iron rusts over years, with the wet/dry cycle and salt acting as rate catalysts. Same lesson: the reaction is "true" but the *rate law* is what the player experiences. 
- **Explosives/combustion:** the reverse case — reactions both favorable AND fast (low barrier, exothermic, self-accelerating). These are the ones a game must not let the player trigger by accident with a lookup-only chemistry system.

The practical schema for a game chemistry engine: per reaction, store $\Delta H$, $\Delta S$ (or ΔG at reference T), an activation energy $E_a$ and a prefactor class (instant / seconds / minutes / geological), plus catalysts that lower $E_a$. That's four to six numbers per curated reaction, all obtainable from NIST for the common cases, and it reproduces the *behavioral* truth (graphite forever, rust slowly, TNT now) without any runtime quantum chemistry.

---

## 7. Medical, health, and compound-safety data

Where the biological layer lives, and its licensing reality:

- **DrugBank**: two tiers. The full dataset is free only for approved *academic* use under CC BY-NC 4.0 (application required); anything commercial needs a paid license — **not embeddable in a shipped game** ([terms](https://go.drugbank.com/legal/terms_of_use); [academic program](https://www.drugbank.com/academic_research)). The exception: the **DrugBank Open Data** subsets (drug vocabulary/identifiers, structures as SDF, ~1 MB CSV / 5 MB SDF) are **CC0 — genuinely free including commercial use** ([releases page](https://go.drugbank.com/releases/latest)).
- **ChEMBL** (EBI): ~2.4M compounds with drug-like bioactivity — **CC BY-SA 3.0**, full SQL/SDF downloads ([chembl](https://www.ebi.ac.uk/chembl/); [licensing FAQ](https://chembl.gitbook.io/chembl-interface-documentation/frequently-asked-questions/general-questions)). Embeddable with attribution and share-alike; the share-alike clause means your derived reaction/drug tables must carry the same license.
- **openFDA** (FDA): public-domain US government data via API — drug adverse events (FAERS, 2004→present), product labels (the actual prescribing text, structured), NDC directory; all harmonized to RxNorm identifiers ([API index](https://open.fda.gov/apis/); [adverse events](https://open.fda.gov/apis/drug/event/); [labels](https://open.fda.gov/apis/drug/label/)). Bulk downloadable. Fine for a game.
- **The terminology stacks:** **MeSH** (NLM's vocabulary) is public domain. **RxNorm** (normalized drug names) is free with a UMLS license, no fee. **SNOMED CT** (comprehensive clinical terminology) is **free only in IHTSDO member countries (incl. the US) and for qualifying research; fees apply elsewhere, and redistribution is restricted** — [SNOMED licensing at NLM](https://www.nlm.nih.gov/healthit/snomedct/snomed_licensing.html), [UMLS license agreement](https://uts.nlm.nih.gov/uts/assets/LicenseAgreement.pdf). The UMLS Metathesaurus itself is free but its license is per-user (not per-organization) and prohibits wholesale redistribution of restricted sources. For a game: use MeSH and openFDA freely; treat SNOMED/UMLS as off-limits for anything embedded in a shipped binary.
- **GHS hazard data — verified in PubChem:** yes. PubChem's Laboratory Chemical Safety Summary aggregates GHS classifications (H/P statements, pictograms), first aid, handling, and exposure data from authoritative sources (HSDB, ILO, ECHA-aligned contributors), retrievable per compound via PUG View (`?toc=LCSS+TOC`) and even as QR codes linking to LCSS pages ([PUG View docs](https://pubchem.ncbi.nlm.nih.gov/docs/pug-view); [PUG-View paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC6688265/)). ECHA's C&L (classification & labelling) inventory is the other public GHS source (EU regulation). This is the *right* safety layer for a game: the 9 GHS pictograms (explosive, flammable, oxidizer, corrosive, acute toxicity, health hazard, irritant, gas pressure, environment) map directly onto gameplay hazard tiers.
- **LD50/toxicity:** GHS acute-toxicity categories embed LD50 bands (e.g. Category 1 oral = LD50 ≤ 5 mg/kg; Category 5 = 2,000–5,000 mg/kg). Finer-grained LD50 values are scattered across PubChem annotations (HSDB, ECHA) and the open EPA CompTox Dashboard (DSSTox) — retrievable per compound, not as a clean bulk table.

The honest note: **pharmacology data is mostly NOT freely embeddable.** Drug–target interactions, dosages, metabolism pathways — the interesting biological layer — live in DrugBank's commercial tier, ChEMBL (share-alike), and the licensed terminology stacks. A game can legitimately ship: GHS tiers (PubChem/ECHA), drug *identifiers and structures* (DrugBank CC0 subset), adverse-event statistics (openFDA), and enzyme reaction data (BRENDA CC BY). It cannot ship a clinical pharmacology database without a license.

---

## 8. Practical synthesis for the engine

Opinionated minimum offline dataset for a "real chemistry" voxel world:

| Asset | Source | Size (order) | License reality |
|---|---|---|---|
| **Element table** (~118 rows × ~40 fields) | Wikidata SPARQL + CIAAW + NIST overrides | < 200 KB | CC0 + public-domain-ish |
| **Isotope/half-life table** (~5,300 rows) | NUBASE flat file | ~1 MB parsed | Free with attribution |
| **Common-compound subset** (1,000–10,000 compounds: minerals, water, acids/bases, fuels, ores, alloys, biomass) | PubChem FTP filtered to ChEBI + a curated list | 5–50 MB | Public domain (US gov) / CC |
| **Ontology roles** ("toxin," "fuel," "nutrient," "structural material") | ChEBI FULL ontology | ~100 MB, or CORE ~20 MB | CC BY 4.0 |
| **Formation enthalpies/entropies** for the compound subset | NIST WebBook (manual/per-record harvest) + MP API for solids | tiny (KBs) | NIST terms; fine for a game, verify redistribution |
| **Binary-phase/alloy hint table** for the ~50 gameplay-relevant element pairs | Materials Project `/materials/alloys` + convex hulls | KBs–MBs | CC BY 4.0 (MP data) |
| **Curated reaction table**: ~200–1,000 reactions with ΔH, ΔS, Ea-class, catalysts, conditions | NIST + hand-curation (USPTO/ORD for organic inspiration) | KBs | Own work + public data |
| **GHS hazard tier per compound** | PubChem LCSS via PUG View (one-time harvest) | KBs | Public domain (US gov) + source terms |

**Total: well under 100 MB** for a chemistry simulation that is genuinely data-grounded. That's a rounding error next to the engine's existing golden-capture directory (21 MB) and assets.

Keep it current: everything above has a stable release cadence — PubChem monthly FTP, ChEBI monthly, CIAAW biennial, NUBASE triennial, MP continuous. The correct build-system answer mirrors this repo's CPM pin discipline: vendor the extracted tables under `data/chemistry/` with a per-source date+version stamp in a manifest, and a refresh script (`tools/chem_fetch`) that re-harvests and diffs. Do not fetch any of it at runtime; do not depend on the live APIs (PubChem throttles at 5 req/s; MP needs a key).

**What to fake, bluntly:**

1. **Rates.** Even where Ea exists, real rate constants depend on medium, surface area, and catalysis in ways no table captures. Ship an Ea *class* (instant/seconds/minutes/hours/never-at-room-temp) and tune for gameplay; cite the real value in a comment.
2. **Everything outside the curated set.** When the player mixes two compounds with no table entry: no reaction is the correct default, and it's more honest than a hallucinated one. (If you must have emergent chemistry, restrict it to the element-pair layer, where MP's hull data actually covers the space.)
3. **Seawater/biochemical concentrations.** Fine-grained pharmacology (§7) is licensed; use GHS tiers + a coarse "toxic if ingested at X" scalar.
4. **The long tail of isotopes.** You need the four decay chains plus a "generic hot rock" hazard scalar, not 5,300 live nuclides — unless radioactivity is a core mechanic, in which case NUBASE is genuinely enough.
5. **Superheavy-element physical properties.** They're predictions (§1.3); if your game includes them at all, invent freely — it's no less accurate than the literature.

The deepest design point from all eight sections: **chemistry simulation is a data problem, not a compute problem.** Every layer of the answer chain — element properties, phase stability, formation enthalpies, hazard tiers — is table lookup at runtime, with kinetics reduced to a classified scalar. A C++ engine needs a good indexed table store, a ΔG evaluator, and a curated reaction list — not a chemistry library, not a neural net, and not a live internet connection.

---

## Provenance

**Verified live during this research (searched and opened, 2026-07):** PubChem scale figures (NAR 2025 paper + statistics page), CIAAW atomic-weight intervals and 2021/2024 revisions, NIST WebBook scope (>7,000 compounds thermochem), Johnson nucleosynthesis tables (Science 2019 + Phil Trans 2020 + SDSS blog + NASA SVS), GW170817 r-process evidence chain (Pian, Kasen, Chornock, Cowperthwaite papers; the Au/Pt non-detection), NUBASE2020 counts (3,340 + 1,938), ORD scale (>2M, ~1.7M USPTO) and CC BY-SA licensing, USPTO dataset (Lowe figshare, ~1.8M), Molecular Transformer/IBM RXN lineage and open model releases, Materials Project scale (178,627 per Nature Materials 2025; >200k per LBL Jan 2026), OQMD (1,407,395), AFLOW (>3.5M entries), the cross-DB disagreement study (PRM 2023), ChemSpider size and non-open status, ChEBI download formats and licenses, ChEMBL CC BY-SA 3.0, DrugBank open (CC0) vs academic (CC BY-NC) vs commercial tiers, CAS Common Chemistry (~500k, token API) vs CAS REGISTRY (290M+), openFDA endpoints, SNOMED CT/UMLS licensing constraints, PubChem PUG View LCSS/GHS exposure, BRENDA CC BY 4.0 + downloadable dumps, WebElements status and copyright terms, Wikidata element SPARQL patterns, RInChI format and IUPAC release.

**Unverified / taken from general knowledge without a fresh source:** Hume-Rothery rule details (textbook metallurgy); the specific FeO formation enthalpy (−272 kJ/mol) and diamond-graphite energy difference (~2.9 kJ/mol) — standard reference values, not re-verified against NIST in this pass; Rudnick & Gao (2003) cited via the Hu & Gao 2008 paper rather than directly; ChEMBL's "~2.4M compounds" current count (its site states the licensing and character clearly; the exact current compound count was not pinned to a dated source here).

**Disagreements found and how they're handled:** (1) r-process site — Johnson et al. 2020 explicitly dissent from the strong "NS mergers explain everything past Ni" reading; the document reports the split-by-mass consensus instead. (2) Materials Project scale differs between its own 2025 paper (178,627) and LBL's 2026 press (200,000+) — both are cited with dates. (3) The brief's "BKL/Brenda" — no public database named "BKL" could be verified; BRENDA + Rhea + MetaCyc are given as the actual open biochemistry stack. (4) CAS REGISTRY's headline number appears as both "165M+" (Common Chemistry page) and "290M+" (cas.org) — the vendor's own registry page is the authoritative larger figure, dated by retrieval.

---

## Sources

1. PubChem Statistics — https://pubchem.ncbi.nlm.nih.gov/docs/statistics
2. Kim et al., "PubChem 2025 update," Nucleic Acids Research — https://doi.org/10.1093/nar/gkae1059
3. CIAAW Standard Atomic Weights — https://www.ciaaw.org/atomic-weights.htm
4. CIAAW Isotopic Abundances — https://www.ciaaw.org/isotopic-abundances.htm
5. Prohaska et al., "Standard atomic weights of the elements 2021," Pure Appl. Chem. — https://doi.org/10.1515/pac-2019-0603
6. NIST Chemistry WebBook — https://webbook.nist.gov/chemistry/
7. NIST Chemistry WebBook guide — https://webbook.nist.gov/chemistry/guide/index.html.en-us.en
8. WebElements — https://www.webelements.com/
9. WebElements copyright — https://winter.group.shef.ac.uk/webelements/nexus/copyright.html
10. Wikidata WikiProject Chemistry / Elements — https://www.wikidata.org/wiki/Wikidata:WikiProject_Chemistry/Elements
11. Wikidata SPARQL query service — https://www.wikidata.org/wiki/Wikidata:SPARQL_query_service
12. Johnson, "Populating the periodic table: Nucleosynthesis of the elements," Science — https://www.science.org/doi/10.1126/science.aau9540
13. Johnson, Fields & Thompson, "The origin of the elements: a century of progress," Phil. Trans. R. Soc. A — https://royalsocietypublishing.org/doi/10.1098/rsta.2019.0301
14. Johnson, "Origin of the Elements in the Solar System" (SDSS blog, periodic table graphic) — https://blog.sdss.org/2017/01/09/origin-of-the-elements-in-the-solar-system/
15. NASA SVS, Periodic Table of the Elements: Origins of the Elements — https://svs.gsfc.nasa.gov/13873/
16. Pian et al., "Spectroscopic identification of r-process nucleosynthesis in a double neutron-star merger," Nature — https://arxiv.org/abs/1710.05858
17. Kasen et al., "Origin of the heavy elements in binary neutron-star mergers from a gravitational wave event," Nature — https://arxiv.org/pdf/1710.05463
18. Chornock et al., "Detection of Near-infrared Signatures of r-process Nucleosynthesis with Gemini-South," ApJL — https://iopscience.iop.org/article/10.3847/2041-8213/aa905c
19. Cowperthwaite et al., "Light curves of the neutron star merger GW170817/SSS17a," Science — https://www.science.org/doi/10.1126/science.aaq0049
20. "Constraints on the presence of platinum and gold in the spectra of the kilonova AT2017gfo" — https://arxiv.org/abs/2101.08271
21. Lemoine, Vangioni-Flam & Cassé, "Galactic Cosmic Rays and the Evolution of Light Elements," ApJ — https://doi.org/10.1086/305650
22. Reeves, "On the origin of the light elements (Z<6)," Rev. Mod. Phys. — https://doi.org/10.1103/RevModPhys.66.193
23. NUBASE2020 evaluation (Chinese Physics C) — https://www-nds.iaea.org/amdc/ame2020/NUBASE2020.pdf
24. IAEA LiveChart of Nuclides — https://www-nds.iaea.org/relnsd/vcharthtml/VChartHTML.html
25. NuDat 3 (BNL) — https://www.nndc.bnl.gov/nudat3/
26. Hu & Gao, "Upper crustal abundances of trace elements: A revision and update," Chemical Geology — https://www.sciencedirect.com/science/article/abs/pii/S000925410800185X
27. USGS, The Geology of Rare Earth Elements — https://geology.com/usgs/ree-geology/
28. "A Cross-Disciplinary Review of Rare Earth Elements," Minerals — https://www.mdpi.com/2075-163X/15/7/720
29. PubChem PUG REST — https://pubchem.ncbi.nlm.nih.gov/docs/pug-rest
30. PubChem PUG View — https://pubchem.ncbi.nlm.nih.gov/docs/pug-view
31. Kim, "PUG-View: programmatic access to chemical annotations integrated in PubChem," J. Cheminform. — https://pmc.ncbi.nlm.nih.gov/articles/PMC6688265/
32. ChemSpider — https://www.chemspider.com/ ; about — https://www.chemspider.com/about
33. "Caveat Usor: Assessing Differences between Major Chemistry Databases" — https://pmc.ncbi.nlm.nih.gov/articles/PMC5900829/
34. ChEBI downloads — https://www.ebi.ac.uk/chebi/downloads
35. ChEMBL — https://www.ebi.ac.uk/chembl/ ; licensing FAQ — https://chembl.gitbook.io/chembl-interface-documentation/frequently-asked-questions/general-questions
36. DrugBank terms of use — https://go.drugbank.com/legal/terms_of_use ; releases — https://go.drugbank.com/releases/latest ; academic — https://www.drugbank.com/academic_research
37. CAS Common Chemistry — https://commonchemistry.cas.org/ ; API — https://commonchemistry.cas.org/api-overview
38. CAS REGISTRY — https://www.cas.org/cas-data/cas-registry
39. Open Reaction Database — https://open-reaction-database.org/about
40. Kearnes et al., "The Open Reaction Database," JACS — https://doi.org/10.1021/jacs.1c09820
41. Lowe, "Chemical reactions from US patents (1976–Sept 2016)," figshare — https://figshare.com/articles/dataset/Chemical_reactions_from_US_patents_1976-Sep2016_/5104873
42. Wigh et al., "ORDerly: Data Sets and Benchmarks for Chemical Reaction Data" — https://pmc.ncbi.nlm.nih.gov/articles/PMC11094788/
43. Schwaller et al., "Molecular Transformer," ACS Central Science — https://pubs.acs.org/doi/full/10.1021/acscentsci.9b00576
44. Molecular Transformer code — https://github.com/pschwllr/MolecularTransformer/
45. rxn4chemistry open-source RXN models — https://github.com/rxn4chemistry/rxn-models
46. IBM RXN for Chemistry — https://rxn.res.ibm.com
47. RInChI v1.00 release (IUPAC) — https://iupac.org/rinchi-version-1-00-software-release/
48. RInChI format specification (PDF) — https://www.inchi-trust.org/wp/download/RInChI/RInChI%20V1-00-0.pdf
49. IUPAC-InChI/RInChI source — https://github.com/IUPAC-InChI/RInChI
50. Materials Project API getting started — https://docs.materialsproject.org/downloading-data/using-the-api/getting-started
51. Horton et al., "Accelerated data-driven materials science with the Materials Project," Nature Materials (2025) — https://perssongroup.lbl.gov/papers/Horton_et_al-2025-Nature_Materials.pdf
52. Berkeley Lab, Materials Project news (Jan 2026) — https://newscenter.lbl.gov/2026/01/13/accelerating-discovery-how-the-materials-project-is-helping-to-usher-in-the-ai-revolution-for-materials-science/
53. OQMD — https://oqmd.org/
54. AFLOW — https://aflowlib.org/ ; REST API — https://aflow.org/documentation/
55. Esters & Curtarolo, "aflow.org: A web ecosystem of databases, software and tools," Comput. Mater. Sci. — https://doi.org/10.1016/j.commatsci.2022.111808
56. Hegde et al., "Quantifying uncertainty in high-throughput DFT: AFLOW, Materials Project, OQMD," Phys. Rev. Materials — https://doi.org/10.1103/PhysRevMaterials.7.053805
57. openFDA API index — https://open.fda.gov/apis/ ; drug adverse events — https://open.fda.gov/apis/drug/event/ ; drug labels — https://open.fda.gov/apis/drug/label/ ; NDC — https://open.fda.gov/apis/drug/ndc/
58. NLM, SNOMED CT licensing — https://www.nlm.nih.gov/healthit/snomedct/snomed_licensing.html
59. UMLS Metathesaurus License Agreement — https://uts.nlm.nih.gov/uts/assets/LicenseAgreement.pdf
60. BRENDA — https://www.brenda-enzymes.org/ ; license — https://www.brenda-enzymes.org/license.php
61. NIST Atomic Spectra Database (portal) — https://www.nist.gov/pml/atomic-spectra-database

---

## Appendix A — Twenty further questions

1. **Does NUBASE2024 exist and where?** Yes — published in Chinese Physics C (2024) via the AMDC; NUBASE2020's URL is cited here as the verified stable one.
2. **Is PubChem's whole compound set really bulk-downloadable?** Yes — monthly FTP dumps of compounds, substances, and RDF; the statistics/API pages link them. Verified policy, not fetched in this pass.
3. **Can we run IBM RXN models offline?** Yes — the rxn4chemistry/rxn-models repo ships trained weights for reaction prediction, retrosynthesis (USPTO-50k), and classification; laptop-scale.
4. **Do Wikidata element records have all 40 fields?** No — coverage is good for mass/config/abundance basics, spotty for ionization energies and radii; NIST/CIAAW overrides are mandatory.
5. **Are Materials Project hull data usable offline?** Yes — bulk export via API/MPRester with a free key; CC BY 4.0.
6. **What's the licensing of NIST WebBook data for a shipped game?** Publicly accessible per-record; no official full-dump license — treat as per-compound harvest, verify redistribution terms before shipping.
7. **Is there a public phase-diagram database?** Not as a free bulk set; MP/OQMD convex hulls are the open substitute (see §6.1).
8. **How many of the 2M ORD reactions have yields?** A minority; ORDerly's yield-cleaning column (~127k–658k depending on strictness) gives the scale.
9. **Where do the four decay chains live as data?** Derivable from NUBASE decay-mode/branch columns by transitive closure — no separate source needed.
10. **Does GHS cover mixtures?** Classification rules for mixtures exist (GHS purple book, UN), but PubChem GHS data is per-substance; mixture tiers must be computed from component data.
11. **Can a game legally embed ChEMBL?** Yes under CC BY-SA 3.0, but derived tables must be share-alike — check with your lawyer for a commercial release.
12. **Is there an open solubility database?** Partially — NIST WebBook has Henry's law constants; PubChem annotations carry experimental solubility excerpts; no single clean bulk set. Disposition: harvest per-compound for the curated subset.
13. **What's the difference between NUBASE and AME?** NUBASE = nuclear properties (half-lives, decays); AME = masses only; they're co-published.
14. **Are cosmic-ray spallation rates stable over galactic history?** No — Be/B vs Fe in halo stars goes ~linear where naive GCR models predict quadratic; that's the whole superbubble-modification debate (§2.1 refs). Only matters if the game models metallicity evolution.
15. **Could the transformer models handle inorganic reactions?** Not reliably — trained on organic patents; inorganic/precipitation is out of distribution (§6.2).
16. **What does "/materials/alloys" in MP actually return?** Suggested alloy pairs based on computed phase stability — an MP API endpoint, verified in the docs table.
17. **Is WebElements still maintained?** Live and served (winter.group.shef.ac.uk mirrors), but effectively legacy single-maintainer; use as cross-check, not pipeline source.
18. **Are there 118 or more elements?** 118 confirmed/synthesized; Z ≥ 119 unmade. NUBASE covers estimated unobserved nuclides too.
19. **How do we get mineral compositions (quartz = SiO₂ etc.)?** Mindat (IMA database) is the public mineral authority; not covered in this pass — open question for the merged doc's Part 2 if needed.
20. **Biggest single risk in this data plan?** NIST WebBook's no-bulk-dump status — formation enthalpies are the one layer without a clean open bulk source; budget hand-harvest time or negotiate.
