#include <fftw3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

void stream_to_disk(wav_file* wv, const char* out_filename, SpectralProcessor process_unused) {
    if (!wv || !wv->pWav) return;

    const int fs = wv->fs;
    int N = (int)(0.200f * fs); 
    if (N < 1024) N = 1024;

    const int channels = wv->pWav->channels;
    drwav_uint64 total_frames = wv->pWav->totalPCMFrameCount;
    size_t total_slices = (size_t)((total_frames + N - 1) / N);

    // Initialize stateful processor to get bin count
    SpectralState state_init(N, fs);
    int num_bins = state_init.total_bins;

    std::ofstream outFile(out_filename, std::ios::binary);
    if (!outFile) return;

    spec_header header = { fs, num_bins, 20.0f, (float)fs/2.0f, 0.0f, 0.0f };
    outFile.write(reinterpret_cast<const char*>(&header), sizeof(spec_header));

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 1;
    
    std::cout << "Using " << num_threads << " threads for chunked processing." << std::endl;

    const size_t CHUNK_SIZE = 500; // ~4MB in spectral results (500 * 2000 * 4)
    std::vector<std::vector<float>> chunk_results(CHUNK_SIZE);

    for (size_t chunk_start = 0; chunk_start < total_slices; chunk_start += CHUNK_SIZE) {
        size_t current_chunk_size = std::min(CHUNK_SIZE, total_slices - chunk_start);
        std::atomic<size_t> slices_in_chunk_done(0);

        auto worker = [&](size_t start, size_t size) {
            SpectralState local_state(N, fs);
            float* local_read_buf = (float*)malloc(sizeof(float) * N * channels);
            float* local_mono_buf = (float*)malloc(sizeof(float) * N);

            while (true) {
                size_t offset = slices_in_chunk_done.fetch_add(1);
                if (offset >= size) break;

                size_t abs_slice = start + offset;
                drwav_uint64 start_frame = (drwav_uint64)abs_slice * N;
                
                // We need to seek because multiple threads share the same file handle
                // Although it's slower, it keeps memory usage LOW.
                {
                    static std::mutex file_mutex;
                    std::lock_guard<std::mutex> lock(file_mutex);
                    drwav_seek_to_pcm_frame(wv->pWav, start_frame);
                    drwav_read_pcm_frames_f32(wv->pWav, N, local_read_buf);
                }

                for (int i = 0; i < N; i++) {
                    local_mono_buf[i] = (channels == 2) ? 
                        (local_read_buf[i * 2] + local_read_buf[i * 2 + 1]) * 0.5f : local_read_buf[i];
                }

                chunk_results[offset] = hybrid_millidecade_processor_stateful(&local_state, local_mono_buf);
            }
            free(local_read_buf);
            free(local_mono_buf);
        };

        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < num_threads; i++) {
            threads.emplace_back(worker, chunk_start, current_chunk_size);
        }
        for (auto& t : threads) t.join();

        // Dump chunk to disk
        for (size_t i = 0; i < current_chunk_size; i++) {
            outFile.write(reinterpret_cast<const char*>(chunk_results[i].data()), num_bins * sizeof(float));
            chunk_results[i].clear(); // Free memory immediately
        }

        print_progress(chunk_start + current_chunk_size, total_slices);
    }

    std::cout << "\r[" << std::string(40, '#') << "] 100%\n";
    outFile.close();
}

