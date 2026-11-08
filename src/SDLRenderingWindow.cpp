#include "SDLRenderingWindow.h"

#include "ProjectMSDLApplication.h"
#include "ProjectMWrapper.h"

#include <Poco/Delegate.h>
#include <Poco/NotificationCenter.h>

#include <Poco/Util/Application.h>

#ifdef USE_GLEW
#include <GL/glew.h>
#endif

#include <SDL2/SDL_opengl.h>

#include <algorithm>

#if defined(__linux__) && !defined(__ANDROID__)
#include <dlfcn.h>

namespace {

// Minimal copy of Xlib's XErrorEvent, so the app doesn't need the X11 headers or link to libX11.
struct XErrorEventFields
{
    int type;
    void* display;
    unsigned long resourceId;
    unsigned long serial;
    unsigned char errorCode;
    unsigned char requestCode;
    unsigned char minorCode;
};

using XErrorHandlerFunc = int (*)(void*, XErrorEventFields*);
using XSetErrorHandlerFunc = XErrorHandlerFunc (*)(XErrorHandlerFunc);

int NonFatalXErrorHandler(void* /* display */, XErrorEventFields* event)
{
    poco_warning_f3(Poco::Logger::get("SDLRenderingWindow"),
                    "Ignored X11 error %?d (request %?d.%?d).",
                    static_cast<int>(event->errorCode), static_cast<int>(event->requestCode), static_cast<int>(event->minorCode));
    return 0;
}

/**
 * Xlib's default error handler ends the whole process on any X11 error. SDL2 can trigger harmless ones,
 * e.g. a BadMatch from XSetInputFocus when it re-focuses the window while the window manager is still
 * switching it in or out of fullscreen (seen on GNOME/XWayland). Log those errors instead of crashing.
 * SDL loads libX11 dynamically, so the handler is set in the same, already loaded library.
 */
void InstallNonFatalXErrorHandler()
{
    const char* driver = SDL_GetCurrentVideoDriver();
    if (driver == nullptr || std::string(driver) != "x11")
    {
        return;
    }

    void* libX11 = dlopen("libX11.so.6", RTLD_NOW | RTLD_NOLOAD);
    if (libX11 == nullptr)
    {
        return;
    }

    auto setErrorHandler = reinterpret_cast<XSetErrorHandlerFunc>(dlsym(libX11, "XSetErrorHandler"));
    if (setErrorHandler != nullptr)
    {
        setErrorHandler(&NonFatalXErrorHandler);
    }

    dlclose(libX11); // Only drops the reference taken above, SDL keeps the library loaded.
}

} // namespace
#endif

const char* SDLRenderingWindow::name() const
{
    return "SDL2 Rendering Window";
}

void SDLRenderingWindow::initialize(Poco::Util::Application& app)
{
    auto& projectMSDLApp = dynamic_cast<ProjectMSDLApplication&>(app);
    _userConfig = projectMSDLApp.UserConfiguration();
    _config = app.config().createView("window");

    if (!_renderingWindow)
    {
        CreateSDLWindow();
    }

    Poco::NotificationCenter::defaultCenter().addObserver(_updateWindowTitleObserver);

    // Observe user configuration changes (set via the settings window)
    _userConfig->propertyChanged += Poco::delegate(this, &SDLRenderingWindow::OnConfigurationPropertyChanged);
    _userConfig->propertyRemoved += Poco::delegate(this, &SDLRenderingWindow::OnConfigurationPropertyRemoved);
}

void SDLRenderingWindow::uninitialize()
{
    _userConfig->propertyRemoved -= Poco::delegate(this, &SDLRenderingWindow::OnConfigurationPropertyRemoved);
    _userConfig->propertyChanged -= Poco::delegate(this, &SDLRenderingWindow::OnConfigurationPropertyChanged);
    Poco::NotificationCenter::defaultCenter().removeObserver(_updateWindowTitleObserver);

    if (_renderingWindow)
    {
        DestroySDLWindow();
        _renderingWindow = nullptr;
    }
}

void SDLRenderingWindow::GetDrawableSize(int& width, int& height) const
{
    SDL_GL_GetDrawableSize(_renderingWindow, &width, &height);
}

void SDLRenderingWindow::Swap() const
{
    SDL_GL_SwapWindow(_renderingWindow);
}

