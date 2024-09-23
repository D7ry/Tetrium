#include "DeltaTimer.h"

void DeltaTimer::Tick()
{
    if (!_started) { // if the timer hasn't started yet, start it
        _prev = std::chrono::high_resolution_clock::now();
        _started = true;
        return;
    }
    auto now = std::chrono::high_resolution_clock::now();
    _deltaNano = now - _prev;
    _prev = std::chrono::high_resolution_clock::now();
}

DeltaTimer::DeltaTimer() { _prev = std::chrono::high_resolution_clock::now(); }

double DeltaTimer::GetDeltaTime() const { return GetDeltaTimeSeconds(); }

double DeltaTimer::GetDeltaTimeSeconds() const { return _deltaNano.count() * 1e-9; }

double DeltaTimer::GetDeltaTimeNanoSeconds() const { return _deltaNano.count(); }
