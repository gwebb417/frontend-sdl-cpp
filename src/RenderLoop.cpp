#include "RenderLoop.h"

#include "FPSLimiter.h"
#include "FrameStats.h"
#include "GpuTimer.h"

#include "gui/ProjectMGUI.h"

#include <Poco/Format.h>
#include <Poco/JSON/Object.h>
#include <Poco/NumberParser.h>
#include <Poco/NotificationCenter.h>

#include <Poco/Util/Application.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <sstream>

#include "ProjectMSDLApplication.h"

RenderLoop::RenderLoop()
    : _audioCapture(Poco::Util::Application::instance().getSubsystem<AudioCapture>())
    , _projectMWrapper(Poco::Util::Application::instance().getSubsystem<ProjectMWrapper>())
    , _sdlRenderingWindow(Poco::Util::Application::instance().getSubsystem<SDLRenderingWindow>())
    , _projectMHandle(_projectMWrapper.ProjectM())
    , _projectMGui(Poco::Util::Application::instance().getSubsystem<ProjectMGUI>())
    , _userConfig(ProjectMSDLApplication::instance().UserConfiguration())
{
}

void RenderLoop::Run()
{
    FPSLimiter limiter;

    auto& notificationCenter{Poco::NotificationCenter::defaultCenter()};

    notificationCenter.addObserver(_quitNotificationObserver);

    _projectMWrapper.DisplayInitialPreset();

    if (!ProjectMSDLApplication::instance().UserDataWritable())
    {
        notificationCenter.postNotification(new Notification::DisplayToast(
            "Settings and favorites can't be saved: this folder is read-only. Move projectM to a folder you can write to."));
    }

    GpuTimer gpuTimer;
    gpuTimer.Initialize();

    _outputPipeline.Initialize();

    FrameStats stats;
    stats.limiter = &limiter;

    while (!_wantsToQuit)
    {
        limiter.TargetFPS(_projectMWrapper.TargetFPS());
        limiter.StartFrame();

        PollEvents();
        _remoteControl.Update([this] { return RemoteStatusJson(); },
                              [this](const RemoteControl::Command& command) { ExecuteRemoteCommand(command); });
        // Apply window size/position/monitor/fullscreen changes from the settings window once per frame.
        _sdlRenderingWindow.ApplyPendingConfigChanges();
        _audioCapture.FillBuffer();

        gpuTimer.Begin();
        RenderProjectM();
        gpuTimer.End();

        stats.gpuTimeMs = gpuTimer.LastResultMs();
        stats.expectedIntervalMs = ExpectedFrameIntervalMs(limiter);
        _projectMGui.Draw(stats);

        limiter.WorkDone();
        _sdlRenderingWindow.Swap();

        limiter.EndFrame();

        // Pass projectM the actual FPS value of the last frame.
        _projectMWrapper.UpdateRealFPS(limiter.FPS());

        TrackSlowFrames(limiter, stats);
    }

    gpuTimer.Destroy();
    _remoteControl.Stop();
    _spoutOutput.Release();
    _outputPipeline.Destroy();

    notificationCenter.removeObserver(_quitNotificationObserver);
}

float RenderLoop::ExpectedFrameIntervalMs(const FPSLimiter& limiter)
{
    // The refresh rate rarely changes, so only query it once per second.
    auto now = SDL_GetTicks64();
    if (_refreshRateCheckedTicks == 0 || now - _refreshRateCheckedTicks >= 1000)
    {
        _refreshRate = _sdlRenderingWindow.RefreshRate();
        _refreshRateCheckedTicks = now;
    }

    // Frames can't come faster than the refresh rate with vertical sync, nor faster than the FPS limit.
    float refreshIntervalMs = _refreshRate > 0 ? 1000.0f / static_cast<float>(_refreshRate) : 0.0f;
    return std::max(refreshIntervalMs, static_cast<float>(limiter.TargetFrameTimeMs()));
}

