#include "SettingsWindow.h"

#include "AudioCapture.h"
#include "ProjectMSDLApplication.h"
#include "OutputPipeline.h"
#include "ProjectMWrapper.h"
#include "RemoteControl.h"
#include "SDLRenderingWindow.h"

#include "ProjectMGUI.h"
#include "NumberInput.h"
#include "ScreenPosition.h"

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
            DrawEffectsSettingsTab();
            DrawRemoteSettingsTab();
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
            LabelWithTooltip("  Transition Effects",
                             "Effects used for smooth transitions. Each transition picks one of the checked effects at random.\n"
                             "Fade is a plain crossfade. Beat Pulse dissolves the old preset in bubbles that react to the bass,\n"
                             "Datamosh drags the old picture along a moving path like a video glitch.\n"
                             "At least one effect stays on. Hard cuts never use an effect.");
            TransitionEffectsSetting();

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
                             "Window size, position and monitor. Changes apply immediately and are also used when starting projectM.\nIn fullscreen, size and position apply when you leave fullscreen; a monitor change moves it right away.\nClick the button to copy the current window size, position and display to the settings.");
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
            // Adaptive vsync is only used while "Wait for Vertical Sync" is on, so grey it out otherwise.
            const bool verticalSyncEnabled = _effectiveConfiguration->getBool("window.waitForVerticalSync", true);
            ImGui::BeginDisabled(!verticalSyncEnabled);
            LabelWithTooltip("  Use Adaptive VSync",
                             "If a frame is late, show it right away (may tear briefly) instead of waiting a whole extra refresh.\nReduces stutter when a preset can't keep up with the refresh rate.\nOnly used while \"Wait for Vertical Sync\" is on. Falls back to normal vertical sync if the graphics driver\ndoesn't support it (the log file says which mode is used).\nThis is not FreeSync/G-Sync, which are enabled in the graphics driver settings.");
            BooleanSetting("window.adaptiveVerticalSync", true);
            ImGui::EndDisabled();

            ImGui::TableNextRow();
            LabelWithTooltip("Preset Name in Title",
                             "Controls displaying the current preset name after the application name in the window title.");
            BooleanSetting("window.displayPresetNameInTitle", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Display Toast Messages",
                             "Controls displaying toast messages/notifications, e.g. when changing the audio device.");
            BooleanSetting("projectM.displayToasts", true);

            const std::vector<std::pair<std::string, std::string>> corners{
                {ScreenPosition::TopLeft, "Top left"},
                {ScreenPosition::TopRight, "Top right"},
                {ScreenPosition::BottomLeft, "Bottom left"},
                {ScreenPosition::BottomRight, "Bottom right"}};

            ImGui::TableNextRow();
            LabelWithTooltip("  Toast Position",
                             "Where toast messages appear.\nIf the FPS counter is in the same corner, toasts are shown next to it instead of on top of it.");
            auto toastPositions = corners;
            toastPositions.emplace_back(ScreenPosition::Center, "Center");
            ChoiceSetting("projectM.toastPosition", ScreenPosition::TopRight, toastPositions);

            ImGui::TableNextRow();
            LabelWithTooltip("UI Scaling Factor", "Multiplies the default UI/font size with the given factor.");
            DoubleSetting("window.uiScale", 1.0, 0.1, 3.0);

            ImGui::TableNextRow();
            LabelWithTooltip("Show FPS Counter", "Shows the current frames per second in a corner of the screen.");
            BooleanSetting("window.fpsCounter", false);

            const bool fpsCounterEnabled = _effectiveConfiguration->getBool("window.fpsCounter", false);
            ImGui::BeginDisabled(!fpsCounterEnabled);

            ImGui::TableNextRow();
            LabelWithTooltip("  Position", "Corner of the screen the FPS counter is shown in.");
            ChoiceSetting("window.fpsCounter.position", ScreenPosition::TopLeft, corners);

            ImGui::TableNextRow();
            LabelWithTooltip("  Size", "Size of the FPS counter. Independent of the UI scaling factor.");
            DoubleSetting("window.fpsCounter.size", 1.0, 0.25, 4.0);

            ImGui::TableNextRow();
            LabelWithTooltip("  Show Frame Times",
                             "Also shows how long frames take, with a graph of the last 240 frames.\n"
                             "Frame: time between frames as they are shown, average (and maximum) of the last half second.\n"
                             "GPU: time the graphics card needs to draw the preset. CPU: time the program needs per frame.\n"
                             "Graph: one bar per frame, the line is the frame time the refresh rate or FPS limit should give.\n"
                             "Red bars took more than 1.5 times as long.\n"
                             "If GPU and CPU are low but motion still stutters, frames are being dropped after projectM\n"
                             "(e.g. by the desktop compositor in borderless mode); exclusive fullscreen may help.");
            BooleanSetting("window.fpsCounter.frameTimes", false);

            ImGui::EndDisabled();

            ImGui::EndTable();
        }
        StartupOnlyNote();
        ImGui::EndTabItem();
    }
}

