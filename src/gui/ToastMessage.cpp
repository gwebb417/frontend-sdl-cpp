#include "ToastMessage.h"

#include "gui/ProjectMGUI.h"
#include "gui/ScreenPosition.h"

#include "imgui.h"

#include <Poco/Util/Application.h>

ToastMessage::ToastMessage(std::string toastText, float displayTime)
    : _gui(Poco::Util::Application::instance().getSubsystem<ProjectMGUI>())
    , _toastText(std::move(toastText))
    , _displayTimeLeft(displayTime)
{
}

bool ToastMessage::Draw(float lastFrameTime, const std::string& position, float verticalOffset)
{
    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                             ImGuiWindowFlags_AlwaysAutoResize |
                                             ImGuiWindowFlags_NoSavedSettings |
                                             ImGuiWindowFlags_NoFocusOnAppearing |
                                             ImGuiWindowFlags_NoNav |
                                             ImGuiWindowFlags_NoMove;

    // Anchor the toast to the configured corner (or the center), with some padding.
    // WorkPos/WorkSize exclude the main menu bar while it's visible, so toasts don't overlap it.
    ScreenPosition::SetNextWindowPosition(position, ImGui::GetFontSize(), verticalOffset);

    // Fade out toast in the last second-
    _displayTimeLeft -= lastFrameTime;
    auto alpha = std::min(_displayTimeLeft, 1.0f);
    ImGui::SetNextWindowBgAlpha(0.35f * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 10));

    if (ImGui::Begin("Toast", nullptr, windowFlags))
    {
        _gui.PushToastFont();
        ImGui::Text("%s", _toastText.c_str());
        _gui.PopFont();
        if (!_broughtToFront)
        {
            ImGui::SetWindowFocus();
            _broughtToFront = true;
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(2);

    return _displayTimeLeft > .0f;
}
