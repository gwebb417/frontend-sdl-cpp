#include "SettingsWindow.h"

#include "AudioCapture.h"
#include "ProjectMSDLApplication.h"
#include "SDLRenderingWindow.h"

#include "ProjectMGUI.h"
#include "NumberInput.h"

#include "notifications/DisplayToast.h"

#include <imgui.h>

#include <Poco/NotificationCenter.h>
#include <Poco/NumberFormatter.h>
#include <Poco/NumberParser.h>
#include <Poco/String.h>

#include <Poco/Util/Application.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

/**
 * @brief Width of a table column holding a button with the given label, at the current font size.
 * Fixed pixel widths cut off the button text at larger UI scales.
 */
float ButtonColumnWidth(const char* buttonLabel)
{
    return ImGui::CalcTextSize(buttonLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
}

// ToConfigString(): format a value the way Poco stores it in a .properties file, so it can be
// passed to StoreSetting() and compared against the fallback value from the default config file.
std::string ToConfigString(bool value)
{
    return value ? "true" : "false";
}

std::string ToConfigString(int value)
{
    return Poco::NumberFormatter::format(value);
}

std::string ToConfigString(double value)
{
    return Poco::NumberFormatter::format(value);
}

std::string ToConfigString(float value)
{
    // Format with float precision, so e.g. 1.1f is stored as "1.1" and not "1.100000023841858".
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.6g", static_cast<double>(value));
    return buffer;
}

/**
 * @brief Compares two config values by meaning, e.g. "30" == "30.0", "1.1" == "1.100000023841858", "true" == "1".
 */
bool SameSettingValue(const std::string& value1, const std::string& value2)
{
    const auto trimmed1 = Poco::trim(value1);
    const auto trimmed2 = Poco::trim(value2);

    double number1{};
    double number2{};
    if (Poco::NumberParser::tryParseFloat(trimmed1, number1) && Poco::NumberParser::tryParseFloat(trimmed2, number2))
    {
        return std::abs(number1 - number2) <= 1e-6 * std::max(1.0, std::abs(number2));
    }

    bool bool1{};
    bool bool2{};
    if (Poco::NumberParser::tryParseBool(trimmed1, bool1) && Poco::NumberParser::tryParseBool(trimmed2, bool2))
    {
        return bool1 == bool2;
    }

    return trimmed1 == trimmed2;
}

} // namespace

SettingsWindow::SettingsWindow(ProjectMGUI& gui)
    : _gui(gui)
    , _audioCapture(ProjectMSDLApplication::instance().getSubsystem<AudioCapture>())
    , _userConfiguration(ProjectMSDLApplication::instance().UserConfiguration())
    , _commandLineConfiguration(ProjectMSDLApplication::instance().CommandLineConfiguration())
    , _defaultConfiguration(ProjectMSDLApplication::instance().DefaultConfiguration())
    , _effectiveConfiguration(&ProjectMSDLApplication::instance().config())
{
}

void SettingsWindow::Show()
{
    _visible = true;
}

