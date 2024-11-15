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

bool SoundManager::loadSound(const std::string& filename, ALuint& buffer)
{
    SF_INFO sfInfo;
    SNDFILE* sndFile = sf_open(filename.c_str(), SFM_READ, &sfInfo);
    if (!sndFile) {
        PANIC("Failed to open sound file: " + filename);
    }

    std::vector<short> samples(sfInfo.frames * sfInfo.channels);
    if (sf_readf_short(sndFile, samples.data(), sfInfo.frames) < sfInfo.frames) {
        sf_close(sndFile);
        PANIC("Failed to read sound file: " + filename);
    }
    sf_close(sndFile);

    ALenum format;
    if (sfInfo.channels == 1) {
        format = AL_FORMAT_MONO16;
    } else if (sfInfo.channels == 2) {
        format = AL_FORMAT_STEREO16;
    } else {
        PANIC("Unsupported channel count: " + std::to_string(sfInfo.channels));
    }

    alGenBuffers(1, &buffer);
    if (alGetError() != AL_NO_ERROR) {
        PANIC("Failed to generate OpenAL buffer");
    }

    alBufferData(buffer, format, samples.data(), samples.size() * sizeof(short), sfInfo.samplerate);
    if (alGetError() != AL_NO_ERROR) {
        PANIC("Failed to fill OpenAL buffer with sound data");
    }

    buffers.push_back(buffer);

    return true;
}

void SoundManager::playSound(ALuint buffer)
{
    ALuint source;
    alGenSources(1, &source);
    sources.push_back(source);

    alSourcei(source, AL_BUFFER, buffer);
    alSourcePlay(source);
}
