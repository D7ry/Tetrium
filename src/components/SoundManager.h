#pragma once

#include <AL/al.h>
#include <AL/alc.h>

#include "Pathing.h"
#include "structs/SharedEngineStructs.h"

class SoundManager
{
  public:

    inline static const std::unordered_map<Sound, std::string> SOUNDS_FILES = {
        {Sound::kProgramStart, ASSETS_PATH + "sounds/costco.wav"},
        {Sound::kVineBoom, ASSETS_PATH + "sounds/vine_boom.wav"},
        {Sound::kMusicGameMenu, ASSETS_PATH + "sounds/music/wii.wav"},
        {Sound::kMusicGamePlay, ASSETS_PATH + "sounds/music/sneaky.wav"},
        {Sound::kCorrectAnswer, ASSETS_PATH + "sounds/correct.wav"},
        {Sound::kMusicInterstellar, ASSETS_PATH + "sounds/music/spin.wav"},
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
