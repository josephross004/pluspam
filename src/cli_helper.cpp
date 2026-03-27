#include <fftw3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

// simple terminal progress bar helper
static void print_progress(size_t current, size_t total) {
    const int barWidth = 40;
    float fraction = total ? (float)current / (float)total : 0.0f;
    int filled   = (int)std::round(barWidth * fraction);
    std::cout << "\r[";
    for (int i = 0; i < barWidth; ++i) {
        if (i < filled) std::cout << '#';
        else std::cout << '-';
    }
    std::cout << "] " << (int)(fraction * 100.0f) << "%";
    std::cout.flush();
}