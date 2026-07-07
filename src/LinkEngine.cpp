#include "LinkEngine.h"

LinkEngine::LinkEngine(double initialTempo, double quantum)
    : link_(initialTempo)
    , quantum_(quantum)
{
    link_.enable(true);
}

LinkDisplayState LinkEngine::snapshot() const
{
    LinkDisplayState out;

    // Keep SessionState snapshots short-lived and reacquire them during polling.
    const auto sessionState = link_.captureAppSessionState();
    const auto now = link_.clock().micros();

    out.linkEnabled = link_.isEnabled();
    out.tempoBpm = sessionState.tempo();
    out.remotePeers = link_.numPeers(); // Remote peers only; do not add this display.
    out.beat = sessionState.beatAtTime(now, quantum_);
    out.phase = sessionState.phaseAtTime(now, quantum_);
    out.quantum = quantum_;

    return out;
}
