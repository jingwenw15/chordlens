#include <iostream>
#include <stdio.h>
#include <sndfile.h>
#include <vector>
#include "audio/audio_reader.h"
#include "dsp/rms.h"
#include "utils/csv_writer.h"

int main(int argc, char *argv[]) {
    std::cout << "Hello Chords!" << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: ./chord_recognition <audio_file>" << std::endl;
        return 1;
    }

    const char *filename = argv[1];

    audio::AudioData audioData;
    try {
        audioData = audio::loadAudioFile(filename);
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    // std::vector<float> rmsValues;
    // try {
    //     rmsValues = dsp::computeRMS(audioData.samples, 1024);
    //     for (size_t i = 50; i < 100; ++i) {
    //         std::cout << "RMS value for window " << i << ": " << rmsValues[i] << std::endl;
    //     }
    // } catch (const std::invalid_argument& e) {
    //     std::cerr << e.what() << std::endl;
    //     return 1;
    // }

    std::vector<dsp::RMSPoint> rmsPoints;
    try {
        rmsPoints = dsp::computeRMSOverTime(audioData.samples, 1024, audioData.sampleRate);
        utils::writeToCSV("rms_values.csv", rmsPoints);
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}