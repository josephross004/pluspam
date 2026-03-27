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

std::pair<spec_header, std::vector<std::vector<float>>> read_hmsb(const char* filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open " << filename << std::endl;
        return {spec_header{}, {}};
    }

    spec_header header;
    in.read(reinterpret_cast<char*>(&header), sizeof(spec_header));
    if (!in) {
        std::cerr << "Failed to read header" << std::endl;
        return {spec_header{}, {}};
    }

    int num_bins = header.num_bins;
    std::vector<std::vector<float>> data;
    while (in) {
        std::vector<float> slice(num_bins);
        in.read(reinterpret_cast<char*>(slice.data()), num_bins * sizeof(float));
        if (in.gcount() == static_cast<std::streamsize>(num_bins * sizeof(float))) {
            data.push_back(slice);
        } else {
            break;
        }
    }
    return {header, data};
}

std::vector<std::vector<float>> compute_percentiles(const std::vector<std::vector<float>>& data) {
    if (data.empty()) return {};
    int num_bins = data[0].size();
    int num_slices = data.size();
    std::vector<std::vector<float>> percentiles(num_bins);
    std::vector<float> percents = {1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 75.0f, 90.0f, 95.0f, 99.0f};
    for (int bin = 0; bin < num_bins; ++bin) {
        if (bin % (std::max(1, num_bins / 100)) == 0) {
            print_progress(bin, num_bins);
        }
        std::vector<float> values;
        values.reserve(num_slices);
        for (const auto& slice : data) {
            values.push_back(slice[bin]);
        }
        std::sort(values.begin(), values.end());
        for (float p : percents) {
            size_t idx = static_cast<size_t>(std::floor((p / 100.0f) * (num_slices - 1)));
            percentiles[bin].push_back(values[idx]);
        }
    }
    std::cout << "\r" << std::endl;
    return percentiles;
}

std::pair<spec_header, std::vector<std::vector<float>>> compute_percentiles_streaming(const char* filename) {
    std::ifstream in(filename, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open " << filename << std::endl;
        return {spec_header{}, {}};
    }
    
    spec_header header;
    in.read(reinterpret_cast<char*>(&header), sizeof(spec_header));
    int num_bins = header.num_bins;
    
    // Pass 1: Find min/max for each bin
    std::vector<float> minv(num_bins, std::numeric_limits<float>::max());
    std::vector<float> maxv(num_bins, std::numeric_limits<float>::lowest());
    
    // Buffer reading for speed (1000 slices at a time)
    const int read_chunk = 1000;
    std::vector<float> buffer(num_bins * read_chunk);
    
    while (in.read(reinterpret_cast<char*>(buffer.data()), num_bins * read_chunk * sizeof(float)) || in.gcount() > 0) {
        int slices_read = in.gcount() / (num_bins * sizeof(float));
        for (int s = 0; s < slices_read; ++s) {
            float* slice = &buffer[s * num_bins];
            for (int i = 0; i < num_bins; ++i) {
                float v = slice[i];
                if (v < minv[i]) minv[i] = v;
                if (v > maxv[i]) maxv[i] = v;
            }
        }
        if (in.eof()) break;
    }
    
    // Pass 2: Fill histograms
    const int buckets = 100;
    std::vector<uint32_t> hist(num_bins * buckets, 0); 
    
    in.clear();
    in.seekg(sizeof(spec_header));
    while (in.read(reinterpret_cast<char*>(buffer.data()), num_bins * read_chunk * sizeof(float)) || in.gcount() > 0) {
        int slices_read = in.gcount() / (num_bins * sizeof(float));
        for (int s = 0; s < slices_read; ++s) {
            float* slice = &buffer[s * num_bins];
            for (int i = 0; i < num_bins; ++i) {
                float range = maxv[i] - minv[i];
                int idx = 0;
                if (range > 0.0f) {
                    float norm = (slice[i] - minv[i]) / range;
                    idx = (int)(norm * (buckets - 1));
                    idx = std::max(0, std::min(buckets - 1, idx));
                }
                hist[i * buckets + idx]++; 
            }
        }
        if (in.eof()) break;
    }
    
    // Pass 3: Compute percentiles
    std::vector<float> percents = {1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 75.0f, 90.0f, 95.0f, 99.0f};
    std::vector<std::vector<float>> percentiles(num_bins, std::vector<float>(percents.size()));
    
    for (int i = 0; i < num_bins; ++i) {
        uint32_t total = 0;
        for (int b = 0; b < buckets; ++b) {
            total += hist[i * buckets + b];
        }
        
        uint32_t cum = 0;
        size_t pi = 0;
        for (int b = 0; b < buckets && pi < percents.size(); ++b) {
            cum += hist[i * buckets + b]; 
            float threshold = (percents[pi] / 100.0f) * total;
            while (pi < percents.size() && (float)cum >= threshold) {
                float ratio = (float)b / (buckets - 1);
                percentiles[i][pi] = minv[i] + ratio * (maxv[i] - minv[i]);
                pi++;
                if (pi < percents.size()) threshold = (percents[pi] / 100.0f) * total;
            }
        }
    }
    return {header, percentiles};
}

void write_hmsp(const char* filename, const spec_header& header, const std::vector<std::vector<float>>& percentiles) {
    float min_val = std::numeric_limits<float>::max();
    float max_val = std::numeric_limits<float>::lowest();
    for (const auto& bin : percentiles) {
        for (float p : bin) {
            min_val = std::min(min_val, p);
            max_val = std::max(max_val, p);
        }
    }
    spec_header header_with_minmax = {header.sample_rate, header.num_bins, header.min_freq, header.max_freq, min_val, max_val};
    
    std::ofstream out(filename, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open " << filename << std::endl;
        return;
    }
    out.write(reinterpret_cast<const char*>(&header_with_minmax), sizeof(spec_header));
    for (const auto& bin_perc : percentiles) {
        for (float p : bin_perc) {
            uint8_t val = (uint8_t)std::round((p - min_val) / (max_val - min_val) * 255.0f);
            out.write(reinterpret_cast<const char*>(&val), sizeof(uint8_t));
        }
    }
}