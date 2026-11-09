#pragma once

#include <string>

struct FrameStats;
struct ImFont;

/**
 * @brief Draws the FPS counter overlay, optionally with frame times and a frame-time graph.
 *
 * Its position and size come from the configuration (window.fpsCounter.*). The size is independent
 * of the UI scaling factor.
 */
class FpsCounter
{
public:
    /**
     * @brief Returns true if the counter is enabled in the configuration.
     */
    static bool Enabled();

    /**
     * @brief Returns the configured screen position (see ScreenPosition).
     */
    static std::string Position();

    /**
     * @brief Draws the counter.
     * @param stats Frame timing from the render loop.
     * @param font The font to use.
     * @param displayScale Scale factor of the display (e.g. 2.0 for 200% OS scaling), without the UI scale.
     * @return The height of the drawn overlay in pixels, including its distance to the screen edge.
     */
    float Draw(const FrameStats& stats, ImFont* font, float displayScale);

private:
    float _displayedFps{0.0f}; //!< FPS value currently shown, updated a few times per second so it's readable.
    float _displayedFrameMs{0.0f};
    float _displayedMaxFrameMs{0.0f};
    float _displayedGpuMs{-1.0f};
    float _displayedCpuMs{0.0f};
    double _lastUpdateTime{0.0};
};
