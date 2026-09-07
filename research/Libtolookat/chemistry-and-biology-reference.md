# Biology and Chemistry: Data, Databases, and C++ Libraries — A Look-Up Reference

**Scope:** the public landscape of chemical and biological data and code — the 118 elements and their authoritative datasets, how the elements were born (nucleosynthesis), isotope and radioactivity data, the compound and reaction databases (PubChem, Materials Project, ORD, USPTO), the honest computational answer to "what happens if we mix X with Y", the medical/health data layer (openFDA, RxNorm, ChEMBL, DrugBank and their licensing realities), and every C++ (and C++-adjacent) chemistry library worth knowing — with licenses verified, dead projects called dead, and a blunt final verdict on what actually fits a CMake/CPM C++20 voxel engine that vendors its dependencies.

**How this was built:** a deliberately lighter follow-up to the project's big research documents (terrain, sound, movement, vision — all in `research/`), per request: two research agents with live web access instead of seven, fixed section numbering so the merge preserves structure, and the same discipline — every URL verified live, every number dated, unverifiable claims dropped or explicitly flagged. Where an agent's research corrected the parent's own prompt (RDKit's license is BSD-3-Clause not Apache 2.0; MOPAC is Apache 2.0 not public domain; the "BKL" database does not exist as a public resource), the correction is documented in the text, not silently absorbed.

**How to read it:** Part 1 is the data side — what exists, where it lives, how current it is, and what it costs to license. Part 2 is the code side — which libraries are alive, which are license-poisoned, and what an engine like this one should actually embed versus precompute offline. Section 8 (Part 1) and Section 7 (Part 2) are the two "what do we do with this" sections; read them last.

---

# Part 1 - Elements, Databases, and the Public Chemistry Data Landscape

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


# Part 2 - C++ Chemistry Libraries, Simulation Codes, and the Medical Data Layer

**Scope:** the CODE side of chemistry — C++ and C++-adjacent libraries for cheminformatics, element data, molecular simulation, reaction modeling, and medical/health terminology — each evaluated for embedding in this project's shape: C++20, CMake + CPM.cmake, every dependency vendored and pinned to a verified tag in `cmake/Dependencies.cmake` (DiligentEngine, EnTT, GLM, FastNoise2 already in the tree). This is Part 2 of the merged reference in `research/Libtolookat/`; the numbering is fixed and preserved on merge.

**How to read this:** Sections 1–2 are "what exists as code you could link." Sections 3–4 are "what high-performance chemistry actually costs" — and why none of it runs in a frame loop. Section 5 is how software represents reactions. Section 6 is the medical data layer. Section 7 is the opinionated architecture call for THIS engine. Every license below was read from the project's own license page or repository, not from memory; every maintenance claim was checked against a release page or commit activity. Two claims from the original brief turned out to be wrong and are corrected inline: RDKit is **BSD-3-Clause, not Apache 2.0**, and MOPAC is **Apache 2.0 (formerly LGPL), not public domain**.

---

## 1. The C++ chemistry toolkit landscape

### 1.1 RDKit — the heavyweight, and the only one that matters

**What it is:** the reference cheminformatics toolkit. A C++ core (`GraphMol`, `SmilesParse`, `Fingerprints`, `ChemReactions`, `Descriptors`, `SubstructMatch`, 3D conformer ops) with Python wrappers via Boost.Python, Java/C# via SWIG, and JavaScript via emscripten — plus a PostgreSQL cartridge for substructure/similarity search and KNIME nodes. It parses and writes SMILES/SMARTS (Daylight standard plus extensions), does substructure search with stereochemistry awareness, reaction handling via reaction SMARTS (`REACTANTS>PRODUCTS` with atom maps), the full fingerprint family (Morgan/ECFP, atom-pair, topological torsion, pharmacophore), 2D depiction, 3D operations, R-group decomposition, maximum common substructure, and — since 2025 — synthon-space search over combinatorial libraries ([RDKit GitHub](https://github.com/rdkit/rdkit), [RDKit Book](https://www.rdkit.org/docs/RDKit_Book.html)).

