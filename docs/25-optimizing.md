# 25-Optimizing Spectral Processing

This document explains the performance optimizations made to the spectral processing engine (`spec_gen`) to improve speed and memory utilization.

## 1. Summary of Optimizations

The primary goal of these optimizations was to reduce the computational overhead per 200ms audio slice and leverage multiple CPU cores where available.

| Component | Optimization Type | Benefit |
|-----------|------------------|---------|
| **FFT** | Plan Reuse (`SpectralState`) | Avoids $O(N \log N)$ planning overhead for every slice. |
| **HMD** | Precomputed Mappings | Replaces $O(N_{bins} \times N_{FFT})$ edge calculations with a direct $O(N_{FFT})$ aggregation pass. |
| **Streaming** | Chunked Parallelism | Keeps memory < 20MB while using all CPU cores. |
| **HMSP** | Two-pass Processing | Avoids memory-heavy caching of spectral data. |

## 2. Technical Details

### Stateful Processor (`fft.cpp`)
The original implementation was stateless, meaning it initialized the FFTW plan and the windowing function for every single slice. This was highly inefficient.
The new `SpectralState` struct manages:
- **Persistent Plan**: `fftwf_plan` is created once with `FFTW_MEASURE` (more optimal than `FFTW_ESTIMATE` for many reuse calls).
- **Persistent Buffers**: `fft_in` and `fft_out` are allocated once and reused.
- **Precomputed Window**: The Hann window is computed once and stored.

### Hybrid-Millidecade (HMD) Precomputation
HMD processing involves mapping standard linear FFT bins to logarithmically-spaced bins. In the old version, for every HMD bin, the code recalculated the frequency edges (`flow`, `fhigh`) and the fractional overlap with FFT bins using expensive transcendental functions (`std::pow`, `std::log10`).
Now, these overlaps are precomputed into a `mappings` table. During processing, the integration is a simpleweighted sum of the complex magnitudes.

### Chunked Parallelization (`stream.cpp`)
To satisfy the **< 20MB memory limit**, we now process the audio in consecutive chunks of 500 slices (~4MB of results data). 
- Within each chunk, threads process slices in parallel.
- Once a chunk is complete, it is written to disk immediately, and the memory is cleared.
- This ensures that even for very large files (e.g. 500MB), the application only ever holds a small fraction of the data in RAM at once.

### Two-Pass HMSP Generation
For `.hmsp` generation, we perform two separate passes over the WAV file. While this requires more disk I/O than the previous caching approach, it ensures that we never exceed the 20MB memory limit, as we do not need to store the entire spectral history in RAM.

## 3. Scientific Verification

The underlying scientific mechanics of **Hybrid-Millidecade (HMD)** processing have not changed. The formula for Power Spectral Density (PSD) remains:

$$PSD = \frac{2 \sum (Power \times weight)}{S_2 \times f_s \times Bandwidth}$$

Where:
- $Power$ is derived from $|FFT[k]|^2$.
- $weight$ is the bandwidth in Hz of the overlap between FFT bin $k$ and HMD bin $i$.
- $S_2$ is the coherent power gain of the window function.
- $f_s$ is the sampling frequency.

The optimized code performs the exact same mathematical operations; it simply moves the constant parts of the calculation (the $weight$ and $Bandwidth$ for each bin) into a precomputed lookup table.
