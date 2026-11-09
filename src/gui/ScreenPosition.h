#pragma once

#include "imgui.h"

#include <string>

/**
 * @brief Screen positions for overlays (FPS counter, toasts), as stored in the configuration.
 */
namespace ScreenPosition {

constexpr const char* TopLeft = "top-left";
constexpr const char* TopRight = "top-right";
constexpr const char* BottomLeft = "bottom-left";
constexpr const char* BottomRight = "bottom-right";
constexpr const char* Center = "center";

inline bool IsTop(const std::string& position)
{
    return position == TopLeft || position == TopRight;
}

inline bool IsBottom(const std::string& position)
{
    return position == BottomLeft || position == BottomRight;
}

/**
 * @brief Sets the next ImGui window's position so it sits in the given corner (or the center) of the
 *        viewport's work area, which excludes the main menu bar while it's visible.
 * @param position One of the position names above. Unknown values are treated as top-right.
 * @param padding Distance from the screen edges in pixels.
 * @param verticalOffset Extra distance from the top/bottom edge, e.g. to make room for another overlay.
 */
inline void SetNextWindowPosition(const std::string& position, float padding, float verticalOffset = 0.0f)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float left = viewport->WorkPos.x + padding;
    const float right = viewport->WorkPos.x + viewport->WorkSize.x - padding;
    const float top = viewport->WorkPos.y + padding + verticalOffset;
    const float bottom = viewport->WorkPos.y + viewport->WorkSize.y - padding - verticalOffset;

    if (position == TopLeft)
    {
        ImGui::SetNextWindowPos(ImVec2(left, top), ImGuiCond_Always, ImVec2(0.0f, 0.0f));
    }
    else if (position == BottomLeft)
    {
        ImGui::SetNextWindowPos(ImVec2(left, bottom), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    }
    else if (position == BottomRight)
    {
        ImGui::SetNextWindowPos(ImVec2(right, bottom), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
    }
    else if (position == Center)
    {
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
                                       viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    }
    else
    {
        ImGui::SetNextWindowPos(ImVec2(right, top), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    }
}

} // namespace ScreenPosition
