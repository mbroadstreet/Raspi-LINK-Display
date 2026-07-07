#pragma once
#include <ableton/Link.hpp>
#include <string>

class LinkEngine {
public:
    LinkEngine(double tempo = 120.0, double quantum = 4.0);
    void enable(bool on);
    bool isEnabled() const;
    int numPeers() const;
    double tempo() const;
    double beat() const;
    double phase() const;
    double quantum() const;

private:
    ableton::Link link;
    double mQuantum;
};