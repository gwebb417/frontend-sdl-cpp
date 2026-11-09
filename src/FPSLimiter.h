#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

/**
 * @brief Limits FPS by adding a delay if necessary. Also keeps track of actual FPS and frame times.
 *
 * Uses SDL's high-resolution performance counter. Earlier versions used whole milliseconds
 * (1000 / fps, rounded down), so e.g. 60 FPS was really ~62.5 and 144 FPS ~166.
 */
class FPSLimiter
{
public:
    static constexpr size_t HistorySize = 240; //!< Number of frames kept for the frame-time graph.

    FPSLimiter();

    /**
     * @brief Sets the target frames per second value.
     * @param fps The targeted frames per second. Set to 0 for unlimited FPS.
     */
    void TargetFPS(int fps);

    /**
     * @brief Returns the target frame time in milliseconds, or 0 if FPS is unlimited.
     */
    double TargetFrameTimeMs() const;

    /**
     * @brief Calculates the current real FPS.
     * @return The frames per second value as an average over the last ten frames. Zero if no frame was measured yet.
     */
    float FPS() const;

    /**
     * @brief Marks the start of a new frame.
     *
     * Should be the first call in the render loop.
     */
    void StartFrame();

    /**
     * @brief Marks the end of the frame's own work (rendering), before swapping buffers.
     *
     * The time from StartFrame() to this call is the frame's CPU time.
     */
    void WorkDone();

    /**
     * @brief Marks the end of a frame.
     *
     * Will pause if required to lower FPS to target value. Also records the frame time for FPS calculation.
     */
    void EndFrame();

    /**
     * @brief Time between the ends of the last two frames, in ms (what the viewer sees as frame time).
     */
    float LastFrameIntervalMs() const;

    /**
     * @brief CPU time of the last frame (StartFrame() to WorkDone()), in ms.
     */
    float LastWorkTimeMs() const;

    /**
     * @brief Frame interval history in ms. Use HistoryValue() to read it in chronological order.
     * @param age 0 = most recent frame, HistoryCount()-1 = oldest.
     */
    float HistoryValue(size_t age) const;

    /**
     * @brief Number of valid entries in the frame interval history.
     */
    size_t HistoryCount() const;

protected:
    double TicksToMs(uint64_t ticks) const;

    uint64_t _frequency{1}; //!< Performance counter ticks per second.
    uint64_t _targetTicks{0}; //!< Targeted time per frame in counter ticks, 0 = unlimited.
    uint64_t _frameStart{0}; //!< Counter value when the current frame was started.
    uint64_t _workDone{0}; //!< Counter value when the current frame's work was done.
    uint64_t _lastFrameEnd{0}; //!< Counter value at the end of the previous frame.
    uint64_t _deadline{0}; //!< Counter value at which the current frame should end. Advances by _targetTicks
                           //!< each frame, so rounding errors don't add up and the average FPS matches the target.

    float _lastWorkTimeMs{0.0f};
    std::array<float, HistorySize> _frameIntervals{}; //!< Ring buffer of frame intervals in ms.
    size_t _nextHistoryOffset{0};
    size_t _historyCount{0};
};
