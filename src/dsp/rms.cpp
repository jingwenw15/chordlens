#include "rms.h"
#include <cmath>
#include <vector> 

namespace dsp 
{
    std::vector<float> computeRMS(const std::vector<float>& samples, int windowSize) 
    {
        if (samples.empty()) {
            throw std::invalid_argument("Input samples vector is empty.");
        }
        if (windowSize <= 0) {
            throw std::invalid_argument("Window size must be greater than zero.");
        }

        std::vector<float> rmsValues;
        size_t numWindows = samples.size() / windowSize;
        rmsValues.reserve(numWindows); 

        for (size_t i = 0; i < numWindows; ++i) {
            float sumOfSquares = 0.0f;
            for (size_t j = 0; j < windowSize; ++j) {
                float sample = samples[i * windowSize + j]; 
                sumOfSquares += sample * sample; 
            }
            float rms = std::sqrt(sumOfSquares / windowSize);
            rmsValues.push_back(rms);
        }

        // Note for future: if the number of samples is not a multiple of windowSize, the remaining samples are ignored
        return rmsValues;
    }

    std::vector<RMSPoint> computeRMSOverTime(const std::vector<float>& samples, int windowSize, int sampleRate)
    {
        std::vector<float> rmsValues = computeRMS(samples, windowSize);
        float secondsPerWindow = static_cast<float>(windowSize) / sampleRate;

        std::vector<RMSPoint> rmsPoints;
        rmsPoints.reserve(rmsValues.size());
        for (size_t i = 0; i < rmsValues.size(); ++i) {
            rmsPoints.push_back({i * secondsPerWindow, rmsValues[i]});
        }
        
        return rmsPoints;
    }
}