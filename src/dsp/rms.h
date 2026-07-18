#pragma once

#include <vector> 

namespace dsp 
{
    struct RMSPoint {
        float time;
        float rms;
    };

    std::vector<float> computeRMS(const std::vector<float>& samples, int windowSize);

    std::vector<RMSPoint> computeRMSOverTime(const std::vector<float>& samples, int windowSize, int sampleRate);
}