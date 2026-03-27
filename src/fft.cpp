#include <fftw3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#ifndef NUM_BINS
#define NUM_BINS 2000
#endif

typedef struct {
    int sample_rate;
    int num_bins;
    float min_freq;
    float max_freq;
    float min_val;
    float max_val;
} spec_header;

#include <mutex>

static std::mutex g_fftw_mutex;

/**
 * SpectralState holds precomputed plans, buffers, and mapping tables to avoid 
 * redundant calculations during streaming.
 */
struct SpectralState {
    int N;
    int fs;
    float* fft_in;
    fftwf_complex* fft_out;
    fftwf_plan plan;
    std::vector<float> window;
    float s2;
    float df;

    // HMD mappings
    int n_linear;
    int n_log;
    int total_bins;
    float transition_f = 435.0f;

    struct WeightInfo {
        int k;
        float weight;
    };
    struct BinMapping {
        std::vector<WeightInfo> weights;
        float flow;
        float fhigh;
    };
    std::vector<BinMapping> mappings;

    SpectralState(int n, int sample_rate) : N(n), fs(sample_rate) {
        fft_in = (float*)fftwf_malloc(sizeof(float) * N);
        fft_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (N / 2 + 1));
        
        if (!fft_in || !fft_out) {
            std::cerr << "Failed to allocate FFTW buffers" << std::endl;
            // In a real app we'd handle this better, but let's at least not crash here
            return;
        }

        {
            std::lock_guard<std::mutex> lock(g_fftw_mutex);
            // Using FFTW_ESTIMATE is safer on some platforms/architectures 
            // than FFTW_MEASURE which tries to execute and time the transforms.
            plan = fftwf_plan_dft_r2c_1d(N, fft_in, fft_out, FFTW_ESTIMATE);
        }

        window.resize(N);
        s2 = 0.0f;
        for (int i = 0; i < N; i++) {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (N - 1)));
            s2 += window[i] * window[i];
        }
        df = (float)fs / N;

        const float f_max = (float)fs / 2.0f;
        n_linear = (f_max < transition_f) ? (int)std::floor(f_max) : (int)transition_f;
        n_log = (f_max > transition_f) ? (int)std::ceil(1000.0f * std::log10(f_max / transition_f)) : 0;
        total_bins = n_linear + n_log;

        mappings.resize(total_bins);
        for (int i = 0; i < total_bins; i++) {
            float flow, fhigh;
            if (i < n_linear) {
                flow = (float)i;
                fhigh = (float)i + 1.0f;
            } else {
                float k = (float)(i - n_linear);
                flow = transition_f * std::pow(10.0f, k / 1000.0f);
                fhigh = transition_f * std::pow(10.0f, (k + 1.0f) / 1000.0f);
            }
            if (flow >= f_max) break;
            if (fhigh > f_max) fhigh = f_max;

            mappings[i].flow = flow;
            mappings[i].fhigh = fhigh;

            int k_start = std::max(0, (int)std::floor(flow / df));
            int k_end = std::min(N / 2, (int)std::ceil(fhigh / df));

            for (int k = k_start; k <= k_end; k++) {
                float k_f_low = (k == 0) ? 0.0f : (k - 0.5f) * df;
                float k_f_high = (k + 0.5f) * df;
                float overlap_low = std::max(flow, k_f_low);
                float overlap_high = std::min(fhigh, k_f_high);

                if (overlap_high > overlap_low) {
                    mappings[i].weights.push_back({k, (overlap_high - overlap_low)});
                }
            }
        }
    }

    ~SpectralState() {
        {
            std::lock_guard<std::mutex> lock(g_fftw_mutex);
            fftwf_destroy_plan(plan);
        }
        fftwf_free(fft_in);
        fftwf_free(fft_out);
    }
};

// Stateless versions for backward compatibility (could be removed later if not used)
typedef std::vector<float> (*SpectralProcessor)(const float*, int, int);

std::vector<float> hybrid_millidecade_processor_stateful(SpectralState* state, const float* mono_samples) {
    for (int i = 0; i < state->N; i++) {
        state->fft_in[i] = mono_samples[i] * state->window[i];
    }

    fftwf_execute(state->plan);

    std::vector<float> hmd_spectrum(state->total_bins, -120.0f);
    const float s2_fs = state->s2 * state->fs;

    for (int i = 0; i < state->total_bins; i++) {
        const auto& m = state->mappings[i];
        if (m.weights.empty()) continue;

        double bin_power_sum = 0.0;
        for (const auto& w : m.weights) {
            double p = (double)state->fft_out[w.k][0] * state->fft_out[w.k][0] + 
                       (double)state->fft_out[w.k][1] * state->fft_out[w.k][1];
            bin_power_sum += p * w.weight;
        }

        float bandwidth = (m.fhigh - m.flow);
        float psd = (float)((2.0 * bin_power_sum) / (s2_fs * bandwidth));
        hmd_spectrum[i] = 10.0f * std::log10(psd + 1e-18f);
    }

    return hmd_spectrum;
}

// Old functions updated to use state internally (inefficient, but keeps compatibility)
// We should probably remove these if possible, but let's keep them for now.
std::vector<float> hybrid_millidecade_processor(const float* mono_samples, int N, int fs) {
    SpectralState state(N, fs);
    return hybrid_millidecade_processor_stateful(&state, mono_samples);
}

std::vector<float> log_mapping_processor(const float* mono_samples, int N, int fs) {
    const int num_log_bins = NUM_BINS;
    const float min_f = 20.0f;
    const float max_f = (float)fs / 2.0f;
    
    float* fft_buffer = (float*)fftwf_malloc(sizeof(float) * (N + 2));
    fftwf_complex* fft_out = (fftwf_complex*)fft_buffer;
    fftwf_plan plan = fftwf_plan_dft_r2c_1d(N, fft_buffer, fft_out, FFTW_ESTIMATE);

    for (int i = 0; i < N; i++) {
        float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (N - 1)));
        fft_buffer[i] = mono_samples[i] * window;
    }

    fftwf_execute(plan);
    std::vector<float> log_slice(num_log_bins, 0.0f);

    for (int k = 1; k <= N / 2; k++) {
        float freq = (float)k * fs / N;
        if (freq < min_f) continue;

        int bin_idx = (int)(num_log_bins * std::log2(freq / min_f) / std::log2(max_f / min_f));
        if (bin_idx >= 0 && bin_idx < num_log_bins) {
            float mag = std::sqrt(fft_out[k][0] * fft_out[k][0] + fft_out[k][1] * fft_out[k][1]);
            log_slice[bin_idx] += mag;
        }
    }

    for (float &val : log_slice) {
        val = 20.0f * std::log10(val + 1e-6f);
    }

    fftwf_destroy_plan(plan);
    fftwf_free(fft_buffer);

    return log_slice;
}