void RenderLoop::TrackSlowFrames(const FPSLimiter& limiter, const FrameStats& stats)
{
    auto now = SDL_GetTicks64();
    if (_slowFrameWindowStart == 0)
    {
        _slowFrameWindowStart = now;
    }

    float interval = limiter.LastFrameIntervalMs();
    _framesInWindow++;
    if (stats.expectedIntervalMs > 0.0f && interval > stats.expectedIntervalMs * 1.5f)
    {
        _slowFramesInWindow++;
        _longestSlowFrameMs = std::max(_longestSlowFrameMs, interval);
    }
    _maxGpuTimeInWindow = std::max(_maxGpuTimeInWindow, stats.gpuTimeMs);
    _maxCpuTimeInWindow = std::max(_maxCpuTimeInWindow, limiter.LastWorkTimeMs());

    if (now - _slowFrameWindowStart < 10000)
    {
        return;
    }

    if (_slowFramesInWindow > 0)
    {
        std::string gpuText = _maxGpuTimeInWindow >= 0.0f ? Poco::format("%.1f ms", static_cast<double>(_maxGpuTimeInWindow)) : std::string("n/a");
        std::string message = Poco::format(
            "Slow frames: %u of %u in the last 10 s took longer than %.1f ms (longest %.1f ms). Highest render times: GPU %s, CPU %.1f ms.",
            _slowFramesInWindow, _framesInWindow,
            static_cast<double>(stats.expectedIntervalMs * 1.5f), static_cast<double>(_longestSlowFrameMs),
            gpuText, static_cast<double>(_maxCpuTimeInWindow));

        // A single slow frame happens on most preset switches (shader compilation), so only
        // repeated slow frames are logged at the default log level.
        if (_slowFramesInWindow >= 3)
        {
            poco_information(_logger, message);
        }
        else
        {
            poco_debug(_logger, message);
        }
    }

    _slowFrameWindowStart = now;
    _framesInWindow = 0;
    _slowFramesInWindow = 0;
    _longestSlowFrameMs = 0.0f;
    _maxGpuTimeInWindow = -1.0f;
    _maxCpuTimeInWindow = 0.0f;
}

void RenderLoop::PollEvents()
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        _projectMGui.ProcessInput(event);

        switch (event.type)
        {
            case SDL_MOUSEWHEEL:

                if (!_projectMGui.WantsMouseInput())
                {
                    ScrollEvent(event.wheel);
                }

                break;

            case SDL_KEYDOWN:
                if (!_projectMGui.WantsKeyboardInput())
                {
                    KeyEvent(event.key, true);
                }
                break;

            case SDL_KEYUP:
                if (!_projectMGui.WantsKeyboardInput())
                {
                    KeyEvent(event.key, false);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (!_projectMGui.WantsMouseInput())
                {
                    MouseDownEvent(event.button);
                }

                break;

            case SDL_MOUSEBUTTONUP:
                if (!_projectMGui.WantsMouseInput())
                {
                    MouseUpEvent(event.button);
                }

                break;

            case SDL_DROPFILE: {
                HandleDropEvent(event);
                break;
            }


            case SDL_QUIT:
                _wantsToQuit = true;
                break;
        }
    }
}

void RenderLoop::CheckViewportSize(int fixedWidth, int fixedHeight)
{
    int windowWidth;
    int windowHeight;
    _sdlRenderingWindow.GetDrawableSize(windowWidth, windowHeight);

    if (windowWidth != _windowWidth || windowHeight != _windowHeight)
    {
        _windowWidth = windowWidth;
        _windowHeight = windowHeight;
        _projectMGui.UpdateFontSize();
    }

    int renderWidth = windowWidth;
    int renderHeight = windowHeight;
    if (fixedWidth > 0 && fixedHeight > 0)
    {
        renderWidth = fixedWidth;
        renderHeight = fixedHeight;
    }
    else if (renderWidth <= 0 || renderHeight <= 0)
    {
        // Minimized: keep the previous size, so the Spout output keeps running.
        return;
    }

    if (renderWidth != _renderWidth || renderHeight != _renderHeight)
    {
        projectm_set_window_size(_projectMHandle, renderWidth, renderHeight);
        _renderWidth = renderWidth;
        _renderHeight = renderHeight;

        poco_debug_f2(_logger, "Resized rendering canvas to %?dx%?d.", renderWidth, renderHeight);
    }
}

