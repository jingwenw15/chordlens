#include "chord_detector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace dsp {
namespace {
constexpr int kFrameSize = 8192;
constexpr int kHopSize = 4096;
constexpr double kMinFrequency = 55.0;
constexpr double kMaxFrequency = 1800.0;
constexpr double kCqtMinFrequency = 55.0;
constexpr double kCqtMaxFrequency = 1760.0;
constexpr int kCqtBinsPerOctave = 24;
constexpr double kCqtSigma = 0.72;
constexpr std::array<const char*, 12> kPitchNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

struct Template {
    const char* suffix;
    std::vector<int> intervals;
};

const std::vector<Template> kTemplates = {
    {"", {0, 4, 7}},
    {"m", {0, 3, 7}},
    {"7", {0, 4, 7, 10}},
    {"maj7", {0, 4, 7, 11}},
    {"m7", {0, 3, 7, 10}},
    {"sus4", {0, 5, 7}},
    {"dim", {0, 3, 6}},
};

struct Frame {
    double time = 0.0;
    double rms = 0.0;
    double harmonicity = 0.0;
    std::vector<float> spectrum;
    std::array<float, 12> chroma{};
    int root = -1;
    std::vector<int> intervals;
    std::string label;
    int confidence = 0;
};

struct ChordState {
    int root;
    std::vector<int> intervals;
    std::string label;
};

struct CqtBin {
    double frequency;
    int pitchClass;
};

int pitchClass(int note) {
    return (note % 12 + 12) % 12;
}

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

float medianOfThree(float a, float b, float c) {
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return b;
}

void fft(std::vector<std::complex<float>>& values) {
    const int n = static_cast<int>(values.size());
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(values[i], values[j]);
        }
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

std::vector<CqtBin> buildCqtBins() {
    const int binCount = static_cast<int>(std::floor(std::log2(kCqtMaxFrequency / kCqtMinFrequency) * kCqtBinsPerOctave)) + 1;
    std::vector<CqtBin> bins;
    bins.reserve(binCount);
    for (int index = 0; index < binCount; ++index) {
        const double frequency = kCqtMinFrequency * std::pow(2.0, static_cast<double>(index) / kCqtBinsPerOctave);
        const double midi = 69.0 + 12.0 * std::log2(frequency / 440.0);
        bins.push_back({frequency, pitchClass(static_cast<int>(std::lround(midi)))});
    }
    return bins;
}

const std::vector<CqtBin>& cqtBins() {
    static const std::vector<CqtBin> bins = buildCqtBins();
    return bins;
}

Frame analyseFrame(const std::vector<float>& mono, int start, int sampleRate) {
    Frame frame{};
    double mean = 0.0;
    double squares = 0.0;
    for (int i = 0; i < kFrameSize; ++i) {
        mean += mono[start + i];
        squares += mono[start + i] * mono[start + i];
    }
    mean /= kFrameSize;
    frame.rms = std::sqrt(squares / kFrameSize);

    frame.spectrum.resize(kFrameSize / 2);
    std::vector<std::complex<float>> samples(kFrameSize);
    for (int i = 0; i < kFrameSize; ++i) {
        const float window = 0.5f - 0.5f * std::cos(2.0f * static_cast<float>(M_PI) * i / (kFrameSize - 1));
        samples[i] = static_cast<float>((mono[start + i] - mean) * window);
    }
    fft(samples);
    for (int bin = 0; bin < kFrameSize / 2; ++bin) {
        frame.spectrum[bin] = std::abs(samples[bin]);
    }
    return frame;
}

std::vector<float> buildHarmonicSpectrum(const std::vector<std::vector<float>>& spectra, size_t index, double& harmonicity) {
    const auto& current = spectra[index];
    std::vector<float> harmonic(current.size(), 0.0f);
    const size_t previousIndex = index > 0 ? index - 1 : index;
    const size_t nextIndex = index + 1 < spectra.size() ? index + 1 : index;
    const auto& previous = spectra[previousIndex];
    const auto& next = spectra[nextIndex];

    double harmonicEnergy = 0.0;
    double totalEnergy = 0.0;
    for (size_t bin = 0; bin < current.size(); ++bin) {
        const float baseline = medianOfThree(previous[bin], current[bin], next[bin]);
        const float consistency = baseline / (current[bin] + 1e-6f);
        const float harmonicWeight = static_cast<float>(0.42 + 0.58 * clamp01(consistency));
        const float value = current[bin] * harmonicWeight;
        harmonic[bin] = value;
        harmonicEnergy += value;
        totalEnergy += current[bin];
    }

    harmonicity = totalEnergy > 0.0 ? std::clamp(harmonicEnergy / totalEnergy, 0.0, 1.0) : 0.0;
    return harmonic;
}

void accumulateCqtChroma(const std::vector<float>& spectrum, int sampleRate, std::array<float, 12>& chroma) {
    const auto& bins = cqtBins();
    constexpr double kSupportRadius = 2.0;
    for (int fftBin = 2; fftBin < static_cast<int>(spectrum.size()); ++fftBin) {
        const double frequency = static_cast<double>(fftBin) * sampleRate / kFrameSize;
        if (frequency < kMinFrequency || frequency > kMaxFrequency) {
            continue;
        }

        const double magnitude = spectrum[fftBin] / std::sqrt(frequency);
        if (magnitude <= 0.0) {
            continue;
        }

        const double logPosition = std::log2(frequency / kCqtMinFrequency) * kCqtBinsPerOctave;
        const int center = static_cast<int>(std::lround(logPosition));
        for (int offset = -2; offset <= 2; ++offset) {
            const int cqtIndex = center + offset;
            if (cqtIndex < 0 || cqtIndex >= static_cast<int>(bins.size())) {
                continue;
            }
            const auto& cqtBin = bins[cqtIndex];
            const double logDistance = std::abs(std::log2(frequency / cqtBin.frequency)) * kCqtBinsPerOctave;
            if (logDistance > kSupportRadius) {
                continue;
            }
            const double weight = std::exp(-0.5 * std::pow(logDistance / kCqtSigma, 2.0));
            chroma[cqtBin.pitchClass] += static_cast<float>(magnitude * weight);
        }
    }
}

void normalizeChroma(std::array<float, 12>& chroma) {
    double norm = 0.0;
    for (float value : chroma) {
        norm += value * value;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0) {
        for (float& value : chroma) {
            value = static_cast<float>(value / norm);
        }
    }
}

const std::vector<ChordState>& chordStates() {
    static const std::vector<ChordState> states = [] {
        std::vector<ChordState> result;
        for (int root = 0; root < 12; ++root) {
            for (const auto& chord : kTemplates) {
                result.push_back({root, chord.intervals, std::string(kPitchNames[root]) + chord.suffix});
            }
        }
        return result;
    }();
    return states;
}

double scoreState(const std::array<float, 12>& chroma, double harmonicity, const ChordState& state) {
    std::array<float, 12> profile{};
    for (int interval : state.intervals) {
        profile[pitchClass(state.root + interval)] = interval == 0 ? 1.25f : 1.0f;
    }

    double norm = 0.0;
    for (float value : profile) {
        norm += value * value;
    }
    norm = std::sqrt(norm);

    double score = 0.0;
    for (int pc = 0; pc < 12; ++pc) {
        score += chroma[pc] * profile[pc] / norm;
    }

    // Transient-heavy frames are less trustworthy than harmonically stable ones.
    const double harmonicBoost = 0.78 + 0.22 * clamp01(harmonicity);
    const double complexityPenalty = 0.08 * (static_cast<int>(state.intervals.size()) - 3);
    return score * harmonicBoost - complexityPenalty;
}

void decodeChordRun(std::vector<Frame>& frames, int first, int last) {
    const auto& states = chordStates();
    const int stateCount = static_cast<int>(states.size());
    const int frameCount = last - first;
    constexpr double kChangePenalty = 0.18;

    std::vector<std::vector<int>> back(frameCount, std::vector<int>(stateCount));
    std::vector<double> previous(stateCount, -std::numeric_limits<double>::infinity());
    std::vector<double> current(stateCount, -std::numeric_limits<double>::infinity());
    std::vector<double> emissions(stateCount, -std::numeric_limits<double>::infinity());

    for (int offset = 0; offset < frameCount; ++offset) {
        const auto& frame = frames[first + offset];
        for (int state = 0; state < stateCount; ++state) {
            emissions[state] = scoreState(frame.chroma, frame.harmonicity, states[state]);
        }

        if (offset == 0) {
            current = emissions;
            std::iota(back[offset].begin(), back[offset].end(), 0);
        } else {
            const int bestPrevious = static_cast<int>(std::distance(previous.begin(), std::max_element(previous.begin(), previous.end())));
            for (int state = 0; state < stateCount; ++state) {
                const double stay = previous[state];
                const double change = previous[bestPrevious] - kChangePenalty;
                if (stay >= change) {
                    current[state] = emissions[state] + stay;
                    back[offset][state] = state;
                } else {
                    current[state] = emissions[state] + change;
                    back[offset][state] = bestPrevious;
                }
            }
        }

        previous.swap(current);
    }

    int state = static_cast<int>(std::distance(previous.begin(), std::max_element(previous.begin(), previous.end())));
    for (int offset = frameCount - 1; offset >= 0; --offset) {
        auto& frame = frames[first + offset];
        const auto& selected = states[state];
        frame.root = selected.root;
        frame.intervals = selected.intervals;
        frame.label = selected.label;

        const double selectedScore = scoreState(frame.chroma, frame.harmonicity, selected);
        double runnerUp = -std::numeric_limits<double>::infinity();
        for (int other = 0; other < stateCount; ++other) {
            if (other == state) {
                continue;
            }
            runnerUp = std::max(runnerUp, scoreState(frame.chroma, frame.harmonicity, states[other]));
        }
        frame.confidence = std::clamp(static_cast<int>(std::lround(42.0 + std::max(0.0, selectedScore - runnerUp) * 260.0)), 0, 99);
        state = back[offset][state];
    }
}

void mergeBriefChordRuns(std::vector<Frame>& frames, int minimumStableFrames) {
    struct Run {
        int first;
        int last;
    };

    std::vector<Run> runs;
    for (int index = 0; index < static_cast<int>(frames.size());) {
        if (frames[index].label == "No chord") {
            ++index;
            continue;
        }
        const int first = index;
        const std::string label = frames[index].label;
        while (index < static_cast<int>(frames.size()) && frames[index].label == label) {
            ++index;
        }
        runs.push_back({first, index});
    }

    if (runs.size() < 2) {
        return;
    }

    for (int run = 0; run < static_cast<int>(runs.size()); ++run) {
        if (runs[run].last - runs[run].first >= minimumStableFrames) {
            continue;
        }

        const int replacement = run > 0 ? runs[run - 1].first : runs[run + 1].first;
        const Frame& chosen = frames[replacement];
        for (int index = runs[run].first; index < runs[run].last; ++index) {
            frames[index].root = chosen.root;
            frames[index].intervals = chosen.intervals;
            frames[index].label = chosen.label;
            frames[index].confidence = chosen.confidence;
        }
    }
}

} // namespace

