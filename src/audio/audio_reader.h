#pragma once

#include <vector>

struct AudioData 
{
    int sampleRate;
    int channels;
    std::vector<float> samples;
};

AudioData loadAudioFile(const char* filename);