void SettingsWindow::Draw()
{
    if (!_visible)
    {
        return;
    }

    constexpr ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse;
    constexpr ImGuiTabBarFlags tabBarFlags = ImGuiTabBarFlags_None;

    std::string windowId = "Settings";
    if (_changed)
    {
        windowId.append(" [CHANGED - NOT SAVED]");
    }
    windowId.append("###Settings");

    ImGui::SetNextWindowSize(ImVec2(1050, 550), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(windowId.c_str(), &_visible, windowFlags))
    {
        if (ImGui::BeginTabBar("projectM Settings", tabBarFlags))
        {
            DrawProjectMSettingsTab();
            DrawWindowSettingsTab();
            DrawAudioSettingsTab();
            DrawHelpTab();

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        SaveButton();
    }
    ImGui::End();

    if (_pathChooser.Draw())
    {
        auto& selectedDirectory = _pathChooser.SelectedFiles();
        if (!selectedDirectory.empty())
        {
            StoreSetting(_pathChooser.Context(),
                         Poco::Path(selectedDirectory.at(0).path()).makeDirectory().toString(), "");
            _changed = true;
        }
    }
}

void SettingsWindow::DrawProjectMSettingsTab()
{
    if (ImGui::BeginTabItem("projectM"))
    {
        if (ImGui::BeginTable("projectM", 5, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("##desc", ImGuiTableColumnFlags_WidthFixed, .0f);
            ImGui::TableSetupColumn("##setting", ImGuiTableColumnFlags_WidthStretch, .0f);
            ImGui::TableSetupColumn("##choose", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("..."));
            ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("Reset"));
            ImGui::TableSetupColumn("##override", ImGuiTableColumnFlags_WidthFixed, .0f);

            ImGui::TableNextRow();
            // Labels ending in " *" are settings only read on startup, see StartupOnlyNote().
            LabelWithTooltip("Preset Path *", "Path to search for preset files if no playlist is loaded.");
            PathSetting("projectM.presetPath");

            ImGui::TableNextRow();
            LabelWithTooltip("Texture Path *", "Path to search for texture/image files requested by presets.");
            PathSetting("projectM.texturePath");

            ImGui::TableNextRow();
            LabelWithTooltip("Display projectM Logo Preset *",
                             "If enabled, the projectM logo preset is shown on startup.");
            BooleanSetting("projectM.enableSplash", false);

            ImGui::TableNextRow();
            LabelWithTooltip("Lock Preset", "If enabled, presets will not be switched automatically.");
            BooleanSetting("projectM.presetLocked", false);

            ImGui::TableNextRow();
            LabelWithTooltip("Shuffle Presets", "Selects presets randomly from the current playlist.");
            BooleanSetting("projectM.shuffleEnabled", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Skip To Dropped Presets",
                             "If enabled, will skip to the new presets when preset(s) are dropped onto the window and added to the playlist");
            BooleanSetting("projectM.skipToDropped", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Dropped Folder Overrides Playlist",
                             "When dropping a folder, clear the playlist and add all presets from the folder.");
            BooleanSetting("projectM.droppedFolderOverride", false);

            ImGui::TableNextRow();
            LabelWithTooltip("Preset Display Duration",
                             "Time in seconds a preset will be displayed before it's switched.");
            DoubleSetting("projectM.displayDuration", 30.0, 1.0, 240.0);

            ImGui::TableNextRow();
            LabelWithTooltip("Preset Transition Duration",
                             "Time in seconds it takes to transition softly from one preset to another.");
            DoubleSetting("projectM.transitionDuration", 3.0, .0, 10.0);

            ImGui::TableNextRow();
            LabelWithTooltip("Enable Hard Cuts",
                             "Enables beat-driven, fast preset changes.\nSensitivity and earliest switch time can be configured separately.");
            BooleanSetting("projectM.hardCutsEnabled", false);

            ImGui::TableNextRow();
            LabelWithTooltip("  Hard Cut Duration",
                             "Time in seconds before a preset will be switched at\nthe earliest on hard cuts. If larger than display duration,\nhard cuts won't happen at all.");
            DoubleSetting("projectM.hardCutDuration", 20.0, 1.0, 240.0);

            ImGui::TableNextRow();
            LabelWithTooltip("  Hard Cut Threshold",
                             "Volume difference between measurements required to trigger a hard cut.\nHigher values mean fewer hard cuts.");
            DoubleSetting("projectM.hardCutSensitivity", 1.0, 0.0, 10.0);

            ImGui::TableNextRow();
            LabelWithTooltip("Aspect Correction",
                             "Enables aspect ration correction in presets.\nOnly affects presets using the aspect ratio variables.");
            BooleanSetting("projectM.aspectCorrectionEnabled", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Per-Point Mesh Size X/Y",
                             "Size of the per-point transformation grid.\nHigher values produce better quality, but require exponentially more CPU time to calculate.\nMilkdrop's default is 48x32.");
            // Built-in defaults below match the shipped projectMSDL.properties and the defaults used in
            // ProjectMWrapper/SDLRenderingWindow, so the displayed value is the one actually in use.
            IntegerSettingVec("projectM.meshX", "projectM.meshY", 96, 54, 8, 300);

            ImGui::EndTable();
        }
        StartupOnlyNote();
        ImGui::EndTabItem();
    }
}

void SettingsWindow::DrawWindowSettingsTab()
{
    if (ImGui::BeginTabItem("Window / Rendering"))
    {
        if (ImGui::BeginTable("projectM", 5, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("##desc", ImGuiTableColumnFlags_WidthFixed, .0f);
            ImGui::TableSetupColumn("##setting", ImGuiTableColumnFlags_WidthStretch, .0f);
            ImGui::TableSetupColumn("##choose", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("..."));
            ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("Reset"));
            ImGui::TableSetupColumn("##override", ImGuiTableColumnFlags_WidthFixed, .0f);

            ImGui::TableNextRow();
            LabelWithTooltip("Window Placement",
                             "Window size, position and monitor. Changes apply immediately and are also used when starting projectM.\nClick the button to copy the current window size, position and display to the settings.");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button("Use Current Size and Position"))
            {
                auto& renderingWindow = Poco::Util::Application::instance().getSubsystem<SDLRenderingWindow>();
                int x;
                int y;

                renderingWindow.GetWindowSize(x, y);
                StoreSetting("window.width", ToConfigString(x), "1920");
                StoreSetting("window.height", ToConfigString(y), "1080");

                renderingWindow.GetWindowPosition(x, y, true);
                StoreSetting("window.left", ToConfigString(x), "0");
                StoreSetting("window.top", ToConfigString(y), "0");

                StoreSetting("window.overridePosition", ToConfigString(true), "false");
                StoreSetting("window.monitor", ToConfigString(renderingWindow.GetCurrentDisplay() + 1), "0");
            }

            ImGui::TableNextRow();
            LabelWithTooltip("  Window Size",
                             "Window size. Applies immediately (or when leaving fullscreen) and on startup.\nThis might or might not include the window decoration, depending on the OS.");
            IntegerSettingVec("window.width", "window.height", 1920, 1080, 32, 8192);

            ImGui::TableNextRow();
            LabelWithTooltip("  Window Position",
                             "Window position. Applies immediately (or when leaving fullscreen) and on startup.\nThe coordinates are relative to the selected monitor (or absolute if Monitor is 0).\nA borderless window is always kept fully on its monitor.");
            WindowPositionSetting();

            ImGui::TableNextRow();
            LabelWithTooltip("  Monitor",
                             "Use 0 to let the OS select the monitor, or any positive number to select a specific monitor.\nIf the number is larger than the number of connected monitors, the last available one will be used.");
            IntegerSetting("window.monitor", 0, 0, 10);

            ImGui::TableNextRow();
            LabelWithTooltip("Borderless Window",
                             "Don't display the window border and title bar if the OS supports it.\nCan make the window immovable.\nA borderless window is kept fully on its monitor: Windows shows a borderless, monitor-sized window\nlike fullscreen at the monitor's corner, so placing it elsewhere would offset the mouse.");
            BooleanSetting("window.borderless", false);

            ImGui::TableNextRow();
            LabelWithTooltip("Start Fullscreen *", "Start projectM in fullscreen mode");
            BooleanSetting("window.fullscreen", false);

            ImGui::TableNextRow();
            LabelWithTooltip("  Exclusive Fullscreen Mode",
                             "Use exclusive mode if fullscreen, e.g. not as a borderless window.\nThis can improve performance, but may switch the desktop resolution!");
            BooleanSetting("window.fullscreen.exclusiveMode", false);

            ImGui::TableNextRow();
            LabelWithTooltip("  Exclusive Mode Resolution",
                             "Resolution to change to in exclusive fullscreen mode.\nNot all graphics driver support arbitrary resolution and will use the next-best supported one.\nSet either value to 0 to use the nearest resolution for the current window size.\nApplies immediately if currently in exclusive fullscreen, otherwise the next time fullscreen is entered.");
            // The keys must be "window.fullscreen.*" (read via the "window" config view in SDLRenderingWindow).
            // This previously used "fullscreen.width/height", so the setting had no effect. 0 = automatic.
            IntegerSettingVec("window.fullscreen.width", "window.fullscreen.height", 0, 0, 0, 8192);

            ImGui::TableNextRow();
            LabelWithTooltip("Target FPS",
                             "Limit frames rendered per second to the given FPS value.\nNOTE: A value of 0 will NOT limit FPS and render at either VSync or unlimited pace, possibly using all CPU/GPU resources.");
            IntegerSetting("projectM.fps", 60, 0, 300);

            ImGui::TableNextRow();
            LabelWithTooltip("Wait for Vertical Sync",
                             "Wait for vertical sync interval before displaying the next frame.\nThis will limit max FPS to the vertical sync frequency but prevents tearing.");
            // Defaults (true) match SDLRenderingWindow::UpdateSwapInterval() and projectMSDL.properties (were false here).
            BooleanSetting("window.waitForVerticalSync", true);

            ImGui::TableNextRow();
            LabelWithTooltip("  Use Adaptive Sync",
                             "Tries to use adaptive vertical sync if vertical sync is enabled.\nWhen using a monitor capable of adaptive sync, setting FPS to 0 gives the best results.");
            BooleanSetting("window.adaptiveVerticalSync", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Preset Name in Title",
                             "Controls displaying the current preset name after the application name in the window title.");
            BooleanSetting("window.displayPresetNameInTitle", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Display Toast Messages",
                             "Controls displaying toast messages/notifications, e.g. when changing the audio device.");
            BooleanSetting("projectM.displayToasts", true);

            ImGui::TableNextRow();
            LabelWithTooltip("UI Scaling Factor", "Multiplies the default UI/font size with the given factor.");
            DoubleSetting("window.uiScale", 1.0, 0.1, 3.0);

            ImGui::EndTable();
        }
        StartupOnlyNote();
        ImGui::EndTabItem();
    }
}

