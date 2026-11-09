#pragma once

class FPSLimiter;

/**
 * @brief Per-frame timing information passed from the render loop to the UI (FPS counter / frame-time graph).
 */
struct FrameStats
{
    const FPSLimiter* limiter{nullptr}; //!< FPS and frame interval history.
    float gpuTimeMs{-1.0f}; //!< GPU time for rendering the preset, negative if unavailable.
    float expectedIntervalMs{0.0f}; //!< Frame interval the frame rate should have (refresh rate or FPS limit), 0 if unknown.
};
