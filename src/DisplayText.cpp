#include "DisplayText.h"
#include <iomanip>
#include <sstream>

std::string DisplayText::statusLine(bool enabled, int peers) {
    if (!enabled) return "LINK Inactive";
    if (peers == 0) return "LINK Active · No Peers";
    return "LINK Network Devices: " + std::to_string(peers);
}

std::string DisplayText::tempoLine(double tempo) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << tempo << " BPM";
    return oss.str();
}

std::string DisplayText::beatPhaseLine(double beat, double phase, double quantum) {
    std::ostringstream oss;
    oss << "Beat " << std::fixed << std::setprecision(1) << beat
        << " · Phase " << phase << " / " << static_cast<int>(quantum);
    return oss.str();
}