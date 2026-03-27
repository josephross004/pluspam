#include <iostream>
#include <cstdio> 
#include <string>
#include <utility>


int percentiles_streaming_p_in_args(int argc, char* argv[]){
    // Mode: going from hmsb to hmsp (streaming percentile calculation)
    std::cout << "Processing percentiles from: " << argv[2] << std::endl;
    auto [header, percentiles] = compute_percentiles_streaming(argv[2]);
    if (percentiles.empty()) {
        std::cerr << "Failed to compute percentiles from " << argv[2] << std::endl;
        return 1;
    }
    write_hmsp(argv[3], header, percentiles);
    std::cout << "Success! Output written to " << argv[3] << std::endl;
    return 0;
}

int out_to_hmsb_and_hmsp(int argc, char* argv[]){
    // Mode: wav to hmsb and hmsp
    wav_file* my_wav = create_wav(argv[1]);
    if (!my_wav) {
        std::cerr << "Failed to load WAV file." << std::endl;
        return 1;
    }

    std::cout << "Processing: " << argv[1] << std::endl;
    std::cout << "Sample Rate: " << my_wav->fs << " Hz" << std::endl;

    // Generate hmsb
    stream_to_disk(my_wav, argv[2], hybrid_millidecade_processor);

    destroy_wav(my_wav);

    // Now compute percentiles from the hmsb (streaming, low memory)
    std::cout << "Computing percentiles..." << std::endl;
    auto [header, percentiles] = compute_percentiles_streaming(argv[2]);
    if (percentiles.empty()) {
        std::cerr << "Failed to compute percentiles from generated " << argv[2] << std::endl;
        return 1;
    }
    write_hmsp(argv[3], header, percentiles);
    std::cout << "Success! Spectrogram written to " << argv[2] << ", percentiles to " << argv[3] << std::endl;
    return 0;
}

int out_to_hmsb(int argc, char* argv[]){
    // Mode: wav to hmsb
    wav_file* my_wav = create_wav(argv[1]);
    if (!my_wav) {
        std::cerr << "Failed to load WAV file." << std::endl;
        return 1;
    }

    std::cout << "Processing: " << argv[1] << std::endl;
    std::cout << "Sample Rate: " << my_wav->fs << " Hz" << std::endl;

    // We pass our specific algorithm into the streamer
    stream_to_disk(my_wav, argv[2], hybrid_millidecade_processor);

    destroy_wav(my_wav);
    std::cout << "Success! Output written to " << argv[2] << std::endl;
    return 0;
}

int simple_stream_directly_to_hmsp(int argc, char* argv[]){
    // Mode: wav to hmsp, no other byproducts
    wav_file* my_wav = create_wav(argv[2]);

    if(!my_wav) {
        std::cerr << "Failed to load WAV file." << std::endl;
    }

    std::cout << "Processing: " << argv[2] << std::endl;
    std::cout << "Sample Rate: " << my_wav->fs << " Hz" << std::endl;

    stream_hmsp_to_disk(my_wav, argv[3], hybrid_millidecade_processor);

    destroy_wav(my_wav);
    std::cout << "Success! Output written to " << argv[3] << std::endl;
    return 0;
}