void SettingsWindow::DrawEffectsSettingsTab()
{
    if (ImGui::BeginTabItem("Effects"))
    {
        if (ImGui::BeginTable("projectM", 5, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("##desc", ImGuiTableColumnFlags_WidthFixed, .0f);
            ImGui::TableSetupColumn("##setting", ImGuiTableColumnFlags_WidthStretch, .0f);
            ImGui::TableSetupColumn("##choose", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("..."));
            ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("Reset"));
            ImGui::TableSetupColumn("##override", ImGuiTableColumnFlags_WidthFixed, .0f);

            ImGui::TableNextRow();
            LabelWithTooltip("Mirror",
                             "Mirrors the picture. M key cycles through the modes.\n"
                             "Left to Right: the left half is mirrored onto the right half.\n"
                             "Top to Bottom: the top half is mirrored onto the bottom half.\n"
                             "4-Way: the top left quarter is mirrored into all four quarters.\n"
                             "Also applies to the Spout output.");
            ChoiceSetting("effects.mirror", "off", OutputEffects::MirrorModeNames());

            ImGui::TableNextRow();
            LabelWithTooltip("Zoom",
                             "Zooms into the center of the picture: 1 = off, 2 = twice as large.\n"
                             "+/- keys change it too. Also applies to the Spout output.");
            DoubleSetting("effects.zoom", 1.0, OutputEffects::MinZoom, OutputEffects::MaxZoom);

            ImGui::TableNextRow();
            LabelWithTooltip("Ambient Speed",
                             "Slows down all preset animation evenly: motion, zoom, rotation, warp,\n"
                             "fading trails and the reaction to the music.\n"
                             "1 = normal speed, 0.5 = half speed, 0.1 = slowest. Left/Right arrow keys change it too.\n"
                             "Display and transition durations stay in real seconds.");
            DoubleSetting("projectM.ambientSpeed", 1.0, 0.1, 1.0);

            ImGui::TableNextRow();
            LabelWithTooltip("Same Speed at Any FPS",
                             "Presets move as fast at any FPS as they do at 60 FPS.\n"
                             "Off: presets move faster at higher FPS and slower at lower FPS,\n"
                             "like in Milkdrop and the official projectM.");
            BooleanSetting("projectM.fpsIndependentMotion", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Aspect Correction",
                             "Enables aspect ration correction in presets.\nOnly affects presets using the aspect ratio variables.");
            BooleanSetting("projectM.aspectCorrectionEnabled", true);

            ImGui::TableNextRow();
            LabelWithTooltip("Per-Point Mesh Size X/Y",
                             "Size of the per-point transformation grid.\nHigher values produce better quality, but require exponentially more CPU time to calculate.\nMilkdrop's default is 48x36.");
            // Built-in defaults below match the shipped projectMSDL.properties and the defaults used in
            // ProjectMWrapper/SDLRenderingWindow, so the displayed value is the one actually in use.
            IntegerSettingVec("projectM.meshX", "projectM.meshY", 96, 54, 8, 300);


            ImGui::EndTable();
        }
        ImGui::EndTabItem();
    }
}

