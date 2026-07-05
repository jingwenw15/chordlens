#include <iostream>
#include <stdio.h>
#include <sndfile.h>

int main(int argc, char *argv[]) {
    std::cout << "Hello Chords!" << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: ./chord_recognition <audio_file>" << std::endl;
        return 1;
    }

    const char *filename = argv[1];
    SF_INFO sfinfo; 
    SNDFILE *file = sf_open(filename, SFM_READ, &sfinfo);

    if (!file) {
        std::cerr << "Error opening file: " << sf_strerror(NULL) << std::endl;
        return 1;
    }

    std::cout << "Sample rate: " << sfinfo.samplerate << std::endl;
    std::cout << "Channels: " << sfinfo.channels << std::endl;
    std::cout << "Frames: " << sfinfo.frames << std::endl;

    std::vector<float> samples(sfinfo.frames * sfinfo.channels); 
    sf_readf_float(file, samples.data(), sfinfo.frames);

    int err = sf_close(file);
    if (err != 0) {
        std::cerr << "Error closing file: " << sf_strerror(file) << std::endl;
    }

    std::cout << "First 10 samples: " << std::endl;
    for (int i = 0; i < 10; i++) {
        std::cout << samples[i] << std::endl;
    }

    return 0;
}