void SettingsWindow::DrawAudioSettingsTab()
{
    if (ImGui::BeginTabItem("Audio"))
    {
        if (ImGui::BeginTable("projectM", 5, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("##desc", ImGuiTableColumnFlags_WidthFixed, .0f);
            ImGui::TableSetupColumn("##setting", ImGuiTableColumnFlags_WidthStretch, .0f);
            ImGui::TableSetupColumn("##choose", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("..."));
            ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("Reset"));
            ImGui::TableSetupColumn("##override", ImGuiTableColumnFlags_WidthFixed, .0f);

            ImGui::TableNextRow();
            LabelWithTooltip("Audio Capturing Device", "The device to capture audio from.");
            AudioDeviceSetting();

            ImGui::TableNextRow();
            LabelWithTooltip("Beat Sensitivity", "How strongly presets react to the music: 0 = not at all, 1 = normal, 2 = twice as strong. Also changes the size of waveforms drawn from the audio. Up/Down arrow keys change it too.");
            DoubleSetting("projectM.beatSensitivity", 1.0, 0.0, 2.0);

            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

void SettingsWindow::DrawHelpTab() const
{
    if (ImGui::BeginTabItem("Help"))
    {
        const auto& userDataDir = ProjectMSDLApplication::instance().UserDataDir();

        const auto& defaultConfigurationPath = ProjectMSDLApplication::instance().DefaultConfigurationPath();

        ImGui::SeparatorText("General");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Hover over a setting's name to see a short description.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Changes take effect immediately, except settings marked with * which are only used when projectM starts.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "A red [!] marks a setting that was passed on the command line. The command-line value is used for as long as projectM runs, regardless of what is set here.");

        ImGui::SeparatorText("Saving");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Changes only last until projectM is closed unless you click \"Save Settings\". The window title shows [CHANGED - NOT SAVED] while there are unsaved changes.");
        ImGui::Bullet();
        ImGui::TextWrapped("Your settings are saved to projectMSDL.properties in \"%s\".",
                           userDataDir.c_str());
        ImGui::Bullet();
        if (ProjectMSDLApplication::instance().Portable())
        {
            ImGui::TextWrapped("%s", "Portable mode is on (portable.txt is next to the executable), so settings and favorites are kept in the \"userdata\" folder next to it. Delete portable.txt to use the normal user folder instead.");
        }
        else
        {
            ImGui::TextWrapped("%s", "To keep settings and favorites next to the executable instead (portable mode), create an empty file named portable.txt next to it.");
        }

        ImGui::SeparatorText("Number settings");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Type a value, then press Enter or click the \"Apply\" button next to it.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Values that are typed but not applied yet are highlighted in orange. Tab or clicking elsewhere keeps them without applying, e.g. to type width and height and then apply both.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "While typing, Up/Down step the value (hold Shift for 10x steps). Escape cancels the typed value.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Values outside the allowed range are clamped to the nearest limit.");

        ImGui::SeparatorText("Defaults and Reset");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "\"Reset\" appears only for settings whose value differs from the default. Click it to go back to the default value.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Changing a setting back to its default value has the same effect as clicking \"Reset\".");
        ImGui::Bullet();
        if (!defaultConfigurationPath.empty())
        {
            ImGui::TextWrapped("Default values come from the application's configuration file \"%s\", or from built-in defaults for settings it doesn't contain.",
                               defaultConfigurationPath.c_str());
        }
        else
        {
            ImGui::TextWrapped("%s", "No application configuration file (projectMSDL.properties next to or above the executable) was found, so built-in defaults are used.");
        }

        ImGui::SeparatorText("Window placement");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Window size, position and monitor apply immediately and are also used on the next start. In fullscreen, size and position are applied when leaving fullscreen, while a monitor change moves the fullscreen window right away.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "\"Use Current Size and Position\" copies the window's current size, position and monitor into the settings.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "A borderless window is always kept fully on its monitor. Otherwise Windows may show it at the monitor corner while the window is somewhere else, which offsets the mouse.");

        ImGui::EndTabItem();
    }
}

void SettingsWindow::SaveButton()
{
    if (ImGui::Button("Save Settings"))
    {
        try
        {
            auto configFile = _commandLineConfiguration->getString("app.UserConfigurationFile", "");
            if (!configFile.empty())
            {
                _userConfiguration->save(configFile);
                Poco::NotificationCenter::defaultCenter().postNotification(
                    new Notification::DisplayToast("Settings saved!"));
            }
            else
            {
                Poco::NotificationCenter::defaultCenter().postNotification(
                    new Notification::DisplayToast("Error saving settings"));
            }
        }
        catch (...)
        {
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast("Error saving settings"));
        }

        _changed = false;
    }
}

