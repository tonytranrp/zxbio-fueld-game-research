# Fluvial Erosion and Landscape Evolution: The Mathematical Core

**Scope:** how water carves terrain — the stream-power incision model and its whole family, drainage-network geometry, hillslope transport, landscape evolution models and their response times, the Grand Canyon as the worked case, meandering, long profiles and base level, deltas and sediment cascades, landscape memory, and the practical synthesis for a game terrain pipeline. This is the most mathematical part of the Earth-terrain document. Every formula is either derived (shown) or cited to a specific paper (URL). Measured quantities are ranges with sources; the numeric anchors from the parent brief are independently checked and disagreements are reported, not silently copied.

**How to read this:** Sections 1–2 are the load-bearing physics. Sections 3–4 give the static anatomy (networks, hillslopes). Sections 5–6 give the dynamics (LEMs, response times, the Grand Canyon). Sections 7–10 cover planform, base level, deposition, and memory. Section 11 is the opinionated implementation guide — read it last.

---

## 1. The stream power incision model (SPIM)

### 1.1 From energy to the stream power

A river of density $\rho = 1000$ kg/m³ falling a vertical distance through a reach does work on the bed. Total stream power (power per unit channel length, W/m) is the rate of potential-energy loss:

$$\Omega = \rho g Q S$$

with $Q$ the discharge (m³/s), $S$ the water-surface slope (dimensionless), $g = 9.81$ m/s². Divide by channel width $W$ to get **unit stream power** (W/m²), the quantity the bed actually feels:

$$\omega = \frac{\Omega}{W} = \rho g q S, \qquad q = Q/W \ \text{(unit discharge, m²/s)}$$

Equivalently, $\omega = \tau_b U$ (boundary shear stress × mean velocity), which is the standard derivation route: wide-channel shear stress $\tau_b = \rho g h S$, Chezy/friction closure $U \sim \sqrt{ghS}$, so $\omega \sim \rho g^{3/2} h^{3/2} S^{3/2}$ — this $S^{3/2}$, $h^{3/2}$ scaling is why incision is so violently nonlinear in slope and depth.

### 1.2 The detachment-limited incision law

Assume (i) the river can transport everything supplied to it (detachment-limited), (ii) discharge scales with drainage area, $Q \propto A$, (iii) width scales with discharge, $W \propto Q^{1/2}$ (Section 1.5). Then erosion rate scales as unit stream power, which scales as $A^{1/2}S$ (the classic $m=1/2$, $n=1$ of specific stream power; [Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120), [Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035)). Generalizing the exponents gives the workhorse of quantitative geomorphology:

$$\boxed{E = K A^m S^n}$$