void SettingsWindow::DrawRemoteSettingsTab()
{
#ifdef _WIN32
    const char* tabName = "Remote / Spout###RemoteTab";
#else
    const char* tabName = "Remote Control###RemoteTab";
#endif
    if (ImGui::BeginTabItem(tabName))
    {
        if (ImGui::BeginTable("projectM", 5, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("##desc", ImGuiTableColumnFlags_WidthFixed, .0f);
            ImGui::TableSetupColumn("##setting", ImGuiTableColumnFlags_WidthStretch, .0f);
            ImGui::TableSetupColumn("##choose", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("..."));
            ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("Reset"));
            ImGui::TableSetupColumn("##override", ImGuiTableColumnFlags_WidthFixed, .0f);

            ImGui::TableNextRow();
            LabelWithTooltip("Enable Remote Control",
                             "Control projectM from a web browser, e.g. on your phone: next/previous/random preset,\n"
                             "lock, favorites, disable, modes, beat sensitivity, ambient speed, mirror and zoom.");
            BooleanSetting("remote.enabled", false);

            ImGui::BeginDisabled(!_effectiveConfiguration->getBool("remote.enabled", false));

            ImGui::TableNextRow();
            LabelWithTooltip("  Port", "Network port of the remote control page. Change it if another program uses it.");
            IntegerSetting("remote.port", RemoteControl::DefaultPort, 1024, 65535);

            ImGui::TableNextRow();
            LabelWithTooltip("  Allow Other Devices",
                             "Off: only this computer can open the remote control page.\n"
                             "On: other devices in your network, like your phone, can open it too. Needs a password.\n"
                             "The connection isn't encrypted, so only use this in your home network.\n"
                             "Windows may ask whether projectM may use the network the first time: allow private networks.");
            BooleanSetting("remote.allowOtherDevices", false);

            ImGui::TableNextRow();
            LabelWithTooltip("  Password",
                             "Required when other devices are allowed. Your phone's browser asks for it once\n"
                             "(any user name). Press Enter to apply. Stored in the settings file as plain text.");
            PasswordSetting("remote.password");

            ImGui::EndDisabled();

            ImGui::EndTable();
        }

        if (_effectiveConfiguration->getBool("remote.enabled", false))
        {
            // Listing the network addresses takes a moment, so only refresh them every few seconds.
            static std::vector<std::string> urls;
            static uint64_t urlsTicks{0};
            if (urls.empty() || SDL_GetTicks64() - urlsTicks > 3000)
            {
                urls = RemoteControl::Urls();
                urlsTicks = SDL_GetTicks64();
            }

            ImGui::SeparatorText("Open the remote control at");
            ImGui::BulletText("This computer: %s", urls.front().c_str());
            for (size_t index = 1; index < urls.size(); index++)
            {
                ImGui::BulletText("Phone/other devices (same network): %s", urls[index].c_str());
            }
            if (_effectiveConfiguration->getBool("remote.allowOtherDevices", false) &&
                _effectiveConfiguration->getString("remote.password", "").empty())
            {
                ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", "Set a password to allow other devices.");
            }
        }

#ifdef _WIN32
        ImGui::SeparatorText("Spout output (OBS)");
        if (ImGui::BeginTable("spout", 5, ImGuiTableFlags_None))
        {
            ImGui::TableSetupColumn("##desc", ImGuiTableColumnFlags_WidthFixed, .0f);
            ImGui::TableSetupColumn("##setting", ImGuiTableColumnFlags_WidthStretch, .0f);
            ImGui::TableSetupColumn("##choose", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("..."));
            ImGui::TableSetupColumn("##reset", ImGuiTableColumnFlags_WidthFixed, ButtonColumnWidth("Reset"));
            ImGui::TableSetupColumn("##override", ImGuiTableColumnFlags_WidthFixed, .0f);

            ImGui::TableNextRow();
            LabelWithTooltip("Spout Output",
                             "Shares the picture with other programs via Spout, e.g. OBS with the obs-spout2-plugin\n"
                             "(add a \"Spout2 Capture\" source and pick \"projectM\").\n"
                             "Keeps working while projectM is minimized or covered, and contains no menus, toasts or FPS counter.");
            BooleanSetting("output.spout.enabled", false);

            ImGui::BeginDisabled(!_effectiveConfiguration->getBool("output.spout.enabled", false));
            ImGui::TableNextRow();
            LabelWithTooltip("  Spout Resolution",
                             "Size of the Spout picture in pixels, e.g. 1920 x 1080. 0 x 0 = same as the window.\n"
                             "projectM renders at this size. The window then shows the same picture scaled to fit,\n"
                             "with black bars if the window has a different shape.");
            IntegerSettingVec("output.spout.width", "output.spout.height", 0, 0, 0, 7680);
            ImGui::EndDisabled();


            ImGui::EndTable();
        }
#endif

        ImGui::EndTabItem();
    }
}

