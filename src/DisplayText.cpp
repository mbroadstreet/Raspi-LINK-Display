#include "DisplayText.h"

#include <iomanip>
#include <sstream>
#include <string>

namespace DisplayText
{
    std::string formatStatusLine(const LinkDisplayState& state)
    {
        if (!state.linkEnabled)
        {
            return "LINK Inactive";
        }

        if (state.remotePeers == 0)
        {
            return "LINK Active · No Peers";
        }

        return "LINK Network Devices: " + std::to_string(state.remotePeers);
    }

    std::string formatTempoLine(const LinkDisplayState& state)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << state.tempoBpm;
        return out.str();
    }

    std::string formatBeatPhaseLine(const LinkDisplayState& state)
    {
        std::ostringstream out;
        out << "Beat " << std::fixed << std::setprecision(1) << state.beat
            << " · Phase " << std::fixed << std::setprecision(1) << state.phase
            << " / " << std::fixed << std::setprecision(0) << state.quantum;
        return out.str();
    }
}
