#pragma once

#include <Poco/Notification.h>
#include <Poco/NotificationCenter.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace Notification {

/**
 * @brief Passes new audio data to all active projectM instances.
 * @note Channels must be >0 and samples must contain at least one audio frame/sample.
 */
class AudioDataAvailable : public Poco::Notification
{
public:
    std::string name() const override;

    /**
     * @brief The notification center audio data is posted to and observed on.
     * Audio data arrives on the audio capture thread. Posting it to Poco's default notification center
     * could deadlock: while posting, a thread holds the center's lock and briefly locks every observer,
     * while the main thread holds an observer's lock during its handler (e.g. a preset switch) and then
     * needs the center's lock to post another notification. Using a separate center only the audio
     * observer is registered on avoids this lock-order inversion.
     * @return The audio data notification center.
     */
    static Poco::NotificationCenter& Center();

    /**
     * @brief Sample rate in Hz of the audio data currently being captured.
     * Set by the capture implementation when it opens a device, read on the render thread
     * to convert the band frequency ranges (Hz) to spectrum samples. 44100 until a device was opened.
     * @return The sample rate value.
     */
    static std::atomic<uint32_t>& SampleRate();

    AudioDataAvailable() = delete;

    /**
     * @brief Creates a new AudioDataAvailable notification instance
     * The number of @a samples must be divisible by @a channels.
     * @param channels The channel count of the current capture device.
     * @param samples The new audio samples captured from the active audio device.
     * @param sampleCount The number of float samples passed to @a samples.
     */
    explicit AudioDataAvailable(uint32_t channels, const float* samples, uint32_t sampleCount);

    /**
     * Returns the number of audio channels of the recording device.
     * @return The number of audio channels of the recording device.
     */
    [[nodiscard]] uint32_t Channels() const;

    /**
     * Returns the new audio samples captured from the active audio device.
     * @return The new audio samples captured from the active audio device.
     */
    [[nodiscard]] const std::vector<float>& Samples() const;

private:
    uint32_t _channels; //!< Number of audio channels of the capture device.
    std::vector<float> _samples; //!< The new audio samples, containing frames x channels floats.
};

} // namespace Notification