void SettingsWindow::LabelWithTooltip(const std::string& label, const std::string& tooltipText)
{
    ImGui::TableSetColumnIndex(0);

    ImGui::TextUnformatted(label.c_str());
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tooltipText.c_str());
        ImGui::EndTooltip();
    }
}

void SettingsWindow::PathSetting(const std::string& property)
{
    ImGui::TableSetColumnIndex(1);

    auto path = _effectiveConfiguration->getString(property, "");
    char pathBuffer[2048]{};
    strncpy(pathBuffer, path.c_str(), std::min<size_t>(2047, path.size()));

    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputText(std::string("##path_" + property).c_str(), &pathBuffer[0], IM_ARRAYSIZE(pathBuffer),
                         ImGuiInputTextFlags_EnterReturnsTrue))
    {
        StoreSetting(property, std::string(pathBuffer), "");
        _changed = true;
    }

    ImGui::TableSetColumnIndex(2);

    ImGui::PushID(std::string(property + "_ChoosePathButton").c_str());
    if (ImGui::Button("..."))
    {
        _pathChooser.CurrentDirectory(path);
        _pathChooser.Title("Select directory");
        _pathChooser.Context(property);
        _pathChooser.Show();
    }
    ImGui::PopID();

    ResetButton(property, "");

    if (_commandLineConfiguration->has(property))
    {
        OverriddenSettingMarker();
    }
}


