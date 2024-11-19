#pragma once

#include <AL/al.h>
#include <AL/alc.h>

#include "structs/SharedEngineStructs.h"

class SoundManager
{
  public:

    inline static const std::unordered_map<Sound, const char*> SOUNDS_FILES
        = {{Sound::kProgramStart, "../assets/sounds/costco.mp3"},
           {Sound::kVineBoom, "../assets/sounds/vine_boom.mp3"},
           {Sound::kMusicGameMenu, "../assets/sounds/music/wii.mp3"},
           {Sound::kMusicGamePlay, "../assets/sounds/music/sneaky.mp3"},
           {Sound::kCorrectAnswer, "../assets/sounds/correct.mp3"},
           // {Sound::kMusicGamePlay, "../assets/sounds/music/powerup.mp3"},
        };


    SoundManager();
    ~SoundManager();

    void Tick();

    void SetMusic(Sound music);
    void DisableMusic();

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

    std::optional<Sound> currentMusic = std::nullopt;
};