void RenderLoop::CycleMirrorMode()
{
    // Stored like a change in the settings window, so both always show the same value.
    const auto& modes = OutputEffects::MirrorModeNames();
    auto next = (static_cast<size_t>(OutputEffects::FromConfig().mirror) + 1) % modes.size();
    ProjectMSDLApplication::instance().UserConfiguration()->setString("effects.mirror", modes[next].first);
    Poco::NotificationCenter::defaultCenter().postNotification(
        new Notification::DisplayToast("Mirror: " + modes[next].second));
}

void RenderLoop::ChangeZoom(float value)
{
    float zoom = std::clamp(std::round((OutputEffects::FromConfig().zoom + value) * 10.0f) / 10.0f,
                            OutputEffects::MinZoom, OutputEffects::MaxZoom);
    ProjectMSDLApplication::instance().UserConfiguration()->setDouble("effects.zoom", zoom);
    Poco::NotificationCenter::defaultCenter().postNotification(
        new Notification::DisplayToast(zoom <= OutputEffects::MinZoom ? std::string("Zoom: Off") : Poco::format("Zoom: %.1hfx", zoom)));
}

void RenderLoop::ExecuteRemoteCommand(const RemoteControl::Command& command)
{
    using Action = Notification::PlaybackControl::Action;
    auto& notificationCenter = Poco::NotificationCenter::defaultCenter();
    auto userConfig = ProjectMSDLApplication::instance().UserConfiguration();
    const auto& action = command.action;

    auto toast = [&notificationCenter](const std::string& message) {
        notificationCenter.postNotification(new Notification::DisplayToast(message));
    };
    auto number = [&command](double& value) {
        return Poco::NumberParser::tryParseFloat(command.value, value) && std::isfinite(value);
    };

    if (action == "next")
    {
        notificationCenter.postNotification(new Notification::PlaybackControl(Action::NextPreset, command.smooth));
    }
    else if (action == "previous")
    {
        notificationCenter.postNotification(new Notification::PlaybackControl(Action::PreviousPreset, command.smooth));
    }
    else if (action == "random")
    {
        notificationCenter.postNotification(new Notification::PlaybackControl(Action::RandomPreset, command.smooth));
    }
    else if (action == "back")
    {
        notificationCenter.postNotification(new Notification::PlaybackControl(Action::LastPreset, command.smooth));
    }
    else if (action == "lock")
    {
        notificationCenter.postNotification(new Notification::PlaybackControl(Action::TogglePresetLocked));
    }
    else if (action == "shuffle")
    {
        notificationCenter.postNotification(new Notification::PlaybackControl(Action::ToggleShuffle));
    }
    else if (action == "favorite-save")
    {
        _projectMWrapper.SaveCurrentPresetToFavorites();
    }
    else if (action == "favorite-remove")
    {
        _projectMWrapper.RemoveCurrentPresetFromFavorites();
    }
    else if (action == "disable")
    {
        // Only while all presets play: in favorites mode the current file is the favorite's copy,
        // and in disabled-only mode it's disabled already.
        if (_projectMWrapper.PlaylistModeName() == "all")
        {
            _projectMWrapper.DisableCurrentPreset();
        }
        else
        {
            toast("Disable works while all presets play");
        }
    }
    else if (action == "reenable")
    {
        if (_projectMWrapper.PlaylistModeName() == "disabled")
        {
            _projectMWrapper.ReenableCurrentPreset();
        }
        else
        {
            toast("Re-enable works while disabled presets play");
        }
    }
    else if (action == "undo")
    {
        _projectMWrapper.UndoFavoriteChange();
    }
    else if (action == "mode-favorites")
    {
        _projectMWrapper.ToggleFavoritesOnly();
    }
    else if (action == "mode-disabled")
    {
        _projectMWrapper.ToggleDisabledOnly();
    }
    else if (action == "beat-averaging")
    {
        bool on = command.value == "1" || command.value == "true";
        userConfig->setBool("projectM.beatAveraging", on);
        toast(on ? "Beat Averaging: On" : "Beat Averaging: Off");
    }
    else if (action == "beat-sensitivity")
    {
        double value{};
        if (number(value) && !_projectMWrapper.BeatAveragingEnabled())
        {
            value = std::clamp(std::round(value * 100.0) / 100.0, 0.0, 2.0);
            userConfig->setDouble("projectM.beatSensitivity", value);
            toast(Poco::format("Beat Sensitivity: %.2f", value));
        }
    }
    else if (action == "ambient-speed")
    {
        double value{};
        if (number(value))
        {
            value = std::clamp(std::round(value * 100.0) / 100.0, static_cast<double>(ProjectMWrapper::MinAmbientSpeed), 1.0);
            userConfig->setDouble("projectM.ambientSpeed", value);
            toast(Poco::format("Ambient Speed: %.2f", value));
        }
    }
    else if (action == "mirror")
    {
        for (const auto& mode : OutputEffects::MirrorModeNames())
        {
            if (mode.first == command.value)
            {
                userConfig->setString("effects.mirror", mode.first);
                toast("Mirror: " + mode.second);
            }
        }
    }
    else if (action == "zoom")
    {
        double value{};
        if (number(value))
        {
            value = std::clamp(std::round(value * 10.0) / 10.0, static_cast<double>(OutputEffects::MinZoom), static_cast<double>(OutputEffects::MaxZoom));
            userConfig->setDouble("effects.zoom", value);
            toast(value <= OutputEffects::MinZoom ? std::string("Zoom: Off") : Poco::format("Zoom: %.1fx", value));
        }
    }
    else
    {
        poco_debug_f1(_logger, "Unknown remote control command: %s", action);
    }
}

