#include <iostream>
#include <stdio.h>
#include <sndfile.h>
#include "audio/audio_reader.h"

int main(int argc, char *argv[]) {
    std::cout << "Hello Chords!" << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: ./chord_recognition <audio_file>" << std::endl;
        return 1;
    }

    const char *filename = argv[1];

    try {
        audio::AudioData audioData = audio::loadAudioFile(filename);
    } catch (const std::runtime_error& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}