ChordAnalysis detectChords(const std::vector<float>& interleavedSamples, int channels, int sampleRate, int minimumStableFrames) {
    if (channels < 1 || sampleRate < 1) {
        throw std::invalid_argument("Invalid audio format.");
    }
    if (minimumStableFrames < 1 || minimumStableFrames > 30) {
        throw std::invalid_argument("Stability must be between 1 and 30 frames.");
    }

    const size_t frameCount = interleavedSamples.size() / channels;
    if (frameCount < kFrameSize) {
        throw std::invalid_argument("Audio is too short; provide at least one second.");
    }

    std::vector<float> mono(frameCount, 0.0f);
    for (size_t i = 0; i < frameCount; ++i) {
        for (int channel = 0; channel < channels; ++channel) {
            mono[i] += interleavedSamples[i * channels + channel] / channels;
        }
    }

    std::vector<Frame> frames;
    double peakRms = 0.0;
    for (size_t start = 0; start + kFrameSize <= mono.size(); start += kHopSize) {
        auto frame = analyseFrame(mono, static_cast<int>(start), sampleRate);
        frame.time = static_cast<double>(start) / sampleRate;
        peakRms = std::max(peakRms, frame.rms);
        frames.push_back(std::move(frame));
    }

    const double silenceThreshold = std::max(0.003, peakRms * 0.12);
    for (auto& frame : frames) {
        if (frame.rms < silenceThreshold) {
            frame.label = "No chord";
        }
    }

    std::vector<std::vector<float>> rawSpectra;
    rawSpectra.reserve(frames.size());
    for (const auto& frame : frames) {
        rawSpectra.push_back(frame.spectrum);
    }

    for (size_t index = 0; index < frames.size(); ++index) {
        if (frames[index].label == "No chord") {
            continue;
        }
        frames[index].spectrum = buildHarmonicSpectrum(rawSpectra, index, frames[index].harmonicity);
        accumulateCqtChroma(frames[index].spectrum, sampleRate, frames[index].chroma);
        normalizeChroma(frames[index].chroma);
    }

    for (int first = 0; first < static_cast<int>(frames.size());) {
        while (first < static_cast<int>(frames.size()) && frames[first].label == "No chord") {
            ++first;
        }
        const int last = first;
        while (first < static_cast<int>(frames.size()) && frames[first].label != "No chord") {
            ++first;
        }
        if (last < first) {
            decodeChordRun(frames, last, first);
        }
    }

    mergeBriefChordRuns(frames, minimumStableFrames);

    ChordAnalysis result{static_cast<double>(frameCount) / sampleRate, {}};
    const double hopSeconds = static_cast<double>(kHopSize) / sampleRate;
    for (const auto& frame : frames) {
        // Silence does not form its own chord segment. Consecutive frames are
        // merged until the recognized chord label changes.
        if (frame.label == "No chord") {
            continue;
        }
        if (!result.segments.empty() && result.segments.back().label == frame.label) {
            auto& segment = result.segments.back();
            segment.end = frame.time + hopSeconds;
            segment.confidence += frame.confidence;
        } else {
            result.segments.push_back({frame.label, frame.root, frame.intervals, frame.time, frame.time + hopSeconds, frame.confidence});
        }
    }

    for (auto& segment : result.segments) {
        const int count = std::max(1, static_cast<int>(std::lround((segment.end - segment.start) / hopSeconds)));
        segment.confidence = std::clamp(segment.confidence / count, 0, 99);
        segment.end = std::min(segment.end, result.duration);
    }

    return result;
}

std::string analysisToJson(const ChordAnalysis& analysis) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3) << "{\"duration\":" << analysis.duration << ",\"segments\":[";
    for (size_t i = 0; i < analysis.segments.size(); ++i) {
        const auto& segment = analysis.segments[i];
        if (i) {
            out << ',';
        }
        out << "{\"label\":\"" << segment.label << "\",\"root\":" << segment.root << ",\"intervals\":[";
        for (size_t j = 0; j < segment.intervals.size(); ++j) {
            if (j) {
                out << ',';
            }
            out << segment.intervals[j];
        }
        out << "],\"start\":" << segment.start << ",\"end\":" << segment.end << ",\"confidence\":" << segment.confidence << '}';
    }
    out << "]}";
    return out.str();
}

} // namespace dsp