std::string RenderLoop::RemoteStatusJson() const
{
    auto& config = Poco::Util::Application::instance().config();
    auto effects = OutputEffects::FromConfig();

    Poco::JSON::Object status;
    status.set("preset", _projectMWrapper.CurrentPresetName());
    status.set("mode", _projectMWrapper.PlaylistModeName());
    status.set("locked", _projectMWrapper.PresetLocked());
    status.set("favorite", _projectMWrapper.CurrentPresetIsFavorite());
    status.set("shuffle", config.getBool("projectM.shuffleEnabled", true));
    status.set("beatAveraging", _projectMWrapper.BeatAveragingEnabled());
    auto rounded = [](float value) { return std::round(static_cast<double>(value) * 100.0) / 100.0; };
    status.set("beatSensitivity", rounded(_projectMWrapper.BeatSensitivity()));
    status.set("ambientSpeed", rounded(_projectMWrapper.AmbientSpeed()));
    status.set("mirror", OutputEffects::MirrorModeNames()[static_cast<size_t>(effects.mirror)].first);
    status.set("zoom", rounded(effects.zoom));

    std::ostringstream json;
    status.stringify(json);
    return json.str();
}

void RenderLoop::RenderProjectM()
{
    auto& config = Poco::Util::Application::instance().config();

    auto effects = OutputEffects::FromConfig();
    bool spout = _outputPipeline.Available() && SpoutOutput::Available() && config.getBool("output.spout.enabled", false);

    int fixedWidth = 0;
    int fixedHeight = 0;
    if (spout)
    {
        fixedWidth = std::clamp(config.getInt("output.spout.width", 0), 0, 7680);
        fixedHeight = std::clamp(config.getInt("output.spout.height", 0), 0, 4320);
    }
    CheckViewportSize(fixedWidth, fixedHeight);

    if (!spout)
    {
        _spoutOutput.Release();
    }

    // Without effects and Spout, projectM renders straight into the window, exactly as before.
    unsigned int framebuffer = 0;
    if (_outputPipeline.Available() && (spout || effects.Active()))
    {
        framebuffer = _outputPipeline.BeginFrame(_renderWidth, _renderHeight);
    }

    _projectMWrapper.RenderFrame(framebuffer);

    if (framebuffer != 0)
    {
        _outputPipeline.EndFrame(_windowWidth, _windowHeight, effects,
                                 spout ? &_spoutOutput : nullptr,
                                 config.getString("output.spout.name", "projectM"));
    }
}

