#pragma once
#include <chrono>

class DeltaTimer
{
  public:
    DeltaTimer();
    void Tick();
    double GetDeltaTime() const;
    double GetDeltaTimeSeconds() const;
    double GetDeltaTimeNanoSeconds() const;

  private:
    std::chrono::time_point<std::chrono::high_resolution_clock> _prev;
    std::chrono::duration<uint64_t, std::nano> _deltaNano;
    bool _started = false;
};
