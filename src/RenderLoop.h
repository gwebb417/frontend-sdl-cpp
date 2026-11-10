#pragma once

#include "AudioCapture.h"
#include "OutputPipeline.h"
#include "ProjectMWrapper.h"
#include "RemoteControl.h"
#include "SDLRenderingWindow.h"
#include "SpoutOutput.h"

#include "notifications/Quit.h"

#include <Poco/Logger.h>
#include <Poco/NObserver.h>

class FPSLimiter;
class ProjectMGUI;
struct FrameStats;

class RenderLoop
{
public:
    RenderLoop();

    void Run();

protected:
    struct ModifierKeyStates {
        bool AnyPressed() const
        {
            return _shiftPressed || _ctrlPressed || _altPressed || _metaPressed;
        }

        bool _shiftPressed{false}; //!< L/R shift keys
        bool _ctrlPressed{false}; //!< L/R control keys
        bool _altPressed{false}; //!< L/R alt keys
        bool _metaPressed{false}; //!< Logo/meta/command key
    };

    /**
     * @brief Polls all SDL events in the queue and takes action if required.
     */
    void PollEvents();

    /**
     * @brief Checks if the GL viewport size has changed and if so, reconfigured projectM accordingly.
     */
    /**
     * @brief Updates the window size and the size projectM renders at.
     * @param fixedWidth Fixed render width (Spout resolution), 0 = window size.
     * @param fixedHeight Fixed render height, 0 = window size.
     */
    void CheckViewportSize(int fixedWidth, int fixedHeight);

    /**
     * @brief Renders projectM, directly into the window or through the output pipeline (effects, Spout).
     */
    void RenderProjectM();

    /**
     * @brief Switches to the next mirror mode (M key) and shows it in a toast.
     */
    void CycleMirrorMode();

    /**
     * @brief Changes the zoom effect by the given value (+/- keys) and shows it in a toast.
     * @param value A positive or negative delta value.
     */
    void ChangeZoom(float value);

    /**
     * @brief Runs a command from the remote control (main thread).
     */
    void ExecuteRemoteCommand(const RemoteControl::Command& command);

    /**
     * @brief Returns the status shown on the remote control page, as a JSON object.
     */
    std::string RemoteStatusJson() const;

    /**
     * @brief Handles SDL key press events.
     * @param event The key event.
     */
    void KeyEvent(const SDL_KeyboardEvent& event, bool down);

    /**
     * @brief Handles SDL mouse wheel events.
     * @param event The mouse wheel event
     */
    void ScrollEvent(const SDL_MouseWheelEvent& event) const;

    /**
     * @brief Handles SDL mouse button down events.
     * @param event The mouse button event
     */
    void MouseDownEvent(const SDL_MouseButtonEvent& event);

    /**
     * @brief Handles SDL mouse button up events.
     * @param event The mouse button event
     */
    void MouseUpEvent(const SDL_MouseButtonEvent& event);

    /**
     * @brief Handler for quit notifications.
     * @param notification The received notification.
     */
    void QuitNotificationHandler(const Poco::AutoPtr<Notification::Quit>& notification);

    /**
     * Handles a file/dir drop event into the window.
     * @param event The drop event from SDL
     */
    void HandleDropEvent(const SDL_Event& event);

    /**
     * @brief Returns the frame interval the current settings should give (refresh rate or FPS limit).
     */
    float ExpectedFrameIntervalMs(const FPSLimiter& limiter);

    /**
     * @brief Counts frames that took much longer than expected and logs a summary every 10 seconds.
     */
    void TrackSlowFrames(const FPSLimiter& limiter, const FrameStats& stats);

    int _refreshRate{0}; //!< Refresh rate of the window's display in Hz, 0 if unknown.
    uint64_t _refreshRateCheckedTicks{0}; //!< When the refresh rate was last queried.

    uint64_t _slowFrameWindowStart{0}; //!< Start of the current 10-second slow-frame logging window.
    unsigned int _framesInWindow{0};
    unsigned int _slowFramesInWindow{0};
    float _longestSlowFrameMs{0.0f};
    float _maxGpuTimeInWindow{-1.0f};
    float _maxCpuTimeInWindow{0.0f};

    AudioCapture& _audioCapture;
    ProjectMWrapper& _projectMWrapper;
    SDLRenderingWindow& _sdlRenderingWindow;

    projectm_handle _projectMHandle{nullptr};

    ProjectMGUI& _projectMGui;

    Poco::NObserver<RenderLoop, Notification::Quit> _quitNotificationObserver{*this, &RenderLoop::QuitNotificationHandler}; //!< The observer for quit notifications.

    bool _wantsToQuit{false};

    bool _mouseDown{false}; //!< Left mouse button is pressed

    int _renderWidth{0};  //!< Size projectM renders at: the window size, or the fixed Spout resolution.
    int _renderHeight{0};
    int _windowWidth{0};  //!< Drawable size of the window.
    int _windowHeight{0};

    OutputPipeline _outputPipeline; //!< Offscreen rendering for mirror/zoom and Spout output.
    SpoutOutput _spoutOutput;       //!< Spout2 sender (Windows).
    RemoteControl _remoteControl;   //!< HTTP remote control with a phone page.

    ModifierKeyStates _keyStates; //!< Current "pressed" states of modifier keys

    Poco::AutoPtr<Poco::Util::AbstractConfiguration> _userConfig; //!< View of the "projectM" configuration subkey in the "user" configuration.

    Poco::Logger& _logger{Poco::Logger::get("RenderLoop")}; //!< The class logger.
};