void RenderLoop::KeyEvent(const SDL_KeyboardEvent& event, bool down)
{
    auto keyModifier{static_cast<SDL_Keymod>(event.keysym.mod)};
    auto keyCode{event.keysym.sym};
    bool modifierPressed{false};

    if (keyModifier & KMOD_LGUI || keyModifier & KMOD_RGUI || keyModifier & KMOD_LCTRL)
    {
        modifierPressed = true;
    }

    // Handle modifier keys and save state for use in other methods, e.g. mouse events
    switch (keyCode)
    {
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            _keyStates._ctrlPressed = down;
            break;

        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
            _keyStates._shiftPressed = down;
            break;

        case SDLK_LALT:
        case SDLK_RALT:
            _keyStates._altPressed = down;
            break;

        case SDLK_LGUI:
        case SDLK_RGUI:
            _keyStates._metaPressed = down;
            break;

        default:
            break;
    }

    if (!down)
    {
        return;
    }

    switch (keyCode)
    {
        case SDLK_ESCAPE:
            if (!modifierPressed)
            {
                _projectMGui.Toggle();
                _sdlRenderingWindow.ShowCursor(_projectMGui.Visible());
            }
            break;

        case SDLK_a: {
            if (!modifierPressed)
            {
                bool aspectCorrectionEnabled = !projectm_get_aspect_correction(_projectMHandle);
                projectm_set_aspect_correction(_projectMHandle, aspectCorrectionEnabled);
                Poco::NotificationCenter::defaultCenter().postNotification(
                    new Notification::DisplayToast(aspectCorrectionEnabled ? "Aspect correction: On" : "Aspect correction: Off"));
            }
            break;
        }

        case SDLK_c:
            if (modifierPressed)
            {
                _projectMWrapper.PresetFileNameToClipboard();
                Poco::NotificationCenter::defaultCenter().postNotification(
                    new Notification::DisplayToast("Preset path copied to clipboard"));
            }
            break;

#ifdef _DEBUG
        case SDLK_d:
            if (!modifierPressed)
            {
                // Write next rendered frame to file
                projectm_write_debug_image_on_next_frame(_projectMHandle, nullptr);
            }
            break;
#endif

        case SDLK_f:
            if (modifierPressed)
            {
                _sdlRenderingWindow.ToggleFullscreen();
            }
            else
            {
                _projectMWrapper.ToggleFavoritesOnly();
            }
            break;

        case SDLK_s:
            // Either Ctrl key (modifierPressed only covers the left one), or Cmd on macOS.
            if (modifierPressed || (keyModifier & KMOD_CTRL))
            {
                // The File menu always listed Ctrl+s for Settings, but it was never wired up.
                // The settings window is only drawn while the UI is visible, so show that too.
                _projectMGui.Visible(true);
                _sdlRenderingWindow.ShowCursor(true);
                _projectMGui.ShowSettingsWindow();
            }
            else
            {
                _projectMWrapper.SaveCurrentPresetToFavorites();
            }
            break;

        case SDLK_i:
            if (modifierPressed)
            {
                _audioCapture.NextAudioDevice();
            }
            break;

        case SDLK_m:
            if (modifierPressed)
            {
                _sdlRenderingWindow.NextDisplay();
            }
            else
            {
                CycleMirrorMode();
            }
            break;

        case SDLK_EQUALS: // + without Shift on most layouts
        case SDLK_PLUS:
        case SDLK_KP_PLUS:
            if (!modifierPressed)
            {
                ChangeZoom(0.1f);
            }
            break;

        case SDLK_MINUS:
        case SDLK_KP_MINUS:
            if (!modifierPressed)
            {
                ChangeZoom(-0.1f);
            }
            break;

        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            // Plain Enter only, so Alt+Enter etc. stay free. Ignore key repeat while held.
            if (!_keyStates.AnyPressed() && !event.repeat)
            {
                _projectMGui.ShowPresetChooser();
            }
            break;

        case SDLK_n:
            if (!modifierPressed)
            {
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::NextPreset, _keyStates._shiftPressed));
            }
            break;

        case SDLK_p:
            if (!modifierPressed)
            {
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::PreviousPreset, _keyStates._shiftPressed));
            }
            break;

        case SDLK_r: {
            if (!modifierPressed)
            {
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::RandomPreset, _keyStates._shiftPressed));
            }
            break;
        }

        case SDLK_q:
            if (modifierPressed)
            {
                _wantsToQuit = true;
            }
            break;

        case SDLK_y:
            if (!modifierPressed)
            {
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::ToggleShuffle));
            }
            break;
        case SDLK_DELETE:
            // Either Ctrl key (modifierPressed only covers the left one).
            if (keyModifier & KMOD_CTRL)
            {
                _projectMWrapper.ToggleDisabledOnly();
            }
            else
            {
                _projectMWrapper.DeleteKeyPressed();
            }
            break;

        case SDLK_z:
            if (modifierPressed)
            {
                _projectMWrapper.UndoFavoriteChange();
            }
            break;
        case SDLK_BACKSPACE:
            if (!modifierPressed)
            {
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::LastPreset, _keyStates._shiftPressed));
            }
            break;

        case SDLK_SPACE:
            if (!modifierPressed)
            {
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::PlaybackControl(Notification::PlaybackControl::Action::TogglePresetLocked));
            }
            break;

        case SDLK_UP:
            if (!modifierPressed)
            {
                // Increase beat sensitivity
                _projectMWrapper.ChangeBeatSensitivity(0.01f);
            }
            break;

        case SDLK_DOWN:
            if (!modifierPressed)
            {
                // Decrease beat sensitivity
                _projectMWrapper.ChangeBeatSensitivity(-0.01f);
            }
            break;

        case SDLK_RIGHT:
            if (!modifierPressed)
            {
                _projectMWrapper.ChangeAmbientSpeed(0.05f);
            }
            break;

        case SDLK_LEFT:
            if (!modifierPressed)
            {
                _projectMWrapper.ChangeAmbientSpeed(-0.05f);
            }
            break;
    }
}