void SettingsWindow::TransitionEffectsSetting()
{
    const std::string property = "projectM.transitionEffects";
    ImGui::TableSetColumnIndex(1);

    auto current = _effectiveConfiguration->getString(property, "all");
    const auto& names = ProjectMWrapper::TransitionEffectNames();
    auto& projectMWrapper = Poco::Util::Application::instance().getSubsystem<ProjectMWrapper>();

    // Four per line, so the list stays compact.
    for (size_t index = 0; index < names.size(); index++)
    {
        if (index % 4 != 0)
        {
            ImGui::SameLine();
        }
        bool enabled = projectMWrapper.TransitionEffectEnabled(names[index].first);
        if (ImGui::Checkbox((names[index].second + "##transition_" + names[index].first).c_str(), &enabled))
        {
            StoreSetting(property, ProjectMWrapper::TransitionEffectsWith(current, names[index].first, enabled), "all");
            current = _effectiveConfiguration->getString(property, "all");
        }
    }

    ResetButton(property, "all");

    if (_commandLineConfiguration->has(property))
    {
        OverriddenSettingMarker();
    }
}

void SettingsWindow::PasswordSetting(const std::string& property)
{
    ImGui::TableSetColumnIndex(1);

    auto value = _effectiveConfiguration->getString(property, "");
    char buffer[256]{};
    strncpy(buffer, value.c_str(), std::min<size_t>(255, value.size()));

    ImGui::SetNextItemWidth(-1);
    ImGui::InputText(std::string("##password_" + property).c_str(), &buffer[0], IM_ARRAYSIZE(buffer), ImGuiInputTextFlags_Password);
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        StoreSetting(property, std::string(buffer), "");
    }

    ResetButton(property, "");

    if (_commandLineConfiguration->has(property))
    {
        OverriddenSettingMarker();
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
            LabelWithTooltip("Beat Averaging",
                             "On: presets react to the music the standard Milkdrop way. The beat is compared with the last few\n"
                             "seconds of music, so quiet and loud songs get about the same amount of movement.\n"
                             "Beat Sensitivity and the bass/mid/treble sensitivities are not used then (greyed out),\n"
                             "and the Up/Down arrow keys don't change Beat Sensitivity.\n"
                             "Off: the sensitivity settings set how strongly presets react.");
            BooleanSetting("projectM.beatAveraging", false);

            ImGui::TableNextRow();
            ImGui::BeginDisabled(_effectiveConfiguration->getBool("projectM.beatAveraging", false));
            LabelWithTooltip("  Beat Sensitivity",
                             "How strongly presets react to the music: 0 = not at all, 1 = normal, 2 = twice as strong.\n"
                             "Up/Down arrow keys change it too. Not used while Beat Averaging is on.");
            DoubleSetting("projectM.beatSensitivity", 1.0, 0.0, 2.0);

            ImGui::TableNextRow();
            LabelWithTooltip("    Bass Sensitivity",
                             "How strongly presets react to the bass, on top of Beat Sensitivity:\n"
                             "0 = not at all, 1 = normal, 2 = twice as strong.");
            DoubleSetting("projectM.bassSensitivity", 1.0, 0.0, 2.0);

            ImGui::TableNextRow();
            LabelWithTooltip("    Mid Sensitivity",
                             "How strongly presets react to the mids, on top of Beat Sensitivity:\n"
                             "0 = not at all, 1 = normal, 2 = twice as strong.");
            DoubleSetting("projectM.midSensitivity", 1.0, 0.0, 2.0);

            ImGui::TableNextRow();
            LabelWithTooltip("    Treble Sensitivity",
                             "How strongly presets react to the treble, on top of Beat Sensitivity:\n"
                             "0 = not at all, 1 = normal, 2 = twice as strong.");
            DoubleSetting("projectM.trebleSensitivity", 1.0, 0.0, 2.0);
            ImGui::EndDisabled();

            ImGui::TableNextRow();
            LabelWithTooltip("Frequency Bands",
                             "Which frequencies count as bass, mids and treble for the music reaction.\n"
                             "Music: bass 20-250 Hz, mid 250-4000 Hz, treble 4000-20000 Hz.\n"
                             "Milkdrop Standard: the split of Milkdrop and the official projectM, which only uses\n"
                             "0-11025 Hz in three equal parts (bass 0-3675 Hz, mid 3675-7350 Hz, treble 7350-11025 Hz).\n"
                             "Custom: the ranges below.\n"
                             "The analyzer has a resolution of about 45 Hz, so very narrow ranges get rounded.");
            ChoiceSetting("projectM.bandRanges", ProjectMWrapper::BandRangesMusic,
                          {{ProjectMWrapper::BandRangesMusic, "Music"},
                           {ProjectMWrapper::BandRangesStandard, "Milkdrop Standard"},
                           {ProjectMWrapper::BandRangesCustom, "Custom"}});

            ImGui::BeginDisabled(_effectiveConfiguration->getString("projectM.bandRanges", ProjectMWrapper::BandRangesMusic) != ProjectMWrapper::BandRangesCustom);
            const struct {
                const char* label;
                const char* lowKey;
                const char* highKey;
                int low;
                int high;
            } bandRanges[]{{"  Bass From/To (Hz)", "projectM.bandBassLow", "projectM.bandBassHigh", 20, 250},
                           {"  Mid From/To (Hz)", "projectM.bandMidLow", "projectM.bandMidHigh", 250, 4000},
                           {"  Treble From/To (Hz)", "projectM.bandTrebleLow", "projectM.bandTrebleHigh", 4000, 20000}};
            for (const auto& band : bandRanges)
            {
                ImGui::TableNextRow();
                LabelWithTooltip(band.label, "Frequency range for Custom frequency bands, in Hz.");
                IntegerSettingVec(band.lowKey, band.highKey, band.low, band.high, 0, 24000);
            }
            ImGui::EndDisabled();

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

        ImGui::SeparatorText("Settings");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Hover over a setting's name to see what it does.");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Changes apply immediately. Settings marked * are used at the next start. A red [!] means the value was set on the command line, which wins while projectM runs.");

        ImGui::SeparatorText("Saving");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Click \"Save Settings\" to keep changes after projectM closes, including those made with keys, the Options menu or the remote control. Changes made in this window also show [CHANGED - NOT SAVED] in the title.");
        ImGui::Bullet();
        ImGui::TextWrapped("Saved in: %s", userDataDir.c_str());
        ImGui::Bullet();
        if (ProjectMSDLApplication::instance().Portable())
        {
            ImGui::TextWrapped("%s", "Portable mode is on (portable.txt next to the program). Delete portable.txt to use the normal user folder.");
        }
        else
        {
            ImGui::TextWrapped("%s", "Portable mode is off. Put a file named portable.txt next to the program to keep settings and favorites there.");
        }

        ImGui::SeparatorText("Number boxes");
        ImGui::Bullet();
        ImGui::TextWrapped("%s", "Type a value, then press Enter or click Apply. Orange = typed, not applied yet. Up/Down step the value (Shift: x10), Esc cancels, Tab moves on without applying.");

        ImGui::SeparatorText("Defaults");
        ImGui::Bullet();
        if (!defaultConfigurationPath.empty())
        {
            ImGui::TextWrapped("\"Reset\" appears when a value differs from its default. Defaults come from %s.",
                               defaultConfigurationPath.c_str());
        }
        else
        {
            ImGui::TextWrapped("%s", "\"Reset\" appears when a value differs from its default. No projectMSDL.properties was found next to the program, so built-in defaults are used.");
        }

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
    // AllowWhenDisabled: greyed-out settings still explain themselves (and why they're greyed out).
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
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

void SettingsWindow::ChoiceSetting(const std::string& property, const std::string& defaultValue,
                                   const std::vector<std::pair<std::string, std::string>>& choices)
{
    ImGui::TableSetColumnIndex(1);

    auto value = _effectiveConfiguration->getString(property, defaultValue);

    std::string currentLabel = value;
    for (const auto& choice : choices)
    {
        if (choice.first == value)
        {
            currentLabel = choice.second;
        }
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo(std::string("##choice_" + property).c_str(), currentLabel.c_str()))
    {
        for (const auto& choice : choices)
        {
            bool selected = choice.first == value;
            if (ImGui::Selectable(choice.second.c_str(), selected))
            {
                StoreSetting(property, choice.first, defaultValue);
                _changed = true;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ResetButton(property, defaultValue);

    if (_commandLineConfiguration->has(property))
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
