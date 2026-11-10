#include "AudioCaptureImpl_SDL.h"

#include "notifications/AudioDataAvailable.h"

#include <Poco/NotificationCenter.h>
#include <Poco/Util/Application.h>

AudioCaptureImpl::AudioCaptureImpl()
{
    auto targetFps = Poco::Util::Application::instance().config().getUInt("projectM.fps", 60);
    if (targetFps > 0)
    {
        _requestedSampleCount = std::min(_requestedSampleFrequency / targetFps, _requestedSampleCount);
        // Don't let the buffer get too small to prevent excessive updates calls.
        // 300 samples is enough for 144 FPS.
        _requestedSampleCount = std::max(_requestedSampleCount, 300U);
    }

#ifdef SDL_HINT_AUDIO_INCLUDE_MONITORS
    SDL_SetHint(SDL_HINT_AUDIO_INCLUDE_MONITORS, "1");
#endif
    SDL_InitSubSystem(SDL_INIT_AUDIO);
}

AudioCaptureImpl::~AudioCaptureImpl()
{
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

std::map<int, std::string> AudioCaptureImpl::AudioDeviceList()
{
    std::map<int, std::string> deviceList{
        {-1, "Default capturing device"}};

    auto recordingDeviceCount = SDL_GetNumAudioDevices(true);

    for (int i = 0; i < recordingDeviceCount; i++)
    {
        auto deviceName = SDL_GetAudioDeviceName(i, true);
        if (deviceName)
        {
            deviceList.insert(std::make_pair(i, deviceName));
        }
        else
        {
            poco_error_f2(_logger, "Could not get device name for device ID %d: %s", i, std::string(SDL_GetError()));
        }
    }

    return deviceList;
}

int AudioCaptureImpl::DefaultAudioDeviceIndex(const std::map<int, std::string>& deviceList)
{
    static const std::string monitorPrefix = "Monitor of ";

    // Prefer the monitor of the default output device. SDL_GetDefaultAudioInfo() exists since SDL 2.24.
    // The release build is compiled against an older SDL, so look it up in the SDL library actually running.
    using GetDefaultAudioInfoFunc = int (*)(char**, SDL_AudioSpec*, int);
    auto getDefaultAudioInfo = reinterpret_cast<GetDefaultAudioInfoFunc>(SDL_LoadFunction(nullptr, "SDL_GetDefaultAudioInfo"));
    if (getDefaultAudioInfo != nullptr)
    {
        char* outputName{nullptr};
        SDL_AudioSpec spec{};
        if (getDefaultAudioInfo(&outputName, &spec, 0) == 0 && outputName != nullptr)
        {
            const std::string wantedName = monitorPrefix + outputName;
            SDL_free(outputName);

            for (const auto& device : deviceList)
            {
                if (device.second == wantedName)
                {
                    return device.first;
                }
            }
        }
    }

    for (const auto& device : deviceList)
    {
        if (device.first >= 0 && device.second.compare(0, monitorPrefix.size(), monitorPrefix) == 0)
        {
            return device.first;
        }
    }

    return -1;
}

void AudioCaptureImpl::StartRecording(int audioDeviceIndex)
{
    _currentAudioDeviceIndex = audioDeviceIndex;

    poco_debug_f1(_logger, "Using SDL audio driver \"%s\".", std::string(SDL_GetCurrentAudioDriver()));

    if (OpenAudioDevice())
    {
        SDL_PauseAudioDevice(_currentAudioDeviceID, false);

        poco_debug(_logger, "Started audio recording.");
    }
}

void AudioCaptureImpl::StopRecording()
{
    if (_currentAudioDeviceID)
    {
        SDL_PauseAudioDevice(_currentAudioDeviceID, true);
        SDL_CloseAudioDevice(_currentAudioDeviceID);
        _currentAudioDeviceID = 0;

        poco_debug(_logger, "Stopped audio recording and closed device.");
    }
}

void AudioCaptureImpl::NextAudioDevice()
{
    StopRecording();

    // Will wrap around to default capture device (-1).
    int nextAudioDeviceId = ((_currentAudioDeviceIndex + 2) % (SDL_GetNumAudioDevices(true) + 1)) - 1;

    StartRecording(nextAudioDeviceId);
}

void AudioCaptureImpl::AudioDeviceIndex(int index)
{
    if (index >= -1 && index < SDL_GetNumAudioDevices(true))
    {
        StopRecording();
        _currentAudioDeviceIndex = index;
        StartRecording(index);
    }
}

int AudioCaptureImpl::AudioDeviceIndex() const
{
    return _currentAudioDeviceIndex;
}

std::string AudioCaptureImpl::AudioDeviceName() const
{
    if (_currentAudioDeviceIndex >= 0)
    {
        return SDL_GetAudioDeviceName(_currentAudioDeviceIndex, true);
    }
    else
    {
        return "Default capturing device";
    }
}

bool AudioCaptureImpl::OpenAudioDevice()
{
    SDL_AudioSpec requestedSpecs{};
    SDL_AudioSpec actualSpecs{};

    requestedSpecs.freq = _requestedSampleFrequency;
    requestedSpecs.format = AUDIO_F32;
    requestedSpecs.channels = 2;
    requestedSpecs.samples = _requestedSampleCount;
    requestedSpecs.callback = AudioCaptureImpl::AudioInputCallback;
    requestedSpecs.userdata = this;

    // Will be NULL on error, which happens if the requested index is -1. This automatically selects the default device.
    auto deviceName = SDL_GetAudioDeviceName(_currentAudioDeviceIndex, true);
    _currentAudioDeviceID = SDL_OpenAudioDevice(deviceName, true, &requestedSpecs, &actualSpecs, SDL_AUDIO_ALLOW_CHANNELS_CHANGE);

    if (_currentAudioDeviceID == 0)
    {
        poco_error_f3(_logger, R"(Failed to open audio device "%s" (ID %?d): %s)",
                      std::string(deviceName != nullptr ? deviceName : "System default capturing device"),
                      _currentAudioDeviceIndex,
                      std::string(SDL_GetError()));
        return false;
    }

    _channels = actualSpecs.channels;
    if (actualSpecs.freq > 0)
    {
        Notification::AudioDataAvailable::SampleRate() = static_cast<uint32_t>(actualSpecs.freq);
    }

    poco_information_f4(_logger, R"(Opened audio recording device "%s" (ID %?d) with %?d channels at %?d Hz.)",
                        std::string(deviceName != nullptr ? deviceName : "System default capturing device"),
                        _currentAudioDeviceIndex,
                        actualSpecs.channels,
                        actualSpecs.freq);

    return true;
}

void AudioCaptureImpl::AudioInputCallback(void* userData, unsigned char* stream, int len)
{
    poco_assert_dbg(userData);

    if (len < sizeof(float))
    {
        return;
    }

    auto instance = reinterpret_cast<AudioCaptureImpl*>(userData);

    unsigned int samples = len / sizeof(float);

    Notification::AudioDataAvailable::Center().postNotification(
        new Notification::AudioDataAvailable(instance->_channels,
                                           reinterpret_cast<float*>(stream),
                                           samples));
}