void RenderLoop::ScrollEvent(const SDL_MouseWheelEvent& event) const
{
    // Wheel up is positive
    if (event.y > 0)
    {
        _projectMWrapper.Playlist().Next(true);
    }
    // Wheel down is negative
    else if (event.y < 0)
    {
        _projectMWrapper.Playlist().Previous(true);
    }
}

void RenderLoop::MouseDownEvent(const SDL_MouseButtonEvent& event)
{
    if (_projectMGui.WantsMouseInput())
    {
        return;
    }

    switch (event.button)
    {
        case SDL_BUTTON_LEFT:
            if (!_mouseDown && _keyStates._shiftPressed)
            {
                // ToDo: Improve this to differentiate between single click (add waveform) and drag (move waveform).
                int x;
                int y;
                int width;
                int height;

                _sdlRenderingWindow.GetDrawableSize(width, height);

                SDL_GetMouseState(&x, &y);

                // Scale those coordinates. libProjectM uses a scale of 0..1 instead of absolute pixel coordinates.
                float scaledX = (static_cast<float>(x) / static_cast<float>(width));
                float scaledY = (static_cast<float>(height - y) / static_cast<float>(height));

                // Add a new waveform.
                projectm_touch(_projectMHandle, scaledX, scaledY, 0, PROJECTM_TOUCH_TYPE_RANDOM);
                poco_debug_f2(_logger, "Added new random waveform at %?d,%?d", x, y);

                _mouseDown = true;
            }
            break;

        case SDL_BUTTON_RIGHT:
            if (!_keyStates.AnyPressed())
            {
                _sdlRenderingWindow.ToggleFullscreen();
            }
            break;

        case SDL_BUTTON_MIDDLE:
            projectm_touch_destroy_all(_projectMHandle);
            poco_debug(_logger, "Cleared all custom waveforms.");
            break;

        default:;
    }
}

