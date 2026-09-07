# The Mathematics of Sound as a Signal: Fourier Analysis, Sampling, Noise, and Synthesis

> **Provenance.** Drafted 2026-09-06 by a delegated research agent using Exa web search (`mcp__exa__web_search_exa` / `mcp__exa__web_fetch_exa`); 29 search queries across 12 batches against primary sources (AES e-library, ITU, CCRMA/Stanford online books — verified live on 2026-09-06 — original paper PDFs hosted at Stanford, UCSD, Rochester, and elsewhere). Every formula, constant, paper attribution, and date below was checked against at least one live web source; corrections to the commissioning brief are flagged inline (notably: Voss & Clarke 1975 appeared in *Nature*, not PNAS; the "16-bit ≈ 96 dB" figure is exactly 98.08 dB under the full-scale-sine convention). This section is written to feed the engine's procedural-audio work: every result here is either a formula a C++ audio module will implement verbatim, a constant it will embed, or a constraint that bounds what it can perceptually get away with.

**Summary.** Sound, once transduced, is a real-valued function of time. Because the acoustic wave equation and (approximately) the ear are linear and time-invariant at ordinary levels, the entire machinery of linear systems applies: sinusoids are the eigenfunctions, so Fourier analysis is not a convention but *the* natural coordinate system for sound. This section develops that machinery rigorously — Fourier series and transform with the convolution theorem and the time-frequency uncertainty bound, the sampling theorem with exact reconstruction, quantization noise with the full $6.02N + 1.76$ dB derivation, the statistics and generation algorithms of colored noise (Voss–McCartney, Kellet), filters and resonators culminating in the RBJ biquad cookbook, envelope and modulation mathematics including the complete Jacobi–Anger/Bessel treatment of Chowning FM, additive/subtractive synthesis with the exact Fourier series of the classic waveforms and the source–filter model of vowels, physical modeling synthesis (Karplus–Strong, digital waveguides, modal, banded waveguides, FDTD), the perceptual frequency and loudness scales an audio engine needs in code (mel, Bark, ERB, dBFS/LUFS/LU per ITU-R BS.1770), and the reverberation mathematics of Schroeder comb/allpass networks, feedback delay networks, and partitioned convolution.

---

## 2.1 Sound as a Signal — the Mathematical Model

### 2.1.1 Acoustic pressure as a function

Sound in air is a longitudinal pressure disturbance. The canonical mathematical model takes the instantaneous acoustic pressure deviation from ambient,

$$p(t) = p_{\text{atm}} + s(t),$$

and works with $s(t)$ — a real scalar function of time once we fix a listening point (the full field $s(\mathbf{x},t)$ is a function of space and time; the single-point signal is what a microphone, and one ear, sees). Human hearing spans roughly $20\ \mu\text{Pa}$ (threshold of hearing) to $20\ \text{Pa}$ (threshold of pain), a factor of $10^6$ in amplitude, which motivates the logarithmic sound-pressure level

$$L_p = 20\log_{10}\!\left(\frac{p_{\text{rms}}}{20\ \mu\text{Pa}}\right)\ \text{dB SPL},$$

a range of about 120 dB — a number that will recur when we size our quantization word length in §2.3.

### 2.1.2 Linearity and superposition

The small-amplitude acoustic wave equation is linear in $s$; at ordinary sound levels (roughly below $\sim$130–140 dB SPL, where local pressure variations are still a tiny fraction of atmospheric pressure) the propagation medium itself does not distort, so two sources playing simultaneously produce, to excellent approximation, the arithmetic sum of their individual pressure fields. The ear is *approximately* linear over its central operating range, but not exactly: the middle-ear reflex kicks in above roughly 80–90 dB SPL, and the cochlea's outer-hair-cell active process compresses dynamic range (a roughly compressive, power-law input–output curve rather than a linear one). Linearity breaks in three places the engine must respect: very high SPLs (nonlinear propagation, shock formation), the cochlear compression just mentioned, and phase — the ear is largely phase-deaf for steady tones but exquisitely sensitive to temporal fine structure below ~1.5 kHz. For synthesis purposes, the linear model is the correct first-order theory, and its failure modes are perceptual effects to be modeled separately (e.g., loudness compression) rather than corrections to the signal algebra.

### 2.1.3 Why sinusoids: eigenfunctions of LTI systems

A linear time-invariant (LTI) system is characterized by its impulse response $h(t)$; its action is convolution $y = x * h$. The single fact that makes Fourier analysis *the* tool of audio is:

**Claim.** If $x(t) = e^{i\omega t}$, then $y(t) = H(i\omega)\, e^{i\omega t}$, where $H(i\omega) = \int_{-\infty}^{\infty} h(\tau) e^{-i\omega \tau}\, d\tau$.

*Derivation (one line of substance).* $y(t) = \int h(\tau)\, x(t - \tau)\, d\tau = \int h(\tau) e^{i\omega(t-\tau)} d\tau = e^{i\omega t} \int h(\tau) e^{-i\omega \tau} d\tau$. ∎

Complex exponentials pass through an LTI system *unchanged in form* — only scaled and phase-shifted. No other family of signals has this property, so sinusoids are not a habit but the eigenbasis of every linear audio process: filters, rooms (in the linear regime), and (approximately) the ear's basilar membrane mechanics. This is why decomposing sound into sinusoids, doing arithmetic on the coefficients, and recombining, is not an approximation strategy but an exact change of coordinates.

**Phasor representation.** A real sinusoid $A\cos(\omega t + \varphi)$ is written as the real part of $A e^{i\varphi} e^{i\omega t}$; the complex number $A e^{i\varphi}$ is the *phasor*. Amplitude and phase become a single complex gain, and cascading LTI stages becomes multiplying phasors.

### 2.1.4 Amplitude, frequency, phase, RMS vs peak

For $x(t) = A\sin(\omega t)$:

- **Peak amplitude** $A$; **peak-to-peak** $2A$.
- **RMS**: derive it — $x^2(t) = A^2 \sin^2(\omega t) = \tfrac{A^2}{2}\left(1 - \cos(2\omega t)\right)$. The mean over one period kills the cosine term (its average is zero), so

$$x_{\text{rms}}^2 = \frac{A^2}{2} \quad\Longrightarrow\quad x_{\text{rms}} = \frac{A}{\sqrt{2}} \approx 0.707\,A.$$

That is the entire content of the famous $1/\sqrt{2}$: a sinusoid spends its time near its peaks, so its power ($\propto$ mean square) is half the square of its peak. Worked example: a sine at 0 dBFS (peak $=1$ in normalized digital units) has RMS $1/\sqrt2$, i.e. $-3.01$ dBFS — the same $-3.01$ that will reappear in the ITU loudness calibration (§2.9.4).

### 2.1.5 Beats and roughness

Sum two sines of nearby frequency: using $\sin a + \sin b = 2 \sin\!\big(\tfrac{a+b}{2}\big)\cos\!\big(\tfrac{a-b}{2}\big)$,

$$\sin(2\pi f_1 t) + \sin(2\pi f_2 t) = 2\cos\!\big(\pi (f_1 - f_2) t\big)\, \sin\!\big(\pi (f_1 + f_2) t\big).$$

The result is a tone at the average frequency $\tfrac{f_1+f_2}{2}$ whose amplitude envelope is $2|\cos(\pi \Delta f\, t)|$ with $\Delta f = |f_1 - f_2|$. The envelope's maxima recur every $1/\Delta f$ seconds, so the **beat frequency is $f_b = |f_1 - f_2|$** — e.g. 440 Hz and 443 Hz produce 3 beats per second. (Zeros of the envelope occur twice as often, at rate $2\Delta f$; the perceptual "throb" is at $\Delta f$.)

When $\Delta f$ exceeds roughly 10–20 Hz, the ear can no longer follow individual beats and the percept becomes *roughness*; as $\Delta f$ exceeds the critical bandwidth of the pair's center frequency (§2.9), the two tones resolve as separate pitches. This progression — beats → roughness → two clean tones — is the raw material of sensory dissonance curves (Plomp & Levelt 1965) and is why detuned unisons sound "fat" (deliberate beating) and minor seconds close together sound "sour" (roughness inside one critical band).

---

## 2.2 Fourier Analysis — the Full Treatment

### 2.2.1 Fourier series of periodic signals

A signal periodic with period $T$ ($f(t+T) = f(t)$, fundamental angular frequency $\omega_0 = 2\pi/T$) that satisfies the Dirichlet conditions (absolutely integrable over one period, finitely many maxima/minima and discontinuities per period) admits

$$f(t) = \sum_{n=-\infty}^{\infty} c_n\, e^{in\omega_0 t}, \qquad c_n = \frac{1}{T}\int_{0}^{T} f(t)\, e^{-in\omega_0 t}\, dt.$$

The coefficients are projections onto the orthogonal basis $\{e^{in\omega_0 t}\}$; the orthogonality integral $\int_0^T e^{i(n-m)\omega_0 t} dt = T\,\delta_{nm}$ is what makes the analysis formula fall out of the synthesis formula. For real $f$, $c_{-n} = \overline{c_n}$, so the spectrum is conjugate-symmetric and the signal is a sum of real sinusoids at $n f_0$ (the *harmonics*) with amplitude $2|c_n|$ and phase $\arg c_n$.

