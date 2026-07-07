#include "LinkEngine.h"

LinkEngine::LinkEngine(double tempo, double quantum)
    : link(tempo), mQuantum(quantum) {}

void LinkEngine::enable(bool on) {
    link.enable(on);
}

bool LinkEngine::isEnabled() const {
    return link.isEnabled();
}

int LinkEngine::numPeers() const {
    return static_cast<int>(link.numPeers());
}

double LinkEngine::tempo() const {
    auto state = link.captureAppSessionState();
    return state.tempo();
}

double LinkEngine::beat() const {
    auto state = link.captureAppSessionState();
    return state.beatAtTime(link.clock().micros(), mQuantum);
}

double LinkEngine::phase() const {
    auto state = link.captureAppSessionState();
    return state.phaseAtTime(link.clock().micros(), mQuantum);
}

double LinkEngine::quantum() const {
    return mQuantum;
}