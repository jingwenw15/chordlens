#include "chord_detector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace dsp {
namespace {
constexpr int kFrameSize = 8192;
constexpr int kHopSize = 4096;
constexpr std::array<const char*, 12> kPitchNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct Template { const char* suffix; std::vector<int> intervals; };
const std::vector<Template> kTemplates = {
    {"", {0, 4, 7}}, {"m", {0, 3, 7}}, {"7", {0, 4, 7, 10}},
    {"maj7", {0, 4, 7, 11}}, {"m7", {0, 3, 7, 10}}, {"sus4", {0, 5, 7}}, {"dim", {0, 3, 6}},
};

struct Frame { double time; double rms; std::array<float, 12> chroma; int root = -1; std::vector<int> intervals; std::string label; int confidence = 0; };

int pitchClass(int note) { return (note % 12 + 12) % 12; }

void fft(std::vector<std::complex<float>>& values) {
    const int n = static_cast<int>(values.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(values[i], values[j]);
    }
    for (int length = 2; length <= n; length <<= 1) {
        const float angle = -2.0f * static_cast<float>(M_PI) / length;
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (int start = 0; start < n; start += length) {
            std::complex<float> rotation(1.0f, 0.0f);
            for (int offset = 0; offset < length / 2; ++offset) {
                const auto even = values[start + offset];
                const auto odd = values[start + offset + length / 2] * rotation;
                values[start + offset] = even + odd;
                values[start + offset + length / 2] = even - odd;
                rotation *= step;
            }
        }
    }
}

Frame analyseFrame(const std::vector<float>& mono, int start, int sampleRate) {
    Frame frame{};
    double mean = 0.0, squares = 0.0;
    for (int i = 0; i < kFrameSize; ++i) { mean += mono[start + i]; squares += mono[start + i] * mono[start + i]; }
    mean /= kFrameSize;
    frame.rms = std::sqrt(squares / kFrameSize);
    std::vector<std::complex<float>> spectrum(kFrameSize);
    for (int i = 0; i < kFrameSize; ++i) {
        const float window = 0.5f - 0.5f * std::cos(2.0f * static_cast<float>(M_PI) * i / (kFrameSize - 1));
        spectrum[i] = static_cast<float>((mono[start + i] - mean) * window);
    }
    fft(spectrum);
    for (int bin = 2; bin < kFrameSize / 2; ++bin) {
        const double frequency = static_cast<double>(bin) * sampleRate / kFrameSize;
        if (frequency < 55.0 || frequency > 1800.0) continue;
        const double midi = 69.0 + 12.0 * std::log2(frequency / 440.0);
        const int lower = static_cast<int>(std::floor(midi));
        const float fraction = static_cast<float>(midi - lower);
        const float magnitude = std::abs(spectrum[bin]) / std::sqrt(static_cast<float>(frequency));
        frame.chroma[pitchClass(lower)] += magnitude * (1.0f - fraction);
        frame.chroma[pitchClass(lower + 1)] += magnitude * fraction;
    }
    double norm = 0.0;
    for (float value : frame.chroma) norm += value * value;
    norm = std::sqrt(norm);
    if (norm > 0.0) for (float& value : frame.chroma) value = static_cast<float>(value / norm);
    return frame;
}

Frame classify(Frame frame) {
    double bestScore = -1e9, runnerUp = -1e9;
    for (int root = 0; root < 12; ++root) for (const auto& chord : kTemplates) {
        std::array<float, 12> profile{};
        for (int interval : chord.intervals) profile[pitchClass(root + interval)] = interval == 0 ? 1.25f : 1.0f;
        double norm = 0.0; for (float value : profile) norm += value * value; norm = std::sqrt(norm);
        double score = 0.0; for (int pc = 0; pc < 12; ++pc) score += frame.chroma[pc] * profile[pc] / norm;
        score -= 0.03 * (static_cast<int>(chord.intervals.size()) - 3); // avoid calling plain triads sevenths from their harmonics
        if (score > bestScore) {
            runnerUp = bestScore; bestScore = score; frame.root = root; frame.intervals = chord.intervals;
            frame.label = std::string(kPitchNames[root]) + chord.suffix;
        } else if (score > runnerUp) runnerUp = score;
    }
    frame.confidence = std::clamp(static_cast<int>(std::lround(42.0 + std::max(0.0, bestScore - runnerUp) * 260.0)), 0, 99);
    return frame;
}

} // namespace

