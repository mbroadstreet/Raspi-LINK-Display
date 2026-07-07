#pragma once

#include <cstddef>

struct LinkDisplayState
{
    bool linkEnabled = false;
    double tempoBpm = 120.0;
    std::size_t remotePeers = 0;
    double beat = 0.0;
    double phase = 0.0;
    double quantum = 4.0;
};
