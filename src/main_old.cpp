#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include <iostream>
#include <vector>
#include <fftw3.h>
#include <cmath>
#include <fstream>

int main() {
    const char* input_path = "/work/pluspam/test_files/test.wav";
    const char* output_path = "spectrogram.csv";

    drwav wav;
    if (!drwav_init_file(&wav, input_path, NULL)) {
        std::cerr << "ERROR: File not found at " << input_path << std::endl;
        return -1;
    }

    std::cout << "Loading " << wav.totalPCMFrameCount << " samples..." << std::endl;
    std::vector<float> pSampleData(wav.totalPCMFrameCount);
    drwav_read_pcm_frames(&wav, wav.totalPCMFrameCount, pSampleData.data());
    drwav_uninit(&wav);

    int fs = 5000;
    int win_len = fs * 10;      // 50,000 samples
    int hop_size = win_len / 2; // 25,000 samples overlap

    if (pSampleData.size() < (size_t)win_len) {
        std::cerr << "ERROR: File is shorter than the 10s window!" << std::endl;
        return -1;
    }

    float* in = (float*)fftwf_malloc(sizeof(float) * win_len);
    fftwf_complex* out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * (win_len / 2 + 1));
    fftwf_plan p = fftwf_plan_dft_r2c_1d(win_len, in, out, FFTW_ESTIMATE | FFTW_NO_SIMD);

    std::ofstream outFile(output_path);
    if (!outFile.is_open()) {
        std::cerr << "ERROR: Could not create " << output_path << std::endl;
        return -1;
    }
    outFile << "Time_Sec,Freq_Hz,Mag\n";

    int numWindows = (pSampleData.size() - win_len) / hop_size + 1;
    std::cout << "Starting processing for " << numWindows << " windows..." << std::endl;

    for (int w = 0; w < numWindows; ++w) {
        int start = w * hop_size;
        for (int i = 0; i < win_len; ++i) {
            float hann = 0.5f * (1.0f - cosf(2.0f * M_PI * i / (win_len - 1)));
            in[i] = pSampleData[start + i] * hann;
        }

        fftwf_execute(p);

        float timeCenter = (start + (win_len / 2)) / (float)fs;
        int linesWritten = 0;

        for (int i = 0; i < (win_len / 2 + 1); ++i) {
            float freq = (i * (float)fs) / win_len;
            if (freq <= 2000.0f) {
                float mag = sqrtf(out[i][0]*out[i][0] + out[i][1]*out[i][1]);
                
                // Temporary: Write everything to ensure output works
                outFile << timeCenter << "," << freq << "," << mag << "\n";
                linesWritten++;
            }
        }
        // std::cout << "Window " << w << " processed. Wrote " << linesWritten << " frequency bins." << std::endl;
    }

    std::cout << "Processing complete. Closing file..." << std::endl;
    fftwf_destroy_plan(p);
    fftwf_free(in);
    fftwf_free(out);
    outFile.close();

    return 0;
}