- **License: BSD-3-Clause.** The repo states it plainly — "Code released under the BSD license… a business friendly license" ([RDKit GitHub](https://github.com/rdkit/rdkit)). The brief's "Apache 2.0" assumption was wrong; BSD-3 is equally clean for proprietary embedding (attribution + no-endorsement clauses only).
- **Language:** C++ core, ~everything else is generated wrappers. A first-class documented C++ API exists: `ROMol`/`RWMol`, `MolOps.h`, SMARTS-built queries for substructure matching ([Getting Started in C++](https://rdkit.org/docs/GettingStartedInC++.html)).
- **Build:** CMake. Modern C++ (the C++ guide says up to C++17). **The catch: it depends on Boost** — serialization/iostreams at minimum for the full build; the minimal build can drop boost::serialization ([PR #6932](https://github.com/rdkit/rdkit/pull/6932)). This project vendors Boost 1.92.0 already (`unordered` only), but RDKit wants a different, larger slice.
- **Health:** quarterly releases like clockwork. 132 releases; latest tagged line includes `2026_03_1` (March 27, 2026) with a `2026_09_1` in the release notes, and `Release_2025_09_5` (Jan 2026) on Zenodo ([releases](https://github.com/rdkit/rdkit/releases/tag/Release_2025_09_3), [ReleaseNotes](https://github.com/rdkit/rdkit/blob/master/ReleaseNotes.md)). Extremely alive.
- **Size reality:** it is a *large* library — dozens of static-lib targets even in a trimmed build. There is a `RDK_BUILD_MINIMAL_LIB` path, but it exists to feed the emscripten/JS wrapper and links ~14 internal libraries (SmilesParse, GraphMol, Fingerprints, Descriptors, SubstructMatch, MolDraw2D, CIPLabeler…) ([CMakeLists.txt](https://github.com/rdkit/rdkit/blob/master/CMakeLists.txt), [MinimalLib CMakeLists](https://github.com/rdkit/rdkit/blob/1159026a/Code/MinimalLib/CMakeLists.txt)).

**Embeddability verdict: license-clean (BSD-3), buildable under MSVC/CMake, but too heavy to vendor for what this engine actually needs.** Correct role: an OFFLINE tool (pip-installed Python bindings are fine) that generates the data files the game ships. If you ever truly need it at runtime, the C++ API is real and documented — but budget a day of CMake pain and a Boost slice you don't currently vendor.

### 1.2 Open Babel — actively maintained, license-poisoned for a proprietary engine

**What it is:** the universal chemical file-format converter — 90+ formats read/written, plus SMARTS filtering, 2D/3D coordinate generation, force fields (MMFF94, UFF), and a `obabel` CLI. C++ core with CMake and bindings for Python/Java/Perl/R etc. ([Open Babel GitHub](https://github.com/openbabel/openbabel)).

- **License: GPL-2.0-only** ([GitHub license](https://github.com/openbabel/openbabel), [Zenodo 3.2.0](https://zenodo.org/records/20399798) — "GNU General Public License v2.0 only"). That is the whole verdict: linking GPL-2.0-only code into a closed-source engine makes the engine's linked unit GPL. No.
- **Health: genuinely active, not the zombie its 2016-era reputation suggests.** 3.2.0 shipped May 2026 with CVE fixes (SMILES heap overflow, zipstream overlaps) and CMake-4 compatibility; 3.2.1 followed July 11, 2026 with more fuzz-hardening; a Ketcher KET JSON format reader/writer landed May 2026 ([3.2.1 release](https://github.com/openbabel/openbabel/releases/tag/openbabel-3-2-1)).

**Embeddability verdict: dead on arrival for a proprietary engine — the license, not the code, is the problem.** If you need format conversion offline, `obabel` the CLI (as a separate process, unmodified) is a legitimate tool in your data pipeline; process-boundary use of an unmodified GPL binary imposes no linking obligations.

### 1.3 chemkit — confirmed dead

A C++ cheminformatics/molecular-modeling library on Qt, BSD-3-Clause, by Kyle Lutz (Kitware-adjacent contributors). Last real activity ~2013–2014; SourceForge shows last update 2013, Open Hub shows **0 commits in the last 12 months and "most recent commit almost 7 years ago," maintained by nobody** ([GitHub](https://github.com/kylelutz/chemkit), [Open Hub](http://www.openhub.net/p/chemkit), [SourceForge](https://sourceforge.net/projects/chemkit/)). **Dead — do not recommend, do not vendor.** It is useful only as evidence that the "small clean C++ chem library" niche has been tried and abandoned.

### 1.4 cclib — Python data consumer, not a candidate

A BSD-3-Clause **Python** library (94.7% Python) that parses the *output files* of computational chemistry packages (Gaussian, GAMESS, ORCA, Psi4, …) into a uniform interface — coordinates, orbitals, vibrational modes, TD-DFT ([cclib GitHub](https://github.com/cclib/cclib), [docs](https://cclib.github.io/)). Actively maintained (v1.8.1 stable, v2.0a1 prerelease 2024, commits March 2026 — [PyPI](https://pypi.org/project/cclib/)). **Not embeddable in any sense — it's the downstream consumer of the quantum codes in §4.** Its relevance here: if you ever run Psi4/xTB offline to generate game data, cclib is one convenient way to scrape their logs from Python.

### 1.5 Indigo (EPAM) — the real surprise: clean Apache 2.0 and C++-first

**What it is:** a universal cheminformatics toolkit from EPAM with a from-scratch portable **C++ core** ("no third-party code or dependencies except the ubiquitous zlib and libcairo"), bindings to Python/Java/.NET/R/WASM, and a family of tools: canonical SMILES, depiction, R-group decomposition, and the Bingo chemistry search engine for Oracle/SQL Server/PostgreSQL/Elasticsearch ([epam/Indigo](https://github.com/epam/indigo), [Indigo site](https://lifescience.opensource.epam.com/indigo/index.html)).

- **License: Apache 2.0 since version 1.4.0** (versions ≤1.0 were GPL-3 — check the vintage of any fork you find) ([license page](https://lifescience.opensource.epam.com/indigo/index.html)).
- **Build:** CMake, GCC/Clang/MSVC official, C++14 floor ([repo](https://github.com/epam/Indigo)). It sits *inside* this repo's proven CPM-comfort zone far better than RDKit does — no Boost.
- **Health:** very active — releases 1.38 through 1.46 in the current line, 1.43.0 dated 2026-05-13, multi-threaded NoSQL substructure search added recently ([CHANGELOG](https://github.com/epam/Indigo/blob/master/CHANGELOG.md)).
- **Capability:** SMILES/SMARTS, substructure, fingerprints/similarity, stereochemistry (a stated strength), reactions, KET/CDXML formats. It does NOT have RDKit's breadth (no 3D conformer stack, no PostgreSQL cartridge of the same maturity — Bingo is separate, no synthon search).

**Embeddability verdict: the best *runtime-embeddable* C++ chemistry library found — Apache 2.0, CMake, no Boost, MSVC-supported.** If this engine ever needs live SMILES parsing/substructure search in-process, Indigo is the call, not RDKit.

### 1.6 libmolgrid — C++/CUDA, and only for a niche you don't have

GPU-accelerated molecular *gridding* for deep-learning docking workflows (the gnina project): turns molecules into voxelized density tensors for CNN scoring, with PyTorch/Caffe integration. C++/CUDA core, CMake, Apache 2.0 per the repository badge — note the JCIM paper's own text says "GPLv2," so trust the repo, not the paper, for the current license ([gnina/libmolgrid](https://github.com/gnina/libmolgrid), [JCIM paper](https://doi.org/10.1021/acs.jcim.9b01145)). Python-binding-first, CUDA-optional, small community. **Verdict: skip** — it solves ML input pipelines for protein–ligand scoring, not chemistry modeling; the "voxel" in its name is a coincidence of vocabulary.

### 1.7 The field's shape, honestly

Beyond the above, the search for "genuinely C++ cheminformatics" thins out fast. MMTF-CPP (RCSB's header-only C++ serializer for the MMTF macromolecular format) exists but serves a structural-biology data format this game has no use for; EleChemCpp could not be verified to exist as a maintained project this session (dispositioned in Appendix A). Every serious C++ project that needs element data or molecule handling either vendors RDKit/Indigo or rolls its own small tables — see §2, which is exactly that story. The one adjacent library worth knowing: **Gemmi** (structural biology, mmCIF/PDB) ships a beautiful self-contained `elem.hpp` — a `constexpr` 118-element table with weights, covalent/vdW radii, and a metal flag — as *internal* data, not as a published element library ([elem.hpp](https://project-gemmi.github.io/cxx-api/elem_8hpp_source.html)). That header is the whole genre in miniature.

---

## 2. Element/periodic-table data libraries

The blunt answer first: **no maintained, standalone C++ periodic-table library exists.** Every C++ project that needs element data ships its own private table, generated once from authoritative data:

- RDKit: internal `RDKit::PeriodicTable` singleton — atomic weights, isotope masses/abundances, default valences, outer-shell electrons, van der Waals radii, electronegativity ordering. Compiled data, not a library ([PeriodicTable.h](https://rdkit.org/docs/cppapi/PeriodicTable_8h_source.html)).
- Gemmi: `constexpr` arrays in `elem.hpp` — weights, Cordero covalent radii, vdW radii, names, a `static_assert`-checked metal table ([elem.hpp](https://project-gemmi.github.io/cxx-api/elem_8hpp_source.html)).
- SCINE (ETH): `ElementData.h` singleton — mass, Z, vdW radius, valence electron counts ([SCINE docs](https://scine.ethz.ch/static/download/documentation/utilities/file/_element_data_8h.html)).
- Even small academic tools hand-roll the arrays ([example](https://github.com/conradhuebler/curcuma/blob/master/src/core/elements.h)).

The Python world has the real libraries, and they're useful as **data sources, not dependencies**:

- **mendeleev** (MIT): SQLAlchemy/SQLite-backed access to 80+ properties per element, isotopes, ionic radii, ionization energies, oxidation states, 15+ electronegativity scales — and crucially, its raw data is published as a separate repo in **CSV/JSON/SQL/Markdown formats**, version-tagged in lockstep with the package ([GitHub](https://github.com/lmmentel/mendeleev), [docs](https://mendeleev.readthedocs.io/en/stable/faq.html)). The `mendeleev-data` exports are the fastest path to a generated C++ table.
- **periodictable** (Python): mass, density, X-ray/neutron scattering data; the older, narrower sibling, referenced alongside mendeleev in the latter's bibliography ([mendeleev citing page](https://mendeleev.readthedocs.io/en/stable/citing.html)).

**Verdict: generate your own.** A one-off script (Python, run at content-build time, not in CMake) reads `mendeleev-data` CSVs or NIST/Wikidata dumps and emits a `generated/element_table.inc` — a `constexpr std::array<ElementData, 118>` with the ~10 columns the game needs (Z, symbol, name, atomic mass, common valences, electronegativity, state at STP, category). This is a data file, not a dependency, and it fits the project's vendoring philosophy: pinned input, generated output, no runtime dependency to rot.

---

## 3. Molecular simulation codes that are actually C++

"High-performance chemistry" in practice means molecular dynamics: integrate atoms forward under a force field. The codes are excellent, C++, CMake-built, and almost all legally or architecturally wrong for a game engine. Understanding *why* is the point of this section.

### 3.1 The codes

- **LAMMPS** (Sandia; GPL, whole distribution): the classic MD workhorse — C++, CMake, enormous package ecosystem, GPU offload via Kokkos. Its license is GPL ([ReaxFF source header](https://github.com/lammps/lammps/blob/develop/src/REAXFF/reaxff_ffield.cpp) carries the GPL-2+ notice; LAMMPS as a whole is GPL-2.0) — same verdict as Open Babel: don't link it, ever, into a proprietary engine.
- **GROMACS**: biomolecular MD, C++/CUDA/SYCL, one of the fastest codes in existence. **LGPL-2.1** — and this genuinely changed over time: versions before 4.6 were GPL, 4.6+ are LGPL ([GROMACS about](https://www.gromacs.org/about.html), [Wikipedia](https://en.wikipedia.org/wiki/GROMACS), [manual preface](https://manual.gromacs.org/documentation/2025.5/reference-manual/preface.html)). Active: 2026.x releases. LGPL means dynamic linking is legally clean for a proprietary app — but see §3.2 for why you still won't.
- **OpenMM**: "toolkit for molecular simulation using high performance GPU code," C++ core with C/Fortran/Python wrappers, custom forces/integrators. **Split license: MIT** for the public API, reference and CPU platforms; **LGPL** for the CUDA/HIP/OpenCL platforms ([user guide](https://docs.openmm.org/latest/userguide/library/01_introduction.html), [Licenses.txt](https://github.com/openmm/openmm/blob/master/docs-source/licenses/Licenses.txt)). Active, well-maintained, the most library-shaped of all MD codes.
- **HOOMD-blue**: particle simulation (soft-matter focus) on CPU/GPU, C++ core with a Python front-end. **BSD-3-Clause** ([repo](https://github.com/glotzerlab/hoomd-blue/blob/trunk/README.md), [license page](https://hoomd-blue.readthedocs.io/en/stable/license.html)). Active. Legally the cleanest MD code — and still wrong for a frame loop.

### 3.2 ReaxFF: what "simulate the reaction" actually costs

ReaxFF (van Duin/Goddard) is the famous one: a reactive force field where bonds are continuous functions of interatomic distance and bond order, so **bonds break and form during the simulation** — it literally computes "what happens when you mix A and B," including charge transfer via QEq (a sparse linear solve for charge equilibration **every timestep**) ([LAMMPS reaxff docs](https://docs.lammps.org/latest/pair%5Freaxff.html), [Aktulga et al. 2012](https://www.uio.no/studier/emner/matnat/fys/FYS4460/v25/papers/aktulga-pandit-duin-grama-reactmdnummethandalgotech-siamjscicomp-2012.pdf)).

The cost structure, from the primary literature and benchmark reports:

- **Timesteps are tenths of a femtosecond** (0.25 fs is the standard ReaxFF step) — the potentials are stiff and bond-order dynamics demand it ([Aktulga 2012](https://www.uio.no/studier/emner/matnat/fys/FYS4460/v25/papers/aktulga-pandit-duin-grama-reactmdnummethandalgotech-siamjscicomp-2012.pdf)).
- Per-step cost is dominated by QEq (a GMRES/ILUT sparse solve) plus multi-body bonded terms; a Sandia benchmark of 116K atoms of crystalline HNS shows the GPU (Kokkos-CUDA) version spending ~34% in the QEq H-matrix construction alone, ~17% in the sparse matvec, ~16% in LJ/Coulomb — and a single P100 GPU is 17.6× a BlueGene/Q node for that workload ([OSTI ReaxFF benchmark deep dive](https://www.osti.gov/servlets/purl/1806475)).
- Scale the units: a "reaction happening" — proton transfer, a combustion front crossing a cell — takes **picoseconds to nanoseconds** of physical time. At 0.25 fs/step that is 10⁴–10⁶ timesteps. Even at an optimistic 1 ms/step on a consumer GPU for a few thousand atoms, one reaction event costs **10–1000 seconds of wall time**. A 16 ms frame budget fits ~16 steps. You can simulate one femtosecond of chemistry per frame — four orders of magnitude short of one reaction event.
- Non-reactive MD (GROMACS-class force fields) is 10–100× cheaper per step and fits more atoms, but it *cannot change bonding at all* — molecules are rigid topology. It answers "how do these fixed molecules jiggle," not "what forms."

**Verdict: MD is a precomputation technology, full stop.** What a game CAN do: (a) run ReaxFF-class simulations offline (LAMMPS as a separate GPL binary in your build pipeline — no linking, no license problem) for the handful of reactions the design cares about, and bake the *outcomes* (products, rates, energy release, phase behavior) into lookup tables; (b) at runtime, use cheap phenomenological models — Arrhenius rates on table entries, state machines, particle systems — that replay the precomputed truth. This is exactly the same move as precomputing ocean-wave spectra instead of solving Navier–Stokes (see the water reference, §1.2 there: pick the cheapest approximation that captures the effect you want to show).

---

## 4. Quantum chemistry for the brave

These compute electronic structure: formation energies, reaction enthalpies, optimized geometries — the **ground truth for "does compound X form from A+B, and how much energy does it release."** All are offline tools. None is a runtime candidate in any universe (a single DFT energy on 20 atoms is seconds-to-minutes; a formation-energy table for a few hundred compounds is an overnight batch job).

- **Psi4** — open-source *ab initio* QC, C++ core (66.8%) driven by Python, **LGPL-3.0**, active (v1.11, June 2026) ([GitHub](https://github.com/psi4/psi4/), [manual](https://psicode.org/psi4manual/master/introduction.html)). Routine DFT/MP2/CC computations with >2500 basis functions; the natural "real quantum numbers" engine for a data pipeline.
- **PySCF** — **Apache-2.0** Python/C quantum chemistry framework, very active ([about](https://pyscf.org/about.html), [GitHub](https://github.com/pyscf/pyscf)). The cleanest license in the quantum stack; the go-to for scripted formation-energy generation.
- **NWChem** — PNNL's supercomputer-scale code, Gaussian + plane-wave, ground/excited states, **Educational Community License 2.0 (ECL 2.0)** — an open, OSI-style license, though not one of the usual game-dev suspects ([site](https://nwchemgit.github.io/index.html)). Overkill unless you want periodic solid-state chemistry.
- **xTB / GFN** (Grimme lab) — semiempirical tight-binding: GFN1/GFN2-xTB and the GFN-FF force field give QC-flavored energies/optimizations at ~10⁶× DFT speed. **License: LGPL-3.0 today** ([GitHub](https://github.com/grimme-lab/xtb), [man page](https://github.com/grimme-lab/xtb/blob/main/man/xtb.1.adoc)) — the brief's memory of a restrictive academic license matches the *historical* distribution (pre-2019 binaries under a custom academic-use license); the current repository relicensing to LGPL is verified and clean. Fortran-97% codebase; active (pushes April 2026). This is the practical sweet spot for generating hundreds of formation energies cheaply.
- **MOPAC** — the 1983-vintage semiempirical package (AM1/PM6/PM7, COSMO solvation, MOZYME, periodic 1D–3D). **License: Apache 2.0** as of the v23 line — the release notes say it outright: "This is the first open-source MOPAC release under an Apache license. The switch from an LGPL license is to encourage… contributions from industry" ([releases](https://github.com/openmopac/mopac/releases), [repo](https://github.com/openmopac/mopac), [site](https://openmopac.net/)). **Not public domain** — the brief's guess is corrected. Active (23.2.x, 2026). PM7 formation enthalpies are a directly game-usable quantity.

**Verdict: offline data generators, never runtime.** The realistic pipeline for §7: PySCF or Psi4 (or MOPAC for bulk) → formation enthalpies for the compound list → baked into the same data files as everything else. cclib (§1.4) can scrape their logs if you want a uniform Python scraping layer.

---

## 5. Reaction modeling in code

How software actually represents and predicts reactions — and what a game can copy.

**Representation: reaction SMILES.** The industry-standard encoding is a SMILES variant: `REACTANTS>REAGENTS>PRODUCTS`, with optional atom-mapping numbers (`[CH3:1]...`) that pair atoms across the arrow; reaction SMARTS extend this to *queries* (transformation patterns). RDKit parses and applies both; Indigo handles reactions as first-class objects; the whole ML-reaction literature is built on this string form ([RDKit Book — reaction SMARTS](https://www.rdkit.org/docs/RDKit_Book.html), [DRFP paper](https://pubs.rsc.org/en/content/articlehtml/2022/dd/d1dd00006c)). This is the interchange format your offline tools should emit; the game runtime never needs to parse it.

**Reaction fingerprints.** The verified lineage: **Schneider, Lowe, Sayle & Landrum (2015)** built the first widely-used reaction fingerprint — the *difference* of atom-pair fingerprints (products minus reactants) plus computed properties of agents — and trained a 50-class classifier reaching **97% accuracy** on an external USPTO patent set ([JCIM, doi:10.1021/ci5006614](https://doi.org/10.1021/ci5006614)). Follow-up work by the same group assigned reaction *roles* (reactant vs. reagent vs. product) fingerprint-first, in ~8 ms per reaction, shipped inside RDKit ([What's What, JCIM 2016](https://pubs.acs.org/doi/abs/10.1021/acs.jcim.6b00564)). The modern non-ML baseline is **DRFP** (Reymond group): symmetric difference of circular substructure sets from both sides of the arrow, hashed and folded — no atom mapping, no training set, ~0.96 classification accuracy on the Schneider 50k benchmark ([Digital Discovery 2022](https://pubs.rsc.org/en/content/articlehtml/2022/dd/d1dd00006c), [code](https://github.com/reymond-group/drfp)). Point for a game: **reaction similarity search is a solved, cheap, non-magical technique** — difference-of-fingerprints + Tanimoto — if you ever want "find similar known reactions" as a UI feature.

**Retrosynthesis planners.** **AiZynthFinder** (AstraZeneca, **MIT license**): Monte-Carlo tree search over a target molecule, guided by a neural policy trained on reaction templates, terminating at purchasable precursors; typically finds a route in <10 s, full search <1 min ([GitHub](https://github.com/MolecularAI/aizynthfinder), [paper](https://link.springer.com/article/10.1186/s13321-020-00472-1)). **IBM RXN for Chemistry** is the closed cloud product, but its science is public: the **Molecular Transformer** (Schwaller et al. 2019) treats reaction prediction as SMILES→SMILES machine translation, >90% top-1 accuracy on patent data ([IBM Research blog](https://research.ibm.com/blog/thieme-rxn-for-chemistry)). The fully open lineage is AiZynthFinder + ASKCOS (MIT) + LillyMol (Eli Lilly) ([paper's survey](https://link.springer.com/article/10.1186/s13321-020-00472-1)). All Python/PyTorch; all irrelevant at runtime.

**The thermodynamic sanity checker — 30 lines you can write yourself.** Before any ML: does reaction R balance, and is it thermodynamically plausible? (1) Check atom counts balance (stoichiometry). (2) Compute ΔH_rxn = Σ ΔH_f°(products) − Σ ΔH_f°(reactants) — Hess's law — from standard formation enthalpies; strongly endothermic reactions don't happen spontaneously at game temperatures, strongly exothermic ones release |ΔH| as heat. Data source: the NIST Chemistry WebBook for gas-phase species ([NIST WebBook](https://webbook.nist.gov/chemistry/)), or your §4-generated values. This catches nonsense the ML models will happily emit and costs microseconds.

**What a game realistically does at runtime: lookup + enthalpy sanity check.** A curated reaction table (reactant set → product set → ΔH → rate class), consulted by a small engine module, with the balance/enthalpy math as validation. Not ML prediction, not MD, not quantum — table lookup with unit checks. Everything heavier happens offline.

---

## 6. Medical/health terminology libraries and APIs

The code side of medical data is dominated by government-maintained vocabularies and databases, accessed by HTTP or bulk download. There is no C++ "medical terminology library" to vendor — the layer is data + licenses.

- **UMLS** (NLM's metathesaurus unifying 200+ vocabularies): **free but licensed to individuals** — UTS account + license request form, approval within ~5 business days, API key from your profile; annual usage report required; and the commercial clause that matters for a game: *"if your product incorporates data from the UMLS, you must have a license. Additionally, we require that your clients have a license as well"* ([UMLS FAQ](https://www.nlm.nih.gov/research/umls/faq_main.html), [licensing page](https://www.nlm.nih.gov/databases/umls.html), [REST auth](https://documentation.uts.nlm.nih.gov/rest/authentication.html)). Distributing UMLS-derived data inside a shipped game means every player technically needs a license — a distribution non-starter unless you restrict to unencumbered vocabularies.
- **RxNorm** (normalized drug names, the backbone of pharmacy software): **non-proprietary; "with one exception, no license is needed to use the RxNorm API"**; no API key; rate limit 20 req/s/IP; bulk downloads exist via UTS account; and there's **RxNav-in-a-Box** — a locally installable Docker distribution of the whole REST API stack ([RxNorm API docs](https://lhncbc.nlm.nih.gov/RxNav/APIs/RxNormAPIs.html), [ToS](https://lhncbc.nlm.nih.gov/RxNav/TermsofService.html), [APIs index](https://lhncbc.nlm.nih.gov/RxNav/APIs/index.html)). The API is open; the *bulk files* sit behind the UTS signup.
- **openFDA** (FAERS adverse events, drug labels, NDC, enforcement, recalls): **CC0 1.0 — public domain**; no key required (one recommended for regular use); 1000-record max per call; **bulk JSON downloads of every endpoint** at download.openfda.gov in exactly the API's format, and the platform itself is open source so you can self-host without limits ([terms](https://open.fda.gov/terms/), [drug event API](https://open.fda.gov/apis/drug/event/), [downloads](https://open.fda.gov/apis/downloads/)). The cleanest data source in this entire section.
- **ChEMBL** (EMBL-EBI; 2M+ bioactive molecules, 13M+ activities): **CC BY-SA 3.0** — attribution + share-alike; full database dumps as **SQLite/MySQL/PostgreSQL + SDF** (release 37, May 2026) plus a REST API and the myChEMBL VM ([downloads](https://chembl.gitbook.io/chembl-interface-documentation/downloads), [FAQ/license](https://chembl.gitbook.io/chembl-interface-documentation/frequently-asked-questions/general-questions), [web services](https://chembl.gitbook.io/chembl-interface-documentation/web-services/chembl-data-web-services)). This is where compound structures come from if you want drug-like molecules in-game.
- **MeSH** (NLM's medical subject headings): freely available, no license, published as linked data (RDF); listed by NLM among its open terminologies ([UMLS FAQ](https://www.nlm.nih.gov/research/umls/faq_main.html)). The vocabulary for tagging medical content with sane semantics.
- **SNOMED CT** (the comprehensive clinical terminology): **territory-licensed** — free in IHTSDO member countries (US included) and low-income countries via the UMLS license; *fees may apply in non-member countries*, with registration through IHTSDO's MLDS required before non-member use ([SNOMED licensing](https://www.nlm.nih.gov/healthit/snomedct/snomed_licensing.html), [UMLS FAQ](https://www.nlm.nih.gov/research/umls/faq_main.html)). A shipped game is worldwide by default — this is a legal tangle, avoid.
- **DrugBank**: the often-cited drug database is **free for academic/non-commercial use only (CC BY-NC 4.0 academic tier)**; any commercial product — a sold game — requires a paid commercial license; the terms explicitly forbid use "as a component of a data product or within software to be made commercially available" ([Terms of Use](https://trust.drugbank.com/drugbank-trust-center/terms-of-use), [FAQs](https://dev.drugbank.com/guides/faqs)). Not usable here; use ChEMBL/RxNorm/openFDA instead.
- **PubChem** (honorable mention, chemistry side): 60M+ compounds, SMILES/properties, keyless REST (PUG REST), public-domain licensing ([PUG REST tutorial](https://pubchem.ncbi.nlm.nih.gov/docs/pug-rest-tutorial), [service paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC4489244/)). The best source for the *compound* data that §7's element/compound files need.

**The RDKit fingerprint tie-in:** ChEMBL's SDF dump → RDKit Morgan fingerprints (offline) → Tanimoto similarity — this is the standard industry pipeline for "compounds similar to X." For a game, precompute fingerprints offline and ship them; in-engine similarity search is then a vector dot-product loop over a few thousand entries — trivially fast, no chemistry library linked.

**Verdict table:**

| Source | Access | License | Embeddable offline in a shipped game? |
|---|---|---|---|
| openFDA | API + bulk JSON | CC0 public domain | **Yes — bulk download, ship as data files** |
| PubChem | API + bulk | Public domain | **Yes** |
| ChEMBL | API + SQLite/SDF dumps | CC BY-SA 3.0 | **Yes, with attribution + share-alike notice** |
| MeSH | RDF download | Free, no license | **Yes** |
| RxNorm | Keyless API; bulk via UTS account | Non-proprietary | **Yes** (download the monthlyRxNorm dump; keep the NLM attribution) |
| UMLS API | Keyed REST | Individual license, client-propagation clause | **No — API-only, license tangle on redistribution** |
| SNOMED CT | Via UMLS | Territory-dependent, fees outside member countries | **No — legally tangled** |
| DrugBank | API/datasets | CC BY-NC 4.0 academic; commercial = paid | **No — commercial use barred** |

---

## 7. What actually fits THIS engine

Given a C++20 CMake/CPM project that vendors everything and pins every tag, here are the calls:

**(a) The element table is a generated data file, not a library.** §2's finding stands: no maintained C++ periodic-table library exists; RDKit, Gemmi, and SCINE all prove the pattern is "generate your own." Concretely: a content-time Python script reads `mendeleev-data` CSVs (MIT) + NIST values, emits `generated/elements.hpp` — `constexpr std::array<element::Element, 118>` with Z, symbol, mass, valences, electronegativity, phase, category. ~200 lines of engine code total, zero dependencies, committed as a pinned generated artifact. This belongs in the same bucket as FastNoise2's compiled-in SIMD levels: deterministic, auditable, no runtime surprises.

**(b) Compound/reaction logic: a small internal module driven by data files — custom formula representation, NOT a SMILES subset.** Argument: SMILES is a graph grammar — parsing it correctly (aromaticity, ring-closure digits, stereo markers) is a real parser project, and its payoff is *molecular structure*, which a voxel game's chemistry loop doesn't need. What the game needs is: formula (Hill-notation string or a `std::array<uint8_t,118>` atom-count vector — the array is better: balance checking becomes integer arithmetic), canonical name, category/state, formation enthalpy, and derived properties (molar mass from the element table). Reactions reference formula-vectors, not molecular graphs: `Reaction{ reactants: [(H2,2),(O2,1)], products: [(H2O,2)], dH: -572 kJ }`. Balance check = atom-count equality, ~10 lines. If you *ever* need substructure search ("does this contain a benzene ring"), that's the trigger to revisit Indigo (§1.5) — not before. Keep a SMILES *string* per compound in the data files as an identifier for external tooling (RDKit, PubChem lookups) — storing one is free; parsing one at runtime is not.

**(c) RDKit is the offline generator, and offline only.** BSD-3 is clean even for runtime, but the Boost dependency, dozens of library targets, and the fact that everything it computes (fingerprints, canonical forms, enthalpy scraping, reaction templates) is *content-time* work make the call easy: pip `rdkit` in a `tools/` Python script, generating the game's data files. Nothing gets vendored into `Dependencies.cmake`. If live SMILES parsing ever becomes a hard requirement, Indigo (Apache 2.0, C++14, no Boost) is the embedded candidate, not RDKit.

**(d) ReaxFF-class interaction is data precomputation, never per-frame.** §3.2's arithmetic is not close: one reaction event is 10⁴–10⁶ MD steps; a frame budget is ~16. The pipeline if the design wants reactive-chemistry realism: LAMMPS (GPL, run as a separate unmodified binary in the content pipeline — no linking, no license issue) simulates the N reactions the design cares about → outcomes (products, ratios, energies, timescales) are extracted → the game runtime replays them from the reaction table with cheap kinetics (Arrhenius lookup, particle systems, heat accumulation). The engine-side chemistry module is then a state machine over a table — microseconds per tick, budgeted like any other gameplay system.

**(e) The medical layer is pure data files, sourced from the CC0/CC-BY/open tier.** If the game ever wants drug names, adverse-event flavor text, or compound databases: openFDA bulk JSON (CC0), ChEMBL SDF→ precomputed fingerprints (CC BY-SA, one attribution screen), PubChem compound dumps (public domain), RxNorm monthly release (non-proprietary). No API keys in shipped binaries, no UMLS/SNOMED/DrugBank entanglement, no HTTP at runtime — the game is offline-first and the licenses survive redistribution. This matches the existing engine philosophy exactly: everything the game needs at runtime is vendored, pinned, and auditable before the build.

**The one-sentence summary:** chemistry in this engine is a 118-row generated table, a formula-integer reaction module, and an offline pipeline of RDKit/LAMMPS/PySCF generating the data files — the only genuinely embeddable C++ chemistry library found (Indigo) stays on the bench until there's a runtime feature that demands it.

---

## Provenance

Researched 2026-09-06 with live web search (Exa). Every license claim was read from the project's own repository/license page or official documentation; every maintenance claim from release pages, changelogs, or commit-activity data within the last year. RDKit's license (BSD-3, not Apache 2.0) and MOPAC's (Apache 2.0 via LGPL relicensing, not public domain) correct assumptions in the original brief. Two brief-suggested candidates could not be verified and are dispositioned in Appendix A (EleChemCpp; MMTF-CPP evaluated as out-of-scope without deep verification). ReaxFF cost figures come from the primary SIAM J. Sci. Comput. paper and a Sandia/OSTI benchmark report, not vendor marketing. Word counts and further questions are the author's.

## Sources

1. RDKit repository and readme (license, architecture, wrappers): https://github.com/rdkit/rdkit
2. RDKit release notes (2026_03_1, 2025_09_5 lines): https://github.com/rdkit/rdkit/blob/master/ReleaseNotes.md
3. RDKit Book (SMILES/SMARTS, reaction SMARTS, pharmacophore fingerprints): https://www.rdkit.org/docs/RDKit_Book.html
4. Getting Started with RDKit in C++ (ROMol/RWMol, substructure search): https://rdkit.org/docs/GettingStartedInC++.html
5. RDKit root CMakeLists.txt (RDK_BUILD_MINIMAL_LIB options): https://github.com/rdkit/rdkit/blob/master/CMakeLists.txt
6. RDKit MinimalLib CMakeLists.txt (minimal library composition): https://github.com/rdkit/rdkit/blob/1159026a/Code/MinimalLib/CMakeLists.txt
7. RDKit PeriodicTable.h source (internal element data): https://rdkit.org/docs/cppapi/PeriodicTable_8h_source.html
8. RDKit release 2025_09_3 (release cadence evidence): https://github.com/rdkit/rdkit/releases/tag/Release_2025_09_3
9. Open Babel repository (GPL-2.0, capabilities): https://github.com/openbabel/openbabel
10. Open Babel 3.2.1 release (July 2026, CVE fixes): https://github.com/openbabel/openbabel/releases/tag/openbabel-3-2-1
11. Open Babel 3.2.0 Zenodo record (GPL-2.0-only, CMake-4 compat): https://zenodo.org/records/20399798
12. chemkit repository (last active ~2013): https://github.com/kylelutz/chemkit
13. chemkit Open Hub (0 commits/12mo, "maintained by nobody"): http://www.openhub.net/p/chemkit
14. chemkit SourceForge (last update 2013): https://sourceforge.net/projects/chemkit/
15. cclib repository (BSD-3, Python, parsers): https://github.com/cclib/cclib
16. cclib documentation (scope, supported programs): https://cclib.github.io/
17. cclib on PyPI (v1.8.1, license text): https://pypi.org/project/cclib/
18. EPAM Indigo repository (Apache 2.0, C++ core, CMake, bindings): https://github.com/epam/Indigo
19. Indigo project site (license history GPL→Apache, no-third-party core): https://lifescience.opensource.epam.com/indigo/index.html
20. Indigo CHANGELOG (1.43.0, 2026-05-13; 1.46 line): https://github.com/epam/Indigo/blob/master/CHANGELOG.md
21. libmolgrid repository (Apache 2.0 badge, C++/CUDA, CMake): https://github.com/gnina/libmolgrid
22. libmolgrid JCIM paper (paper-stated GPLv2 vs repo Apache 2.0): https://doi.org/10.1021/acs.jcim.9b01145
23. Gemmi elem.hpp source (constexpr element table pattern): https://project-gemmi.github.io/cxx-api/elem_8hpp_source.html
24. SCINE ElementData.h docs (another hand-rolled element table): https://scine.ethz.ch/static/download/documentation/utilities/file/_element_data_8h.html
25. mendeleev repository (MIT, data exports): https://github.com/lmmentel/mendeleev
26. mendeleev FAQ (80+ properties, SQLite backend, license): https://mendeleev.readthedocs.io/en/stable/faq.html
27. mendeleev citing page (periodictable reference): https://mendeleev.readthedocs.io/en/stable/citing.html
28. mendeleev API overview (tables: elements, isotopes, ionicradii…): https://mendeleev.readthedocs.io/en/stable/api_overview.html
29. LAMMPS pair_style reaxff documentation (QEq requirement, capabilities): https://docs.lammps.org/latest/pair%5Freaxff.html
30. LAMMPS ReaxFF source (GPL header, PuReMD lineage): https://github.com/lammps/lammps/blob/develop/src/REAXFF/reaxff_ffield.cpp
31. OSTI LAMMPS ReaxFF benchmark deep dive (P100 perf, QEq cost breakdown): https://www.osti.gov/servlets/purl/1806475
32. Aktulga et al., Reactive MD numerical methods (timestep scales, QEq): https://www.uio.no/studier/emner/matnat/fys/FYS4460/v25/papers/aktulga-pandit-duin-grama-reactmdnummethandalgotech-siamjscicomp-2012.pdf
33. GROMACS about page (LGPL-2.1): https://www.gromacs.org/about.html
34. GROMACS manual preface (LGPL-2.1, current 2025.5/2026 docs): https://manual.gromacs.org/documentation/2025.5/reference-manual/preface.html
35. GROMACS Wikipedia (GPL before 4.6, 2026.0 release): https://en.wikipedia.org/wiki/GROMACS
36. OpenMM user guide (MIT/LGPL split, C++ API design): https://docs.openmm.org/latest/userguide/library/01_introduction.html
37. OpenMM Licenses.txt (per-component licenses): https://github.com/openmm/openmm/blob/master/docs-source/licenses/Licenses.txt
38. HOOMD-blue README (BSD-3-Clause, scope): https://github.com/glotzerlab/hoomd-blue/blob/trunk/README.md
39. HOOMD-blue license page: https://hoomd-blue.readthedocs.io/en/stable/license.html
40. Psi4 repository (LGPL-3.0, C++ core, v1.11): https://github.com/psi4/psi4/
41. Psi4 manual license page (LGPL-3.0, dependency licenses): https://psicode.org/psi4manual/master/introduction.html
42. PySCF about page (Apache-2.0): https://pyscf.org/about.html
43. PySCF repository: https://github.com/pyscf/pyscf
44. NWChem site (ECL 2.0, capabilities): https://nwchemgit.github.io/index.html
45. xtb repository (LGPL-3.0, GFN methods): https://github.com/grimme-lab/xtb
46. xtb man page (methods, license): https://github.com/grimme-lab/xtb/blob/main/man/xtb.1.adoc
47. MOPAC releases (Apache 2.0 relicensing note, 23.2.x): https://github.com/openmopac/mopac/releases
48. MOPAC repository (Apache 2.0, history): https://github.com/openmopac/mopac
49. MOPAC site (AM1/PM6/PM7, since 1983): https://openmopac.net/
50. Schneider et al., reaction fingerprint paper (JCIM 2015): https://doi.org/10.1021/ci5006614
51. Schneider et al., What's What: reaction role assignment (JCIM 2016): https://pubs.acs.org/doi/abs/10.1021/acs.jcim.6b00564
52. DRFP paper (Digital Discovery 2022): https://pubs.rsc.org/en/content/articlehtml/2022/dd/d1dd00006c
53. DRFP code repository: https://github.com/reymond-group/drfp
54. AiZynthFinder repository (MIT license, MCTS): https://github.com/MolecularAI/aizynthfinder
55. AiZynthFinder paper (JOSS/J. Cheminformatics 2020): https://link.springer.com/article/10.1186/s13321-020-00472-1
56. IBM RXN / Molecular Transformer (accuracy figures): https://research.ibm.com/blog/thieme-rxn-for-chemistry
57. NIST Chemistry WebBook (formation enthalpies): https://webbook.nist.gov/chemistry/
58. UMLS FAQ (license process, commercial clause, SNOMED appendix): https://www.nlm.nih.gov/research/umls/faq_main.html
59. UMLS licensing overview (individual licenses, SNOMED fees): https://www.nlm.nih.gov/databases/umls.html
60. UMLS REST authentication (API key): https://documentation.uts.nlm.nih.gov/rest/authentication.html
61. SNOMED CT licensing page (territory rules, MLDS): https://www.nlm.nih.gov/healthit/snomedct/snomed_licensing.html
62. RxNorm API documentation (no license needed): https://lhncbc.nlm.nih.gov/RxNav/APIs/RxNormAPIs.html
63. RxNav Terms of Service (20 req/s, caching, RxNav-in-a-Box): https://lhncbc.nlm.nih.gov/RxNav/TermsofService.html
64. RxNav APIs index (RxNav-in-a-Box, family of APIs): https://lhncbc.nlm.nih.gov/RxNav/APIs/index.html
65. openFDA terms (CC0 public domain): https://open.fda.gov/terms/
66. openFDA drug adverse event API (FAERS, limits): https://open.fda.gov/apis/drug/event/
67. openFDA downloads (bulk JSON, self-hosting): https://open.fda.gov/apis/downloads/
68. ChEMBL downloads (release 37, SQLite/SDF, May 2026): https://chembl.gitbook.io/chembl-interface-documentation/downloads
69. ChEMBL FAQ (CC BY-SA 3.0 license): https://chembl.gitbook.io/chembl-interface-documentation/frequently-asked-questions/general-questions
70. ChEMBL web services (REST API, activity endpoint): https://chembl.gitbook.io/chembl-interface-documentation/web-services/chembl-data-web-services
71. DrugBank Terms of Use (non-commercial restriction): https://trust.drugbank.com/drugbank-trust-center/terms-of-use
72. DrugBank FAQs (CC BY-NC 4.0 academic, commercial licensing): https://dev.drugbank.com/guides/faqs
73. PubChem PUG REST tutorial (keyless access, formats): https://pubchem.ncbi.nlm.nih.gov/docs/pug-rest-tutorial
74. PUG-REST paper (PubChem scale, design): https://pmc.ncbi.nlm.nih.gov/articles/PMC4489244/
75. Project dependency-pinning philosophy (house context): `cmake/Dependencies.cmake` in this repository

## Appendix A — Twenty further questions

1. **Can RDKit link into this engine legally?** Yes — BSD-3-Clause, no copyleft; the obstacle is build weight (Boost slice, dozens of targets), not law.
2. **Does RDKit do 3D?** Yes — conformer generation, 3D descriptors, shape scores (2026 line added Gaussian shape alignment); still offline-grade work.
3. **Is Open Babel dead?** No — 3.2.1 shipped July 2026; it's license-dead for a proprietary engine, not maintenance-dead.
4. **Is there any GPL chemistry code we can still use?** Yes, as unmodified separate binaries in the content pipeline (LAMMPS, obabel) — process boundary avoids the linking question.
5. **GROMACS license changed — which applies?** LGPL-2.1 for ≥4.6 (current); pre-4.6 was GPL. Irrelevant for us either way (§3.2 cost).
6. **Why not just embed OpenMM's MIT parts?** You could (API+CPU platform are MIT), but per-step costs in §3.2 still forbid frame-loop MD; embedding buys nothing a table doesn't.
7. **Does xTB still have the restrictive academic license?** Not the current grimme-lab release — verified LGPL-3.0; the restriction memory applies to pre-2019 distribution.
8. **Is MOPAC public domain?** No — Apache 2.0 since the v23 line, LGPL before that; the "public domain" label belongs to ancient MOPAC 7 folklore.
9. **What's the cheapest source of formation enthalpies?** MOPAC PM7 or xTB GFN2 in bulk; PySCF/Psi4 DFT for the flagship compounds; NIST WebBook to spot-check both.
10. **Do we need reaction SMILES at runtime?** No — store them as identifiers in data files; runtime works on atom-count vectors.
11. **What is reaction atom-mapping for?** Pairing atoms across the arrow (for mechanism analysis and the Schneider-style fingerprints); a game never needs it live.
12. **Can we classify reactions without ML?** Yes — DRFP-style difference fingerprints + k-NN over a labeled table; no training set, sub-millisecond per query.
13. **Could AiZynthFinder plan syntheses for a crafting system?** Technically yes (MIT license, <10 s per route) but it's a Python/PyTorch stack — content-time demo material, not engine code.
14. **Is there a maintained C++ periodic-table library?** No — verified by survey; RDKit/Gemmi/SCINE all hand-roll. Generate your own (§2).
15. **Where does the generated element table's data come from?** `mendeleev-data` CSV exports (MIT) cross-checked against NIST; emit a `constexpr std::array` header.
16. **Can we ship ChEMBL data in a commercial game?** Yes under CC BY-SA 3.0 — attribution screen + share-alike for the adapted data; fingerprint derivatives follow the same notice.
17. **Can we ship DrugBank data?** Not without a paid commercial license — CC BY-NC 4.0 academic tier forbids exactly our use case.
18. **Does RxNorm need an API key?** No for the REST API (20 req/s cap); bulk files need a free UTS account; RxNav-in-a-Box offers a local Docker mirror.
19. **Is SNOMED CT usable in a worldwide game?** No — free only in member territories; fees and MLDS registration elsewhere; avoid entirely.
20. **EleChemCpp / MMTF-CPP — checked?** EleChemCpp: could not be verified to exist as a maintained project — dropped. MMTF-CPP: exists (RCSB, header-only C++ for the MMTF format) but serves macromolecular structure streaming this game doesn't need — out of scope, not evaluated further.


