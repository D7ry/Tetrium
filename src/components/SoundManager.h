#pragma once

#include <AL/al.h>
#include <AL/alc.h>

class SoundManager
{
  public:
    SoundManager();
    ~SoundManager();

    bool loadSound(const std::string& filename, ALuint& buffer);
    void playSound(ALuint buffer);

  private:
    ALCdevice* device;
    ALCcontext* context;
    std::vector<ALuint> buffers;
    std::vector<ALuint> sources;
};
