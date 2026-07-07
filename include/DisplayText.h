#pragma once
#include <string>

class DisplayText {
public:
    static std::string statusLine(bool enabled, int peers);
    static std::string tempoLine(double tempo);
    static std::string beatPhaseLine(double beat, double phase, double quantum);
};