void RenderLoop::MouseUpEvent(const SDL_MouseButtonEvent& event)
{
    if (event.button == SDL_BUTTON_LEFT)
    {
        _mouseDown = false;
    }
}

void RenderLoop::QuitNotificationHandler(const Poco::AutoPtr<Notification::Quit>& notification)
{
    _wantsToQuit = true;
}

void RenderLoop::HandleDropEvent(const SDL_Event& event)
{
    if (event.drop.file == nullptr)
    {
        return;
    }

    std::string droppedFilePath = event.drop.file;
    SDL_free(event.drop.file);

    Poco::File droppedFile(droppedFilePath);
    if (!droppedFile.exists() && !droppedFile.isFile() && !droppedFile.isDirectory())
    {
        const std::string toastMessage = std::string("Invalid file or directory dropped: ") + droppedFilePath;
        Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(toastMessage));
        poco_information(_logger, toastMessage);
        return;
    }

    // first we want to get the config settings that are relevant here
    // namely skipToDropped and droppedFolderOverride
    // we can get them from the projectMWrapper, in the _projectMConfigView available on it
    const bool skipToDropped = _userConfig->getBool("projectM.skipToDropped", true);
    const bool droppedFolderOverride = _userConfig->getBool("projectM.droppedFolderOverride", false);

    const bool shuffle = _projectMWrapper.Playlist().ShuffleEnabled();
    if (shuffle && skipToDropped)
    {
        // if shuffle is enabled, we disable it temporarily, so the dropped preset is played next
        // if skipToDropped is false, we also keep shuffle enabled, as it doesn't matter since the current preset is unaffected
        _projectMWrapper.Playlist().ShuffleEnabled(false);
    }

    auto insertIndex = _projectMWrapper.Playlist().CurrentIndex() + 1;

    do
    {
        Poco::File droppedFile(droppedFilePath);
        if (!droppedFile.isDirectory())
        {
            // handle dropped preset file
            const Poco::Path droppedFileP(droppedFilePath);
            if (droppedFileP.getExtension() != "milk" && droppedFileP.getExtension() != "prjm")
            {
                const std::string toastMessage = std::string("Invalid file extension: ") + droppedFilePath;
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(toastMessage));
                poco_information(_logger, toastMessage);
                break; // exit the block and go to the shuffle check
            }

            if (_projectMWrapper.Playlist().InsertItem(PresetPlaylist::Item(droppedFileP.toString()), insertIndex, true))
            {
                if (skipToDropped)
                {
                    _projectMWrapper.Playlist().Next(true);
                }
                poco_information_f1(_logger, "Added preset: %s", std::string(droppedFilePath));
                // no need to toast single presets, as its obvious if a preset was loaded.
            }
        }
        else
        {
            // handle dropped directory
            _projectMWrapper.Playlist().BeginBatchEdit();

            // if droppedFolderOverride is enabled, we clear the playlist first
            // current edge case: if the dropped directory is invalid or contains no presets, then it still clears the playlist
            if (droppedFolderOverride)
            {
                _projectMWrapper.Playlist().Clear();
                insertIndex = 0;
            }

            uint32_t addedFilesCount = _projectMWrapper.Playlist().InsertPath(droppedFilePath, insertIndex, true, true);
            if (addedFilesCount > 0)
            {
                std::string toastMessage = "Added " + std::to_string(addedFilesCount) + " presets from " + droppedFilePath;
                poco_information_f1(_logger, "%s", toastMessage);
                if (skipToDropped || droppedFolderOverride)
                {
                    // if skip to dropped is true, or if a folder was dropped and it overrode the playlist, we skip to the next preset
                    _projectMWrapper.Playlist().Next(true);
                }
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(toastMessage));
            }
            else
            {
                const std::string toastMessage = std::string("No presets found in directory: ") + droppedFilePath;
                Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(toastMessage));
                poco_information_f1(_logger, "%s", toastMessage);
            }

            _projectMWrapper.Playlist().EndBatchEdit();
        }
    } while (false);

    if (shuffle && skipToDropped)
    {
        _projectMWrapper.Playlist().ShuffleEnabled(true);
    }
}