ChordAnalysis detectChords(const std::vector<float>& interleavedSamples, int channels, int sampleRate) {
    if (channels < 1 || sampleRate < 1) throw std::invalid_argument("Invalid audio format.");
    const size_t frameCount = interleavedSamples.size() / channels;
    if (frameCount < kFrameSize) throw std::invalid_argument("Audio is too short; provide at least one second.");
    std::vector<float> mono(frameCount);
    for (size_t i = 0; i < frameCount; ++i) for (int channel = 0; channel < channels; ++channel) mono[i] += interleavedSamples[i * channels + channel] / channels;
    std::vector<Frame> frames;
    double peakRms = 0.0;
    for (size_t start = 0; start + kFrameSize <= mono.size(); start += kHopSize) {
        auto frame = analyseFrame(mono, static_cast<int>(start), sampleRate); frame.time = static_cast<double>(start) / sampleRate; peakRms = std::max(peakRms, frame.rms); frames.push_back(std::move(frame));
    }
    const double silenceThreshold = std::max(0.003, peakRms * 0.12);
    for (auto& frame : frames) if (frame.rms >= silenceThreshold) frame = classify(std::move(frame)); else frame.label = "No chord";
    std::vector<Frame> smoothed;
    for (int i = 0; i < static_cast<int>(frames.size()); ++i) {
        std::array<int, 5> matching{}; std::vector<std::string> labels;
        for (int j = std::max(0, i - 2); j < std::min(static_cast<int>(frames.size()), i + 3); ++j) labels.push_back(frames[j].label);
        std::string selected = labels.front(); int count = 0;
        for (const auto& label : labels) { const int current = static_cast<int>(std::count(labels.begin(), labels.end(), label)); if (current > count) { selected = label; count = current; } }
        auto source = std::find_if(frames.begin() + std::max(0, i - 2), frames.begin() + std::min(static_cast<int>(frames.size()), i + 3), [&](const Frame& item) { return item.label == selected; });
        smoothed.push_back(*source);
        smoothed.back().time = frames[i].time;
    }
    ChordAnalysis result{static_cast<double>(frameCount) / sampleRate, {}};
    const double hopSeconds = static_cast<double>(kHopSize) / sampleRate;
    bool previousWasSilence = true;
    for (const auto& frame : smoothed) {
        if (frame.label == "No chord") { previousWasSilence = true; continue; }
        if (!previousWasSilence && !result.segments.empty() && result.segments.back().label == frame.label) {
            auto& segment = result.segments.back(); segment.end = frame.time + hopSeconds; segment.confidence += frame.confidence;
        } else result.segments.push_back({frame.label, frame.root, frame.intervals, frame.time, frame.time + hopSeconds, frame.confidence});
        previousWasSilence = false;
    }
    // Merge confidence is accumulated above; calculate an average from frame count by duration.
    for (auto& segment : result.segments) {
        const int count = std::max(1, static_cast<int>(std::lround((segment.end - segment.start) / hopSeconds)));
        segment.confidence = std::clamp(segment.confidence / count, 0, 99); segment.end = std::min(segment.end, result.duration);
    }
    result.segments.erase(std::remove_if(result.segments.begin(), result.segments.end(), [](const ChordSegment& segment) { return segment.end - segment.start < 0.45; }), result.segments.end());
    return result;
}

std::string analysisToJson(const ChordAnalysis& analysis) {
    std::ostringstream out; out << std::fixed << std::setprecision(3) << "{\"duration\":" << analysis.duration << ",\"segments\":[";
    for (size_t i = 0; i < analysis.segments.size(); ++i) {
        const auto& segment = analysis.segments[i]; if (i) out << ',';
        out << "{\"label\":\"" << segment.label << "\",\"root\":" << segment.root << ",\"intervals\":[";
        for (size_t j = 0; j < segment.intervals.size(); ++j) { if (j) out << ','; out << segment.intervals[j]; }
        out << "],\"start\":" << segment.start << ",\"end\":" << segment.end << ",\"confidence\":" << segment.confidence << '}';
    }
    out << "]}";
    return out.str();
}

} // namespace dsp
