#pragma once

#include <vector>

namespace audio
{
    struct AudioData
    {
        int sampleRate;
        int channels;
        std::vector<float> samples;
    };

    AudioData loadAudioFile(const char *filename);
}