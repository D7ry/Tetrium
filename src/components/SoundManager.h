#pragma once

#include <AL/al.h>
#include <AL/alc.h>

class SoundManager
{
  public:
    enum class Sound
    {
        kProgramStart,
        kVineBoom,
        kMusicGameMenu,
    };

    inline static const std::unordered_map<Sound, const char*> SOUNDS_FILES
        = {{Sound::kProgramStart, "../assets/sounds/costco.mp3"},
           {Sound::kVineBoom, "../assets/sounds/vine_boom.mp3"},
           {Sound::kMusicGameMenu, "../assets/sounds/music/powerup.mp3"}};

    SoundManager();
    ~SoundManager();

    void PlaySound(Sound sound);
    void StartSound(Sound sound);

    void StopSound(Sound sound);

    void LoadAllSounds();

  private:
    ALvoid* loadSoundFile(const char* filename, ALsizei* size, ALsizei* freq, ALenum* format);

    ALCdevice* device;
    ALCcontext* context;
    std::vector<ALuint> buffers;
    std::vector<ALuint> sources;
    std::unordered_map<Sound, ALuint> soundToSourceMap;
};
