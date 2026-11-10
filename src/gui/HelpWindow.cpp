#include "HelpWindow.h"

#include "imgui.h"

#include <Poco/Util/Application.h>

#include <algorithm>

void HelpWindow::Show()
{
    if (_commandLineArguments.empty())
    {
        FillCommandLineArgumentTable();
    }

    if (_shortcuts.empty())
    {
        FillKeyboardShortcutsTable();
    }

    _visible = true;
    ImGui::SetWindowFocus("Help");
}

void HelpWindow::Draw()
{
    if (!_visible)
    {
        return;
    }

    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
    constexpr ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_None;
    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

    ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Quick Help###Help", &_visible, windowFlags))
    {
        if (ImGui::BeginTabBar("Help Topics", tabBarFlags))
        {
            if (ImGui::BeginTabItem("Keyboard Shortcuts"))
            {
                // Size the function column to the longest text across all groups, so the tables line up.
                float functionColumnWidth{0.0f};
                for (const auto& group : _shortcuts)
                {
                    for (const auto& shortcut : group.shortcuts)
                    {
                        functionColumnWidth = std::max(functionColumnWidth, ImGui::CalcTextSize(shortcut.first.c_str()).x);
                    }
                }

                for (const auto& group : _shortcuts)
                {
                    if (!ImGui::CollapsingHeader(group.title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        continue;
                    }

                    if (ImGui::BeginTable(group.title.c_str(), 2, tableFlags))
                    {
                        ImGui::TableSetupColumn("Function", ImGuiTableColumnFlags_WidthFixed, functionColumnWidth);
                        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch, .0f);
                        ImGui::TableHeadersRow();

                        for (const auto& shortcut : group.shortcuts)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(shortcut.first.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::TextWrapped("%s", shortcut.second.c_str());
                        }

                        ImGui::EndTable();
                    }
                    ImGui::Spacing();
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Command Line Arguments"))
            {
                if (ImGui::BeginTable("Command Line Arguments", 3, tableFlags))
                {
                    ImGui::TableSetupColumn("Short", ImGuiTableColumnFlags_WidthFixed, .0f);
                    ImGui::TableSetupColumn("Argument", ImGuiTableColumnFlags_WidthFixed, .0f);
                    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, .0f);
                    ImGui::TableHeadersRow();

                    for (const auto& argument : _commandLineArguments)
                    {
                        int columnIndex = 0;
                        ImGui::TableNextRow();
#ifdef POCO_OS_FAMILY_UNIX
                        ImGui::TableSetColumnIndex(columnIndex++);
                        ImGui::TextUnformatted(std::get<0>(argument).c_str());
#endif
                        ImGui::TableSetColumnIndex(columnIndex++);
                        ImGui::TextUnformatted(std::get<1>(argument).c_str());
                        ImGui::TableSetColumnIndex(columnIndex++);
                        ImGui::TextWrapped("%s", std::get<2>(argument).c_str());
                    }

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

void HelpWindow::FillCommandLineArgumentTable()
{
    auto& app = Poco::Util::Application::instance();
    const auto& options = app.options();

    for (const auto& option : options)
    {
        std::string shortArgument = option.shortName();
        std::string longArgument = option.fullName();
        std::string description = option.description();

        if (option.takesArgument())
        {
            if (!shortArgument.empty())
            {
#ifdef POCO_OS_FAMILY_UNIX
                shortArgument = "-" + shortArgument;
#else
                shortArgument = "/" + shortArgument;
#endif
                if (!option.argumentRequired())
                {
                    shortArgument += "[";
                }
                shortArgument += option.argumentName();
                if (!option.argumentRequired())
                {
                    shortArgument += "]";
                }
            }

            if (!longArgument.empty())
            {
#ifdef POCO_OS_FAMILY_UNIX
                longArgument = "--" + longArgument;
#else
                longArgument = "/" + longArgument;
#endif
                if (!option.argumentRequired())
                {
                    longArgument += "[";
                }
                longArgument += "=" + option.argumentName();
                if (!option.argumentRequired())
                {
                    longArgument += "]";
                }
            }
        }

        _commandLineArguments.emplace_back(shortArgument, longArgument, description);
    }
}

void HelpWindow::FillKeyboardShortcutsTable()
{
    _shortcuts = {
        {"General",
         {{"Toggle Menu/UI", "Escape"},
          {"Open Settings", "Ctrl+s"},
          {"Quit projectM", "Ctrl+q"},
          {"Toggle Fullscreen", "Ctrl+f, Right Mouse"},
          {"Move to Next Monitor", "Ctrl+m"},
          {"Next Audio Input Device", "Ctrl+i"},
          {"Copy Current Preset Path to Clipboard", "Ctrl+c"}}},

        {"Preset Playback",
         {{"Next Preset (immediate / smooth)", "n / Shift+n"},
          {"Previous Preset (immediate / smooth)", "p / Shift+p"},
          {"Last Played Preset (immediate / smooth)", "Backspace / Shift+Backspace"},
          {"Random Preset (immediate / smooth)", "r / Shift+r"},
          {"Lock Current Preset", "Spacebar"},
          {"Toggle Shuffle", "y"}}},

        {"Preset Search",
         {{"Open Preset Search", "Enter"},
          {"Filter List", "Type words of the preset name, in any order"},
          {"Move Selection", "Up/Down, PgUp/PgDown, Home/End"},
          {"Play Selected Preset (immediate / smooth)", "Enter / Shift+Enter, or Double-Click"},
          {"Close Preset Search", "Escape"}}},

        {"Favorites and Disabled Presets",
         {{"Save Current Preset to Favorites", "s"},
          {"Toggle Favorites Only / All Presets", "f"},
          {"Disable Current Preset (moved to userdata\\disabled)", "Delete"},
          {"Remove from Favorites (in favorites-only mode)", "Delete"},
          {"Toggle Disabled Presets Only / All Presets", "Ctrl+Delete"},
          {"Re-enable Current Preset (in disabled-only mode)", "Delete"},
          {"Undo Last Save/Removal/Disable/Re-enable (up to 20 steps)", "Ctrl+z"}}},

        {"Visuals and Audio",
         {{"Toggle Aspect Correction", "a"},
          {"Increase / Decrease Beat Sensitivity by 1% (off with Beat Averaging)", "Cursor Up / Cursor Down"},
          {"Faster / Slower Ambient Speed by 0.05", "Cursor Right / Cursor Left"},
          {"Next Mirror Mode (Off, Left to Right, Top to Bottom, 4-Way)", "m"},
          {"Zoom In / Out", "+ / -"},
          {"Add Random Waveform at Mouse Pointer", "Shift+Left Mouse"},
          {"Clear Random Waveforms", "Middle Mouse"}}},

        {"Settings: Number Inputs",
         {{"Apply Typed Value(s)", "Enter, or the Apply button (unapplied boxes are orange)"},
          {"Move to Next Box Without Applying", "Tab"},
          {"Step Typed Value", "Cursor Up/Down (Shift: 10x), then Enter/Apply"},
          {"Cancel Typed Value", "Escape"}}}};
}
