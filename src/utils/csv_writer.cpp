#include <fstream>

#include "../dsp/rms.h"

namespace utils 
{
    void writeRMSValuesToCSV(const std::string& filename, const std::vector<dsp::RMSPoint>& rmsPoints) 
    {
        std::ofstream outFile(filename);
        if (!outFile) {
            throw std::runtime_error("Could not open file for writing: " + filename);
        }

        for (const dsp::RMSPoint &point : rmsPoints) {
            outFile << point.time << "," << point.rms << "\n";
        }
    }
}