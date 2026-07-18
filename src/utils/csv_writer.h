#pragma once

#include <vector>
#include <string>
#include "../dsp/rms.h"

namespace utils 
{
    void writeToCSV(const std::string& filename, const std::vector<dsp::RMSPoint>& rmsPoints);
}