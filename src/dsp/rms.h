#pragma once

#include <vector> 

namespace dsp 
{
    std::vector<float> computeRMS(const std::vector<float>& samples, int windowSize);
}