// All *Setting() helpers below follow the same pattern:
// - Read from _effectiveConfiguration (command line > user settings > default config file > built-in default),
//   so the window shows the value actually in use. Reading only the user config showed made-up defaults
//   for anything set in projectMSDL.properties.
// - Write via StoreSetting(), which removes the user setting again if it equals the fallback value.
// - ResetButton() only shows if the user value differs from the fallback value.
void SettingsWindow::BooleanSetting(const std::string& property, bool defaultValue)
{
    ImGui::TableSetColumnIndex(1);

    auto value = _effectiveConfiguration->getBool(property, defaultValue);

    if (ImGui::Checkbox(std::string("##boolean_" + property).c_str(), &value))
    {
        StoreSetting(property, ToConfigString(value), ToConfigString(defaultValue));
        _changed = true;
    }

    ResetButton(property, ToConfigString(defaultValue));

    if (_commandLineConfiguration->has(property))
    {
        OverriddenSettingMarker();
    }
}

void SettingsWindow::IntegerSetting(const std::string& property, int defaultValue, int min, int max)
{
    ImGui::TableSetColumnIndex(1);

    auto value = _effectiveConfiguration->getInt(property, defaultValue);

    if (NumberInput::Int(std::string("##integer_" + property).c_str(), &value, 1, min, max))
    {
        StoreSetting(property, ToConfigString(value), ToConfigString(defaultValue));
        _changed = true;
    }

    ResetButton(property, ToConfigString(defaultValue));

    if (_commandLineConfiguration->has(property))
    {
        OverriddenSettingMarker();
    }
}