void SDLRenderingWindow::ToggleFullscreen()
{
    if (_fullscreen)
    {
        Windowed();
    }
    else
    {
        Fullscreen();
    }
}

void SDLRenderingWindow::Fullscreen()
{
    if (!_fullscreen)
    {
        // Only remember the windowed size, not a previous fullscreen size.
        SDL_GetWindowSize(_renderingWindow, &_lastWindowWidth, &_lastWindowHeight);
    }
    SDL_ShowCursor(false);
    if (_config->getBool("fullscreen.exclusiveMode", false))
    {
        int fullscreenWidth = _config->getInt("fullscreen.width", 0);
        int fullscreenHeight = _config->getInt("fullscreen.height", 0);
        if (fullscreenWidth > 0 && fullscreenHeight > 0)
        {
            SDL_RestoreWindow(_renderingWindow);
            SDL_SetWindowSize(_renderingWindow, fullscreenWidth, fullscreenHeight);
        }
        SDL_SetWindowFullscreen(_renderingWindow, SDL_WINDOW_FULLSCREEN);
        if (_logger.debug())
        {
            int width{0};
            int height{0};
            SDL_GetWindowSize(_renderingWindow, &width, &height);
            poco_debug_f2(_logger, "Entered exclusive fullscreen mode with actual resolution %dx%d",
                          width, height);
        }
    }
    else
    {
        SDL_SetWindowFullscreen(_renderingWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (_logger.debug())
        {
            int width{0};
            int height{0};
            SDL_GetWindowSize(_renderingWindow, &width, &height);
            poco_debug_f2(_logger, "Entered fake fullscreen mode with resolution %dx%d",
                          width, height);
        }
    }

    _fullscreen = true;
}

void SDLRenderingWindow::Windowed()
{
    SDL_SetWindowFullscreen(_renderingWindow, 0);
    SDL_SetWindowBordered(_renderingWindow, _config->getBool("borderless", false) ? SDL_FALSE : SDL_TRUE);
    if (_lastWindowWidth > 0 && _lastWindowHeight > 0)
    {
        SDL_SetWindowSize(_renderingWindow, _lastWindowWidth, _lastWindowHeight);
        SDL_ShowCursor(true);

        poco_debug_f2(_logger, "Entered windowed mode with size %dx%d",
                      _lastWindowWidth, _lastWindowHeight);
    }

    _fullscreen = false;

    // Position changes made while in fullscreen are applied when returning to windowed mode.
    if (_pendingPosition)
    {
        _pendingPosition = false;
        ApplyWindowPosition();
    }

    KeepBorderlessWindowOnDisplay();
}

void SDLRenderingWindow::KeepBorderlessWindowOnDisplay()
{
    if (_fullscreen || !_config->getBool("borderless", false))
    {
        return;
    }

    // The monitor the window's center is on.
    const int displayIndex = SDL_GetWindowDisplayIndex(_renderingWindow);
    SDL_Rect bounds;
    if (displayIndex < 0 || SDL_GetDisplayBounds(displayIndex, &bounds) != 0)
    {
        return;
    }

    int left{0};
    int top{0};
    int width{0};
    int height{0};
    SDL_GetWindowPosition(_renderingWindow, &left, &top);
    SDL_GetWindowSize(_renderingWindow, &width, &height);

    // Keep the right/bottom edge on the monitor first, then the left/top edge, so a window
    // larger than the monitor is aligned to the monitor's top-left corner.
    int newLeft = std::max(std::min(left, bounds.x + bounds.w - width), bounds.x);
    int newTop = std::max(std::min(top, bounds.y + bounds.h - height), bounds.y);

    if (newLeft != left || newTop != top)
    {
        SDL_SetWindowPosition(_renderingWindow, newLeft, newTop);
        poco_information_f4(_logger, "Moved borderless window from X=%?d Y=%?d to X=%?d Y=%?d to keep it on its monitor.",
                            left, top, newLeft, newTop);
    }
}

void SDLRenderingWindow::ApplyPendingConfigChanges()
{
    if (!_renderingWindow)
    {
        return;
    }

    if (_fullscreen)
    {
        if (_pendingSize)
        {
            // Used when leaving fullscreen.
            _pendingSize = false;
            _lastWindowWidth = _config->getInt("width", 1920);
            _lastWindowHeight = _config->getInt("height", 1080);
        }

        if (_pendingMonitor || _pendingFullscreenMode)
        {
            // Leave and re-enter fullscreen to switch monitor and/or exclusive resolution.
            // Windowed() also applies a pending position, which places the window on the new monitor.
            _pendingMonitor = false;
            _pendingFullscreenMode = false;
            Windowed();
            Fullscreen();
        }

        // Pure position changes stay pending until fullscreen is left.
        return;
    }

    _pendingMonitor = false;
    _pendingFullscreenMode = false; // Picked up the next time fullscreen is entered.

    const bool placementChanged = _pendingSize || _pendingPosition;

    if (_pendingSize)
    {
        _pendingSize = false;
        SDL_RestoreWindow(_renderingWindow); // Un-maximize, otherwise the new size is ignored.
        SDL_SetWindowSize(_renderingWindow, _config->getInt("width", 1920), _config->getInt("height", 1080));
    }

    if (_pendingPosition)
    {
        _pendingPosition = false;
        ApplyWindowPosition();
    }

    if (placementChanged)
    {
        KeepBorderlessWindowOnDisplay();
    }
}

void SDLRenderingWindow::ApplyWindowPosition()
{
    const bool positionOverridden = _config->getBool("overridePosition", false);
    int display = _config->getInt("monitor", 0);

    const int numDisplays = SDL_GetNumVideoDisplays();
    if (display > numDisplays)
    {
        display = numDisplays;
    }

    if (positionOverridden)
    {
        // Same logic as on startup: coordinates are relative to the selected monitor, or absolute if monitor is 0.
        int left = _config->getInt("left", 0);
        int top = _config->getInt("top", 0);
        if (display > 0)
        {
            SDL_Rect bounds;
            SDL_GetDisplayBounds(display - 1, &bounds);
            left += bounds.x;
            top += bounds.y;
        }
        SDL_SetWindowPosition(_renderingWindow, left, top);
        poco_debug_f2(_logger, "Moved window to X=%?d Y=%?d.", left, top);
    }
    else if (display > 0)
    {
        // No explicit position: center the window on the selected monitor.
        SDL_SetWindowPosition(_renderingWindow,
                              SDL_WINDOWPOS_CENTERED_DISPLAY(display - 1),
                              SDL_WINDOWPOS_CENTERED_DISPLAY(display - 1));
        poco_debug_f1(_logger, "Centered window on monitor %?d.", display);
    }
}

void SDLRenderingWindow::ShowCursor(bool visible)
{
    SDL_ShowCursor(visible);
}

void SDLRenderingWindow::NextDisplay()
{
    auto numDisplays = SDL_GetNumVideoDisplays();

    if (numDisplays < 2)
    {
        poco_debug(_logger, "Cannot move the window anywhere.");
        return;
    }

    bool wasFullscreen{_fullscreen};
    if (_fullscreen)
    {
        poco_debug(_logger, "Leaving fullscreen before moving.");
        Windowed();
    }

    // Find current display
    auto currentDisplay = GetCurrentDisplay();

    int left;
    int top;

    SDL_GetWindowPosition(_renderingWindow, &left, &top);

    int newDisplay = (currentDisplay + 1) % numDisplays;

    poco_debug_f1(_logger, "Moving window to display %?d.", newDisplay);

    if (newDisplay != currentDisplay)
    {

        SDL_Rect oldBounds{};
        SDL_Rect newBounds;

        int leftOffset{0};
        int topOffset{0};

        SDL_GetDisplayBounds(newDisplay, &newBounds);
        if (currentDisplay >= 0)
        {
            SDL_GetDisplayBounds(currentDisplay, &oldBounds);
            leftOffset = left - oldBounds.x;
            topOffset = top - oldBounds.y;
        }

        int newLeft = newBounds.x + leftOffset;
        int newTop = newBounds.y + topOffset;

        SDL_SetWindowPosition(_renderingWindow, newLeft, newTop);

        poco_debug_f4(_logger, "Old position: X=%?d Y=%?d, new position: X=%?d Y=%?d.",
                      left, top, newLeft, newTop);
    }

    if (wasFullscreen)
    {
        poco_debug(_logger, "Was in fullscreen before moving.");
        Fullscreen();
    }
}

void SDLRenderingWindow::CreateSDLWindow()
{
    SDL_InitSubSystem(SDL_INIT_VIDEO);

#if defined(__linux__) && !defined(__ANDROID__)
    InstallNonFatalXErrorHandler();
#endif

    // Default size matches the shipped projectMSDL.properties and the settings window (was 800x600).
    int width{_config->getInt("width", 1920)};
    int height{_config->getInt("height", 1080)};
    int left{_config->getInt("left", 0)};
    int top{_config->getInt("top", 0)};
    bool positionOverridden = _config->getBool("overridePosition", false);

    if (!positionOverridden)
    {
        left = SDL_WINDOWPOS_UNDEFINED;
        top = SDL_WINDOWPOS_UNDEFINED;
    }

    auto display = _config->getInt("monitor", 0);
    if (display > 0)
    {
        poco_debug_f1(_logger, "User requested to place window on monitor %?d.", display);
        auto numDisplays = SDL_GetNumVideoDisplays();
        if (display > numDisplays)
        {
            display = numDisplays;
        }

        SDL_Rect bounds;
        SDL_GetDisplayBounds(display - 1, &bounds);

        if (positionOverridden)
        {
            left += bounds.x;
            top += bounds.y;
        }
        else
        {
            left = bounds.x;
            top = bounds.y;
        }

        poco_debug_f3(_logger, "Creating window on monitor %?d at X=%?d Y=%?d.", display, left, top);
    }

#if USE_GLES
    // use GLES 3.2
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    // projectM Requires at least Core Profile 3.30 for samplers etc.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

	SDL_SetHint(SDL_HINT_APP_NAME, "projectM");
	
    _renderingWindow = SDL_CreateWindow("projectM", left, top, width, height,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!_renderingWindow)
    {
        auto errorMessage = "Could not create SDL rendering window. Error: " + std::string(SDL_GetError());
        poco_fatal(_logger, errorMessage);
        throw Poco::Exception(errorMessage);
    }

    _glContext = SDL_GL_CreateContext(_renderingWindow);
    if (!_glContext)
    {
        auto errorMessage = "Could not create OpenGL rendering context. Error: " + std::string(SDL_GetError());
        poco_fatal(_logger, errorMessage);
        throw Poco::Exception(errorMessage);
    }

    SDL_SetWindowTitle(_renderingWindow, "projectM");
    SDL_GL_MakeCurrent(_renderingWindow, _glContext);
    UpdateSwapInterval();

#ifdef USE_GLEW
    auto glewError = glewInit();
    if (glewError != GLEW_OK)
    {
        auto errorMessage = "Could not initialize GLEW. Error: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewError)));
        poco_fatal(_logger, errorMessage);
        throw Poco::Exception(errorMessage);
    }

    poco_debug_f1(_logger, "Initialized GLEW: %s", std::string(reinterpret_cast<const char*>(glewGetString(GLEW_VERSION))));
#endif

    if (_config->getBool("fullscreen", false))
    {
        Fullscreen();
    }
    else
    {
        Windowed();
    }

    if (_logger.debug())
    {
        DumpOpenGLInfo();
    }
}

void SDLRenderingWindow::DestroySDLWindow()
{
    poco_debug(_logger, "Closing rendering window and destroying OpenGL context.");

    SDL_GL_DeleteContext(_glContext);
    _glContext = nullptr;

    SDL_DestroyWindow(_renderingWindow);
    _renderingWindow = nullptr;

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void SDLRenderingWindow::DumpOpenGLInfo()
{
    poco_debug_f1(_logger, "- GL_VERSION: %s", std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    poco_debug_f1(_logger, "- GL_SHADING_LANGUAGE_VERSION: %s",
                  std::string(reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION))));
    poco_debug_f1(_logger, "- GL_VENDOR: %s", std::string(reinterpret_cast<const char*>(glGetString(GL_VENDOR))));
}

int SDLRenderingWindow::GetCurrentDisplay()
{
    int left;
    int top;

    SDL_GetWindowPosition(_renderingWindow, &left, &top);

    auto numDisplays = SDL_GetNumVideoDisplays();

    poco_debug_f1(_logger, "Displays available: %?d.", numDisplays);
    poco_debug_f2(_logger, "Window position is X=%?d Y=%?d", left, top);

    // Find current display
    int currentDisplay{-1};
    for (int display = 0; display < numDisplays; display++)
    {
        SDL_Rect bounds;
        SDL_GetDisplayBounds(display, &bounds);

        poco_debug_f1(_logger, "Bounds for display %?d:", display);
        poco_debug_f4(_logger, "    X=%?d Y=%?d W=%?d H=%?d", bounds.x, bounds.y, bounds.w, bounds.h);

        // Need to add 1 to left and top, as some window managers will screw up here.
        if (left + 1 >= bounds.x && left + 1 < bounds.x + bounds.w && top + 1 >= bounds.y && top + 1 < bounds.y + bounds.h)
        {
            poco_debug_f1(_logger, "Window is currently on display %?d.", display);
            currentDisplay = display;
            break;
        }
    }

    return currentDisplay;
}

void SDLRenderingWindow::GetWindowSize(int& width, int& height)
{
    SDL_GetWindowSize(_renderingWindow, &width, &height);
}

void SDLRenderingWindow::GetWindowPosition(int& left, int& top, bool relative)
{
    SDL_GetWindowPosition(_renderingWindow, &left, &top);
    if (!relative)
    {
        return;
    }

    SDL_Rect bounds;
    SDL_GetDisplayBounds(GetCurrentDisplay(), &bounds);

    left -= bounds.x;
    top -= bounds.y;
}

SDL_Window* SDLRenderingWindow::GetRenderingWindow() const
{
    return _renderingWindow;
}

SDL_GLContext SDLRenderingWindow::GetGlContext() const
{
    return _glContext;
}

void SDLRenderingWindow::UpdateWindowTitleNotificationHandler(POCO_UNUSED const Poco::AutoPtr<Notification::UpdateWindowTitle>& notification)
{
    UpdateWindowTitle();
}

void SDLRenderingWindow::UpdateWindowTitle()
{
    std::string newTitle = "projectM";

    if (_config->getBool("displayPresetNameInTitle", true))
    {
        auto& app = Poco::Util::Application::instance();
        auto& projectMWrapper = app.getSubsystem<ProjectMWrapper>();

        if (!projectMWrapper.Playlist().Empty())
        {
            const auto& currentItem = projectMWrapper.Playlist().CurrentItem();

            if (!currentItem.PresetName().empty())
            {
                newTitle += " ➫ " + currentItem.PresetName();
            }
        }

        if (projectm_get_preset_locked(projectMWrapper.ProjectM()))
        {
            newTitle += " [locked]";
        }
    }

    SDL_SetWindowTitle(_renderingWindow, newTitle.c_str());
}

void SDLRenderingWindow::UpdateSwapInterval()
{
    if (!_config->getBool("waitForVerticalSync", true))
    {
        SDL_GL_SetSwapInterval(0);
        return;
    }

    if (_config->getBool("adaptiveVerticalSync", true))
    {
        if (SDL_GL_SetSwapInterval(-1) == 0)
        {
            return;
        }
    }

    SDL_GL_SetSwapInterval(1);
}

void SDLRenderingWindow::OnConfigurationPropertyChanged(const Poco::Util::AbstractConfiguration::KeyValue& property)
{
    OnConfigurationPropertyRemoved(property.key());
}

void SDLRenderingWindow::OnConfigurationPropertyRemoved(const std::string& key)
{
    if (!_renderingWindow)
    {
        return;
    }

    if (key == "window.waitForVerticalSync" || key == "window.adaptiveVerticalSync")
    {
        UpdateSwapInterval();
    }

    if (key == "window.borderless")
    {
        SDL_SetWindowBordered(_renderingWindow, _config->getBool("borderless", false) ? SDL_FALSE : SDL_TRUE);
        if (!_fullscreen)
        {
            KeepBorderlessWindowOnDisplay();
        }
    }

    if (key == "window.displayPresetNameInTitle")
    {
        UpdateWindowTitle();
    }

    // Window placement and fullscreen resolution are applied in ApplyPendingConfigChanges(),
    // so changing multiple values at once (e.g. width + height) only resizes/moves once.
    if (key == "window.width" || key == "window.height")
    {
        _pendingSize = true;
    }

    if (key == "window.left" || key == "window.top" || key == "window.overridePosition")
    {
        _pendingPosition = true;
    }

    if (key == "window.monitor")
    {
        _pendingPosition = true;
        _pendingMonitor = true;
    }

    if (key == "window.fullscreen.exclusiveMode" ||
        ((key == "window.fullscreen.width" || key == "window.fullscreen.height") && _config->getBool("fullscreen.exclusiveMode", false)))
    {
        _pendingFullscreenMode = true;
    }
}
