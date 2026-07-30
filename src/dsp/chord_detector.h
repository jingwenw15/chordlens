#pragma once

#include <string>
#include <vector>

namespace dsp {

struct ChordSegment {
    std::string label;
    int root;
    std::vector<int> intervals;
    double start;
    double end;
    int confidence;
};

struct ChordAnalysis {
    double duration;
    std::vector<ChordSegment> segments;
};

// Analyses interleaved PCM samples and returns a smoothed chord timeline.
ChordAnalysis detectChords(const std::vector<float>& interleavedSamples, int channels, int sampleRate, int minimumStableFrames = 9);
std::string analysisToJson(const ChordAnalysis& analysis);

} // namespace dsp