void SettingsWindow::IntegerSettingVec(const std::string& property1, const std::string& property2, int defaultValue1,
                                       int defaultValue2, int min, int max)
{
    ImGui::TableSetColumnIndex(1);

    int values[2] = {
        _effectiveConfiguration->getInt(property1, defaultValue1),
        _effectiveConfiguration->getInt(property2, defaultValue2)};

    if (NumberInput::Int(std::string("##integer_" + property1 + property2).c_str(), values, 2, min, max))
    {
        StoreSetting(property1, ToConfigString(values[0]), ToConfigString(defaultValue1));
        StoreSetting(property2, ToConfigString(values[1]), ToConfigString(defaultValue2));
        _changed = true;
    }

    ResetButton(property1, ToConfigString(defaultValue1), property2, ToConfigString(defaultValue2));

    if (_commandLineConfiguration->has(property1) || _commandLineConfiguration->has(property2))
    {
        OverriddenSettingMarker();
    }
}

void SettingsWindow::DoubleSetting(const std::string& property, double defaultValue, double min, double max)
{
    ImGui::TableSetColumnIndex(1);

    auto value = static_cast<float>(_effectiveConfiguration->getDouble(property, defaultValue));

    if (NumberInput::Float(std::string("##double_" + property).c_str(), &value, static_cast<float>(min),
                               static_cast<float>(max)))
    {
        StoreSetting(property, ToConfigString(value), ToConfigString(defaultValue));
        _changed = true;
    }

    ResetButton(property, ToConfigString(defaultValue));

    if (_commandLineConfiguration->has(property))
    {
        OverriddenSettingMarker();
    }
}

