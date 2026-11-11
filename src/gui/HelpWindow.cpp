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
                // Size the key column to the longest key text across all groups, so the tables line up.
                float keyColumnWidth{0.0f};
                for (const auto& group : _shortcuts)
                {
                    for (const auto& shortcut : group.shortcuts)
                    {
                        keyColumnWidth = std::max(keyColumnWidth, ImGui::CalcTextSize(shortcut.first.c_str()).x);
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
                        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, keyColumnWidth);
                        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, .0f);
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
    // Pairs of (key, action). Settings explain themselves in their tooltips, so this only lists the keys.
    _shortcuts = {
        {"Presets (with Shift: smooth transition)",
         {{"N / P", "Next / previous preset"},
          {"R", "Random preset"},
          {"Backspace", "Back to the last played preset"},
          {"Space", "Lock current preset"},
          {"Y", "Shuffle on/off"},
          {"Enter", "Search presets (Enter plays, Esc closes)"}}},

        {"Favorites and Disabled Presets",
         {{"S", "Save to favorites"},
          {"F", "Favorites only / all presets"},
          {"Del", "Disable preset (in favorites: remove; in disabled: re-enable)"},
          {"Ctrl+Del", "Disabled presets only / all presets"},
          {"Ctrl+Z", "Undo (up to 20 steps)"}}},

        {"Picture and Sound",
         {{"Up / Down", "Beat Sensitivity"},
          {"Left / Right", "Ambient Speed"},
          {"M", "Mirror mode"},
          {"+ / -", "Zoom"},
          {"A", "Aspect correction"},
          {"Shift+Left Click", "Add a waveform"},
          {"Middle Click", "Clear added waveforms"}}},

        {"Window and Program",
         {{"Esc", "Menu on/off"},
          {"Ctrl+S", "Settings"},
          {"Ctrl+F", "Fullscreen (also Right Click)"},
          {"Ctrl+M", "Next monitor"},
          {"Ctrl+I", "Next audio device"},
          {"Ctrl+C", "Copy preset path"},
          {"Ctrl+Q", "Quit"}}}};
}
