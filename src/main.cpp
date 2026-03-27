#include <iostream>
#include <cstdio> // Ensure this is here
#include <string>
#include <utility>

// Force the High-Level API (open/close) to be active
#define DRWAV_HAS_STDIO
#define DR_WAV_IMPLEMENTATION

#ifdef __cplusplus
extern "C" {
#endif
    #include "dr_wav.h"
#ifdef __cplusplus
}
#endif

// Include the logic after the library is fully defined
#include "wav_file.cpp"
#include "cli_helper.cpp"
#include "fft.cpp"
#include "percentiles.cpp"
#include "stream.cpp"
#include "process_routine.cpp"

int STATUS = 0;

int main(int argc, char* argv[]) {
    if (argc == 4 && std::string(argv[1]) == "-c") {
        STATUS = percentiles_streaming_p_in_args(argc,argv);
    } else if (argc == 4 && std::string(argv[1]) == "-p") {
        STATUS = simple_stream_directly_to_hmsp(argc, argv);
    } else if (argc == 4) {
        STATUS = out_to_hmsb_and_hmsp(argc, argv);
    } else if (argc == 3) {
        STATUS = out_to_hmsb(argc, argv);
    } else {
        std::cout << "Usage: ./spec_gen <input.wav> <output.hmsb>" << std::endl;
        std::cout << "Or: ./spec_gen <input.wav> <output.hmsb> <output.hmsp>" << std::endl;
        std::cout << "Or: ./spec_gen -c <input.hmsb> <output.hmsp>" << std::endl;
        std::cout << "Or: ./spec_gen -p <input.wav> <output.hmsp>" << std::endl;
        STATUS = -1;
    }
    fftwf_cleanup();
    return STATUS;
}