void stream_hmsp_to_disk(wav_file* wv, const char* out_filename, SpectralProcessor process_unused) {
    if (!wv || !wv->pWav) return;

    const int fs = wv->fs;
    int N = (int)(0.200f * fs); 
    if (N < 1024) N = 1024;
    const int channels = wv->pWav->channels;
    drwav_uint64 total_frames = wv->pWav->totalPCMFrameCount;
    size_t total_slices = (size_t)((total_frames + N - 1) / N);

    SpectralState state_init(N, fs);
    const int num_bins = state_init.total_bins;

    std::vector<float> min_vals(num_bins, std::numeric_limits<float>::max());
    std::vector<float> max_vals(num_bins, std::numeric_limits<float>::lowest());
    const int num_buckets = 256;
    std::vector<std::vector<uint32_t>> histograms(num_bins, std::vector<uint32_t>(num_buckets, 0));

    unsigned int num_threads = std::thread::hardware_concurrency();
    if (num_threads == 0) num_threads = 1;

    const size_t CHUNK_SIZE = 500;
    std::vector<std::vector<float>> chunk_slices(CHUNK_SIZE);

    // --- PASS 1: Find Min/Max ---
    std::cout << "Pass 1/2: Analyzing dynamic range..." << std::endl;
    for (size_t chunk_start = 0; chunk_start < total_slices; chunk_start += CHUNK_SIZE) {
        size_t current_chunk_size = std::min(CHUNK_SIZE, total_slices - chunk_start);
        std::atomic<size_t> slices_done(0);

        auto worker = [&](size_t start, size_t size) {
            SpectralState local_state(N, fs);
            float* rbuf = (float*)malloc(sizeof(float) * N * channels);
            float* mbuf = (float*)malloc(sizeof(float) * N);
            while (true) {
                size_t offset = slices_done.fetch_add(1);
                if (offset >= size) break;
                {
                    static std::mutex file_mutex;
                    std::lock_guard<std::mutex> lock(file_mutex);
                    drwav_seek_to_pcm_frame(wv->pWav, (drwav_uint64)(start + offset) * N);
                    drwav_read_pcm_frames_f32(wv->pWav, N, rbuf);
                }
                for (int i = 0; i < N; i++) mbuf[i] = (channels == 2) ? (rbuf[i*2] + rbuf[i*2+1])*0.5f : rbuf[i];
                chunk_slices[offset] = hybrid_millidecade_processor_stateful(&local_state, mbuf);
            }
            free(rbuf); free(mbuf);
        };

        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < num_threads; i++) threads.emplace_back(worker, chunk_start, current_chunk_size);
        for (auto& t : threads) t.join();

        for (size_t i = 0; i < current_chunk_size; i++) {
            for (int b = 0; b < num_bins; b++) {
                if (chunk_slices[i][b] < min_vals[b]) min_vals[b] = chunk_slices[i][b];
                if (chunk_slices[i][b] > max_vals[b]) max_vals[b] = chunk_slices[i][b];
            }
            chunk_slices[i].clear();
        }
        print_progress(chunk_start + current_chunk_size, total_slices);
    }
    std::cout << std::endl;

    // --- PASS 2: Histograms ---
    std::cout << "Pass 2/2: Computing distribution..." << std::endl;
    for (size_t chunk_start = 0; chunk_start < total_slices; chunk_start += CHUNK_SIZE) {
        size_t current_chunk_size = std::min(CHUNK_SIZE, total_slices - chunk_start);
        std::atomic<size_t> slices_done(0);

        auto worker = [&](size_t start, size_t size) {
            SpectralState local_state(N, fs);
            float* rbuf = (float*)malloc(sizeof(float) * N * channels);
            float* mbuf = (float*)malloc(sizeof(float) * N);
            while (true) {
                size_t offset = slices_done.fetch_add(1);
                if (offset >= size) break;
                {
                    static std::mutex file_mutex;
                    std::lock_guard<std::mutex> lock(file_mutex);
                    drwav_seek_to_pcm_frame(wv->pWav, (drwav_uint64)(start + offset) * N);
                    drwav_read_pcm_frames_f32(wv->pWav, N, rbuf);
                }
                for (int i = 0; i < N; i++) mbuf[i] = (channels == 2) ? (rbuf[i*2] + rbuf[i*2+1])*0.5f : rbuf[i];
                chunk_slices[offset] = hybrid_millidecade_processor_stateful(&local_state, mbuf);
            }
            free(rbuf); free(mbuf);
        };

        std::vector<std::thread> threads;
        for (unsigned int i = 0; i < num_threads; i++) threads.emplace_back(worker, chunk_start, current_chunk_size);
        for (auto& t : threads) t.join();

        for (size_t i = 0; i < current_chunk_size; i++) {
            for (int b = 0; b < num_bins; b++) {
                float range = max_vals[b] - min_vals[b];
                int bucket = 0;
                if (range > 0) {
                    float norm = (chunk_slices[i][b] - min_vals[b]) / range;
                    bucket = (int)(norm * (num_buckets - 1));
                    bucket = std::max(0, std::min(num_buckets - 1, bucket));
                }
                histograms[b][bucket]++;
            }
            chunk_slices[i].clear();
        }
        print_progress(chunk_start + current_chunk_size, total_slices);
    }
    std::cout << std::endl;

    // --- CALCULATE PERCENTILES & WRITE ---
    std::vector<float> percents = {1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 75.0f, 90.0f, 95.0f, 99.0f};
    std::vector<std::vector<float>> percentiles_results(num_bins, std::vector<float>(percents.size()));

    for (int i = 0; i < num_bins; ++i) {
        uint32_t total = 0;
        for (uint32_t count : histograms[i]) total += count;
        uint32_t cumulative = 0;
        size_t p_idx = 0;
        for (int b = 0; b < num_buckets && p_idx < percents.size(); ++b) {
            cumulative += histograms[i][b];
            float threshold = (percents[p_idx] / 100.0f) * total;
            while (p_idx < percents.size() && (float)cumulative >= threshold) {
                float ratio = (float)b / (num_buckets - 1);
                percentiles_results[i][p_idx] = min_vals[i] + ratio * (max_vals[i] - min_vals[i]);
                p_idx++;
                if (p_idx < percents.size()) threshold = (percents[p_idx] / 100.0f) * total;
            }
        }
    }

    spec_header header = { fs, num_bins, 20.0f, (float)fs/2.0f, 0.0f, 0.0f };
    write_hmsp(out_filename, header, percentiles_results);
}