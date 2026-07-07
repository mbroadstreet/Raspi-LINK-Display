#pragma once

#include "LinkDisplayState.h"

#include <string>

namespace DisplayText
{
    std::string formatStatusLine(const LinkDisplayState& state);
    std::string formatTempoLine(const LinkDisplayState& state);
    std::string formatBeatPhaseLine(const LinkDisplayState& state);
}
