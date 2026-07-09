#include <iostream>
#include <stdio.h>
#include <sndfile.h>
#include "audio_reader.h"


AudioData loadAudioFile(const char* filename) {
    SF_INFO sfinfo; 
    SNDFILE *file = sf_open(filename, SFM_READ, &sfinfo);

    if (!file) {
        std::cerr << "Error opening file: " << sf_strerror(NULL) << std::endl;
        throw std::runtime_error("Error opening audio file");
    }

    std::cout << "Sample rate: " << sfinfo.samplerate << std::endl;
    std::cout << "Channels: " << sfinfo.channels << std::endl;
    std::cout << "Frames: " << sfinfo.frames << std::endl;

    std::vector<float> samples(sfinfo.frames * sfinfo.channels); 
    sf_readf_float(file, samples.data(), sfinfo.frames);

    int err = sf_close(file);
    if (err != 0) {
        std::cerr << "Error closing file: " << sf_strerror(NULL) << std::endl;
        throw std::runtime_error("Error closing audio file");
    }

    std::cout << "First 10 samples: " << std::endl;
    for (int i = 0; i < 10; i++) {
        std::cout << samples[i] << std::endl;
    }

    AudioData audioData;
    audioData.sampleRate = sfinfo.samplerate;
    audioData.channels = sfinfo.channels;
    audioData.samples = std::move(samples);

    return audioData;
}