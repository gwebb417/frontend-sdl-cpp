#include "FPSLimiter.h"

#include <SDL2/SDL.h>

#include <algorithm>

FPSLimiter::FPSLimiter()
    : _frequency(std::max<uint64_t>(1, SDL_GetPerformanceFrequency()))
{
}

void FPSLimiter::TargetFPS(int fps)
{
    uint64_t newTargetTicks = fps > 0 ? _frequency / static_cast<uint64_t>(fps) : 0;
    if (newTargetTicks != _targetTicks)
    {
        _targetTicks = newTargetTicks;
        _deadline = 0; // Restart the schedule.
    }
}

double FPSLimiter::TargetFrameTimeMs() const
{
    return TicksToMs(_targetTicks);
}

float FPSLimiter::FPS() const
{
    size_t count = std::min<size_t>(_historyCount, 10);
    if (count == 0)
    {
        return 0.0f;
    }

    float sum{0.0f};
    for (size_t age = 0; age < count; age++)
    {
        sum += HistoryValue(age);
    }

    if (sum <= 0.0f)
    {
        return 0.0f;
    }

    return 1000.0f * static_cast<float>(count) / sum;
}

void FPSLimiter::StartFrame()
{
    _frameStart = SDL_GetPerformanceCounter();
    _workDone = 0;
}

void FPSLimiter::WorkDone()
{
    _workDone = SDL_GetPerformanceCounter();
    _lastWorkTimeMs = static_cast<float>(TicksToMs(_workDone - _frameStart));
}

void FPSLimiter::EndFrame()
{
    if (_workDone == 0)
    {
        WorkDone();
    }

    if (_targetTicks > 0)
    {
        _deadline = _deadline == 0 ? _frameStart + _targetTicks : _deadline + _targetTicks;

        uint64_t now = SDL_GetPerformanceCounter();
        if (now >= _deadline)
        {
            // Late. If more than a whole frame behind, start a new schedule instead of rushing
            // several frames to catch up.
            if (now - _deadline > _targetTicks)
            {
                _deadline = now;
            }
        }
        else
        {
            // Sleep until about 1.5 ms before the deadline (SDL_Delay is only accurate to ~1 ms),
            // then wait out the rest precisely.
            const uint64_t spinTicks = _frequency * 3 / 2000;
            while (true)
            {
                now = SDL_GetPerformanceCounter();
                if (now >= _deadline)
                {
                    break;
                }

                uint64_t remaining = _deadline - now;
                if (remaining > spinTicks)
                {
                    auto sleepMs = static_cast<Uint32>((remaining - spinTicks) * 1000 / _frequency);
                    SDL_Delay(std::max<Uint32>(sleepMs, 1));
                }
            }
        }
    }
    else
    {
        _deadline = 0;
    }

    uint64_t frameEnd = SDL_GetPerformanceCounter();
    if (_lastFrameEnd != 0)
    {
        _frameIntervals[_nextHistoryOffset] = static_cast<float>(TicksToMs(frameEnd - _lastFrameEnd));
        _nextHistoryOffset = (_nextHistoryOffset + 1) % HistorySize;
        _historyCount = std::min(_historyCount + 1, HistorySize);
    }
    _lastFrameEnd = frameEnd;
}

float FPSLimiter::LastFrameIntervalMs() const
{
    return _historyCount > 0 ? HistoryValue(0) : 0.0f;
}

float FPSLimiter::LastWorkTimeMs() const
{
    return _lastWorkTimeMs;
}

float FPSLimiter::HistoryValue(size_t age) const
{
    if (age >= _historyCount)
    {
        return 0.0f;
    }
    return _frameIntervals[(_nextHistoryOffset + HistorySize - 1 - age) % HistorySize];
}

size_t FPSLimiter::HistoryCount() const
{
    return _historyCount;
}

double FPSLimiter::TicksToMs(uint64_t ticks) const
{
    return static_cast<double>(ticks) * 1000.0 / static_cast<double>(_frequency);
}