- $E$ — long-term bedrock incision rate (m/yr)
- $A$ — upstream drainage area (m² or km², must match K's calibration)
- $S$ — channel slope
- $K$ — erodibility coefficient (lumps lithology, climate, channel width, unit weight of water)
- $m$, $n$ — positive exponents; theory gives $0 < m < 2$, $0 < n < 4$ ([Goren et al. 2014](https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf)); mechanistic derivations give $n \in [2/3, 7/3]$ depending on process ([Whipple et al. 2000](https://www.eoas.ubc.ca/~mjelline/453website/eosc453/E_prints/newfer06/2004whippleAREPS.pdf))

**The units problem (parent anchor check):** $K$ is *not* dimensionally fixed — its units are $[\text{m}^{1-2m} \cdot \text{yr}^{-1}]$ for the area-in-m² convention (e.g., m$^{0.5}$/yr when $m=0.25$; m$^{0.2}$/yr only if $m=0.4$... exactly one of the parent's guesses). Quoting "K ~ 1e-6 to 1e-5" without $m$, $n$ and the $A$ units is meaningless; the Harel et al. global compilation reports a *normalized* erodibility $K_{ref} = 2.9\times10^{-10} \pm 1.0\times10^{-9}$ m$^{1-2m_{ref}}$/yr at a reference concavity $m/n = 0.5$ — i.e., **K varies over ~9 orders of magnitude** across lithologies/climates ([Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035); [Goren et al. 2014](https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf) assume up to 9 orders). **Worked example (checked):** with $m=0.5, n=1$, $K = 10^{-5}$ m$^{0.5}$/yr (soft rock, area in m²), a mid-basin point with $A = 10^7$ m² (10 km²) and $S = 0.01$ gives $E = 10^{-5} \times 3162 \times 0.01 \approx 3.2\times10^{-4}$ m/yr = 320 m/Myr — a fast, active-orogen rate. With $K=10^{-6}$: ~32 m/Myr, a normal mountain-belt rate. With $K = 10^{-6}$ and a hard-rock $A=10^6$ m², $S=0.05$: 50 m/Myr. So "K~1e-6 to 1e-5" gives incision rates of roughly **10–500 m/Myr across typical mountain $A, S$ — parent's framing is right in magnitude but only once $m=0.5, n=1$, area-in-m² is fixed.**

### 1.3 The steady-state (equilibrium) profile — full derivation

At steady state with spatially uniform uplift $U$, incision balances uplift: $U = K A^m S^n$. Solving for slope:

$$S = \left(\frac{U}{K}\right)^{1/n} A^{-m/n}$$

This is **Flint's law** $S = k_s A^{-\theta}$ with the **concavity index** $\theta = m/n$ and **steepness index** $k_s = (U/K)^{1/n}$. Substitute $S = -dz/dx$ and Hack's law (Section 3) $A = (x/c)^{1/h}$ with $h \approx 0.6$:

$$-\frac{dz}{dx} = \left(\frac{U}{K}\right)^{1/n} \left(\frac{x}{c}\right)^{-\frac{m}{nh}}$$

Integrating from the outlet ($x = x_b$, $z = z_b$) upstream:

$$\boxed{z(x) = z_b + \frac{k_s\, c^{-\theta/h}}{1 - \theta/h} \left[ \left(\frac{x_b}{c}\right)^{1-\theta/h} - \left(\frac{x}{c}\right)^{1-\theta/h} \right]}$$

For typical $\theta/h \approx 0.5/0.6 = 0.83 < 1$ the profile is **concave-up** (slope decreases downstream, i.e. as $x$ grows). In the special case $m/n = h$ the exponent is 1 and the profile is exponential in $x$: $z = z_b + k_s' \ln(x_b/x)$ — the "logarithmic profile" special case. Note this derivation requires uniform $U$ and $K$; spatial gradients in either break the log-linear form and are the basis of the $\chi$ methods in Section 10 ([Willett et al. 2014](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf); [Smith & Fox 2024](https://eprints.bbk.ac.uk/id/eprint/54285/1/Smith_and_Fox_2024_Concavity.pdf)).

### 1.4 Concavity: verified values

- Theory predicts $0.4 < m/n < 0.6$ ([Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120); range 0.35–0.6 in [Goren et al. 2014](https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf)).
- Global compilation, N=1457 basins: **median $\theta = 0.51 \pm 0.14$** — the parent's "~0.4–0.6" is verified ([Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035)).
- But the same compilation finds a **global median slope exponent $n = 2.43 \pm 0.15$ (mean 2.6)**, well above the traditional $n=1$: incision is predominantly threshold-controlled and nonlinear. The common practice of assuming $n=1$ is questionable ([Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035); [Attal 2013](https://onlinelibrary.wiley.com/doi/10.1002/esp.3462) concludes the standard SPIM has "a narrow range of validity").

### 1.5 Channel width hydraulic geometry

Downstream hydraulic geometry ([Leopold & Maddock 1953](https://doi.org/10.5194/esurf-9-379-2021) gives $b \approx 0.5$, $k \approx 0.4$, $m \approx 0.1$, $z \approx -0.4$ for $W \propto Q^b$ etc.): the modern global dataset of Dunne & Jerolmack gives width exponent **$b = 0.512 \pm 0.007$** and depth $0.402 \pm 0.006$ ($R^2 = 0.89$, 0.86) ([Pelletier 2021](https://doi.org/10.5194/esurf-9-379-2021)). So $W \propto Q^{0.5}$ is verified.

**Parent anchor check — $W \approx 3.83\sqrt{Q}$:** This is REAL but is a *regional North American gravel-bed river* relation: **Bray (1973, 1982), Alberta single-thread gravel rivers: $W = 3.83\, Q_b^{0.53}$ (SI units, m and m³/s)**, from the classic regime-equation compilations ([NRCS Part 654 Ch. 9, Table 9-1](https://irrigationtoolbox.com/NEH/Part%20654/CHAPTERS/Chapter-09.pdf); also quoted in [Julien & Wargadalam 1995](https://doi.org/10.1061/(asce)0733-9429(1995)121:4(312))). The scatter and regional dependence are large: the same table gives **Nixon (1959, UK): $W = 2.99 Q^{0.5}$**; generalized North American gravel-bed rivers: **$W = 3.68 Q^{0.5}$** (Soar & Thorne 2001, per the NRCS chapter); Hey & Thorne (1986) UK types give 2.3–4.33 with $b = 0.5$. Emmett (1975, Salmon River ID): 2.8 $Q^{0.49}$. So **the coefficient varies 2.3–4.3 (~factor 2) between regions** — sediment load, bank vegetation, and flashiness (the NRCS chapter notes US rivers are systematically wider than UK ones at equal $Q$, possibly due to flashier flow or higher loads; [Parker et al. 2007](https://doi.org/10.1029/2006jf000549) provides the physics-based "quasi-universal" gravel version). **Verdict: parent's 3.83 is a legitimate bankfull relation (Bray), but it is one of a family spanning 2.3–4.3; use 3.0–4.0 as the plausible band, and expect ±50% site scatter.**

### 1.6 Threshold form and the tools-vs-cover effect

Real bedrock channels erode only when boundary shear stress exceeds a critical value. The threshold SPIM ([Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120); [Snyder et al. 2003](https://doi.org/10.1029/2001jb001655)):

$$E = K (A^m S^n - A_c^m S_c^n) \quad \text{or} \quad E = \max(0,\ K A^m S^n - \tau_c \cdot \text{(geometry factor)})$$

The LandLab implementation is literally $E = K A^m S^n - \text{threshold}$, floored at zero ([LandLab FastScape component docs](https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html)). Thresholds combined with stochastic flood distributions push the *effective* long-term $n$ above 1 ([Snyder et al. 2003](https://doi.org/10.1029/2001jb001655); [Lague 2014 per Harel](https://doi.org/10.1016/j.geomorph.2016.05.035)) — big floods do disproportionate work.

**Critical shear stress, verified:** for *coarse gravel motion* the critical Shields number is $\tau_c^* \approx 0.045$ on rough beds (gravel-bed rivers, low gradient) but ranges up to ~0.2 on very steep channels and down to ~0.007–0.01 for isolated grains on smooth bedrock ([Lamb et al. 2015](https://lamb.caltech.edu/documents/19653/Lamb_etal_Geomorphology_2015.pdf)). Converting to dimensional stress: $\tau_c = \tau_c^* (\rho_s - \rho) g D$. For $D = 50$ mm gravel, $\tau_c^* = 0.045$: $\tau_c \approx 0.045 \times 1650 \times 9.81 \times 0.05 \approx 36$ Pa. For $D = 100$ mm: ~73 Pa. For cobble/boulder $D = 0.5$ m on steep beds ($\tau_c^*=0.1$): ~800 Pa. **Parent's "~10–100 Pa for coarse gravel" is verified for the grain sizes 10–100 mm**; the full natural range is wider at both ends (smooth-bedrock incision thresholds can be a few Pa; field measurements at the Erlenbach gave a *motion* threshold around 1.9 N/m² but an effective incision threshold up to ~407 W/m² of unit stream power for the virtual bedload threshold; [Turowski et al. 2015 per Beer et al.](https://esurf.copernicus.org/articles/3/291/2015/esurf-3-291-2015.pdf)).

**Tools vs cover (Gilbert 1877; [Sklar & Dietrich 2004](https://doi.org/10.1029/2003wr002496)):** sediment is both the abrasive tool (more load → more impacts → more erosion) and the protective cover (more load → bed armored → less erosion). The saltation-abrasion model writes incision as

$$E = V_i \, I_r \, R_a$$

— volume eroded per impact × impact rate per unit area × fraction of exposed bedrock. Sklar & Dietrich used $R_a = 1 - Q_s/Q_t$ (linear cover); Turowski et al. (2007) derived an **exponential cover** $R_a = e^{-\gamma Q_s/Q_t}$, which fits the flume data better at high supply and predicts maximum erosion at $Q_s = Q_t$ rather than $Q_t/2$ ([Turowski, Lague & Hovius 2007](https://doi.org/10.1029/2006jf000697)). The generic shape: **erosion peaks at intermediate sediment supply** and →0 both at zero supply (no tools) and at capacity (full cover). Any LEM that ignores this overpredicts incision in supply-rich reaches.

### 1.7 When the SPIM is valid vs transport-limited

The SPIM is **detachment-limited**: erosion is rate-limited by the bed's ability to detach material, sediment exits the reach immediately. It is **transport-limited** when sediment supply exceeds capacity and the bed alluviates: then incision rate = (transport capacity out − supply in)/reach length, and profile form is set by the sediment-transport law, not the detachment law. Transport-limited behavior dominates: low-gradient rivers, sediment-rich orogens, downstream alluviated reaches; detachment-limited dominates steep bedrock channels with thin cover. Attal (2013)'s verdict is the honest one: the SPIM in its simple form has a narrow range of validity, and most natural datasets are threshold-dominated ([Attal 2013](https://onlinelibrary.wiley.com/doi/10.1002/esp.3462)); the FastScape lineage now solves a sediment-transport-enriched SPL precisely to cover this gap ([Braun & Willett 2013](https://doi.org/10.1016/j.geomorph.2012.10.008); Yuan et al. 2019 per [FastScapeLib docs](https://fastscape.org/fastscapelib-fortran/)).

### 1.8 Worked numeric anchors — unit stream power (parent check)

$\omega = \rho g q S$ with $\rho g = 9810$ N/m³:

| Parent anchor | Computation | Verdict |
|---|---|---|
| $\omega = 9.8$ W/m² at $q=1$ m²/s, $S=0.001$ | $9810 \times 1 \times 0.001 = 9.81$ W/m² | ✓ agrees (9.8) |
| $\omega = 981$ W/m² at $q=10$, $S=0.01$ | $9810 \times 10 \times 0.01 = 981$ W/m² | ✓ exact |
| $\omega = 490.5$ W/m² at $q=100$, $S=0.0005$ | $9810 \times 100 \times 0.0005 = 490.5$ W/m² | ✓ exact |

All three unit-stream-power anchors check out exactly (they're just $9810 \cdot qS$). For calibration: measured mean unit stream power at a sediment-starved bedrock chute (Erlenbach) was **5.9 W/m²** at flood stage with transport stage ~75× threshold ([Beer et al. 2015](https://esurf.copernicus.org/articles/3/291/2015/esurf-3-291-2015.pdf)) — i.e., useful incision happens at single-digit to tens of W/m² in steep headwaters; 100–1000 W/m² is big-river/large-flood territory.

---

## 2. Parent anchors consolidated — verdicts

| # | Anchor | Verdict |
|---|---|---|
| 1 | $\omega(1, 0.001) = 9.8$ | ✓ (9.81) |
| 2 | $\omega(10, 0.01) = 981$ | ✓ |
| 3 | $\omega(100, 0.0005) = 490.5$ | ✓ |
| 4 | $W \approx 3.83\sqrt{Q}$ bankfull | ✓ real (Bray 1973/82 Alberta gravel rivers) but regional: coefficient spans 2.3–4.3; scatter ±50% |
| 5 | $K \sim 10^{-6}$–$10^{-5}$ "m^0.2?" | **Units flagged**: K's units depend on m; 10⁻⁶–10⁻⁵ m^0.5/yr (m=0.5, area in m²) gives 10–500 m/Myr — right magnitude; the "m^0.2" guess is wrong for the m=0.5 case |
| 6 | $\theta \approx 0.4$–0.6 | ✓ verified; global median 0.51±0.14 |
| 7 | $\tau_c \sim 10$–100 Pa coarse gravel | ✓ for 10–100 mm gravel; full range wider |
| $\lambda \approx 10$–14 W | meander wavelength | ✓ (Section 7) |
| Niagara 0.3 m/yr to 1–3 m/yr | | ✓ (Section 8) |
| Grand Canyon 400–1700 m/Myr over 5–6 Myr | | ✓ as the young-canyon aggregate (Section 6) |

---

## 3. Drainage-network anatomy

### 3.1 Strahler ordering and Horton's laws

Strahler (1957) ordering: sources (unbranched headwater tips) are order 1; when two streams of equal order $k$ join, the downstream segment is order $k+1$; unequal joins inherit the higher order. Horton's laws (Horton 1945):

- **Law of stream numbers:** $N_\omega = R_B^{\Omega - \omega}$ (geometric decay with order $\omega$)
- **Law of stream lengths:** $\bar{L}_\omega = \bar{L}_1 R_L^{\omega-1}$
- **Law of basin areas:** $\bar{A}_\omega = \bar{A}_1 R_A^{\omega-1}$

Verified values: Horton's own basins gave $R_B$ from ~2 (flat/rolling) to 3–4 (mountainous/dissected), and $R_L$ ~2–3 (average 2.32) ([Horton 1945](https://pdfs.semanticscholar.org/39c3/9bbea565f8f963309e65506d7756f6571c18.pdf)). A modern 800-catchment Carpathian study: mean **$R_B = 3.8$** (σ=0.93; 90% of catchments < 4.8; outliers to 9), mean **$R_L = 2.3$**, mean **$R_A = 4.8$** — matching global norms ($R_B$ 3–5, $R_L$ 1.5–3.5, mean ~2) ([Bryndal 2015](https://doi.org/10.1515/quageo-2015-0008)). **Parent's "bifurcation ratio ~3.5–4, stream-length ratio ~2" is verified.** Two honest caveats: (i) the length law is only fulfilled in ~half of small natural catchments ([Bryndal 2015](https://doi.org/10.1515/quageo-2015-0008)); (ii) Kirchner (1993) showed random-topology networks obey the same laws almost automatically — Horton's ratios are a weak discriminator ([Bryndal 2015](https://doi.org/10.1515/quageo-2015-0008), citing Kirchner). The deep structure is captured by **Tokunaga self-similar trees** (side-branching statistics $T_k = a c^{k-1}$), which unify Horton laws, Hack's law, and fractal dimensions in one parameterized family ([Kovchegov & Zaliapin 2020](https://zaliapin.github.io/pubs/KZ_PS2020.pdf); [Kovchegov, Zaliapin & Foufoula-Georgiou 2022](https://doi.org/10.1103/physreve.105.014301)).

### 3.2 Hack's law

$$L = c A^{h}$$

$L$ = longest stream length, $A$ = basin area. Hack's original (Shenandoah): $L = 1.4 A^{0.6}$ (miles) ([Rigon et al. 1996](https://doi.org/10.1029/96wr02397)). Gray (1961): $h = 0.568$. Regionally $h$ varies; generally "slightly below 0.6"; Muller (1973) found $h$ decreasing for giant basins (0.6 below 20,720 km²; 0.5 to 0.47 above 259,000 km²) ([Rigon et al. 1996](https://doi.org/10.1029/96wr02397)). Arid-vs-humid: US-wide analysis gives **$h \simeq 0.5$ in arid basins, $h \simeq 0.6$ in humid basins** — humid basins get relatively thinner as they grow (groundwater-limited), arid basins scale self-similarly ([Seybold et al. 2018](https://royalsocietypublishing.org/doi/10.1098/rspa.2018.0081)). **Parent's h≈0.6 verified, with the arid/humid 0.5/0.6 split the refinement that matters for procedural generation.**

### 3.3 Drainage density

$D_d = \sum L_{channels}/A$ (km/km²). The climate story is **non-monotonic**: low in arid areas (little runoff), maximum in semi-arid (runoff up, vegetation still sparse), decreasing to a subhumid/humid minimum (vegetation suppresses runoff), possibly rising again in superhumid/tropical (precipitation variability) — the Abrahams (1984) consensus summarized in [Collins & Bras 2010](https://doi.org/10.1029/2009wr008615). Typical magnitudes: **2–12 km/km² across semi-arid to humid landscapes** (NetMap tools summary, citing Abrahams 1972 and Grant 1997: [netmaptools.org](https://www.netmaptools.org/Pages/NetMapHelp/drainage_density.htm)); ~2–5 as a broad "normal" band. Badlands are the extreme: Schumm's Perth Amboy badlands ran to ~60–110+ km/km² (the original table lists drainage density 110.8 for the fifth-order Perth Amboy system — small-scale rilled badlands are one to two orders of magnitude denser than vegetated terrain; [Schumm 1956](https://pdodds.w3.uvm.edu/research/papers/others/1956/schumm1956a.pdf); also [Howard 1997](http://geomorphology.sese.asu.edu/Papers/Howard_ESPL_97.pdf) for Mancos Shale badlands). **Parent's "roughly 1–100+ km/km²" is verified as the full span** (normal landscapes 1–10ish; badlands to ~100). Controls: the channelization threshold $A_c$ — the drainage area needed to sustain a channel — is inversely related to $D_d$; $D_d \sim 1/\sqrt{A_c}$ ([Collins & Bras 2010](https://doi.org/10.1029/2009wr008615)).

### 3.4 Fractal dimension of networks

From Horton ratios, the classic La Barbera–Rosso relation: $D = \log R_B / \log R_L$. With $R_B \approx 4$, $R_L \approx 2$: $D = 2$ exactly; with real-world $R_B=3.8, R_L=2.3$: $D \approx 1.65$. La Barbera & Rosso's field-data synthesis: typical network fractal dimension **1.5–2.0, average ~1.6–1.7** ([La Barbera & Rosso 1989](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/WR025i004p00735)). Peckham's topological fractal dimension for natural networks: **typically 1.7 < $D_T$ < 1.8** ([Gupta & Mesa 2014](https://doi.org/10.5194/npgd-1-705-2014)). **Parent's "~1.6–1.9 for the network as a set" is verified** (1.6–1.8 as the central band; 2.0 is the space-filling limit reached only asymptotically). Note the multiple inequivalent definitions (mainstream sinuosity dimension ~1.1–1.2, network similarity dimension, total-length scaling) — be explicit about which one you compute ([Beer & Borgas 1993](https://doi.org/10.1029/92wr02731)).

### 3.5 Nested self-similarity

Evidence: (i) Horton laws hold within subbasins at all scales; (ii) Tokunaga statistics pass formal self-similarity tests in the majority of 408 US networks ([Zaliapin et al. 2013 per efi.eng.uci.edu](https://efi.eng.uci.edu/papers/efg_128.pdf)); (iii) Hack's law applies to *any interior point* of a basin, not just outlets (the statistical framework of [Rigon et al. 1996](https://doi.org/10.1029/96wr02397)); (iv) deltas' distributary networks obey Hack's law too (Dong et al., per [Wikipedia: Hack's law](https://en.wikipedia.org/wiki/Hack%27s_law) — flag: secondary source). For a game this means: generate one scale-correct branching statistic and it tiles across all zoom levels.

---

## 4. Hillslopes

### 4.1 Linear diffusion — derivation from mass balance

Soil-mantled hillslopes move by creep (rainsplash, bioturbation, freeze–thaw). Continuum mass balance for the soil layer: $\partial h_s/\partial t = -\nabla \cdot \mathbf{q}_s + P_{soil}$, with soil flux $\mathbf{q}_s = -D \nabla z$ (Fickian; Culling 1963, verified in [Roering et al. 1999](https://doi.org/10.1029/1998wr900090)). If soil thickness is steady and production balances erosion, the *land surface* obeys:

$$\boxed{\frac{\partial z}{\partial t} = U + D \nabla^2 z}$$

— uplift plus linear diffusion. At steady state on a 1D hillslope ($\partial z/\partial t = 0$): $D\, z'' = -U$, so $z(x) = \frac{U}{2D}(L^2 - x^2)$: **parabolic, convex-up, constant curvature $-U/D$ everywhere.** The hilltop curvature test: measure $\nabla^2 z$ at the divide, know $U$, get $D$ directly (the standard field inversion; [Perron et al. 2009](https://doi.org/10.1038/nature08174)). Typical $D$: ~10⁻³–10⁻¹ m²/yr in soil-mantled temperate terrain (order 0.01 m²/yr; see [Fernandes & Dietrich 1997 per Roering](https://doi.org/10.1029/1998wr900090) — flagged as order-of-magnitude, not individually verified here).

### 4.2 Critical slope / angle of repose

Linear diffusion has no slope ceiling. Real granular material does. Talus/scree slopes: measured repose angles of coarse angular fragments ~35° (limestone quarry cones: 35°, [Carson 1977](https://doi.org/10.1002/esp.3290020408)); Alpine talus profile break points at **33–34°** ([Francou & Manté 1990](https://doi.org/10.1002/ppp.3430010107)); laboratory sand repose 32–38° with Shields' classic 33° for uniform sand ([Frontiers loess study](https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2021.777467/full)). Chandler (1973) argues the *shearing resistance* of talus materials is 39–40° and that the typical 35° inclination is a degradation-limited, not strength-limited, angle — slopes stand at 39°+ only when rapidly eroded or rapidly deposited ([Chandler 1973](https://doi.org/10.1086/627804)). **Parent's "~33–37°, tan(37°)=0.754" verified** (tan 37° = 0.7536 ✓; the 33–37° band spans measured talus and repose values; note the >39° shearing-resistance nuance). For game thermal-erosion passes: talus angle 33–37°, with lower values (28–33°) for rounded/fine material and cohesion (clay/silt) pushing stable angles well above repose (Schumm's Perth Amboy badlands held mean maximum slopes of **48.8°** in cohesive silt-rich fill; [Schumm 1956](https://pdodds.w3.uvm.edu/research/papers/others/1956/schumm1956a.pdf)).

### 4.3 Nonlinear extensions

1. **Nonlinear (Roering) creep:** from a balance of disturbance-driven kinetic energy against friction+gravity,

$$q_s = \frac{D \nabla z}{1 - (|\nabla z|/S_c)^2}$$

with $S_c = \tan\phi$ the critical gradient. Flux ~linear at low slope, diverges as $|\nabla z| \to S_c$; equilibrium hillslopes become convex near the divide and **planar near $S_c$ downslope** — matching lidar morphology in the Oregon Coast Range, calibrated $S_c \approx 0.65$–0.8 there ([Roering, Kirchner & Dietrich 1999](https://doi.org/10.1029/1998wr900090); response times ≤50 kyr vs 4× longer for linear, [Roering et al. 2001](https://doi.org/10.1029/2001jb000323); experimental confirmation of the creep→landslide transition with 1/f flux spectra, [Roering et al. 2001, Geology](https://doi.org/10.1130/0091-7613(2001)029)). Consequence for terrain generation: **average hillslope gradient is a poor erosion-rate proxy in steep terrain; curvature near the divide is the good one.**
2. **Depth-dependent transport:** flux ∝ (soil depth × slope), not just slope — the "illusion of diffusion" paper shows linear diffusion only fits shallow, convex portions ([Heimsath, Furbish & Dietrich 2005](https://doi.org/10.1130/g21868.1)).

### 4.4 The channelization transition — the critical length scale

Perron, Kirchner & Dietrich's result: the governing equation (creep + SPIM) is a nonlinear advection–diffusion equation; its dimensionless group is a Péclet number $Pe = K L^{2m+2}/D$ (advection/diffusion, at horizontal length scale $L$). Setting $Pe = 1$ defines the **characteristic length**:

$$\boxed{L_c = \left(\frac{D}{K}\right)^{1/(2m+2)}}$$

(up to an $m$-dependent constant). Measured valley spacing across five US field sites is proportional to $L_c$ ([Perron et al. 2009](https://doi.org/10.1038/nature08174); fuller derivation in [Perron et al. 2008](https://doi.org/10.1029/2007jf000977)). Notable predictions: valley spacing is *independent of erosion rate*; the hillslope→valley transition occurs at drainage area $A_c \sim L_c^2$; low $Pe$ → smooth undissected slopes, high $Pe$ → branching networks. **This is the parent's "critical hillslope length scale" — verified, with the exact $L_c$ formula above.** For a game: pick $D/K$ to set your valley wavelength directly.

---

## 5. Landscape evolution models (LEMs)

### 5.1 The field

- **CHILD** (Tucker, Lancaster, Gasparini) — triangulated irregular network, stochastic rainfall, detachment/transport-limited options (cited throughout, e.g., [Tucker & Hancock 2010](https://doi.org/10.1002/esp.1952)).
- **LandLab** — Python component library; its FastScape-style stream power component implements exactly $E = K A^m S^n -$ threshold with $K_{sp}=0.001$, $m=0.5$, $n=1$ defaults ([LandLab docs](https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html)).
- **FastScape** (Braun & Willett) — the algorithm that made geological-scale runs cheap: an **O(n), implicit-in-time solver of the stream power equation**. The trick: process nodes in order of decreasing elevation (a stack built once per step), which makes the otherwise-nonlinear implicit update a single ordered sweep; stable at large timesteps and parallelizable ([Braun & Willett 2013](https://doi.org/10.1016/j.geomorph.2012.10.008)). The library couples SPL + sediment transport/deposition (Yuan et al. 2019) + hillslope diffusion + marine transport, plus a **spectral flexure solver** (thin-elastic-plate biharmonic over an inviscid asthenosphere, E=10¹¹ Pa, ν=0.2) for isostatic rebound ([FastScapeLib docs](https://fastscape.org/fastscapelib-fortran/)). **Parent's "stream-power + flexure solver used in real research" verified.**

### 5.2 Steady state and response times

**Topographic (geomorphic) steady state:** erosion rate = uplift rate everywhere; mean elevation, relief, and hypsometry stop changing. Willett et al.'s coupled models put time-to-steady-state at **~1–50 Myr for mountain belts 50–200 km wide, uplift 0.1–1 mm/yr** — longer times for lower uplift ([Willett et al. 2014 per ajsonline.org PDF](https://ajsonline.org/article/88260-uplift-shortening-and-steady-state-topography-in-active-mountain-belts.pdf)). Whipple & Tucker's SPIM analysis: response time to base-level fall scales as $T \propto L^{?}$... their headline result is that response time is *relatively insensitive to basin size* but depends on $n$: for $n<1$ longer for small perturbations, $n=1$ independent of uplift, $n>1$ shorter for large perturbations ([Whipple & Tucker 1999](https://doi.org/10.1029/1999JB900120)). **Parent's "order 1e5–1e7 yr" verified**: hillslopes adjust in ≤50 kyr ([Roering et al. 2001](https://doi.org/10.1029/2001jb000323)); Taiwan-scale basins reach steady state in 0.5–2 Myr ([Chen et al. 2012 per sciencedirect](https://www.sciencedirect.com/science/article/abs/pii/S0169555X1200205X)); whole mountain belts 1–50 Myr; post-orogenic decay ~50 Myr e-folding with isostasy, longer with resistant rock ([Pelletier 2004](https://doi.org/10.1029/2004gl020052)); the southeastern US is *still* far from equilibrium millions of years after the tectonics quit ([Willett et al. 2014](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf)).

### 5.3 Transients and knickpoints — the messenger

A sudden base-level fall injects a **knickpoint** (steepened reach) that propagates upstream as a kinematic wave with celerity

$$\boxed{C = \frac{dn}{dt} = K A^m S^{n-1}}$$

(derivation: perturb the SPIM about steady state; the characteristic speed of the advective equation $\partial z/\partial t = U - K A^m |\partial z/\partial x|^n$ is $K A^m S^{n-1}$.) For $n=1$ the knickpoint preserves its shape as it migrates. Field rates: **0.001–0.1 m/yr typical, >1 m/yr exceptional** (Niagara, active orogens) ([Loget & Van Den Driessche / wave-train model](https://archimer.ifremer.fr/doc/00000/11075/8056.pdf), compiling Van Heijst & Postma 2001, Philbrick 1970, Tinkler 1994). Retreat rate scales with drainage area, approximately $\propto \sqrt{A}$ ($V = C\sqrt{A}$ with $C \sim 10^{-5}$ yr⁻¹ across the Messinian Salinity Crisis data) ([Loget et al.](https://archimer.ifremer.fr/doc/00000/11075/8056.pdf)). Counterintuitive field result: catchments crossing higher-throw-rate faults have *faster* knickpoints — amplitude speeds the response, partly via channel narrowing ([Whittaker et al. / JGR 2011](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2011JF002157): 0.2–2 mm/yr for 6–65 km² catchments in Turkey/Italy). Knickpoints can also stall (resistant lithology, landslide armoring, sediment cover), protecting upstream relict surfaces ([Clark et al. 2006](https://doi.org/10.1029/2005jf000294)).

---

## 6. Grand Canyon — the worked case study

**The standard story:** Colorado River integrated through the canyon 5–6 Ma, carving most of ~1.5 km depth since → **average incision ~250–300 m/Myr integrated over 5–6 Myr**, with shorter-term terrace-derived rates of **150–175 m/Myr over the last 2–3 Myr** below Lees Ferry ([Darling 2011 thesis, compiling Pederson 2002, Polyak 2008, Karlstrom 2008](https://digitalrepository.unm.edu/eps_etds/15)); and faster late-Pleistocene pulses: Glen Canyon dates ≤500 ka yield rates **up to 500 m/Myr** (though the Darling work argues some of these are age-underestimates), and speleothem water-table decline rates of **166–411 m/Myr in the eastern canyon, 55–123 m/Myr in the west** ([Polyak et al. 2008, Science](https://www.science.org/doi/10.1126/science.1151248)). **Parent's "integrated ~400–1700 m/Myr over 5–6 Myr" is partially verified:** the 5–6 Myr window with 150–500 m/Myr is well-supported; 1700 m/Myr exceeds any main-stem integrated rate I found — the closest are the ≤500 ka local pulses (~500) and eastern-canyon speleothem rates (~411). **Disagreement reported: 1700 m/Myr is not supported by the sources found; treat 150–500 m/Myr as the verified integrated range with local short-term excursions to ~500.**

**The "old Grand Canyon" controversy:** Flowers & Farley's apatite ⁴He/³He thermochronometry suggests the *western* canyon was excavated to within a few hundred meters of modern depth by **~70 Ma** ([Flowers & Farley 2012 / Science summary](https://www.science.org/doi/10.1126/science.1229390)). Karlstrom et al.'s reconciliation — now the mainstream view — is segment-by-segment: Hurricane segment ~half depth by 70–55 Ma, Eastern Grand Canyon 25–15 Ma, but **Marble Canyon and the Westernmost Grand Canyon are young (carved in the past 5–6 Ma)**; the modern canyon is the Colorado River reusing and stitching older palaeocanyons ([Karlstrom et al. 2014, Nature Geoscience](http://geomorphology.sese.asu.edu/Papers/Karlstrom-2014-NatGeoscience.pdf)). Later re-analysis of westernmost-canyon ⁴He/³He data (with measured U-Th zonation) supports the young interpretation ([Fox et al. 2017](https://doi.org/10.1016/j.epsl.2017.06.049)). Genuine literature disagreement — report both.

**Role of uplift and terraces:** incision through Grand Canyon exceeds isostatic-rebound predictions by ~100 m/Myr, implying a real rock-uplift component (mantle-flow tilting of the plateau) ([Darling 2011](https://digitalrepository.unm.edu/eps_etds/15), discussing Karlstrom 2008, Moucha 2008). The **Lees Ferry knickpoint** is interpreted as a transient set up by 6-Ma drainage integration, with incision waves diffusively bypassing it; above it, incision is slower (126 m/Myr at Bullfrog over 1.5 Ma) than below. Terraces record the climate-cycle modulation of discharge and sediment load on top of the tectonic signal.

---

## 7. Meandering

### 7.1 Why rivers bend

The accepted mechanism is a positive feedback between **helical secondary flow** and **bank erosion/deposition**: at a bend, centrifugal force piles superelevated water at the outer bank; the cross-channel pressure gradient drives near-bed flow *toward the inner bank, while surface flow goes outward* — a helical (corkscrew) cell. The near-bed flow toward the point bar sweeps bedload onto the inner bank (deposition, point bar) while high velocity at the outer bank erodes it (cutbank). Bend → bar/cutbank asymmetry → more curvature → stronger helical cell. Experiments show two ingredients are *necessary and sufficient* for self-sustaining meandering: bank strength exceeding bed strength (vegetated/cohesive banks) and fine-sediment deposition plugging chute channels behind point bars — constant discharge is NOT required ([Braudrick et al. 2009 PNAS](https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/)).

### 7.2 Wavelength scaling

Empirical bankfull relation from a 438-site composite: **$L_m = 8.36\, W^{1.05} \approx 10.2\, W$** (the fixed-exponent model; ERDC channel-design manual fig. 7.2, [erdc-library download](https://erdc-library.erdc.dren.mil/bitstreams/81b728f8-6e6d-4ef8-e053-411ac80adeb3/download)); classic texts give the 10–14×W band; the Braudrick experiment stabilized at ≈14 W ([Braudrick et al. 2009](https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/)). **Parent's λ ≈ 10–14 W verified; the parent's specific "11.2 W" is within band but I could not find that exact coefficient — unverified, use 10.2 (Soar/Thorne-type composite) or the band.** Radius of curvature at bends: ~2–3 W (progressively migrating Sacramento bends: R = 2.8 W; chute-cutoff-prone bends: 2.1 W) ([Micheli & Larsen 2010](https://onlinelibrary.wiley.com/doi/10.1002/rra.1360)).

### 7.3 Migration rates, cutoffs, oxbows

Natural single-bend migration: mostly **0.01–0.02 channel-widths/yr**, max ~0.18 W/yr ([Braudrick et al. 2009](https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/), compiling the literature). Sacramento River 1904–1997 (W≈250 m): average lateral change **5.5 ± 0.6 m/yr** (~0.02 W/yr); progressive migration 4.7 m/yr, chute cutoffs locally 22.1 m/yr; one cutoff every ~2.5 years per 160 km ([Micheli & Larsen 2010](https://onlinelibrary.wiley.com/doi/10.1002/rra.1360)). **Parent's "cm–m/yr scale by river size" verified**: small streams cm–dm/yr; large rivers like the Sacramento ~5 m/yr; catastrophic chute events faster. So in absolute terms migration spans ~0.01–20 m/yr with size.

**Neck vs chute cutoff:** neck cutoff = the migrating bend's two limbs meet (sinuosity gets extreme, R/W small); chute cutoff = an overbank flood excavates a straighter channel across the floodplain (often along an existing swale), which then captures the flow via upstream knickpoint migration through the chute. Sacramento geometry thresholds: chute cutoff at sinuosity ≈ 1.97, R = 2.1 W, entrance angle ≈ 111°, vs progressive bends at sinuosity 1.31, R = 2.8 W, 66° ([Micheli & Larsen 2010](https://onlinelibrary.wiley.com/doi/10.1002/rra.1360)). Post-cutoff, the abandoned loop plugs at both ends with fine sediment → **oxbow lake**; chute cutoffs reworked 20% of floodplain area despite only 5% of channel length. Timescale: an meander goes from birth to cutoff in ~10²–10⁴ yr depending on river size (Sacramento: ~5 bends in 93 yr on 160 km).

### 7.4 Planform classes: braided vs meandering vs straight vs anastomosing

The discriminator is **sediment supply vs transport capacity, plus bank strength**: braided when load is high relative to capacity and banks are weak (multiple thalwegs around bars, wide/shallow, steep-ish); meandering when banks resist (single thread, point bars); straight where neither perturbation dominates (rare in nature at sinuosity ~1); **anastomosing** = multiple *stable, vegetated, interconnected* channels separated by cohesive floodplain wetlands — low energy, fine sediment, aggradational (distinct from braiding's dynamic high-energy bar-hopping; the standard distinction per the alluvial-fans/rivers literature, e.g., [Blair & McPherson 1994](https://doi.org/10.1306/d4267dde-2b26-11d7-8648000102c1865d) context and river-classification literature — flagged: no single quantitative threshold source verified here).

---

## 8. Longitudinal profile and base level

### 8.1 Graded rivers and concavity

The **graded river** (Gilbert 1877; Mackin 1948): a profile where, at the prevailing discharge and load, slope is just sufficient to transport the supplied sediment — no net deposition or erosion; the profile is a *condition*, not a deposit. Because $Q$ grows downstream while sediment per unit flow generally doesn't keep pace, the required slope falls downstream → **concave-up** profile, consistent with the Section 1.3 derivation ($S \propto A^{-\theta}$, θ>0). Base level (sea level, lake level, resistant rock) is the floor of the whole system: rivers can never cut below it (locally) for long.

### 8.2 Knickpoints and Niagara Falls — the worked retreat example

Niagara Falls: ~52 m high, on the Lockport dolostone over softer shale — the caprock collapses block-by-block once the plunge pool undercuts the shale. Retreat history (Horseshoe Falls, from crest-line surveys): **1842–1905: 5.3 ft/yr ≈ 1.6 m/yr; 1842–1906 ≈ 1.28 m/yr; long-record 1670–1969 average ≈ 1.1 m/yr; declining 1.28 → 0.98 → 0.67 m/yr through the early 20th century** as hydroelectric diversion ramped up ([Gilbert 1907 USGS Bulletin 306](https://pubs.usgs.gov/bul/0306/report.pdf); [Tinkler et al. 1994](https://doi.org/10.1006/qres.1994.1050); NYSGA 1982 guide at [ottohmuller.com](https://ottohmuller.com/nysga2ge/Files/1982/NYSGA%201982%20B2%20-%20Glacial%20And%20Engineering%20Geology%20Aspects%20Of%20The%20Niagara%20Falls%20And%20Gorge.pdf)). Post-1950-Niagara-Treaty flow regulation (2832 m³/s tourist hours, 1416 off-peak, of a mean natural 5720 m³/s) cut retreat to **<0.3 m/yr, currently ~0.1 m/yr** ([SERC Carleton vignette](https://serc.carleton.edu/vignettes/collection/25474.html); [International Joint Commission](https://www.ijc.org/en/niagara-falls-moving)). Philbrick (1970) adds that crest *shape* modulates rate: notched crests erode up to 3× faster than arched crests, and the pool-bottom profile shows episodic "basin-and-high" retreat, with arch-stage rates up to 5.8 m/yr locally ([NYSGA 1982](https://ottohmuller.com/nysga2ge/Files/1982/NYSGA%201982%20B2%20-%20Glacial%20And%20Engineering%20Geology%20Aspects%20Of%20The%20Niagara%20Falls%20And%20Gorge.pdf)). American Falls: ~0.06–0.15 m/yr (Gilbert estimated <0.5 ft/yr, probably 0.2 ft/yr ≈ 0.06 m/yr) because talus armor protects its base. **Parent's "30 cm/yr to 1–3 m/yr historical, slowing" verified** (0.1–0.3 today; 1–2 m/yr natural historical; short pulses to ~6 m/yr). Total retreat ~11 km since ~12.4 ka deglaciation → long-run average ~0.9 m/yr.

### 8.3 Base-level change consequences

- **Base-level fall** → incision, knickpoint migration upstream, terrace staircases. Climatic (glacial-interglacial) cycles stack **stream terraces**: each incision pulse leaves the old floodplain perched; Grand Canyon and Colorado Plateau terraces record 100-kyr-scale modulation.
- **Base-level rise** → drowning: **rias** (drowned river valleys, e.g. SW England/Galicia), Chesapeake Bay — the river's graded profile is progressively buried from the mouth.
- **Superimposed/entrenched meanders**: meanders formed on a low-gradient plain, then uplift or base-level fall causes the river to incise while *retaining* its planform → incised meander gorge (Goosenecks of the San Juan: sinuosity extreme because incision outpaced lateral migration). (Flag: Goosenecks-specific rate figures not individually verified in this pass; mechanism standard.)
- **Lakes as local base level**: the Great Lakes trap Niagara's sediment, keeping the river sediment-starved and the falls unburied ([SERC vignette](https://serc.carleton.edu/vignettes/collection/25474.html)).

---

## 9. Deltas and depositional terrain

### 9.1 Why deltas differ: the process regime (Galloway triangle)

Deltas sit at the junction of three forcings — river (sediment supply prograding the shoreline), waves (alongshore diffusion smoothing it), tides (widening mouths, funnels) ([Galloway 1975]; [Wright & Coleman 1973](https://doi.org/10.1306/819a4274-16c5-11d7-8645000102c1865d) for the original 7-delta spectrum; modern multiscale confirmation in [Vulis et al. 2023](https://research-portal.uu.nl/ws/files/235410242/Geophysical_Research_Letters_-_2023_-_Vulis_-_River_Delta_Morphotypes_Emerge_From_Multiscale_Characterization_of_Shorelines.pdf)):

- **River-dominated (birdfoot, Mississippi):** low nearshore wave energy + flat offshore profile → long distributaries reach far out, mud-dominated, irregular shoreline. The modern Balize "birdfoot" branches at polyfurcation points marking old shorelines ([Chamberlain et al. 2018, Science Advances](https://www.science.org/doi/10.1126/sciadv.aar4740)).
- **Wave-dominated (arcuate/cuspate, Nile/São Francisco/Senegal):** waves rework the river's supply into smooth beach-ridge shorelines; symmetric if waves perpendicular, asymmetric/flying-spit if oblique.
- **Tide-dominated (funnel/estuarine, Fly/Ganges-Brahmaputra):** strong tidal range widens mouths, builds tidal flats/mangrove plains; concave shoreline intruding landward.
- Key physical control on whether the river can dominate at all: the *subaqueous* slope. Rivers build river-dominated shapes only on flat offshore profiles; steep shoreface → wave forms win regardless of river power ([Wright & Coleman 1973](https://doi.org/10.1306/819a4274-16c5-11d7-8645000102c1865d)).

### 9.2 Progradation rates (verified)

Mississippi Lafourche subdelta (late Holocene, river-dominated): **mouth-bar progradation 100–150 m/yr sustained for ~1 kyr**, building 6–8 km²/yr of new land — several times *below* modern human-enhanced loss (~45 km²/yr) ([Chamberlain et al. 2018](https://www.science.org/doi/10.1126/sciadv.aar4740)). Modern birdfoot: southern/western margins still prograding ~7–14 m/yr while eastern margins retreat up to ~58 m/yr under wave attack; net wetland change ~zero over 1990–2022 ([Yang et al. 2025](https://doi.org/10.1029/2024ef005003)). So game-realistic delta growth: **tens of m/yr typical, 100+ m/yr for big muddy rivers on shallow shelves, negative once sediment supply is cut.**

### 9.3 Alluvial fans

Form where an upland feeder channel loses confinement at a mountain front: flows decelerate, deposit, and repeatedly avulse → semiconical, plano-convex piedmont landform, planar-convex cross-section (inverse of a river's trough) ([Blair & McPherson 1994](https://doi.org/10.1306/d4267dde-2b26-11d7-8648000102c1865d)). Two endmember constructions:

- **Debris-flow dominated:** viscous slurry lobes, matrix-supported, boulder-rich, steep (typically >4–10°); common in tectonically active/semiarid fronts.
- **Waterlaid (sheetflood) dominated:** flash-flood sheetfloods deposit planar-couplet gravels; antidune standing-wave deposits (backsets); example: Anvil Spring fan, Death Valley, slopes 2.5–5° over 9.7 km radial length ([Blair 1999](https://doi.org/10.1046/j.1365-3091.1999.00259.x)).
- **Sieve deposits:** the third, long-disputed mode — coarse open-framework gravels deposited when bedload-laden water *infiltrates* into the permeable fan surface, dropping its entire gravel load; verified active in gravel-rich, matrix-poor alpine fans (sub-annual events triggered by >50 mm/24 h rain, >1000 m³ per event; fans built almost entirely of stacked sieve lobes) ([Novak et al. 2022](https://doi.org/10.1002/esp.5508); [Milana 2010](https://ri.conicet.gov.ar/handle/11336/101758)). For voxel terrain: sieve texture = clast-supported open-framework gravel, downward coarsening, no matrix.

### 9.4 Sediment yields per setting

The Milliman & Farnsworth style numbers, verified from primary sources:

| Setting | Sediment yield (t/km²/yr) | Source |
|---|---|---|
| Global average (all rivers) | ~150 | [Kao & Milliman 2008](https://doi.org/10.1086/590921) |
| Taiwan, 16 rivers mean | **9,500** (60× global) | [Kao & Milliman 2008](https://doi.org/10.1086/590921) |
| Taiwan, individual rivers | 500–71,000 (2+ orders of magnitude spread) | [Kao & Milliman 2008](https://doi.org/10.1086/590921) |
| Taiwan orogen denudation | 3–6 mm/yr across all timescales | [Dadson et al. 2003, Nature](https://www.nature.com/articles/nature02150) |
| Taiwan ECR suspended-sediment basins | 2.2–8.3 mm/yr | [Fuller et al. 2003](https://doi.org/10.1086/344665) |
| Liwu River, ¹⁰Be(met)/⁹Be | 8 to >30 mm/yr (highest cosmogenic rates ever) | [GFZ study](https://gfzpublic.gfz.de/rest/items/item_5008148_2/component/file_5008477/content) |
| Old cratons / lowlands | order 1–30 m/Myr (e.g. Brazil QF quartzites 0.8–5 m/Myr) | [Bezerra et al. 2018 EGU abstract](https://meetingorganizer.copernicus.org/EGU2018/EGU2018-19677.pdf) |

**Taiwan's is the verified extreme**: >75% of the long-term flux in <1% of the time (typhoons), ~⅓ reaching hyperpycnal concentrations ([Kao & Milliman 2008](https://doi.org/10.1086/590921)). The sediment cascade: hillslope production → landslides (stochastic, earthquake+typhoon triggered) → rivers → coastal ocean, with landslide dam-and-flush dynamics modulating delivery (the Nature "lifespan" paper: landslide-river feedbacks explain why *inactive* ranges erode slowly — dams armor beds; [Egholm et al. 2013, Nature](https://preview-www.nature.com/articles/nature12218)).

---

## 10. Landscape memory and relict terrain

### 10.1 Erasing a mountain belt

Denudation after uplift cessation decays mean elevation roughly exponentially, e-folding **~50 Myr** (isostasy-adjusted ~45–70 Myr) from the global sediment-yield–elevation correlation ([Pelletier 2004](https://doi.org/10.1029/2004gl020052), citing Ahnert 1970). Yet Paleozoic orogens (Appalachians ~300 Ma, Urals) still stand >1 km. Resolutions of the paradox in the literature (all three supported): (i) **resistant bedrock + broad piedmont** lowers effective K and sets a high local base level (Pelletier 2004); (ii) **landslide-river feedbacks**: in inactive ranges, large landslides dam rivers, cover beds, and cut tool supply — erosion self-throttles ([Egholm et al. 2013](https://preview-www.nature.com/articles/nature12218)); (iii) coupled tectonic models: orogen decay has two phases, with long-wavelength Phase-2 decay lasting "tens to several hundreds of Myr" controlled by erosional efficiency, isostasy, and width — models retain ~1.5 km maximum topography after 150 Myr of decay for wide, efficient-erosion-limited cases ([GFZ/Bedroi study](https://gfzpublic.gfz.de/rest/items/item_5008148_2/component/file_5008477/content) — flag: exact paper identity partially obscured in source snippet; the two-phase decay framework is corroborated by the same PDF). **Parent's "~1e7–1e8 yr, and what survives" verified**: 10–50 Myr as the rule, 10⁸ yr persistence possible for quartzite-cored or resistant belts; measured low-relief ancient landscapes erode at ~1–30 m/Myr (Brazil QF, tectonically stable 500 Myr: quartzite catchments 0.8–5 m/Myr, gneiss up to ~25 m/Myr; [Bezerra et al. 2018](https://meetingorganizer.copernicus.org/EGU2018/EGU2018-19677.pdf)).

**What survives:** peneplains (Davis's graded-to-sea-level endmember), etchplains (deep-weathering-stripped surfaces), inselbergs (resistant bedrock monoliths on plains), and relict low-relief surfaces elevated wholesale — the eastern Tibetan Plateau's relict landscape preserves a low-relief paleosurface now at 3–4 km, dissected only where 9–13 Ma river incision began; knickpoints at the relict/active boundary still propagate, response time >10 Ma ([Clark et al. 2006](https://doi.org/10.1029/2005jf000294)). For a game: ancient stable continents should carry *flat high surfaces with isolated resistant knobs*, not young rugged mountains.

### 10.2 Drainage-divide migration and stream capture

**Gilbert (1877)'s "law of unequal declivities"** — the ancestor of every modern metric: an asymmetric divide implies unequal erosion rates, and the divide migrates toward the gentler/erosion-poorer side. The modern competition metric is **χ** (chi), the steady-state-elevation integral: with reference concavity $\theta_{ref}$,

$$\chi = \int_0^x \left(\frac{A_0}{A(x')}\right)^{\theta_{ref}} dx'$$

At geometric equilibrium, χ must be equal across divides; **χ-anomalies** (high-χ side vs low-χ side) predict divide migration from low-χ toward high-χ channels ([Willett et al. 2014, Science](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf)). Numerical models confirm: divides move until χ equalizes, and the time to network (not profile) equilibrium is many times the longest river's response time. Caveats: χ assumes uniform uplift/erodibility/climate; where those vary, χ-anomalies can be false positives, and **Gilbert metrics** (cross-divide differences in channel-head elevation at reference area, mean headwater gradient, local relief) can be more reliable indicators of *current* motion — use both; if either says unstable, it is ([Forte & Whipple 2018](https://www.sciencedirect.com/science/article/abs/pii/S0012821X18302292); [Ye et al. 2024](https://doi.org/10.1002/esp.5892) shows hillslopes and channels each absorb part of the cross-divide signal). **Stream capture** is the discrete topological version: headward erosion or divide migration reroutes a drainage; diagnostics are windgaps, beheaded valleys, underfit streams, barbed tributaries, and captured biota (Apalachicola→Savannah capture visible in χ maps; [Willett et al. 2014](http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf); operational guidance in the RBG technical note, [rbg.emnuvens.com.br](https://rbg.emnuvens.com.br/rbg/article/download/2797/386387055/386400679)). **Enough to implement divide migration: compute χ per node (single ordered traversal, choosing θ_ref ~0.45–0.5), find adjacent channel heads across each divide, move the divide node-by-node toward higher χ, and let captured area feed back through the SPIM.**

### 10.3 Hypsometric integral

The hypsometric curve plots relative area above a given relative elevation; the **hypsometric integral (HI)** is the area under it (practical estimator: $HI \approx (\bar{H} - H_{min})/(H_{max} - H_{min})$, Pike & Wilson). Interpretation (Strahler 1952): convex/high HI ≈ youthful disequilibrium; S-shaped/HI≈0.5 ≈ mature/equilibrium; concave/low ≈ old dissected. Typical values: **0.15–0.85 range, clustering 0.4–0.6** ([opengeology.in summary](https://opengeology.in/hypsometric-curve-and-integral/)); Taiwan steady-state basins: **HI → 0.5 exactly at steady state, S-curves, normally distributed elevations**, 0.5 "critical", with response times 0.5–2 Myr by Strahler order; non-steady basins show HI scale-dependence (small basins higher HI) ([Chen et al. 2012](https://www.sciencedirect.com/science/article/abs/pii/S0169555X1200205X)). Caveat from the modern literature: HI measures the tectonics-vs-erosion balance as much as "age" (Weissel et al. 1994, per [Bhattacharjee 2022](https://doi.org/10.56975/ijcsp.v12i2.303932)) — an uplifting young range can have moderate HI. **Parent's "young vs old values" verified with the 0.6/0.3 thresholds (youth ≥0.6, mature 0.35–0.6, old ≤0.35 per the Loess Plateau application, [Duan et al. 2022](https://doi.org/10.3389/feart.2022.827836)).**

---

## 11. Practical simulation synthesis — the opinionated pipeline

**The blunt call:** do your hydrology at *terrain-generation time*, not run time. A hydrologically-correct heightmap is a one-off O(n log n)–O(n) computation per world tile; trying to fake rivers on top of noise afterward is where every procedural world fails the "where does the water go" sniff test.

**The minimal pipeline (each stage annotated with what breaks if skipped):**

1. **Base field:** fBm/ridged noise + regional warp, OR a tectonic-style uplift field. *Skip hydrology entirely* → no coherent valleys, wrong slope-area statistics (the failure modes cataloged in the parent question bank Q69–70).
2. **Priority-Flood depression filling:** flood the DEM inward from edges via a priority queue, raising pit cells to their outlet level — every cell then drains ([Barnes, Lehman & Mulla 2014](https://doi.org/10.1016/j.cageo.2013.04.024)). O(n log n) floating-point, O(n) integer; the +epsilon variant resolves flats by adding infinitesimal gradients so flow directions are defined. *Skip:* flow routing dead-ends in pits, rivers terminate in the middle of continents, drainage areas are garbage.
   - Alternative for LEM use: the Cordonnier–Bovy–Braun basin-graph method computes flow paths *through* depressions with explicit fill-vs-carve choice, O(n), best-in-class for repeated erosion stepping ([Cordonnier et al. 2019](https://doi.org/10.5194/esurf-7-549-2019)).
3. **Flow routing D8 vs D-infinity:** D8 sends all flow to the steepest of 8 neighbors — grid-aligned, cheap, river-like in aggregate but with visible diagonal artifacts; D∞ (Tarboton) partitions flow between the two steepest downslope neighbors — smoother accumulation fields, better on smooth hillslopes. *Skip or use naive sequential accumulation:* accumulation is the expensive part; do it as a single pass over cells sorted by decreasing filled elevation (the same stack FastScape uses).
4. **SPIM-like carving at generation time:** iterate (fill → route → accumulate → erode) with the Braun–Willett implicit solver: order nodes by decreasing elevation, solve each node's implicit update in one sweep — stable at large timesteps, O(n) per step ([Braun & Willett 2013](https://doi.org/10.1016/j.geomorph.2012.10.008)). This is exactly what LandLab ships ([LandLab docs](https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html)). Add uplift U and run to equilibrium or to a chosen "age."
5. **Hillslope diffusion pass:** linear (or Roering-nonlinear for steep terrain) diffusion smooths ridges and sets valley spacing via $L_c = (D/K)^{1/(2m+2)}$ ([Perron et al. 2009](https://doi.org/10.1038/nature08174)). *Skip:* ridges are knife-sharp noise, slope distributions wrong, no characteristic valley wavelength — the single most recognizable "procedural terrain" tell.
6. **Channel-head threshold:** only erode where $A > A_c$ (typically 0.1–5 km² fluvial/debris-flow transition; [Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035) citing Wobus 2006, Stock & Dietrich 2003). $A_c$ sets drainage density (Section 3.3).
7. **Post-passes:** meander the large-river centerlines (λ = 10–14 W, migration as a time-jittered displacement), terrace carving for climate history, delta shape selection by the process regime (river/wave/tide) at each coastline sink, fan deposition at mountain fronts with talus-angle repose.

**Parameter table (literature-grounded):**

| Parameter | Value | Source |
|---|---|---|
| $m$, $n$ | start $m=0.5$, $n=1$; accept $n$ up to ~2.5 for threshold realism | [Harel et al. 2016](https://doi.org/10.1016/j.geomorph.2016.05.035) |
| $\theta = m/n$ | 0.45–0.5 | Harel 2016; Willett 2014 |
| $K$ | 10⁻⁶–10⁻⁵ m^0.5/yr equivalents; 9 orders of magnitude across lithologies | Harel 2016; Goren 2014 |
| $D$ (hillslope diffusivity) | ~10⁻³–10⁻¹ m²/yr (order 0.01) | Roering 1999 (order) |
| $A_c$ (channel head) | 0.1–5 km² | Harel 2016 (citing Wobus) |
| $W$ bankfull | 3.0–4.0 × Q^0.5 (m, m³/s), ±50% | NRCS 654; Bray; Nixon; Hey & Thorne |
| Meander λ | 10–14 W | ERDC; Braudrick 2009 |
| Talus repose angle | 33–37° dry (cohesive slopes to ~49°) | Carson 1977; Francou 1990; Schumm 1956 |
| $\tau_c$ gravel | 10–100 Pa (10–100 mm grains) | Lamb et al. 2015 |
| $L_c$ valley spacing | $(D/K)^{1/(2m+2)}$ × O(1) constant | Perron 2009 |
| Knickpoint celerity | $K A^m S^{n-1}$; 0.001–0.1 m/yr typical | Loget; Whittaker 2011 |
| HI sanity | 0.4–0.6 for mature; <0.3 old; >0.6 young | Strahler; Chen 2012 |

**What breaks if you skip each stage (summary table):** no fill → dead-end rivers; no accumulation → no area-dependent erosion, uniform gullies; no SPIM → no concave profiles, no drainage-area-organized networks; no diffusion → wrong valley spacing and ridge sharpness; no threshold → rills on every pixel, over-dissection; no meandering post-pass → rivers are noise-creek polylines; no process-regime deltas → every coast gets the same fan.

---

## Provenance

**Verified (multi-source or exact computation):** all three unit-stream-power anchors (exact arithmetic); $\theta = 0.51 \pm 0.14$ global median; $n = 2.43$ global median; $W \propto Q^{0.512}$; Bray 3.83 (with the 2.3–4.3 regional band); Horton $R_B$ 3.8, $R_L$ 2.3, $R_A$ 4.8 (Carpathians, matching global norms); Hack $h \approx 0.5$–0.6 (arid/humid split); drainage density 2–12 normal, ~100+ badlands (Schumm Perth Amboy 110.8); network fractal dimension 1.6–1.8; talus 33–37° with the Chandler 39–40° shearing-resistance nuance; Roering nonlinear law and $L_c$ valley-spacing; Braun–Willett FastScape O(n) solver; response times 50 kyr (hillslopes) – 1–50 Myr (belts); knickpoint celerity law and 0.001–0.1 m/yr rates; Grand Canyon 150–500 m/Myr integrated, 5–6 Ma young segments, old-segment controversy documented with both sides; meander λ 10–14 W, migration 0.01–0.02 W/yr typical; Sacramento cutoff statistics; Niagara 0.1–1.6 m/yr declining; Mississippi progradation 100–150 m/yr; Galloway delta regime; Taiwan 9,500 t/km²/yr, 3–6 mm/yr; peneplain persistence 10–50 Myr e-folding with 10⁸-yr exceptions; χ divide analysis and Gilbert metrics; HI 0.4–0.6 mature.

**Unverified / flagged:** parent's exact "11.2 W" meander coefficient (within band, no source found); Goosenecks-specific incision figures; anastomosing quantitative thresholds (described qualitatively only); specific $D$ values cited only at order-of-magnitude via secondary references; the GFZ two-phase orogen-decay paper's exact identity (snippet-sourced); the "1700 m/Myr" Grand Canyon upper anchor (see disagreement #1); delta Hack's-law claim (secondary Wikipedia source).

**Anchor disagreements (parent vs sources):**
1. **Grand Canyon 1700 m/Myr:** not supported by any integrated main-stem rate found; verified integrated range is 150–500 m/Myr with short-term local excursions to ~500 and speleothem rates to ~411. Parent upper bound appears ~3× too high.
2. **K units "m^0.2":** wrong for the stated $m$-family; with $m=0.5$ (the standard), units are m$^{0.5}$/yr. The 10⁻⁶–10⁻⁵ magnitude is fine once fixed to m=0.5, n=1, area-in-m².
3. **W = 3.83√Q:** correct as a citation (Bray, Alberta gravel rivers) but presented as if universal — the coefficient spans 2.3–4.3 across regions (Nixon UK 2.99; Hey & Thorne types 2.3–4.33; Emmett 2.8), and site scatter is ±50%.
4. **Meander 11.2 W:** within the verified 10–14 band but no source found for that exact value; the 438-site composite fixed-exponent fit is 10.2.
5. All other anchors (unit stream powers ×3, θ, τc, Horton ratios, fractal dimension, drainage-density span, repose angle, response times, knickpoint celerity, Niagara, HI) verified or exact.

---

## Sources

1. Attal 2013, "The stream power river incision model: evidence, theory and beyond" — https://onlinelibrary.wiley.com/doi/10.1002/esp.3462
2. Harel, Mudd & Attal 2016, global stream power law analysis — https://doi.org/10.1016/j.geomorph.2016.05.035
3. Whipple & Tucker 1999, dynamics of the stream power model — https://doi.org/10.1029/1999JB900120
4. Smith & Fox 2024, concavity index constraints — https://eprints.bbk.ac.uk/id/eprint/54285/1/Smith_and_Fox_2024_Concavity.pdf
5. Mudd et al. 2018, "How concave are river channels?" — https://esurf.copernicus.org/articles/6/505/2018/esurf-6-505-2018.html
6. Goren et al. 2014, constraining the SPL with FastScape + inversion — https://esurf.copernicus.org/articles/2/155/2014/esurf-2-155-2014.pdf
7. Pelletier 2021, controls on hydraulic geometry (Dunne–Jerolmack dataset) — https://doi.org/10.5194/esurf-9-379-2021
8. Parker et al. 2007, quasi-universal bankfull geometry — https://doi.org/10.1029/2006jf000549
9. NRCS NEH Part 654 Ch. 9, alluvial channel design (Bray/Nixon/Hey tables) — https://irrigationtoolbox.com/NEH/Part%20654/CHAPTERS/Chapter-09.pdf
10. Julien & Wargadalam 1995, alluvial channel geometry — https://doi.org/10.1061/(asce)0733-9429(1995)121:4(312)
11. Lamb et al. 2015, fluvial bedrock erosion mechanics — https://lamb.caltech.edu/documents/19653/Lamb_etal_Geomorphology_2015.pdf
12. Sklar & Dietrich 2004, saltation-abrasion model — https://doi.org/10.1029/2003wr002496
13. Turowski, Lague & Hovius 2007, exponential cover effect — https://doi.org/10.1029/2006jf000697
14. Beer et al. 2015, bedload transport controls bedrock erosion (Erlenbach) — https://esurf.copernicus.org/articles/3/291/2015/esurf-3-291-2015.pdf
15. Aubert et al. 2016, bedrock incision by bedload (DNS) — https://doi.org/10.5194/esurf-4-327-2016
16. Johnson & Whipple 2010, experimental bedrock incision controls — https://doi.org/10.1029/2009jf001335
17. Snyder et al. 2003, stochastic floods and erosion thresholds — https://doi.org/10.1029/2001jb001655
18. Horton 1945, erosional development of streams — https://pdfs.semanticscholar.org/39c3/9bbea565f8f963309e65506d7756f6571c18.pdf
19. Bryndal 2015, Horton's and Schumm's laws in Carpathians — https://doi.org/10.1515/quageo-2015-0008
20. Rigon et al. 1996, On Hack's law — https://doi.org/10.1029/96wr02397
21. Seybold et al. 2018, shapes of river networks (aridity) — https://royalsocietypublishing.org/doi/10.1098/rspa.2018.0081
22. Wikipedia, Hack's law (delta Hack's-law claim, flagged) — https://en.wikipedia.org/wiki/Hack%27s_law
23. Collins & Bras 2010, drainage density and climate — https://doi.org/10.1029/2009wr008615
24. NetMap, drainage density topic page — https://www.netmaptools.org/Pages/NetMapHelp/drainage_density.htm
25. La Barbera & Rosso 1989, fractal dimension of stream networks — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/WR025i004p00735
26. Beer & Borgas 1993, Horton's laws and fractal nature of streams — https://doi.org/10.1029/92wr02731
27. Kovchegov & Zaliapin 2020, random self-similar trees / Horton laws — https://zaliapin.github.io/pubs/KZ_PS2020.pdf
28. Kovchegov, Zaliapin & Foufoula-Georgiou 2022, critical Tokunaga model — https://doi.org/10.1103/physreve.105.014301
29. Gupta & Mesa 2014, Horton laws for hydraulic-geometric variables — https://doi.org/10.5194/npgd-1-705-2014
30. Zaliapin et al. 2013, are American rivers Tokunaga self-similar — https://efi.eng.uci.edu/papers/efg_128.pdf
31. Roering, Kirchner & Dietrich 1999, nonlinear diffusive sediment transport — https://doi.org/10.1029/1998wr900090
32. Heimsath, Furbish & Dietrich 2005, depth-dependent transport — https://doi.org/10.1130/g21868.1
33. Roering et al. 2001a, hillslope evolution experiment (Geology) — https://doi.org/10.1130/0091-7613(2001)029
34. Roering et al. 2001b, nonlinear transport steady state & timescales (JGR) — https://doi.org/10.1029/2001jb000323
35. Carson 1977, angles of repose and talus slopes — https://doi.org/10.1002/esp.3290020408
36. Chandler 1973, inclination of talus — https://doi.org/10.1086/627804
37. Francou & Manté 1990, alpine talus profile segmentation — https://doi.org/10.1002/ppp.3430010107
38. Frontiers 2022, angle of repose in loess (Shields 33° context) — https://www.frontiersin.org/journals/earth-science/articles/10.3389/feart.2021.777467/full
39. Perron, Kirchner & Dietrich 2009, evenly spaced ridges and valleys — https://doi.org/10.1038/nature08174
40. Perron et al. 2008, controls on spacing of first-order valleys — https://doi.org/10.1029/2007jf000977
41. Schumm 1956, Perth Amboy badlands — https://pdodds.w3.uvm.edu/research/papers/others/1956/schumm1956a.pdf
42. Howard 1997, badland morphology and evolution — http://geomorphology.sese.asu.edu/Papers/Howard_ESPL_97.pdf
43. Braun & Willett 2013, O(n) implicit stream power solver — https://doi.org/10.1016/j.geomorph.2012.10.008
44. FastScapeLib documentation (flexure, SPL+transport) — https://fastscape.org/fastscapelib-fortran/
45. LandLab FastScape stream power component — https://landlab.csdms.io/generated/api/landlab.components.stream_power.fastscape_stream_power.html
46. Willett et al. 1999/2014, uplift, shortening, steady-state topography — https://ajsonline.org/article/88260-uplift-shortening-and-steady-state-topography-in-active-mountain-belts.pdf
47. Whipple 2004, bedrock rivers and geomorphology of active orogens — https://www.eoas.ubc.ca/~mjelline/453website/eosc453/E_prints/newfer06/2004whippleAREPS.pdf
48. Loget et al., wave train model for knickpoint migration — https://archimer.ifremer.fr/doc/00000/11075/8056.pdf
49. Whittaker et al. 2011 (JGR), knickpoint retreat rates and response times — https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2011JF002157
50. Flowers & Farley 2012 / Science summary, ancient Grand Canyon — https://www.science.org/doi/10.1126/science.1229390
51. Karlstrom et al. 2014, Grand Canyon 5–6 Ma through palaeocanyons — http://geomorphology.sese.asu.edu/Papers/Karlstrom-2014-NatGeoscience.pdf
52. Fox et al. 2017, westernmost Grand Canyon incision — https://doi.org/10.1016/j.epsl.2017.06.049
53. Darling 2011, Colorado River incision rates (thesis) — https://digitalrepository.unm.edu/eps_etds/15
54. Polyak et al. 2008, Grand Canyon speleothem U-Pb ages — https://www.science.org/doi/10.1126/science.1151248
55. Braudrick et al. 2009, experimental meandering (PNAS) — https://pmc.ncbi.nlm.nih.gov/articles/PMC2761352/
56. Micheli & Larsen 2010, Sacramento cutoff dynamics — https://onlinelibrary.wiley.com/doi/10.1002/rra.1360
57. ERDC channel design manual (meander wavelength, W–Q) — https://erdc-library.erdc.dren.mil/bitstreams/81b728f8-6e6d-4ef8-e053-411ac80adeb3/download
58. Gilbert 1907, rate of recession of Niagara Falls (USGS Bull. 306) — https://pubs.usgs.gov/bul/0306/report.pdf
59. SERC vignette, deglaciation and Niagara knickpoint — https://serc.carleton.edu/vignettes/collection/25474.html
60. IJC, Niagara Falls is moving — https://www.ijc.org/en/niagara-falls-moving
61. NYSGA 1982 guide, glacial/engineering geology of Niagara — https://ottohmuller.com/nysga2ge/Files/1982/NYSGA%201982%20B2%20-%20Glacial%20And%20Engineering%20Geology%20Aspects%20Of%20The%20Niagara%20Falls%20And%20Gorge.pdf
62. Wright & Coleman 1973, delta morphology vs wave/river regimes — https://doi.org/10.1306/819a4274-16c5-11d7-8645000102c1865d
63. Chamberlain et al. 2018, Mississippi delta anatomy (Science Advances) — https://www.science.org/doi/10.1126/sciadv.aar4740
64. Vulis et al. 2023, delta morphotypes from shoreline characterization — https://research-portal.uu.nl/ws/files/235410242/Geophysical_Research_Letters_-_2023_-_Vulis_-_River_Delta_Morphotypes_Emerge_From_Multiscale_Characterization_of_Shorelines.pdf
65. Yang et al. 2025, Mississippi birdfoot wetland gain/loss — https://doi.org/10.1029/2024ef005003
66. LibreTexts Coastal Dynamics, delta classification — https://geo.libretexts.org/Bookshelves/Oceanography/Coastal_Dynamics_(Bosboom_and_Stive)/02%3A_Large-scale_geographical_variation_of_coasts/2.07%3A_Process-based_classification/2.7.3%3A_Classification_of_deltas
67. Blair & McPherson 1994, alluvial fans vs rivers — https://doi.org/10.1306/d4267dde-2b26-11d7-8648000102c1865d
68. Blair 1999, waterlaid Anvil Spring fan — https://doi.org/10.1046/j.1365-3091.1999.00259.x
69. Novak et al. 2022, sieve deposits on an active fan — https://doi.org/10.1002/esp.5508
70. Milana 2010, the sieve lobe paradigm — https://ri.conicet.gov.ar/handle/11336/101758
71. Kao & Milliman 2008, water and sediment discharge from Taiwanese rivers — https://doi.org/10.1086/590921
72. Dadson et al. 2003, erosion–runoff–seismicity in Taiwan (Nature) — https://www.nature.com/articles/nature02150
73. Fuller et al. 2003, erosion rates for Taiwan mountain basins — https://doi.org/10.1086/344665
74. GFZ study, ¹⁰Be(met)/⁹Be upper limit of denudation, Liwu — https://gfzpublic.gfz.de/rest/items/item_5008148_2/component/file_5008477/content
75. Egholm et al. 2013, lifespan of mountain ranges (Nature) — https://preview-www.nature.com/articles/nature12218
76. Pelletier 2004, piedmont deposition and mountain-belt denudation timescale — https://doi.org/10.1029/2004gl020052
77. Bezerra et al. 2018, persistence of topography in ancient belts (EGU abstract) — https://meetingorganizer.copernicus.org/EGU2018/EGU2018-19677.pdf
78. Clark et al. 2006, relict landscape of eastern Tibet — https://doi.org/10.1029/2005jf000294
79. Willett et al. 2014, dynamic reorganization of river basins (Science) — http://geomorphology.sese.asu.edu/Papers/Willett_etal_Drainage_Dynamics_Science-2014.pdf
80. Forte & Whipple 2018, divide stability criteria and tools — https://www.sciencedirect.com/science/article/abs/pii/S0012821X18302292
81. Ye et al. 2024, cross-divide channel-head elevation controls divide migration — https://doi.org/10.1002/esp.5892
82. Oliveira et al. (RBG technical note), inferring divide migration and capture — https://rbg.emnuvens.com.br/rbg/article/download/2797/386387055/386400679
83. Chen et al. 2012, scale independence of basin hypsometry, steady state — https://www.sciencedirect.com/science/article/abs/pii/S0169555X1200205X
84. opengeology.in, hypsometric curve and integral — https://opengeology.in/hypsometric-curve-and-integral/
85. Duan et al. 2022, hypsometric integral of Loess Plateau basins — https://doi.org/10.3389/feart.2022.827836
86. Bhattacharjee 2022, hypsometry operator evaluation — https://doi.org/10.56975/ijcsp.v12i2.303932
87. Barnes, Lehman & Mulla 2014, Priority-Flood — https://doi.org/10.1016/j.cageo.2013.04.024
88. Barnes 2013 depression reference implementation — https://github.com/r-barnes/Barnes2013-Depressions
89. Cordonnier, Bovy & Braun 2019, linear-complexity flow routing in depressions — https://doi.org/10.5194/esurf-7-549-2019

---

## Appendix A — Forty further questions

1. **How should SPIM erodibility K vary spatially for a game biome map?** Map lithology bands to 2–4 orders of magnitude; climate scales it further (Harel 2016 shows insensitivity of θ but strong K variation). Disposition: implementable from Section 1.2's ranges.
2. **What timestep is safe for the Braun–Willett implicit SPIM?** Formally stable at any dt, but transient accuracy degrades (LandLab docs, citing Braun & Willett App. B) — use dt ≤ ~0.1×(dx/celerity) for faithful transients; larger for equilibrium-only runs.
3. **D8 vs D∞ for game rivers — which looks better?** D8 for the carving step (crisper channels, matches SPIM derivation), D∞ if you need smooth accumulation for moisture/biome maps. Disposition: engineering judgment from Section 11.
4. **How do you prevent priority-filling from drowning your map in lakes?** Use Priority-Flood+epsilon (fills to just-draining) or the carving variant; reserve real lakes for explicitly-marked closed basins (Barnes Alg. 3/4).
5. **What's the cheapest way to get Hack's-law-correct basins?** Don't enforce it directly — it emerges from SPIM evolution + the A_c threshold; check it as a validation metric (h≈0.5–0.6 emerges; Seybold 2018).
6. **Can you run SPIM on a heightmap at voxel resolution (7.8 mm voxels)?** No — run it on a coarse macro-grid (10–100 m cells), then displace/detail downward with noise constrained not to invert drainage. Disposition: engineering call.
7. **How do terraces form in an LEM?** Climate-cycle discharge/sediment-load oscillation on top of steady incision leaves perched floodplains; needs a transport-limited or cover-effect model, not pure detachment SPIM. Disposition: literature-consistent (Section 8.3), not implemented in minimal pipeline.
8. **What sets drainage density in your generator?** The channel-head threshold A_c; D_d ~ A_c^{-1/2} (Collins & Bras 2010). Pick A_c per biome from Section 3.3's climate story.
9. **How do you render an oxbow?** Detect cutoff events in the meander simulation, keep the abandoned loop as a water body, shrink it ~exponentially (fine-sediment plug + evaporation). Disposition: standard, rates from Section 7.3.
10. **What is the sinuosity distribution of real rivers?** Sacramento bends average ~1.4, cutoff-prone ~2.0 (Micheli & Larsen); full distribution unverified — use 1.2–2.2 as the working band.
11. **Why do deltas avulse?** Superelevation of the channel belt above the floodplain until a flood finds a steeper path; avulsion sets subdelta lifetimes (~1 kyr for Lafourche; Chamberlain 2018).
12. **How would you simulate delta shape selection?** Classify each coastal sink by (river sediment flux, nearshore slope, wave climate, tidal range) and pick a morphotype template (river/wave/tide; Wright & Coleman; Vulis 2023).
13. **What grain-size story does a sieve deposit carry into voxels?** Open-framework clast-supported gravel, downward coarsening, <2% mud (Novak 2022) — a distinctive material pass for fan fronts.
14. **How fast do alluvial fans aggrade?** Sieve events: >1000 m³ per >50 mm/24h storm, sub-annual frequency where active (Novak 2022); fan-scale long-term rates unverified — use event-based, not average-rate, generation.
15. **Does landscape memory matter at game timescales?** Only if your world has history: relict surfaces, windgaps, and captures are the visible memory (Sections 10.1–10.2); a "young" world can skip them.
16. **How do you implement divide migration?** Compute χ per node with θ_ref≈0.45–0.5, find cross-divide χ contrasts, nudge divide cells toward higher χ, re-route (Section 10.2 recipe; Willett 2014).
17. **When do χ-maps lie?** Non-uniform uplift, erodibility, or climate — cross-check with Gilbert metrics (Forte & Whipple 2018); if either flags instability, believe it.
18. **What's the game-facing test for "hydrologically correct"?** Every cell drains to a coast or marked sink; slope–area plots in log-log show θ≈0.45–0.5 above A_c; valley spacing matches L_c; Hack exponent 0.5–0.6.
19. **How many SPIM iterations to reach steady state?** Scales as (relief)/(U·steps); with the implicit solver, tens to hundreds of macro-steps — bounded by your patience, not stability (Braun & Willett 2013).
20. **Should uplift be uniform?** No — a tilted or gradient uplift field produces asymmetric ranges and migrating divides (Willett 2014 model setup), which is exactly the interesting terrain.
21. **How do you get glacial U-valleys in the same pipeline?** Out of scope here (Agent 3's domain); simplest hack is a curvature-dependent extra erosion term above the snowline. Disposition: deferred.
22. **What does n>1 do to your terrain?** Slower response to small perturbations, faster to large; produces taller, steeper transient relief; Harel 2016 says n≈2.4 is closer to reality than 1 — but n=1 is the cheap, well-behaved default.
23. **How does vegetation enter the erosion story?** Through K (erodibility), bank strength (meandering necessity, Braudrick 2009), and drainage density (the arid/humid non-monotonicity; Collins & Bras 2010).
24. **How do you validate against Earth statistics?** Compare slope distributions, hypsometric integrals (target 0.4–0.6), drainage density, Hack exponent, and slope–area concavity (Section 11 checks; parent Q79).
25. **Can knickpoints be player-triggered events?** Yes — a base-level drop (dam removal, earthquake, sea-level fall) injects a wave at celerity KA^mS^{n-1}; computing that per river is cheap (Section 5.3).
26. **How do you keep rivers from being grid-aligned?** D8 gives 45°-multiples; either route on the macro-grid then smooth/meander the centerline polyline, or use D∞/TIN (CHILD-style) for the routing layer.
27. **What's the failure mode of thermal-erosion-only passes?** They enforce a repose angle but produce no concave profiles and no area organization — ridges look right, valleys look wrong (Section 11 "skip SPIM" row).
28. **How do you pick the flood magnitude for an effective-erosion model?** Use the stochastic-threshold insight: erosion is dominated by rare large floods (Snyder 2003; Harel 2016 on n>1); a single "bankfull" rate underestimates and linearizes.
29. **How would you age a terrain after generation?** Run the same SPIM forward without uplift — relief decays ~exponentially with ~50 Myr e-folding (Pelletier 2004); or stamp relict surfaces and skips per Section 10.1.
30. **How do entrenched vs free meanders differ in the sim?** Free meanders migrate laterally; entrenched ones (Goosenecks) incise vertically with frozen planform — gate the migration term on incision rate vs lateral-erosion capacity. Disposition: mechanism standard, specific thresholds unverified.
31. **What controls where a river braids?** High sediment supply relative to capacity + weak banks (Section 7.4); in-game, check bed material and bank cohesion from biome/soil maps.
32. **How do you make a delta prograde visibly in gameplay time?** Real rates are 10–150 m/yr (Section 9.2); gameplay needs 10–100× that or a time-lapse mechanic — a deliberate, documented fudge.
33. **What is the cheapest flexure?** A spectral 2D FFT solution of the biharmonic plate equation (FastScape's own; docs give E=10¹¹ Pa, ν=0.2) — worth it only for continent-scale tiles.
34. **How do you keep SPIM from sawtooth-instability on steep slopes?** The implicit ordered-sweep solver exists precisely for this (Braun & Willett 2013); explicit schemes need dt ∝ dx²/K.
35. **Do you need the cover effect in a game?** Only if sediment routing matters (deltas, fans, valley fills); otherwise K alone suffices — but a tools/cover toggle is one multiply per node (Section 1.6 forms).
36. **How does base level at a lake differ from the sea?** Lakes can fill or drain (near-instant base-level change); Messinian-style 1500 m falls produce canyon-and-terrace cascades (Loget; Section 5.3).
37. **What density of streams is "too many"?** Above ~10–15 km/km² in a vegetated biome you've set A_c too low or D too high; badlands can justify 50–100+ (Section 3.3).
38. **How do you place waterfalls?** At lithologic breaks and along migrating knickpoints; height set by caprock/plunge-pool mechanics (Niagara is the archetype, Section 8.2) — deterministic placement on K-field discontinuities.
39. **What's the honest ceiling for real-time fluvial realism?** Full macro-hydrology precomputed; run-time effects limited to water rendering, localized erosion decals, and event-driven knickpoints (parent Q80's answer, in brief).
40. **What single number best diagnoses a fake procedural landscape?** The slope–area concavity: fBm noise gives θ ≈ 0 (no area dependence); any real-or-simulated fluvial terrain shows θ ≈ 0.4–0.6 above the channel head (Sections 1.3–1.4). If θ≈0, no river ever flowed there.
