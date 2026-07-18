#pragma once

#include <vector>
#include <string>
#include "../dsp/rms.h"

namespace utils 
{
    void writeRMSValuesToCSV(const std::string& filename, const std::vector<dsp::RMSPoint>& rmsPoints);
}