**Convergence caveats.** At a jump discontinuity, the series converges to the *midpoint* of the jump (Dirichlet's theorem), and — the famous part — the partial sums *overshoot* the jump by a fixed fraction that does **not** vanish as terms are added.

**Gibbs phenomenon.** For a jump of height $h$, the $N$-th partial sum overshoots by

$$h \cdot \left(\frac{1}{\pi}\int_0^{\pi}\frac{\sin t}{t}\,dt - \frac{1}{2}\right) = h \cdot 0.089489872236\ldots$$

— about **8.95%** of the jump on each side (so the partial sum's apparent jump is ≈17.9% too tall). The overshoot's *width* shrinks as $1/N$ (its energy vanishes — convergence in mean square is fine even where pointwise convergence fails), but its *height* is constant. The constant $\int_0^\pi \sin(t)/t\,dt = 1.85194\ldots$ is the Wilbraham–Gibbs constant ([Wikipedia: Gibbs phenomenon](https://en.wikipedia.org/wiki/Gibbs_phenomenon); [MIT 18.03 notes](https://ocw.mit.edu/courses/18-03sc-differential-equations-fall-2011/05cce833730ffd3c39f420a41ad82fd6_MIT18_03SCF11_s22_7text.pdf) derive the 1.089 figure for the unit square wave step by step).

**Why synthesized square waves "ring."** A naive square-wave oscillator is a *truncated* Fourier series — a brick-wall band-limit of an ideal square wave. Convolving the ideal square wave with a sinc kernel (the ideal lowpass) is exactly what Gibbs quantifies: pre-/post-oscillations near every corner at ±9% of the edge, decaying as $1/t$. Every band-limited square wave *must* do this; it is mathematics, not a bug. Practical synthesis removes the audible artifact either by synthesizing transitions as integrated band-limited steps (the BLEP family of algorithms — the overshoot is distributed into the step itself) or by simply not using naive rectangles.

### 2.2.2 The Fourier transform pair

For aperiodic, finite-energy signals the series becomes an integral:

$$\boxed{\;X(f) = \int_{-\infty}^{\infty} x(t)\, e^{-2\pi i f t}\, dt, \qquad x(t) = \int_{-\infty}^{\infty} X(f)\, e^{\,2\pi i f t}\, df\;}$$

(The $e^{-2\pi i f t}$ convention puts the $2\pi$ in the exponent and keeps both formulas unitary and symmetric; Julius O. Smith's CCRMA texts use it throughout — e.g. [*Mathematics of the DFT*](https://ccrma.stanford.edu/~jos/).)

**Properties used daily in audio:**

- **Linearity** — obvious and load-bearing: spectra of a mix are the mix of spectra.
- **Time shift ↔ phase factor**: $x(t - t_0) \leftrightarrow X(f)\, e^{-2\pi i f t_0}$. Delaying a signal rotates every spectral component's phase linearly in $f$; a pure delay does not change magnitude. (This is why inter-channel delay is inaudible as coloration but audible as localization — ITD.)
- **Convolution theorem** — *the* theorem of filtering:

$$\boxed{\;x * h \;\longleftrightarrow\; X \cdot H\;}$$

*Derivation.* Write the transform of the convolution and substitute variables:
$$\mathcal{F}\{x*h\}(f) = \int\!\!\int x(\tau)\, h(t-\tau)\, e^{-2\pi i f t} d\tau\, dt = \int x(\tau) \left[\int h(t-\tau) e^{-2\pi i f t} dt\right] d\tau.$$
Inner integral: substitute $u = t - \tau$: $\int h(u) e^{-2\pi i f (u + \tau)} du = e^{-2\pi i f \tau} H(f)$. Then
$$\int x(\tau)\, e^{-2\pi i f \tau} d\tau \cdot H(f) = X(f)\, H(f). \;∎$$
(Interchange of integrals is justified by absolute integrability of both signals.) Filtering — the single most common audio operation — is multiplication in the frequency domain; a cascade of filters multiplies transfer functions; a room's effect on a signal is (linear regime) the convolution of the source with the room's impulse response. Everything in §§2.5–2.10 rides on this theorem.
- **Parseval / Plancherel** — energy conservation between domains:

$$\int_{-\infty}^{\infty} |x(t)|^2\, dt = \int_{-\infty}^{\infty} |X(f)|^2\, df.$$

*Skeleton of proof:* expand $|x(t)|^2 = x(t)\overline{x(t)}$, substitute the inverse transform for one factor, interchange integrals, and recognize the inner integral as a delta function $\delta(f' - f)$ (the orthogonality of complex exponentials in the distributional limit); the outer integral is then $\int |X(f)|^2 df$. Practically: total energy is domain-independent, so one may compute loudness, spectral tilt, or noise power in whichever domain is convenient, and windowed spectra must be normalized so this still (approximately) holds.

**The uncertainty principle.** With durations defined as normalized second moments, $\Delta t$ of the signal's energy in time and $\Delta f$ of its energy in frequency obey

$$\Delta t \cdot \Delta f \;\ge\; \frac{1}{4\pi} \approx 0.0796,$$

with equality **only** for the Gaussian pulse (Gabor 1946, [*Theory of Communication*](https://jmft.dev/uncertainty-principle-and-spectrograms.html) restates the standard result). This is a property of the Fourier pair itself, not of any instrument. Practical audio version, in mainlobe terms: a window of duration $T$ has frequency resolution on the order of $1/T$ (rectangular mainlobe: $2/T$; Hann: $4/T$).

**Worked example (get this right).** *Can a 20 ms analysis window resolve two tones 25 Hz apart?* The bin spacing alone is $\Delta f = 1/T = 1/0.020 = 50\ \text{Hz} > 25\ \text{Hz}$ — already fatal at the coarsest level of counting bins. The mainlobe makes it worse: a rectangular window's mainlobe is $2/T = 100$ Hz wide, a Hann window's is $4/T = 200$ Hz. To *resolve* 25 Hz separation you need at minimum $T \ge 1/25 = 40$ ms (bin-spacing criterion, rectangular), and realistically $T \approx 4/25 = 160$ ms with a Hann window so the two mainlobes are cleanly separated (a practical design rule: choose $L \ge k_w f_s / \Delta f_{\min}$ with $k_w = 4$ for Hann; [practical STFT guide](https://yuhi-sa.github.io/en/posts/20260703_spectrogram_practice/1/)). Short window = good time localization, poor frequency resolution; long window = the reverse; no window wins both. This trade governs every spectrogram, every pitch detector, and every filterbank choice the audio engine will make.

### 2.2.3 DFT/FFT, windows, spectrograms

The **discrete Fourier transform** of a length-$N$ frame:

$$X[k] = \sum_{n=0}^{N-1} x[n]\, e^{-2\pi i k n / N}, \qquad k = 0, \ldots, N-1,$$

computable in $O(N\log N)$ by the FFT. Bin spacing is $\Delta f = f_s / N$ (e.g. $f_s = 48\,\text{kHz}$, $N = 4096$ → 11.72 Hz per bin). The DFT evaluates the DTFT of the windowed frame at $N$ equally spaced frequencies; a real sinusoid not exactly bin-centered smears energy across all bins through the window's transform — *spectral leakage* — which is why one multiplies the frame by a **window function** before the FFT. Verified window properties (cross-checked against [Smith's *Spectral Audio Signal Processing*, §Windows](https://dsprelated.com/freebooks/sasp/Spectrum_Analysis_Windows.html), the [VRU window table](https://vru.vibrationresearch.com/lesson/table-of-window-function-details/), and [Harris's classic tables as reproduced by Stable32](http://www.stable32.com/Properties%20of%20FFT%20Windows%20Used%20in%20Stable32.pdf)):

| Window | Highest sidelobe | Sidelobe roll-off | Mainlobe width (bins) | ENBW (bins) |
|---|---|---|---|---|
| Rectangular | **−13.3 dB** | −6 dB/oct | 2 | 1.00 |
| Hann | **−31.5 dB** | −18 dB/oct | 4 | 1.50 |
| Hamming | **−42.7 dB** (≈−43) | −6 dB/oct | 4 | 1.36 |
| Blackman | **−58 dB** (−58.1) | −18 dB/oct | 6 | 1.73 |

(The precise values −13.3239, −31.5565/−31.5, −43.7547/−42.7 depending on the exact Hamming parameter, −58.2336 differ by decimals across sources; the one-digit values above are what appear in every authoritative table.) The trade is one-for-one: every dB of sidelobe suppression is bought with mainlobe width, i.e. frequency resolution — the same uncertainty principle again, dressed as filter design.

The **spectrogram** is $|STFT|^2$: slide a window of length $L$ along the signal with hop $H$, FFT each frame, display magnitude-squared, typically in dB and with log-frequency warping. Short windows (e.g. 256 samples @ 48 kHz ≈ 5 ms) resolve onsets but smear pitch; long windows (4096+ samples) resolve harmonics but smear onsets. Standard practice: 50–75% overlap ($H = L/2$ to $L/4$) for display; COLA/NOLA conditions if the STFT must be invertible.

---

## 2.3 Sampling and Digitization

### 2.3.1 The Nyquist–Shannon sampling theorem

**Statement (precise).** Let $x(t)$ be a continuous-time signal whose Fourier transform $X(f)$ vanishes for all $|f| \ge B$. Then $x(t)$ is uniquely determined by its samples $x[n] = x(nT_s)$ taken at any rate $f_s = 1/T_s > 2B$, and is recovered exactly by

$$\boxed{\;x(t) = \sum_{n=-\infty}^{\infty} x[n]\, \operatorname{sinc}\!\left(\frac{t}{T_s} - n\right), \qquad \operatorname{sinc}(u) = \frac{\sin(\pi u)}{\pi u}\;}$$

— the **Whittaker–Shannon interpolation formula** (Whittaker 1915; Kotelnikov 1933; Shannon's 1949 proof in ["Communication in the Presence of Noise"](https://en.wikipedia.org/wiki/44,100_Hz), *Proc. IRE* 37(1):10–21; the cleanest modern treatment is [Smith, *Introduction to Digital Filters* / *Digital Audio Resampling*](https://ccrma.stanford.edu/~jos/resample/resample.pdf)). Sampling a bandlimited signal multiplies it by an impulse train; in frequency this *convolves with* an impulse train, replicating $X(f)$ as images centered at every multiple of $f_s$. If $X$ is supported on $|f| < f_s/2$, the images do not overlap and the baseband copy can be re-isolated by an ideal lowpass — whose impulse response is the sinc, giving the reconstruction formula term by term.

**Aliasing as spectral folding.** If content exists above $f_s/2$ (the *Nyquist frequency*), the image centered at $f_s$ folds down into the baseband: a component at $f_s/2 + \delta$ reappears at $f_s/2 - \delta$ — a phantom tone that was never in the original, non-removable after the fact. Classic game-audio example: a footstep containing energy to 30 kHz sampled at 44.1 kHz aliases 22.05 kHz of it down to the mid-teens — a metallic zip riding the thud. The defense is the **anti-aliasing filter**: an analog lowpass before the ADC (or a decimation filter before any downsampling) that attenuates everything above the new Nyquist to below audibility. Real filters have transition bands, which is why $f_s$ must exceed $2B$ with margin: 44.1 kHz gives a 20 kHz passband and a 2.05 kHz transition band; oversampling converters (e.g. $\Delta\Sigma$ at MHz rates) push the analog filter so far out that a gentle one suffices, then decimate digitally.

**Why 44.1 kHz and 48 kHz (one paragraph).** In the late 1970s the only affordable high-bandwidth storage for digital audio was the video cassette recorder: PCM adapters (Sony PCM-1600, 1979, and successors) encoded audio samples as pseudo-video on U-matic tape, so the sample rate had to lock to the video line/field arithmetic — an integer number of samples per usable scan line. The magical coincidence: NTSC video has 245 usable lines per field × 60 fields/s × 3 samples per line = **44,100**, and PAL has 294 × 50 × 3 = **44,100** — one rate compatible with both world TV systems ([Wikipedia: 44,100 Hz](https://en.wikipedia.org/wiki/44,100_Hz); the account originates in Watkinson, *The Art of Digital Audio*). Color NTSC's 59.94 Hz field rate yields the 44,056 Hz variant. The CD Red Book (1980) inherited 44.1 kHz from the mastering machines; note $44100 = 2^2 \times 3^2 \times 5^2 \times 7^2$ ([verified factorization](https://metanumbers.com/44100)) — a 5-smooth-style highly composite number with 81 divisors, which is why it has so many exact rational relationships, but that is a *consequence* of the video-line arithmetic, not its cause (the "245 × 180" form sometimes quoted is wrong; it is 245 × 60 × 3). 48 kHz descends from the video/broadcast lineage instead (AES5 professional rate; $48 = 32 \times 3/2$, tied to the 32 kHz DAT speech rate), coexisting with 44.1 kHz ever since.

### 2.3.2 Quantization: the $6.02N + 1.76$ dB law, derived

Sampling discretizes time; quantization discretizes amplitude. An $N$-bit uniform quantizer divides the full-scale range $\text{FSR}$ into $2^N$ steps of

$$\Delta = \frac{\text{FSR}}{2^N}.$$

**Noise power.** If the input is "busy" relative to $\Delta$ (crosses many levels between samples), the rounding error $e$ is well modeled as uniform on $(-\Delta/2, \Delta/2)$:

$$P_e = \int_{-\Delta/2}^{\Delta/2} \frac{1}{\Delta} e^2\, de = \frac{1}{\Delta}\left[\frac{e^3}{3}\right]_{-\Delta/2}^{\Delta/2} = \frac{1}{\Delta}\cdot\frac{2\,(\Delta/2)^3}{3} = \frac{\Delta^2}{12}.$$

**Signal power.** A full-scale sine has peak amplitude $\text{FSR}/2 = 2^{N}\Delta/2$, so its RMS is $\frac{2^N \Delta/2}{\sqrt{2}}$ (the §2.1.4 result).

**SNR.**

$$\text{SNR} = 20\log_{10}\!\left(\frac{2^N\Delta / 2\sqrt{2}}{\Delta/\sqrt{12}}\right) = 20\log_{10}\!\left(2^N \cdot \sqrt{\tfrac{12}{8}}\right) = 20\log_{10}\!\left(2^N \sqrt{\tfrac{3}{2}}\right)$$

$$= 20 N\log_{10} 2 + 10\log_{10}(3/2) = 6.02\,N + 1.76\ \text{dB}.$$

$$\boxed{\;\text{SNR}_{\text{quant}} = 6.02\,N + 1.76\ \text{dB} \quad \text{(full-scale sine, noise over } 0 \ldots f_s/2\text{)}\;}$$

This is the Analog Devices MT-001 derivation verbatim ([Kester, "Taking the Mystery out of the Infamous Formula SNR = 6.02N + 1.76 dB"](https://www.analog.com/media/en/training-seminars/tutorials/MT-001.pdf)); each added bit halves $\Delta$, doubling SNR (+6.02 dB).

**Numerically, exactly:** $6.02 \times 16 + 1.76 = 96.32 + 1.76 = \mathbf{98.08\ \text{dB}}$ for 16-bit, and $6.02 \times 24 + 1.76 = 146.24 \approx \mathbf{146\ \text{dB}}$ for 24-bit. The folkloric "16-bit ≈ 96 dB" is the $6.02N$ term alone (or the $6N + 6$ variant used for full-scale-square/headroom conventions); state 98.08 dB for the full-scale-sine definition and say which convention you are using. Against the ear's ~120 dB range: 16-bit's 98 dB is short of full auditory range but adequate given that real program material rarely uses the top 20 dB; 24-bit's 146 dB covers the ear entirely, which is why 24-bit is "more than enough" and why real converters run out of bits to thermal noise around 20–21 ENOB anyway.

**Dither.** The uniform-error model fails for *small or slowly varying* signals: a decaying tone slides down the quantization staircase and the error becomes correlated with the signal — audible as grainy, harmonic-spiced distortion ("quantization distortion") rather than benign noise. **Dither** — adding a small random signal before quantization — decorrelates the error from the signal at the price of a slight noise-floor increase. The rigorous treatment is the Waterloo school: Vanderkooy & Lipshitz, ["Resolution below the Least Significant Bit in Digital Systems with Dither,"](https://secure.aes.org/forum/pubs/journal/?elib=7047) *JAES* 32(3):106–113 (1984), and the full survey Lipshitz, Wannamaker & Vanderkooy, ["Quantization and Dither: A Theoretical Survey,"](https://secure.aes.org/forum/pubs/journal/?elib=7047) *JAES* 40(5):355–375 (1992). Key results: a **rectangular-pdf** dither of 1 LSB peak-to-peak renders the error's *first-order* statistics (mean) independent of the input; a **triangular-pdf** (TPDF) dither of 2 LSB peak-to-peak (sum of two independent rectangulars) additionally renders the error *power* independent of the input and makes the error spectrum white — the standard audio choice. Undithered, a fade-to-zero in 16-bit ends in ghastly stepped distortion over the last few LSBs; TPDF-dithered, it fades smoothly into an innocent hiss.

### 2.3.3 Sample-rate conversion (one paragraph)

Converting between rates is a resampling problem governed by the same theorem: conceptually reconstruct the bandlimited continuous signal (sinc interpolation) and sample it again at the new rate. In practice, upsample by $L$, lowpass-filter (anti-imaging + anti-aliasing, cutoff $\min(f_s, f_s')/2$), downsample by $M$, for a rational ratio $L/M$, implemented with **polyphase** FIR structures so the filter runs only at the output rate ([Crochiere & Rabiner's multirate framework; see Smith, *Digital Audio Resampling*](https://ccrma.stanford.edu/~jos/resample/resample.pdf), which describes the interpolated-lookup table variant used for arbitrary/continuously-varying rates — e.g. Doppler-shifted game audio "scrubbing"). Irrational or time-varying ratios use windowed-sinc or polynomial (Farrow) interpolators; the audio-quality bar is flat passband and >100 dB image rejection (e.g. the [de Soras resampler design](https://ldesoras.fr/doc/articles/resampler-en.pdf)).

---

## 2.4 Noise Theory — Colors and Statistics

### 2.4.1 White noise

A discrete-time noise process is *white* if its power spectral density is flat: $S(f) = \sigma^2$ for all $f$, equivalently (Wiener–Khinchin) its **autocorrelation is a delta**: $R[\tau] = \sigma^2 \delta[\tau]$ — every sample uncorrelated with every other. **Gaussian white noise** (each sample drawn i.i.d. from $\mathcal{N}(0,\sigma^2)$) is the universal model because of the central limit theorem: any sum of many small independent contributions converges to it. It is also the *maximum-entropy* distribution for a given variance — the least-structured noise there is — which is why pure white noise sounds like "static": no correlation, no structure, nothing to grab perceptually.

### 2.4.2 Pink noise (1/f)

**Definition and the slope, verified.** Pink noise has equal *energy per octave* (equivalently per decade, per any log-width band). Derive the slope from that requirement: energy in $[f, 2f]$ is $\int_f^{2f} S(u)\,du$; for this to be constant in $f$ we need $S(f) \propto 1/f$, since

$$\int_f^{2f} \frac{C}{u}\,du = C \ln 2 \quad \text{(independent of } f\text{)}.$$

In dB per octave (power): $10\log_{10} 2 = \mathbf{3.0103 \approx 3\ \text{dB/oct}}$, i.e. **−3 dB/octave = −10 dB/decade** ([Whittle's canonical pink-noise DSP page](https://www.firstpr.com.au/dsp/pink-noise/) states precisely this: −10 dB/decade = 3.0102999 dB/octave, since power $\propto$ amplitude²). Note the subtlety: −3 dB/oct is a *power* slope; the amplitude spectral density falls at −1.5 dB/oct.

**Where it appears.** Voss & Clark**e** measured that loudness fluctuations in music and speech, and pitch (melody) fluctuations in music, exhibit 1/f spectra down to $5\times10^{-4}$ Hz — correlations over minutes. **Attribution correction:** the classic 1975 paper is Voss, R. F. & Clarke, J., ["'1/f noise' in music and speech,"](https://doi.org/10.1038/258317a0) ***Nature*** **258**:317–318 (27 Nov 1975) — *Nature*, not PNAS as often mis-cited (confirmed via the [eScholarship LBL manuscript](https://escholarship.org/uc/item/04t64495) and OSTI records); the follow-up with the generation algorithm and the "music from 1/f noise" listening experiments is Voss & Clarke, [JASA 63:258–263 (1978)](https://doi.org/10.1121/1.381721). Their stochastic compositions: white-noise melodies sounded too random, 1/f² too correlated, 1/f "pleasing" — a strong hint that 1/f is the statistics of *interesting* temporal structure.

**Generation algorithm 1: Voss–McCartney.** The original Voss scheme (popularized by Martin Gardner's 1978 *Scientific American* column) sums $N$ "dice" (white sources) updated at octave-spaced rates: source 0 every sample, source 1 every 2nd sample, source 2 every 4th, and so on. Each row is a sample-and-hold of white noise — a zero-order hold with $|\mathrm{sinc}|^2$ power response — and the rows' power spectra stack into a staircase approximating $1/f$ within about ±1 dB ripple that does *not* shrink with more rows ([Herriman's theoretical analysis](https://www.firstpr.com.au/dsp/pink-noise/allan-2/spectrum2.html); [Downey's walkthrough](https://www.dsprelated.com/showarticle/908.php)). James McCartney's 1999 refinement (music-dsp list, [archived by Whittle](https://www.firstpr.com.au/dsp/pink-noise/)): stagger the updates so *exactly one* row changes per sample — select the row by **counting trailing zeroes** of an incrementing counter (a single `CTZ` instruction), update the running total by `total += (new − old)` in O(1), and add one extra pure-white "row −1" every sample to fill the high-frequency sinc nulls (response is otherwise ~5 dB down at $f_s/4$, with a deep null at Nyquist). Cost: one PRNG draw, one add/subtract pair, one counter increment per sample — unbeatable.

**Generation algorithm 2: filtered white noise (Paul Kellet).** A weighted sum of first-order one-pole lowpass sections approximates the −3 dB/oct slope; Kellet's hand-tuned "instrumentation-grade" filter (17 Oct 1999, music-dsp / [Whittle archive](https://www.firstpr.com.au/dsp/pink-noise/); [musicdsp.org pink.txt](https://www.musicdsp.org/en/latest/_downloads/84bf8a1271c6bb0b3c88253c0546ae0f/pink.txt)), accurate to **±0.05 dB above 9.2 Hz at 44.1 kHz**:

```
b0 = 0.99886*b0 + white*0.0555179;
b1 = 0.99332*b1 + white*0.0750759;
b2 = 0.96900*b2 + white*0.1538520;
b3 = 0.86650*b3 + white*0.3104856;
b4 = 0.55000*b4 + white*0.5329522;
b5 = -0.7616*b5 - white*0.0168980;
pink = b0+b1+b2+b3+b4+b5+b6 + white*0.5362;
b6 = white*0.115926;
```

and the **"economy" version** (±0.5 dB) that everything embeds:

```
b0 = 0.99765*b0 + white*0.0990460;
b1 = 0.96300*b1 + white*0.2965164;
b2 = 0.57000*b2 + white*1.0526913;
pink = b0 + b1 + b2 + white*0.1848;
```

Kellet's design insight (from his own note): a single lowpass knee rolls off at −6 dB/oct, too steep; but the *transition* from 0 to −6 dB/oct at a knee is softer; stacking enough knees in a staircase buys −3 dB/oct, with a little delayed/high-passed signal mixed in to cancel the rise near Nyquist.

### 2.4.3 Brown noise (1/f²)

Integrate white noise: $x[n] = x[n-1] + w[n]$ is a random walk; differencing is the discrete-time integration operator whose magnitude response is $\propto 1/\sin(\omega/2) \approx 1/\omega$ at low frequencies, so the **PSD $\propto 1/f^2$** — −6 dB/octave (power). It is called *Brown* (for Brownian motion, not the color) or *red* noise. Caution for implementations: a raw random walk is not stationary — its variance grows without bound — so practical brown generators leak the integrator ($x[n] = a\,x[n-1] + w[n]$, $a \lesssim 1$), which flattens the spectrum below the leak frequency.

### 2.4.4 Blue and violet

The spectral mirror images: **blue** noise has PSD $\propto f$ (+3 dB/oct, differentiated white); **violet** $\propto f^2$ (+6 dB/oct, twice-differentiated white). Blue noise is the dither of choice for images (least-visible noise on a display); in audio it appears mainly as a conceptual complement and in noise-shaping contexts.

### 2.4.5 Why natural sounds cluster toward 1/f

The dominant theoretical account is **self-organized criticality**: Bak, Tang & Wiesenfeld, ["Self-organized criticality: an explanation of the 1/f noise,"](https://link.aps.org/doi/10.1103/PhysRevLett.59.381) *Phys. Rev. Lett.* **59**:381–384 (1987). Their sandpile model evolves to a critical state with avalanches of *all* sizes — no characteristic scale — and the superposition of uncorrelated events with a scale-free lifetime distribution yields $S(f) \propto 1/f$. One-liner for the engine docs: systems driven slowly, with threshold dynamics and many coupled degrees of freedom, sit at criticality and leak 1/f fluctuations; natural soundscapes (wind, water, crowd murmur, music itself per Voss–Clarke) are such systems, which is why 1/f-modulated noise *sounds natural* and white-modulated noise sounds synthetic.

### 2.4.6 Noise in games

Noise is the raw material of procedural audio texture: wind = pink/brown noise through slowly-modulated bandpass filters (cutoff and amplitude driven by 1/f modulators — noise modulated by noise); water = filtered noise with envelope bursts on the "splash" scale and 1/f texture on the "wash" scale; footsteps on gravel = short bursts of bandpass-filtered noise with randomized spectral tilt per step, layered over an impact transient; fire = brown-ish noise with sparse crackle transients (Poisson-distributed, amplitude-distributed ~1/f). The recipe in every case: colored noise × slow 1/f modulation × event-triggered transients.

---

## 2.5 Filters and Resonators

### 2.5.1 LTI systems and transfer functions

A causal LTI system with impulse response $h(t)$ acts by convolution $y(t) = \int_0^\infty h(\tau) x(t-\tau) d\tau$ (discrete: $y[n] = \sum_k h[k] x[n-k]$). By the convolution theorem, $Y = XH$; the **transfer function** is the transform of $h$: $H(s) = \int_0^\infty h(t) e^{-st} dt$ (Laplace, continuous) or $H(z) = \sum_n h[n] z^{-n}$ (z-transform, discrete), and the frequency response is the evaluation on the stability boundary: $s = i\omega$ or $z = e^{i\omega T_s}$.

### 2.5.2 First-order low-pass (the RC filter)

$$H(s) = \frac{1}{1 + sRC} \quad\Longrightarrow\quad |H(i\omega)| = \frac{1}{\sqrt{1 + (\omega RC)^2}}.$$

At $\omega_c = 1/(RC)$: $|H| = 1/\sqrt2$ — the **−3 dB cutoff $f_c = 1/(2\pi RC)$**. Above cutoff the magnitude falls as $1/\omega$: **−20 dB/decade (−6 dB/oct)** — a single pole per decade of slope. Worked example: $R = 1\ \text{k}\Omega$, $C = 15.9\ \mu\text{F}$ → $f_c = 10$ Hz. The digital one-pole equivalent is in §2.5.4.

### 2.5.3 Second-order resonators, Q, and the RBJ biquad

The canonical resonator (lowpass form):

$$\boxed{\;H(s) = \frac{\omega_0^2}{s^2 + \dfrac{\omega_0}{Q}\,s + \omega_0^2}\;}$$

$\omega_0$ is the natural (resonant) frequency; $Q$ the **quality factor**, which has three equivalent definitions worth keeping straight:

1. $Q = \omega_0 / (2\zeta)$ — pole distance from the $j\omega$-axis over twice the real-part... precisely $Q = \omega_0/(2|\text{Re}\,p|)$ for the pole pair $p = -\omega_0/(2Q) \pm j\omega_0\sqrt{1 - 1/(4Q^2)}$;
2. **Bandwidth**: $Q = f_0 / \text{BW}_{-3\,\text{dB}}$, i.e. $\text{BW} = f_0/Q$ — the definition that matters for EQ;
3. Energy: $Q = 2\pi \times (\text{energy stored}) / (\text{energy dissipated per cycle})$ — the physical definition, tying $Q$ to decay time ($t_{60} \approx 13.8\, Q / f_0$... more usefully: amplitude decays as $e^{-\omega_0 t/(2Q)}$).

The resonant peak: for the bandpass form $H(s) = \frac{(\omega_0/Q)s}{s^2+(\omega_0/Q)s+\omega_0^2}$, $|H(i\omega_0)| = 1$ and the −3 dB width is exactly $\omega_0/Q$. High $Q$ = narrow, ringing, "tonal" (a vowel formant at $Q \sim 10$); low $Q$ = broad, gentle (a shelf-ish tilt). Musical instruments' resonances live at $Q$ from ~5 (body modes) to ~1000+ (sustained string modes).

**The biquad, and the cookbook.** Every practical second-order digital audio filter is the **biquad**

$$H(z) = \frac{b_0 + b_1 z^{-1} + b_2 z^{-2}}{a_0 + a_1 z^{-1} + a_2 z^{-2}}, \qquad y[n] = \tfrac{b_0}{a_0}x[n] + \tfrac{b_1}{a_0}x[n{-}1] + \tfrac{b_2}{a_0}x[n{-}2] - \tfrac{a_1}{a_0}y[n{-}1] - \tfrac{a_2}{a_0}y[n{-}2],$$

and the canonical reference for its coefficients is Robert Bristow-Johnson's **Audio EQ Cookbook** (["Cookbook formulae for audio EQ biquad filter coefficients"](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html), maintained as a W3C note; plain-text original on [GitHub](https://github.com/WebAudio/Audio-EQ-Cookbook/blob/main/Audio-EQ-Cookbook.txt) and [musicdsp.org](https://www.musicdsp.org/en/latest/Filters/197-rbj-audio-eq-cookbook.html)). It gives closed-form coefficients for LPF, HPF, BPF (two gain conventions), notch, APF, peaking EQ, and low/high shelves, all derived by bilinear-transform digitization of the analog prototypes with frequency prewarping. The universal parameterization:

$$\omega_0 = 2\pi f_0/f_s, \qquad \alpha = \frac{\sin\omega_0}{2Q}, \qquad A = 10^{\text{dBgain}/40}.$$

For example, the lowpass:

$$b_0 = \tfrac{1-\cos\omega_0}{2},\; b_1 = 1-\cos\omega_0,\; b_2 = \tfrac{1-\cos\omega_0}{2};\qquad a_0 = 1+\alpha,\; a_1 = -2\cos\omega_0,\; a_2 = 1-\alpha.$$

**Bandpass Q and musical behavior.** In musical terms, $Q$ maps to how much the filter "rings" and how narrow its grab on the spectrum is. A $Q = 1$ bandpass at 2 kHz on pink noise sounds like a soft vowel-colored wash; $Q = 20$ sounds almost like a tone. The "constant-skirt vs constant-0dB-peak" BPF variants in the cookbook correspond to peak gain $= Q$ vs peak gain $= 0$ dB — a classic integration trap when porting the formulas (the engine should wrap both and name them).

### 2.5.4 The one-pole smoother (every game audio parameter ever)

The single most-used filter in game audio is the one-pole lowpass used to smooth a parameter:

$$y[n] = y[n-1] + \alpha\,(x[n] - y[n-1]) = \alpha x[n] + (1-\alpha) y[n-1].$$

Two time-constant descriptions circulate, and they must be **reconciled**, not confused:

- **Exponential-decay view:** the step response is $y[n] = x\,(1 - (1-\alpha)^n)$, i.e. error decays geometrically; in continuous time the error obeys $e^{-t/\tau}$ with time constant $\tau$. The **60 dB time** — the audio-relevant "how long until it's gone" — is $t_{60} = \tau \ln 1000 = \mathbf{6.9078\,\tau} \approx 6.91\,\tau$ (since $e^{-t_{60}/\tau} = 10^{-3}$).
- **Cutoff view:** the one-pole's −3 dB frequency is $f_c = 1/(2\pi\tau)$ — **the same $\tau$**, viewed in the frequency domain. One time constant, two readings: $\tau = 1/(2\pi f_c)$ and $t_{60} = 6.91\,\tau$. A 10 ms time constant is a 15.9 Hz cutoff *and* a 69 ms 60-dB settle; specify whichever the design is actually about.

The exact mapping to the discrete coefficient (matching the pole of $H(z) = \alpha / (1 - (1-\alpha) z^{-1})$ to $e^{-T_s/\tau}$):

$$\alpha = 1 - e^{-T_s/\tau} = 1 - e^{-2\pi f_c / f_s} \;\approx\; 2\pi f_c / f_s \quad (f_c \ll f_s).$$

Use the exponential form (not the approximation) whenever $f_c$ exceeds a few percent of $f_s$ — e.g. for a 5 kHz smoothing at 48 kHz, $\alpha_{\text{exact}} = 0.479$ vs $\alpha_{\text{approx}} = 0.654$, a clearly audible difference. The lerp form $y \mathrel{+}= (x - y)\cdot k$ in game code is exactly this filter with $\alpha = k$.

---

## 2.6 Envelopes and Modulation

### 2.6.1 ADSR and exponential envelope math

The ADSR envelope (attack–decay–sustain–release, Moog-era standard) is piecewise; the mathematically important choice is the *curve* of each segment. **Linear** decay $y = 1 - t/T$ crosses any fixed amplitude exactly once with constant slope; **exponential** decay $y = y_0\, e^{-t/\tau}$ has constant *percentage* rate: loudness falls by a fixed ratio per unit time. Perceptually, loudness is roughly logarithmic in amplitude, so a *linear* amplitude decay has a *quadratic* loudness trajectory — it lingers near the top then rushes to silence — and sounds "robotic"; an exponential amplitude decay is linear-in-dB and sounds like a natural dying-away. The canonical constant is the **60 dB time**: $e^{-t_{60}/\tau} = 10^{-3}$ gives

$$t_{60} = \tau \ln(1000) = 6.9078\,\tau,$$

so "a 2-second reverb tail" and "$\tau \approx 0.29$ s" are the same statement. (Same constant as §2.5.4 — because a decaying resonator *is* an exponential envelope generator.) Exponential segments are also the only segments that splice seamlessly at arbitrary junction times without slope discontinuities, which is why every mature synth envelope implementation uses them (or linear-in-dB, which *is* exponential).

### 2.6.2 Amplitude modulation

$$y(t) = \big(1 + m\cos(\omega_m t)\big)\cos(\omega_c t).$$

Expand with the product-to-sum identity $\cos a \cos b = \tfrac12[\cos(a-b) + \cos(a+b)]$:

$$y(t) = \cos(\omega_c t) + \frac{m}{2}\cos\big((\omega_c - \omega_m) t\big) + \frac{m}{2}\cos\big((\omega_c + \omega_m) t\big).$$

**Sidebands at $\omega_c \pm \omega_m$, each at $m/2$ (i.e. −6 dB relative to the carrier at $m=1$).** The spectrum is *three* lines: carrier plus a symmetric pair. **Tremolo** is AM at $f_m \lesssim 20$ Hz — below the rate at which the ear resolves the sidebands from the carrier, so it is heard as amplitude wobble (and note: at exactly $m=1$, 100% modulation, the perceived wobble depth is not what the naive amplitude plot suggests because the sidebands add *coherently*). **Ring modulation** is $y = \cos(\omega_m t)\cos(\omega_c t)$ — plain AM with the carrier suppressed (the $1+$ gone) — so only the two sidebands remain; with an inharmonic $f_m:f_c$ ratio the result is the classic metallic "ring mod" clang, exactly because the partial set $\{f_c \pm f_m\}$ has no common fundamental.

### 2.6.3 FM synthesis (Chowning 1973)

The crown jewel of compact synthesis mathematics. John Chowning, ["The Synthesis of Complex Audio Spectra by Means of Frequency Modulation,"](https://aes.org/e-lib/browse.cfm?elib=1954) *J. Audio Eng. Soc.* **21**(7):526–534 (Sept. 1973) ([author's PDF at CCRMA](https://ccrma.stanford.edu/sites/default/files/user/jc/fm_synthesis_paper.pdf)):

$$\boxed{\;y(t) = A\,\sin\!\big(\omega_c t + I \sin(\omega_m t)\big)\;}$$

where $I = \Delta f / f_m$ is the **modulation index** (peak phase deviation in radians). Note this is technically *phase* modulation — equivalent to FM for sinusoidal modulators, and the form the analysis actually solves (as [Lazzarini et al. note](https://export.arxiv.org/pdf/2305.07909v1.pdf)).

**The spectrum, via Jacobi–Anger.** The Jacobi–Anger expansion ([Wikipedia: Jacobi–Anger expansion](https://en.wikipedia.org/wiki/Jacobi-Anger_expansion)) states

$$e^{\,i z \sin\theta} = \sum_{n=-\infty}^{\infty} J_n(z)\, e^{\,i n\theta},$$

where $J_n$ is the Bessel function of the first kind, order $n$. Apply with $z = I$, $\theta = \omega_m t$:

$$y(t) = A\,\mathrm{Im}\!\left[e^{i\omega_c t} \sum_{n=-\infty}^{\infty} J_n(I)\, e^{i n \omega_m t}\right] = A \sum_{n=-\infty}^{\infty} J_n(I)\, \sin\big((\omega_c + n\omega_m) t\big),$$

using $J_{-n}(I) = (-1)^n J_n(I)$ (which is why, in Chowning's Eq. 2, the odd-order *lower* sidebands carry negative signs — phase inversion, inaudible in magnitude but essential when sidebands overlap). Collecting terms:

- **Carrier** at $f_c$ with amplitude $J_0(I)$;
- **Sideband pairs** at $f_c \pm n f_m$ with amplitudes $J_n(I)$, $n = 1, 2, \ldots$

Sidebands with $n > f_c/f_m$ land at negative frequency and *reflect* around 0 Hz (with sign flips) — Chowning's "reflected side frequencies," the source of the technique's richest inharmonic spectra. Energy is conserved: $\sum_{n=-\infty}^{\infty} J_n^2(I) = 1$ (Parseval for Bessel series), so raising $I$ does not make the sound louder, it *redistributes* energy outward into more sidebands — brightness as a single scalar knob. The effective bandwidth follows Carson's rule: $B \approx 2(\Delta f + f_m) = 2 f_m (I + 1)$.

**The famous ratio rule.** If $f_m : f_c$ is a ratio of small integers, all sidebands $f_c \pm n f_m$ lie on a common harmonic grid — the sound is *tonal*. If the ratio is irrational-ish (e.g. 1 : 1.4, 1 : √2), the partials have no common fundamental — the sound is *inharmonic, metallic, bell-like*. Chowning's own bell recipe from the paper: $f_c = 200$ Hz, $f_m = 280$ Hz (ratio 1 : 1.4), index swept from $I \approx 10$ down to 0 *proportional to the amplitude envelope* — dense clangorous spectrum at the strike, pure sine at the decay. This one-envelope-drives-both-loudness-and-brightness coupling is why FM reproduces the spectral evolution of struck and plucked things with two oscillators, and why the 1983 Yamaha DX7 (6 operators of exactly this) defined a decade of sound.

### 2.6.4 Vibrato and Doppler-style glides

**Vibrato** is FM with tiny index: $y = \sin(\omega_c t + I\sin(\omega_m t))$ with $I \lesssim 0.5$ and $f_m \sim 5\!-\!7$ Hz — the sidebands ($J_1(I)$-weighted, close to the carrier) are not resolved perceptually; the result is heard as pitch modulation of depth $\approx I \cdot f_m$ Hz. Six-Hz vibrato is near the transition between pitch-wobble and roughness — the same beat/roughness physics as §2.1.5. **A passing source** produces a Doppler glide: $f_{\text{obs}} = f_{\text{src}}\, \dfrac{c}{c \pm v_{\text{radial}}}$ (approaching: denominator $c - v$; receding: $c + v$). A source passing at closest distance $d$ with speed $v$ has radial velocity $v_r(t) = -v^2 t / \sqrt{v^2 t^2 + d^2}$, giving a smooth sigmoid pitch glide centered at closest approach — the "neeee-yooooow" — whose steepness scales with $v/d$. For a game engine: pitch-shifting a looping source by this closed-form $c/(c - v_r(t))$ is the cheap exact model; convolving with a varying delay $\big(\Delta t = v_r t / c$, the physical mechanism$\big)$ additionally produces the correct comb-flutter for free.

---

## 2.7 Additive and Subtractive Synthesis

### 2.7.1 Fourier synthesis and the harmonic series

Additive synthesis is the constructive direction of §2.2.1: build $f(t) = \sum_k A_k \sin(k \omega_0 t + \varphi_k)$. **Timbre = spectrum + envelope**: which harmonics, at what amplitudes, evolving how in time. The three classic geometric waveforms have exactly known series (verified against [MathWorld](https://mathworld.wolfram.com/SawtoothWave.html), [ProofWiki](https://proofwiki.org/wiki/Fourier_Series_for_Sawtooth_Wave), and the [UAM simple Fourier series page](http://matematicas.uam.es/~fernando.chamizo/dark/d_sim_fou.html)):

**Sawtooth** (peak amplitude $A$, all harmonics, $1/n$):

$$x_{\text{saw}}(t) = \frac{2A}{\pi} \sum_{n=1}^{\infty} \frac{(-1)^{n+1}}{n} \sin(2\pi n f t).$$

*Derivation sketch:* the sawtooth on $(-\tfrac{T}{2}, \tfrac{T}{2})$ is the identity ramp $2A\,t/T$; by odd symmetry only sine terms survive; $b_n = \frac{2}{T}\int_{-T/2}^{T/2} \frac{2At}{T} \sin(n\omega_0 t) dt$ integrates by parts to $\frac{2A}{\pi} \frac{(-1)^{n+1}}{n}$ — the $\pm 1$ alternation comes from $\cos(n\pi)$ evaluated at the endpoints.

**Square** (odd harmonics only, $1/n$):

$$x_{\text{sq}}(t) = \frac{4A}{\pi} \sum_{\substack{n=1,3,5,\ldots}}^{\infty} \frac{\sin(2\pi n f t)}{n}.$$

*Sketch:* odd square wave is the sign of a sine; only odd harmonics survive by the half-wave symmetry $f(t + T/2) = -f(t)$ (which kills every even coefficient of *any* waveform possessing it), and the same integration by parts gives $b_n = 4A/(\pi n)$ for odd $n$.

**Triangle** (odd harmonics, $1/n^2$, alternating sign):

$$x_{\text{tri}}(t) = \frac{8A}{\pi^2} \sum_{\substack{n=1,3,5,\ldots}}^{\infty} \frac{(-1)^{(n-1)/2}}{n^2} \sin(2\pi n f t).$$

*Sketch:* the triangle is the integral of the square wave; integrating multiplies each harmonic's coefficient by $1/(n\omega_0)$, turning $1/n$ into $1/n^2$ (and introducing the sign alternation through the integration constant choice). This is why triangle waves are *smooth* — coefficients falling as $1/n^2$ mean the spectrum rolls off at −12 dB/oct in amplitude, versus the saw/square's −6 dB/oct — and why they sound mellow where saws sound buzzy.

Worked amplitude check: sawtooth fundamental = $2A/\pi \approx 0.637 A$; square fundamental = $4A/\pi \approx 1.27 A$ (of the half-peak $A$); triangle fundamental = $8A/\pi^2 \approx 0.81 A$. The $1/n$ vs $1/n^2$ fall-off is also the single most useful fact in **aliasing-aware oscillator design**: a naive sampled sawtooth aliases badly because its harmonic amplitudes fall too slowly; band-limited synthesis (additive up to Nyquist, or BLIT/BLEP) must respect exactly this spectral envelope.

### 2.7.2 Subtractive synthesis and formants

Subtractive synthesis is the subtractive direction: start with a harmonically rich source (sawtooth, pulse train, noise) and *filter* it. This was the analog model — a sawtooth oscillator into a voltage-controlled filter — because generating a rich spectrum and sculpting it was vastly cheaper in analog parts than generating 30 sine oscillators. The source-filter decomposition is also, literally, the physics of the voice.

**Formants: the voice as filter.** Source–filter theory (Fant, *Acoustic Theory of Speech Production*, 1960): the glottal folds produce a harmonic-rich buzz at $f_0$ (adult male mean ≈ 130 Hz, female ≈ 220 Hz); the vocal tract above is a tube (~17.5 cm for an adult male) whose shape determines its resonance frequencies. For the neutral tract — a uniform tube closed at the glottis, open at the lips — the resonances are the quarter-wave modes:

$$F_k = \frac{(2k-1)\,c}{4L} \;\overset{L = 17.5\,\text{cm},\, c = 343\,\text{m/s}}{\longrightarrow}\; F_1 = 500\ \text{Hz},\; F_2 = 1500\ \text{Hz},\; F_3 = 2500\ \text{Hz}$$

([USC source-filter course notes](https://sail.usc.edu/~lgoldste/General_Phonetics/Source_Filter/SFc.html), [Manitoba phonetics notes](https://home.cc.umanitoba.ca/~krussll/phonetics/acoustic/formants.html)). Moving the tongue and jaw reshapes the tube and shifts the resonances — the **formants** $F_1, F_2, \ldots$. The vowel identity is carried by the formant *positions*, not by $f_0$: the same vowel can be sung on any pitch, and different vowels on the same pitch. Representative $F_1/F_2$ for an adult male (from the [Manitoba tables](https://home.cc.umanitoba.ca/~krussll/phonetics/acoustic/formants.html); canonical measurements Peterson & Barney 1952, [Hillenbrand et al. 1995]):

| Vowel | F1 (Hz) | F2 (Hz) |
|---|---|---|
| /i/ ("ee") | ~280 | ~2230 |
| /æ/ ("a" in "had") | ~860 | ~1550 |
| /ɑ/ ("ah") | ~830 | ~1170 |
| /u/ ("oo") | ~330 | ~1260 |

**"Ahh" vs "eee" on the same buzz = two different filters.** $F_1$ tracks vowel *height* (inverse: high tongue → low $F_1$); $F_2$ tracks *frontness*. This is the entire acoustic basis of vowel synthesis and of creature-voice design in games: a glottal buzz (sawtooth or pulse train at $f_0$) through 2–3 formant bandpass biquads (§2.5.3, $Q \sim 8\!-\!12$) at the $F_1$/$F_2$ of your choice, with a noise source mixed in for consonants, is a vowel — and morphing the biquad centers between vowel targets is intelligible speech-like vocalization without a single recorded sample.

---

## 2.8 Physical Modeling Synthesis

### 2.8.1 Karplus–Strong (1983)

Kevin Karplus & Alex Strong, ["Digital Synthesis of Plucked-String and Drum Timbres,"](https://doi.org/10.2307/3680062) *Computer Music Journal* **7**(2):43–55 (Summer 1983) ([PDF at Karplus's site](https://users.soe.ucsc.edu/~karplus/papers/digitar.pdf)); extensions in David A. Jaffe & Julius O. Smith, ["Extensions of the Karplus-Strong Plucked-String Algorithm,"](http://musicweb.ucsd.edu/~trsmyth/papers/KSExtensions.pdf) *CMJ* **7**(2):56–69 — the **same issue**. The algorithm is one line:

$$\boxed{\;y[n] = \tfrac{1}{2}\big(y[n-N] + y[n-N-1]\big) \qquad \text{(initial } y[0..N-1] = \text{noise burst)}\;}$$

A delay line of length $N$ is filled with a short burst of random numbers (the "pluck") and then read back while each circulating sample is replaced by the **average of itself and its predecessor** — a two-point lowpass, $H(z) = \tfrac12(1 + z^{-1})$, in the feedback loop of the delay. (Karplus–Strong's own presentation: $y[n] = x[n] + \tfrac12(y[n-N] + y[n-N-1])$, with $x$ the excitation; setting $x = 0$ after the burst gives the free vibration above.)

**Why it is a plucked string:**

- **Pitch:** the loop length is $N$ (or $N + \tfrac12$ — the loop filter's half-sample average), so the fundamental is $f \approx f_s / N$ (e.g. 44.1 kHz, $N = 100$ → 441 Hz, ≈ concert A).
- **Decay:** each pass around the loop multiplies a partial at $\omega$ by $|H(e^{i\omega})| = \big|\cos(\omega/2)\big|$. Low partials pass nearly unattenuated ($|H| \approx 1 - \omega^2/8$); partials near Nyquist are crushed ($|H| \to 0$). So **high harmonics die first — exactly a plucked string's brightness decay** — and the naturalness comes from *frequency-dependent damping*, which no static wavetable has. Per-second decay of harmonic $k$: $|H|^{f/N \cdot \ldots}$ — concretely, amplitude after 1 s $= |H(e^{i\omega_k})|^{f_s/N}$ where $\omega_k = 2\pi k f/f_s$.
- **Excitation:** a noise burst is a maximally-dumb initial condition; the loop filter does all the timbral work — which is why *any* initial table sounds string-like after a few loop passes.

**Jaffe–Smith extensions** (tuning, brightness, pick position): an allpass filter in the loop to fine-tune the phase (fractional delay) for correct tuning; a one-pole lowpass whose cutoff is raised/lowered for brightness control; a **comb filter applied to the excitation** to model pick *position* (plucking at $1/P$ of the string length kills harmonics of $P$ — $x_{\text{exc}}[n] - x_{\text{exc}}[n - N/P]$); "decay stretching" (blend $y[n-N]$ and $y[n-N-1]$ with weight $S$) so short strings don't die instantly. Historical note from [Smith's own account](https://ccrma.stanford.edu/~jos/smith-nam/Karplus_Strong_Algorithms.html): Strong invented the two-point-average trick to make wavetable synthesis less boring on an 8-bit micro; the string-likeness was a happy accident; Smith recognized the filtered delay loop as the transfer function of an idealized string and derived the whole family as a special case of digital waveguides.

### 2.8.2 Digital waveguide synthesis (Julius O. Smith III)

The wave equation for the ideal string, $\partial^2 y/\partial x^2 = (1/c^2)\,\partial^2 y/\partial t^2$ with $c = \sqrt{T/\mu}$ (tension over linear density), has as its general solution the **d'Alembert decomposition** (published 1747):

$$y(x, t) = y^{+}(x - ct) + y^{-}(x + ct)$$

— the string's state is exactly two arbitrary traveling waves, one moving right, one moving left, at speed $c$ ([Smith, *Physical Audio Signal Processing*](https://ccrma.stanford.edu/~jos/pasp/), verified live 2026-09-06; the [derivation from eigenfunctions/Fourier](https://ccrma.stanford.edu/~jos/smithbook/D_Alembert_Derived.html) is in the appendices). **Sample** the traveling waves at $x_m = mX$, $t_n = nT$ with the *magic choice* $X = cT$:

$$y(t_n, x_m) = y^{+}\big((n-m)T\big) + y^{-}\big((n+m)T\big) = y^{+}[n-m] + y^{-}[n+m].$$

A spatial shift is exactly a time shift — so **each traveling wave is a digital delay line**, and the pair of delay lines *is an exact solution of the 1-D wave equation* (exact, not approximated, for bandlimited content below Nyquist). The string displacement at any point is the sum of the two rails at that position; driving and observing are likewise point operations. Losses and dispersion in a real string are also LTI, so by commutativity they can be **lumped** into a single loop filter at one point — the waveguide string is two delay lines and one small filter, plus reflection/scattering junctions at the boundaries (wave impedance $R = \sqrt{T\mu}$ governs junction arithmetic). This is why waveguide synthesis is orders of magnitude cheaper than grid methods: the propagation itself costs *nothing but memory*. The full treatment — strings, tubes, scattering junctions, wave digital filters, the equivalence to Karplus–Strong — is Smith's free online book [*Physical Audio Signal Processing*](https://ccrma.stanford.edu/~jos/pasp/) (with [*Introduction to Digital Filters*](https://ccrma.stanford.edu/~jos/filters/) and [*Spectral Audio Signal Processing*](https://dsprelated.com/freebooks/sasp/) alongside; all live at [ccrma.stanford.edu/~jos/](https://ccrma.stanford.edu/~jos/)).

### 2.8.3 Modal synthesis

The complement of waveguides: expand the object's vibration in its *eigenmodes* (each a damped second-order resonator — §2.5.3 with $\omega_0$ = mode frequency, $Q$ = mode damping) and drive the bank with an excitation signal. For a stiff object (bars, plates, shells) the modes are measured (tap-and-FFT) or derived from the material's elasticity equations — which ties directly to the engine's materials section: given Young's modulus, density, and geometry, the modal frequencies and $Q$s of e.g. a rectangular bar follow from the Euler–Bernoulli beam equation, and the modal bank *is* the audible signature of the material. Modal synthesis is the natural choice when the object is struck (free vibration, no sustained nonlinearity needed) and when the mode data is available or measurable; each mode is one biquad, so a 20-mode object costs 20 biquads — trivially cheap, embarrassingly parallel, trivially tuneable per-material.

### 2.8.4 Banded waveguides, FDTD, and the engineering moral

**Banded waveguides** (Essl & Cook, ICMC 1999; [theory paper: Essl, Serafin, Cook & Smith, *CMJ* 28(1):37–50, 2004](https://doi.org/10.1162/014892604322970634)): split the spectrum into bands, each containing primarily one mode, and give *each band its own waveguide loop* whose delay matches the mode's round-trip time. This hybrid of modal and waveguide synthesis preserves *spatial sampling* (the ability to excite/observe/listen at a physical point, needed for nonlinear bowing interactions) while keeping cost bounded — designed for bar percussion (marimba, bowed bars, Tibetan bowls). **Finite-difference time-domain (FDTD)** methods (Hiller & Ruiz 1971, *JAES* 19(6):462-470 — ["Synthesizing Musical Sounds by Solving the Wave Equation for Vibrating Objects"](https://soundlab.cs.princeton.edu/publications/1999_icmc_bar.pdf) is the modern entry point) discretize the PDE directly on a grid: general, physical, handles nonlinearities and arbitrary geometry — and costs $O(\text{cells} \times \text{timesteps})$, which for a 3-D object at audio rates is far beyond real time for game use. **The practical insight for a game engine:** procedural audio wants the *cheapest model that passes perceptual muster*. The hierarchy is: wavetable (cheapest, static) → FM (2 oscillators, dynamic spectra) → Karplus–Strong (1 delay line + 1 add/shift, physically-motivated decay) → digital waveguide (2 delay lines + small filter, exact linear physics) → modal bank (N biquads, measured physics) → banded waveguide (spatial + cheap) → FDTD (exact, unaffordable). Choose per sound: a sword *ting* is 5 modes; a plucked lute string is a waveguide; wind through grass is filtered noise; and nothing in a shipping game needs FDTD.

---

## 2.9 Perceptual Scales for Audio Code

### 2.9.1 The mel scale

The mel scale (Stevens & Volkmann, ["The Relation of Pitch to Frequency: A Revised Scale,"](https://en.wikipedia.org/wiki/Mel_scale) *Am. J. Psychol.* 53(3):329–353, 1940) is a perceptual pitch scale with the anchor **1000 Hz = 1000 mel** (1 kHz tone, 40 dB SL). The standard modern formula (attributed to O'Shaughnessy 1987, with the 700 Hz breakpoint introduced by Makhoul & Cosell 1976; [origin traced on the AUDITORY list](http://www.auditory.org/mhonarc/2008/msg00191.html)):

$$m = 2595\,\log_{10}\!\left(1 + \frac{f}{700}\right) = 1127\,\ln\!\left(1 + \frac{f}{700}\right).$$

**The two forms are the same, shown:** $2595/\ln(10) = 2595/2.302585 = 1126.96 \approx 1127$ (the sometimes-seen 1127.01048 is the value that makes the 1 kHz anchor exact to more decimals than the data deserves). **Anchor check:** $m(1000\,\text{Hz}) = 2595\,\log_{10}(1 + 10/7) = 2595 \times \log_{10}(2.42857) = 2595 \times 0.38539 = \mathbf{1000.1}$ ✓. The scale is roughly linear below ~700 Hz and logarithmic above — one bend, one parameter. Use in code: mel-spaced filterbanks for audio analysis/ML features and for any UI that needs "perceptually uniform" frequency sliders.

### 2.9.2 Bark and ERB (for completeness)

The **Bark** scale (Zwicker 1961; 24 critical bands over the audible range) — two standard conversions ([Traunmüller 1990, *JASA* 88:97-100](https://resources.ling.su.se/hartmut/bark.htm)):

$$z = 13\arctan(0.00076\,f) + 3.5\arctan\!\big((f/7500)^2\big) \quad \text{(Zwicker–Terhardt 1980)}, \qquad z = \frac{26.81\,f}{1960 + f} - 0.53 \quad \text{(Traunmüller)}.$$

The **ERB** (equivalent rectangular bandwidth) scale — the modern auditory-filter measurement ([Glasberg & Moore 1990, *Hearing Research* 47:103–138](https://en.wikipedia.org/wiki/Equivalent_rectangular_bandwidth)):

$$\mathrm{ERB}(f) = 24.7\,(4.37\,f_{\text{kHz}} + 1)\ \text{Hz}, \qquad \mathrm{ERB}\text{-rate}(f) = 21.4\,\log_{10}(4.37\,f_{\text{kHz}} + 1)\ \text{ERBs}.$$

ERB is narrower than the classical critical band at all frequencies (they agree above ~500 Hz); Bark measures tonotopic position, ERB measures frequency resolution. For a game engine these matter wherever "can the ear separate these two components?" is the question (roughness, masking, spectral-spreading decisions).

### 2.9.3 Loudness units in code — dBFS, LUFS, LU

**dBFS** (dB full scale): the digital-peak/rms scale of a sample stream — $20\log_{10}(|x|/x_{\text{max}})$; 0 dBFS is the largest representable amplitude, everything else negative. It is a *level* measure with no perceptual weighting: a 0 dBFS 1 kHz sine and a 0 dBFS 20 Hz sine read identically and sound wildly different.

**LUFS / LKFS** (loudness units, full scale): the ITU broadcast loudness measure. [ITU-R BS.1770](https://www.itu.int/rec/R-REC-BS.1770) (current revision [BS.1770-5, 11/2023](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf)) defines the algorithm: (1) **K-weighting** — a shelving pre-filter modeling head acoustics cascaded with a RLB high-pass; (2) mean-square per channel; (3) channel-weighted power summation (surround channels +1.5 dB, LFE excluded); (4) gating over 400 ms blocks (75% overlap) with an absolute −70 LUFS threshold and a relative −10 LU threshold. The result:

$$L_K = -0.691 + 10\log_{10}\sum_i G_i\, z_i \quad \text{LUFS},$$

calibrated so a 0 dBFS 997 Hz sine reads **−3.01 LUFS** (the $1/\sqrt2$ RMS of §2.1.4, minus the 0.691 correction). LUFS and LKFS are the same unit under different naming conventions ([EBU R128](https://tech.ebu.ch/files/live/sites/tech/files/shared/r/r128v5_0.pdf) uses LUFS; broadcast target −23 LUFS ±1 LU; streaming platforms cluster around −14 to −16 LUFS integrated).

**LU** (loudness unit): the *relative* unit — 1 LU ≡ 1 dB of difference on the LUFS scale. Meters display "0 LU" at the target loudness; a program 3 LU below target reads −3 LU. In engine terms: use dBFS for clipping/headroom safety (never normalize in dBFS), use LUFS for "how loud does the game actually sound," and use LU for expressing the difference.

---

## 2.10 Reverberation Mathematics

### 2.10.1 Schroeder's 1962 architecture

Manfred R. Schroeder, ["Natural Sounding Artificial Reverberation,"](http://ece.rochester.edu/~zduan/teaching/ece472/reading/Schroeder_1962.pdf) *J. Audio Eng. Soc.* **10**(3):219–223 (July 1962) (with the companion ["Colorless Artificial Reverberation,"](http://languagelog.ldc.upenn.edu/myl/Logan1961.pdf) IRE Trans. Audio, 1961, for the allpass derivation). Schroeder's diagnosis: delay-based reverberators of the era failed two ways — colored frequency response (comb resonances) and too-low echo density (audible flutter). His architecture, still the skeleton inside most algorithmic reverbs:

**Comb filter** — a delay with feedback:

$$\boxed{\;H_{\text{comb}}(z) = \frac{z^{-D}}{1 - g\, z^{-D}}\;} \qquad y[n] = x[n-D] + g\, y[n-D].$$

Impulse response: exponentially decaying pulse train spaced $D$ samples; frequency response: $|H| = 1/|1 - g e^{-i\omega D}|$ — peaks of $1/(1-g)$ at multiples of $f_s/D$, nulls of $1/(1+g)$ between. The **decay time** to −60 dB: each round trip multiplies by $g$, so $g^{\,t f_s / D} = 10^{-3}$, giving

$$T_{60} = \frac{D}{f_s}\cdot\frac{\ln 1000}{-\ln g} \quad\Longleftrightarrow\quad g = 10^{-3 D / (f_s\, T_{60})}.$$

Worked: $D/f_s = 40$ ms, $g = 0.85$ → $T_{60} = 0.040 \times 6.9078 / 0.1625 \approx 1.7$ s.

**Allpass filter** — the comb plus a judiciously proportioned direct path:

$$\boxed{\;H_{\text{ap}}(z) = \frac{z^{-D} - g}{1 - g\, z^{-D}}\;} \qquad y[n] = -g\,x[n] + x[n-D] + g\, y[n-D].$$

$|H(e^{i\omega})| = 1$ for **all** $\omega$ (numerator and denominator are conjugate reciprocals on the unit circle — Schroeder's Eq. 13–14: $H(\omega) = e^{-i\omega T}\frac{1 - g e^{i\omega}}{1 - g e^{-i\omega}}$, each factor unit magnitude) — "colorless": same resonances as the comb in *decay*, invisible in *magnitude*. Cascades of allpasses multiply echo density without coloring.

**The complete Schroeder reverb:** 4 parallel combs (incommensurate delays ~30–45 ms, gains set per the $T_{60}$ formula) into 2 series allpasses (~5 ms and ~1.7 ms, $g \approx 0.7$). Schroeder's requirements: **echo density ≥ ~1000 echoes/second** for flutter-free tails (a single 40 ms comb gives 25/s; four in parallel give 100/s; each allpass roughly triples), and **mode spacing fine enough that multiple modes fall within a critical band** (§2.9.2) so the magnitude response is statistically smooth — the perceptual criterion behind "incommensurate delays."

### 2.10.2 Feedback delay networks (FDN)

The generalization: $N$ delay lines in parallel, whose outputs are mixed back to their inputs by an $N \times N$ **feedback matrix** $\mathbf{A}$:

$$\mathbf{s}[n] = \mathbf{A}\, \mathbf{s}[n - \mathbf{m}] + \mathbf{B}\mathbf{x}[n], \qquad \mathbf{y}[n] = \mathbf{C}\mathbf{s}[n] + \mathbf{D}\mathbf{x}[n].$$

History, verified: **Gerzon** proposed the "orthogonal matrix feedback reverberation unit" ([*Electronics Letters*, 1976](https://doi.org/10.1049/el:19760215), "Unitary (energy-preserving) multichannel networks with feedback") — noting that cross-coupled combs beat independent ones; **Stautner & Puckette** (1981 ICMC; [*CMJ* 6(1), 1982, "Designing multi-channel reverberators"](https://www.ee.columbia.edu/~dpwe/e4896/papers/StautP82-reverb.pdf)) gave the first concrete 4-channel FDN with stability conditions and the feedback matrix

$$\mathbf{A} = \frac{g}{\sqrt2}\begin{bmatrix} 0 & 1 & 1 & 0 \\ -1 & 0 & 0 & -1 \\ 1 & 0 & 0 & -1 \\ 0 & 1 & -1 & 0 \end{bmatrix}$$

(a signed permutation of a Hadamard matrix — [Smith's FDN history](https://ccrma.stanford.edu/~jos/pasp/History_FDNs_Artificial_Reverberation.html)); **Jot & Chaigne** (["Digital Delay Networks for Designing Artificial Reverberators,"](https://aes.org/e-lib/browse.cfm?elib=5663) AES Convention 90, Paris, paper 3030, Feb. 1991) turned it into a design methodology: a **unitary** (lossless) feedback matrix makes the FDN's modes decay *equally* — no ringing resonances — and then per-frequency reverberation control is achieved by inserting a small attenuation filter $\boldsymbol{\Lambda}(z)$ (e.g. a one-pole lowpass per line, or a common "damping" filter) inside the loop, with a companion **tonal correction filter** on the output equalizing the coloration the damping introduces. The design decouples: delays → mode density; feedback matrix → losslessness/quality; damping filters → frequency-dependent $T_{60}$; correction filter → flat response. This is the architecture of essentially every modern algorithmic reverb.

### 2.10.3 Convolution reverb

The exact linear model of a room is the convolution theorem itself: the room *is* an LTI system with impulse response $h[n]$ (measured — an exponential sine sweep or balloon pop — or synthesized), and the reverberated signal is $y = x * h$. Direct convolution of a signal with an $M$-sample IR costs $O(N M)$ per $N$ output samples — a 3-second IR at 48 kHz ($M = 144{,}000$) costs 144k multiply-accumulates *per sample*, ~7 billion/sec: unshippable. The FFT makes it $O(N \log N)$ via **partitioned (block) convolution**: split the IR into blocks, use overlap-save/overlap-add block FFT convolution per block, and sum the delayed partial results. The catch is latency: a block of length $B$ must accumulate before its FFT can run, so naive uniform partitioning adds $B$ samples of input-output delay. **Gardner's classic solution** (William G. Gardner, ["Efficient Convolution without Input/Output Delay,"](https://aes.org/e-lib/browse.cfm?elib=7957) *JAES* 43(3):127–136, 1995): a **non-uniform partition** — direct (time-domain) convolution for the head of the IR to produce output *immediately*, while the first FFT block's worth of input accumulates, then geometrically growing FFT blocks (each at least twice as long as its start offset into the filter) so that every block's computation is hidden under the accumulation of its own input, with a three-priority scheduler evening the CPU load. Later work (García, ["Optimal Filter Partition for Efficient Convolution with Short Input/Output Delay,"](https://secure.aes.org/forum/pubs/conventions/?elib=11275) AES 113th Conv., 2002) formalized the optimal partition choice; GPU implementations extend the same structure. For the engine: convolution is the reference-grade path (measured spaces, exact coloration); FDN/Schroeder-style algorithmic reverb is the interactive-grade path (parameter morphing — room size, wetness — with zero IR-switching artifacts); both are required, for different sounds.

---

## 2.11 Consolidated numeric sanity checks

| Check | Value | Status |
|---|---|---|
| RMS of sine | $A/\sqrt2 = 0.7071A$ | derived §2.1.4 |
| Gibbs overshoot | $0.08949 \times$ jump ≈ 8.9% | [Wikipedia/MIT 18.03](https://en.wikipedia.org/wiki/Gibbs_phenomenon) ✓ |
| Window sidelobes | rect −13.3, Hann −31.5, Hamming −42.7/−43, Blackman −58 dB | [VRU table](https://vru.vibrationresearch.com/lesson/table-of-window-function-details/) ✓ |
| 20 ms window vs 25 Hz tones | $1/T = 50$ Hz > 25 Hz → unresolved; need ≥ 40 ms (rect) / ~160 ms (Hann mainlobe) | derived §2.2.2 |
| 44,100 | $245 \times 60 \times 3 = 294 \times 50 \times 3 = 2^2 3^2 5^2 7^2$ | [Wikipedia](https://en.wikipedia.org/wiki/44,100_Hz), [metanumbers](https://metanumbers.com/44100) ✓ |
| Quantization SNR, 16-bit | $6.02 \times 16 + 1.76 = \mathbf{98.08}$ dB ("≈96 dB" = the $6.02N$ term alone) | derived §2.3.2, [AD MT-001](https://www.analog.com/media/en/training-seminars/tutorials/MT-001.pdf) ✓ |
| Quantization SNR, 24-bit | $6.02 \times 24 + 1.76 = 146.24$ dB | same |
| Pink slope | −3.0103 dB/oct = −10 dB/decade | derived §2.4.2 ✓ |
| Mel constants | $2595/\ln 10 = 1127.0$; $m(1000\,\text{Hz}) = 1000.1$ | §2.9.1 ✓ |
| $t_{60}$ | $\ln 1000 = 6.9078$; $t_{60} = 6.9078\,\tau$ | §2.5.4, §2.6.1 |
| KS pitch | $f_s/N$ (e.g. 44100/100 = 441 Hz) | §2.8.1 |
| Schroeder comb gain | $g = 10^{-3D/(f_s T_{60})}$ | §2.10.1 |
| BS.1770 calibration | 0 dBFS 997 Hz sine → −3.01 LUFS | [BS.1770-5](https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf) ✓ |

**Primary sources verified live 2026-09-06:** Julius O. Smith's CCRMA books ([index](https://ccrma.stanford.edu/~jos/), [PASP](https://ccrma.stanford.edu/~jos/pasp/), [filters](https://ccrma.stanford.edu/~jos/filters/), [resample](https://ccrma.stanford.edu/~jos/resample/resample.pdf)); [RBJ Audio EQ Cookbook](https://webaudio.github.io/Audio-EQ-Cookbook/audio-eq-cookbook.html); [Chowning 1973 PDF](https://ccrma.stanford.edu/sites/default/files/user/jc/fm_synthesis_paper.pdf); [Karplus–Strong 1983 PDF](https://users.soe.ucsc.edu/~karplus/papers/digitar.pdf) + [Jaffe–Smith PDF](http://musicweb.ucsd.edu/~trsmyth/papers/KSExtensions.pdf); [Schroeder 1962 PDF](http://ece.rochester.edu/~zduan/teaching/ece472/reading/Schroeder_1962.pdf); [Jot & Chaigne 1991](https://aes.org/e-lib/browse.cfm?elib=5663); [Voss & Clarke 1975](https://doi.org/10.1038/258317a0) (Nature 258:317–318 — **not PNAS**); [BTW 1987](https://link.aps.org/doi/10.1103/PhysRevLett.59.381); [ITU-R BS.1770-5](https://www.itu.int/rec/R-REC-BS.1770); [Gardner 1995](https://aes.org/e-lib/browse.cfm?elib=7957); [Stautner–Puckette 1982](https://www.ee.columbia.edu/~dpwe/e4896/papers/StautP82-reverb.pdf); [Gerzon 1976](https://doi.org/10.1049/el:19760215); [Whittle's pink-noise compendium](https://www.firstpr.com.au/dsp/pink-noise/) (Kellet coefficients); [Lipshitz–Wannamaker–Vanderkooy 1992](https://secure.aes.org/forum/pubs/journal/?elib=7047); [Essl et al. banded waveguides](https://doi.org/10.1162/014892604322970634); [Traunmüller 1990](https://resources.ling.su.se/hartmut/bark.htm).
