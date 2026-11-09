#include "FpsCounter.h"

#include "FPSLimiter.h"
#include "FrameStats.h"
#include "gui/ScreenPosition.h"

#include "imgui.h"

#include <Poco/Util/Application.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr float BaseFontSize = 24.0f; //!< Same base size as the UI font, before scaling.
constexpr double UpdateInterval = 0.25; //!< Seconds between updates of the displayed numbers.
} // namespace

bool FpsCounter::Enabled()
{
    return Poco::Util::Application::instance().config().getBool("window.fpsCounter", false);
}

std::string FpsCounter::Position()
{
    return Poco::Util::Application::instance().config().getString("window.fpsCounter.position", ScreenPosition::TopLeft);
}

float FpsCounter::Draw(const FrameStats& stats, ImFont* font, float displayScale)
{
    if (!stats.limiter)
    {
        return 0.0f;
    }

    const auto& config = Poco::Util::Application::instance().config();
    const float size = std::clamp(static_cast<float>(config.getDouble("window.fpsCounter.size", 1.0)), 0.25f, 4.0f);
    const bool showFrameTimes = config.getBool("window.fpsCounter.frameTimes", false);
    const auto position = Position();

    const auto& limiter = *stats.limiter;

    // Update the numbers a few times per second only, otherwise they flicker too fast to read.
    double now = ImGui::GetTime();
    if (now - _lastUpdateTime >= UpdateInterval || _lastUpdateTime == 0.0)
    {
        _lastUpdateTime = now;
        _displayedFps = limiter.FPS();
        _displayedGpuMs = stats.gpuTimeMs;
        _displayedCpuMs = limiter.LastWorkTimeMs();

        // Average and maximum over the last ~half second of frames.
        size_t count = std::min<size_t>(limiter.HistoryCount(), 30);
        float sum{0.0f};
        float maximum{0.0f};
        for (size_t age = 0; age < count; age++)
        {
            float value = limiter.HistoryValue(age);
            sum += value;
            maximum = std::max(maximum, value);
        }
        _displayedFrameMs = count > 0 ? sum / static_cast<float>(count) : 0.0f;
        _displayedMaxFrameMs = maximum;
    }

    const float fontSize = std::floor(BaseFontSize * displayScale * size);
    // Same distance from the screen edge as toasts at the default UI scale, so both line up.
    const float padding = std::floor(BaseFontSize * displayScale);

    ScreenPosition::SetNextWindowPosition(position, padding);
    ImGui::SetNextWindowBgAlpha(0.45f);

    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoSavedSettings |
                                             ImGuiWindowFlags_NoFocusOnAppearing |
                                             ImGuiWindowFlags_NoNav |
                                             ImGuiWindowFlags_NoMove |
                                             ImGuiWindowFlags_NoInputs;

    float height{0.0f};

    ImGui::PushFont(font, fontSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(fontSize * 0.4f, fontSize * 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(fontSize * 0.3f, fontSize * 0.15f));

    if (ImGui::Begin("FPS Counter", nullptr, windowFlags))
    {
        ImGui::Text("%3.0f FPS", static_cast<double>(_displayedFps));

        if (showFrameTimes)
        {
            ImGui::Text("Frame %5.1f ms (max %.1f)", static_cast<double>(_displayedFrameMs), static_cast<double>(_displayedMaxFrameMs));
            if (_displayedGpuMs >= 0.0f)
            {
                ImGui::Text("GPU   %5.1f ms", static_cast<double>(_displayedGpuMs));
            }
            else
            {
                ImGui::TextUnformatted("GPU     n/a");
            }
            ImGui::Text("CPU   %5.1f ms", static_cast<double>(_displayedCpuMs));

            // Frame-time graph: one bar per frame, newest on the right. The line marks the expected
            // frame time (refresh rate or FPS limit); bars more than 50% above it are red.
            const float graphWidth = fontSize * 11.0f;
            const float graphHeight = fontSize * 2.5f;
            const float expected = stats.expectedIntervalMs > 0.0f ? stats.expectedIntervalMs : 1000.0f / 60.0f;
            const float graphMaxMs = expected * 3.0f;

            ImVec2 origin = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(graphWidth, graphHeight));
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(origin, ImVec2(origin.x + graphWidth, origin.y + graphHeight), IM_COL32(0, 0, 0, 90));

            const size_t barCount = std::min<size_t>(limiter.HistoryCount(), FPSLimiter::HistorySize);
            const float barWidth = graphWidth / static_cast<float>(FPSLimiter::HistorySize);
            for (size_t age = 0; age < barCount; age++)
            {
                float value = limiter.HistoryValue(age);
                float barHeight = std::min(value / graphMaxMs, 1.0f) * graphHeight;
                float x1 = origin.x + graphWidth - static_cast<float>(age) * barWidth;
                float x0 = x1 - std::max(barWidth, 1.0f);
                ImU32 color = value > expected * 1.5f ? IM_COL32(255, 70, 70, 255) : IM_COL32(90, 220, 120, 255);
                drawList->AddRectFilled(ImVec2(x0, origin.y + graphHeight - barHeight), ImVec2(x1, origin.y + graphHeight), color);
            }

            float expectedY = origin.y + graphHeight - (expected / graphMaxMs) * graphHeight;
            drawList->AddLine(ImVec2(origin.x, expectedY), ImVec2(origin.x + graphWidth, expectedY), IM_COL32(255, 230, 80, 230), std::max(1.5f, fontSize / 16.0f));
        }

        height = ImGui::GetWindowSize().y + padding;
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopFont();

    return height;
}
