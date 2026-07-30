#include <iostream>
#include <string>
#include "audio/audio_reader.h"
#include "dsp/chord_detector.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: chord_recognition <audio_file> [--json] [--stability frames]" << std::endl;
        return 1;
    }

    const char *filename = argv[1];
    bool json = false;
    int stability = 9;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--json") json = true;
        else if (argument == "--stability" && index + 1 < argc) stability = std::stoi(argv[++index]);
        else { std::cerr << "Unknown option: " << argument << std::endl; return 1; }
    }

    audio::AudioData audioData;
    try {
        audioData = audio::loadAudioFile(filename);
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    try {
        const auto analysis = dsp::detectChords(audioData.samples, audioData.channels, audioData.sampleRate, stability);
        if (json) std::cout << dsp::analysisToJson(analysis) << std::endl;
        else for (const auto& chord : analysis.segments) std::cout << chord.start << "–" << chord.end << "  " << chord.label << " (" << chord.confidence << "%)\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
