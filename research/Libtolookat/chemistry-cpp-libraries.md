# Chemistry C++ Libraries, Simulation Codes, and Medical Data: An Embeddability Reference for a CMake/CPM Engine

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
