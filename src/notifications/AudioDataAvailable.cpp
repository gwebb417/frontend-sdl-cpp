#include "AudioDataAvailable.h"

#include <Poco/Bugcheck.h>

#include <cstring>

namespace Notification {

std::string AudioDataAvailable::name() const
{
    return "AudioDataAvailableNotification";
}

Poco::NotificationCenter& AudioDataAvailable::Center()
{
    static Poco::NotificationCenter center;
    return center;
}

std::atomic<uint32_t>& AudioDataAvailable::SampleRate()
{
    static std::atomic<uint32_t> sampleRate{44100};
    return sampleRate;
}

AudioDataAvailable::AudioDataAvailable(uint32_t channels, const float* samples, const uint32_t sampleCount)
    : _channels(channels)
{
    poco_assert(channels > 0);
    poco_assert(samples != nullptr);
    poco_assert(sampleCount != 0);
    poco_assert(sampleCount % channels == 0);

    _samples.resize(sampleCount);
    std::memcpy(_samples.data(), samples, sampleCount * sizeof(float));
}

uint32_t AudioDataAvailable::Channels() const
{
    return _channels;
}

const std::vector<float>& AudioDataAvailable::Samples() const
{
    return _samples;
}

} // namespace Notification
