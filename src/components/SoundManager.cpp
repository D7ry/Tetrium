#include <iostream>
#include <sndfile.h>

#include "SoundManager.h"

SoundManager::SoundManager()
{
    device = alcOpenDevice(nullptr); // open default device
    if (!device) {
        PANIC("Failed to open sound device");
    }

    context = alcCreateContext(device, nullptr);
    if (!alcMakeContextCurrent(context)) {
        PANIC("Failed to set sound context");
    }
}

SoundManager::~SoundManager()
{
    for (ALuint source : sources) {
        alDeleteSources(1, &source);
    }
    for (ALuint buffer : buffers) {
        alDeleteBuffers(1, &buffer);
    }

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    alcCloseDevice(device);
}

void SoundManager::PlaySound(Sound sound)
{
    auto it = soundToSourceMap.find(sound);
    if (it != soundToSourceMap.end()) {
        ALuint source = it->second;
        alSourcePlay(source);
    } else {
        PANIC("sound not found");
    }
}

void SoundManager::StopSound(Sound sound)
{
    auto it = soundToSourceMap.find(sound);
    if (it != soundToSourceMap.end()) {
        ALuint source = it->second;
        alSourceStop(source);
    } else {
        PANIC("sound not found");
    }
}

ALvoid* SoundManager::loadSoundFile(
    const char* filename,
    ALsizei* size,
    ALsizei* freq,
    ALenum* format
)
{
    SF_INFO sfInfo;
    SNDFILE* sndFile = sf_open(filename, SFM_READ, &sfInfo);
    if (!sndFile) {
        PANIC("Failed to open sound file {}", filename);
    }

    *size = static_cast<ALsizei>(sfInfo.frames * sfInfo.channels * sizeof(short));
    *freq = static_cast<ALsizei>(sfInfo.samplerate);

    if (sfInfo.channels == 1) {
        *format = AL_FORMAT_MONO16;
    } else if (sfInfo.channels == 2) {
        *format = AL_FORMAT_STEREO16;
    } else {
        sf_close(sndFile);
        PANIC("Unsupported channel count");
    }

    short* buffer = static_cast<short*>(malloc(*size));
    if (!buffer) {
        sf_close(sndFile);
        PANIC("Failed to allocate memory for sound data");
    }

    sf_read_short(sndFile, buffer, sfInfo.frames * sfInfo.channels);
    sf_close(sndFile);

    return buffer;
}

void SoundManager::LoadAllSounds()
{
    device = alcOpenDevice(nullptr); // Open default device
    if (!device) {
        throw std::runtime_error("Failed to open sound device");
    }

    context = alcCreateContext(device, nullptr); // Create context
    if (!context || !alcMakeContextCurrent(context)) {
        if (context)
            alcDestroyContext(context);
        alcCloseDevice(device);
        throw std::runtime_error("Failed to set sound context");
    }

    for (const auto& [sound, file] : SOUNDS_FILES) {
        ALuint buffer;
        alGenBuffers(1, &buffer);

        // Load sound data from file (pseudo-code, replace with actual loading code)
        ALsizei size, freq;
        ALenum format;
        ALvoid* data = loadSoundFile(file, &size, &freq, &format);

        alBufferData(buffer, format, data, size, freq);
        free(data); // Free the loaded data

        buffers.push_back(buffer);

        ALuint source;
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, buffer);

        sources.push_back(source);
        soundToSourceMap[sound] = source;
    }
}

void SoundManager::StartSound(Sound sound)
{
    auto it = soundToSourceMap.find(sound);
    if (it != soundToSourceMap.end()) {
        ALuint source = it->second;
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            alSourcePlay(source);
        }
    } else {
        PANIC("sound not found");
    }
}
