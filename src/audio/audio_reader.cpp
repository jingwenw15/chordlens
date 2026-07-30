#include <iostream>
#include <sndfile.h>
#include <stdexcept>
#include "audio_reader.h"

namespace audio
{
    AudioData loadAudioFile(const char *filename)
    {
        SF_INFO sfinfo{};
        SNDFILE *file = sf_open(filename, SFM_READ, &sfinfo);

        if (!file)
        {
            std::cerr << "Error opening file: " << sf_strerror(NULL) << std::endl;
            throw std::runtime_error("Error opening audio file");
        }

        std::vector<float> samples(sfinfo.frames * sfinfo.channels);
        if (sf_readf_float(file, samples.data(), sfinfo.frames) != sfinfo.frames) {
            sf_close(file);
            throw std::runtime_error("Could not read complete audio file");
        }

        int err = sf_close(file);
        if (err != 0)
        {
            std::cerr << "Error closing file: " << sf_strerror(NULL) << std::endl;
            throw std::runtime_error("Error closing audio file");
        }

        AudioData audioData;
        audioData.sampleRate = sfinfo.samplerate;
        audioData.channels = sfinfo.channels;
        audioData.samples = std::move(samples);

        return audioData;
    }
}