void SettingsWindow::WindowPositionSetting()
{
    ImGui::TableSetColumnIndex(1);

    bool positionIsOverridden = _effectiveConfiguration->getBool("window.overridePosition", false);

    if (ImGui::Checkbox("##window_set_pos", &positionIsOverridden))
    {
        StoreSetting("window.overridePosition", ToConfigString(positionIsOverridden), "false");
        _changed = true;
    }

    if (positionIsOverridden)
    {
        ImGui::SameLine();

        int values[2] = {
            _effectiveConfiguration->getInt("window.left", 0),
            _effectiveConfiguration->getInt("window.top", 0)};

        if (NumberInput::Int("##window_pos", values, 2, -8192, 8192))
        {
            StoreSetting("window.left", ToConfigString(values[0]), "0");
            StoreSetting("window.top", ToConfigString(values[1]), "0");
            _changed = true;
        }
    }

    ResetButton("window.overridePosition", "false");

    if (_commandLineConfiguration->has("window.left") || _commandLineConfiguration->has("window.top"))
    {
        OverriddenSettingMarker();
    }
}

void SettingsWindow::AudioDeviceSetting()
{
    ImGui::TableSetColumnIndex(1);

    auto devices = _audioCapture.AudioDeviceList();
    auto currentIndex = _audioCapture.AudioDeviceIndex();

    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##audiodevice", devices.at(currentIndex).c_str(), 0))
    {
        for (const auto& device : devices)
        {
            bool isSelected = device.first == currentIndex;

            if (ImGui::Selectable(device.second.c_str(), "", isSelected))
            {
                _audioCapture.AudioDeviceIndex(device.first);
                if (device.first == -1)
                {
                    StoreSetting("audio.device", "-1", "-1");
                }
                else
                {
                    StoreSetting("audio.device", device.second, "-1");
                }
                _changed = true;
            }

            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ResetButton("audio.device", "-1");

    if (_commandLineConfiguration->has("audio.device"))
    {
        OverriddenSettingMarker();
    }
}

std::string SettingsWindow::FallbackValue(const std::string& property, const std::string& defaultValue) const
{
    if (_defaultConfiguration && _defaultConfiguration->has(property))
    {
        return _defaultConfiguration->getRawString(property);
    }
    return defaultValue;
}

bool SettingsWindow::IsModified(const std::string& property, const std::string& defaultValue) const
{
    if (!_userConfiguration->has(property))
    {
        return false;
    }
    return !SameSettingValue(_userConfiguration->getRawString(property), FallbackValue(property, defaultValue));
}

void SettingsWindow::StoreSetting(const std::string& property, const std::string& value, const std::string& defaultValue)
{
    if (SameSettingValue(value, FallbackValue(property, defaultValue)))
    {
        // Back to the original value: drop the user setting, so the Reset button disappears
        // and the default (config file / built-in) value is used again.
        if (_userConfiguration->has(property))
        {
            _userConfiguration->remove(property);
        }
    }
    else
    {
        _userConfiguration->setString(property, value);
    }
    _changed = true;
}

bool SettingsWindow::ResetButton(const std::string& property1, const std::string& defaultValue1,
                                 const std::string& property2, const std::string& defaultValue2)
{
    if (!IsModified(property1, defaultValue1) && (property2.empty() || !IsModified(property2, defaultValue2)))
    {
        return false;
    }

    ImGui::TableSetColumnIndex(3);

    bool pressed{false};

    ImGui::PushID(std::string(property1 + property2 + "_ResetButton").c_str());
    if (ImGui::Button("Reset"))
    {
        _userConfiguration->remove(property1);
        if (!property2.empty())
        {
            _userConfiguration->remove(property2);
        }
        _changed = true;
        pressed = true;
    }
    ImGui::PopID();

    return pressed;
}

void SettingsWindow::OverriddenSettingMarker()
{
    ImGui::TableSetColumnIndex(4);

    ImGui::TextColored(ImVec4(1, 0, 0, 1), "[!]");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Value passed as command line argument.");
        ImGui::TextUnformatted("The value configured here will only be used if NOT overridden via the command line.");
        ImGui::EndTooltip();
    }
}

void SettingsWindow::StartupOnlyNote()
{
    ImGui::Spacing();
    ImGui::TextDisabled("* Takes effect the next time projectM is started (click \"Save Settings\" first). All other settings apply immediately.");
}
