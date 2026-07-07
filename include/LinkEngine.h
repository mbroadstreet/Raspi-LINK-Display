#pragma once

#include "LinkDisplayState.h"

#include <ableton/Link.hpp>

class LinkEngine
{
public:
    LinkEngine(double initialTempo, double quantum);

    LinkDisplayState snapshot() const;

private:
    ableton::Link link_;
    double quantum_ = 4.0;
};
