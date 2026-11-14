#include "gui/MainMenu.h"

#include "AudioCapture.h"
#include "OutputPipeline.h"
#include "ProjectMSDLApplication.h"
#include "ProjectMWrapper.h"

#include "gui/ProjectMGUI.h"
#include "gui/NumberInput.h"
#include "gui/SystemBrowser.h"

#include "notifications/PlaybackControl.h"
#include "notifications/Quit.h"
#include "notifications/UpdateWindowTitle.h"

#include "imgui.h"

#include <Poco/NotificationCenter.h>


MainMenu::MainMenu(ProjectMGUI& gui)
    : _notificationCenter(Poco::NotificationCenter::defaultCenter())
    , _gui(gui)
    , _projectMWrapper(Poco::Util::Application::instance().getSubsystem<ProjectMWrapper>())
    , _audioCapture(Poco::Util::Application::instance().getSubsystem<AudioCapture>())
{
}

void MainMenu::Draw()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Settings...", "Ctrl+s"))
            {
                _gui.ShowSettingsWindow();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Quit projectM", "Ctrl+q"))
            {
                _notificationCenter.postNotification(new Notification::Quit);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Playback"))
        {
            auto& app = ProjectMSDLApplication::instance();

            if (ImGui::MenuItem("Play Next Preset", "n"))
            {
                _notificationCenter.postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::NextPreset));
            }
            if (ImGui::MenuItem("Play Previous Preset", "p"))
            {
                _notificationCenter.postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::PreviousPreset));
            }
            if (ImGui::MenuItem("Go Back One Preset", "Backspace"))
            {
                _notificationCenter.postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::LastPreset));
            }
            if (ImGui::MenuItem("Random Preset", "r"))
            {
                _notificationCenter.postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::RandomPreset));
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Lock Preset", "Spacebar", app.config().getBool("projectM.presetLocked", false)))
            {
                _notificationCenter.postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::TogglePresetLocked));
            }
            if (ImGui::MenuItem("Enable Shuffle", "y", app.config().getBool("projectM.shuffleEnabled", true)))
            {
                _notificationCenter.postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::ToggleShuffle));
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Copy Current Preset Filename", "Ctrl+c"))
            {
                _projectMWrapper.PresetFileNameToClipboard();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Options"))
        {
            auto& app = ProjectMSDLApplication::instance();

            if (ImGui::BeginMenu("Audio Capture Device"))
            {
                auto devices = _audioCapture.AudioDeviceList();
                auto currentIndex = _audioCapture.AudioDeviceIndex();

                for (const auto& device : devices)
                {
                    if (ImGui::MenuItem(device.second.c_str(), "", device.first == currentIndex))
                    {
                        _audioCapture.AudioDeviceIndex(device.first);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Display Toast Messages", "", app.config().getBool("projectM.displayToasts", true)))
            {
                app.UserConfiguration()->setBool("projectM.displayToasts", !app.config().getBool("projectM.displayToasts", true));
            }
            if (ImGui::MenuItem("Display Preset Name in Window Title", "", app.config().getBool("window.displayPresetNameInTitle", true)))
            {
                app.UserConfiguration()->setBool("window.displayPresetNameInTitle", !app.config().getBool("window.displayPresetNameInTitle", true));
                _notificationCenter.postNotification(new Notification::UpdateWindowTitle);
            }

            ImGui::Separator();

            const bool beatAveraging = _projectMWrapper.BeatAveragingEnabled();
            if (ImGui::MenuItem("Beat Averaging", "", beatAveraging))
            {
                // The configuration change applies it (ProjectMWrapper::OnConfigurationPropertyChanged()).
                app.UserConfiguration()->setBool("projectM.beatAveraging", !beatAveraging);
            }

            // Not used while beat averaging is on. Type-in box with Apply button instead of a slider, see NumberInput.h.
            ImGui::BeginDisabled(beatAveraging);
            float beatSensitivity = _projectMWrapper.BeatSensitivity();
            if (NumberInput::Float("##beat_sensitivity", &beatSensitivity, 0.0f, 2.0f, "Beat Sensitivity"))
            {
                app.UserConfiguration()->setDouble("projectM.beatSensitivity", beatSensitivity);
            }

            // Per-band sensitivities multiply Beat Sensitivity, also not used while beat averaging is on.
            struct BandSetting {
                const char* id;
                const char* key;
                const char* label;
            };
            const BandSetting bands[]{{"##bass_sensitivity", "projectM.bassSensitivity", "Bass Sensitivity"},
                                      {"##mid_sensitivity", "projectM.midSensitivity", "Mid Sensitivity"},
                                      {"##treble_sensitivity", "projectM.trebleSensitivity", "Treble Sensitivity"}};
            for (const auto& band : bands)
            {
                float value = static_cast<float>(app.config().getDouble(band.key, 1.0));
                if (NumberInput::Float(band.id, &value, 0.0f, 2.0f, band.label))
                {
                    app.UserConfiguration()->setDouble(band.key, value);
                }
            }
            ImGui::EndDisabled();

            if (ImGui::BeginMenu("Frequency Bands"))
            {
                const std::pair<const char*, const char*> rangeChoices[]{
                    {ProjectMWrapper::BandRangesMusic, "Music (bass 20-250 Hz, mid 250-4000 Hz, treble 4000-20000 Hz)"},
                    {ProjectMWrapper::BandRangesStandard, "Milkdrop Standard (bass 0-3675 Hz, mid 3675-7350 Hz, treble 7350-11025 Hz)"},
                    {ProjectMWrapper::BandRangesCustom, "Custom (set in Settings > Audio)"}};
                auto current = app.config().getString("projectM.bandRanges", ProjectMWrapper::BandRangesMusic);
                for (const auto& choice : rangeChoices)
                {
                    if (ImGui::MenuItem(choice.second, "", current == choice.first))
                    {
                        app.UserConfiguration()->setString("projectM.bandRanges", choice.first);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::Separator();

            float ambientSpeed = _projectMWrapper.AmbientSpeed();
            if (NumberInput::Float("##ambient_speed", &ambientSpeed, ProjectMWrapper::MinAmbientSpeed, 1.0f, "Ambient Speed (Left/Right)"))
            {
                app.UserConfiguration()->setDouble("projectM.ambientSpeed", ambientSpeed);
            }

            const bool fpsIndependentMotion = app.config().getBool("projectM.fpsIndependentMotion", true);
            if (ImGui::MenuItem("Same Speed at Any FPS", "", fpsIndependentMotion))
            {
                app.UserConfiguration()->setBool("projectM.fpsIndependentMotion", !fpsIndependentMotion);
            }

            if (ImGui::BeginMenu("Transition Effects"))
            {
                // Keep the menu open, so several effects can be toggled in a row.
                ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
                auto current = app.config().getString("projectM.transitionEffects", "all");
                for (const auto& effect : ProjectMWrapper::TransitionEffectNames())
                {
                    bool enabled = _projectMWrapper.TransitionEffectEnabled(effect.first);
                    if (ImGui::MenuItem(effect.second.c_str(), "", enabled))
                    {
                        app.UserConfiguration()->setString("projectM.transitionEffects",
                                                           ProjectMWrapper::TransitionEffectsWith(current, effect.first, !enabled));
                        current = app.config().getString("projectM.transitionEffects", "all");
                    }
                }
                ImGui::PopItemFlag();
                ImGui::EndMenu();
            }

            ImGui::Separator();

            if (ImGui::BeginMenu("Mirror (M)"))
            {
                auto current = app.config().getString("effects.mirror", "off");
                for (const auto& mode : OutputEffects::MirrorModeNames())
                {
                    if (ImGui::MenuItem(mode.second.c_str(), "", current == mode.first))
                    {
                        app.UserConfiguration()->setString("effects.mirror", mode.first);
                    }
                }
                ImGui::EndMenu();
            }

            float zoom = OutputEffects::FromConfig().zoom;
            if (NumberInput::Float("##zoom", &zoom, OutputEffects::MinZoom, OutputEffects::MaxZoom, "Zoom (+/-)"))
            {
                app.UserConfiguration()->setDouble("effects.zoom", zoom);
            }

#ifdef _WIN32
            const bool spoutEnabled = app.config().getBool("output.spout.enabled", false);
            if (ImGui::MenuItem("Spout Output", "", spoutEnabled))
            {
                app.UserConfiguration()->setBool("output.spout.enabled", !spoutEnabled);
            }
#endif

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("Quick Help..."))
            {
                _gui.ShowHelpWindow();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("About projectM..."))
            {
                _gui.ShowAboutWindow();
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Visit the projectM Wiki on GitHub"))
            {
                SystemBrowser::OpenURL("https://github.com/projectM-visualizer/projectm/wiki");
            }
            if (ImGui::MenuItem("Report a Bug or Request a Feature"))
            {
                SystemBrowser::OpenURL("https://github.com/projectM-visualizer/projectm/issues/new/choose");
            }
            if (ImGui::MenuItem("Sponsor projectM on OpenCollective"))
            {
                SystemBrowser::OpenURL("https://opencollective.com/projectm");
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}
