#include "ProjectMWrapper.h"

#include "ProjectMSDLApplication.h"
#include "SDLRenderingWindow.h"

#include "notifications/DisplayToast.h"

#include <Poco/Delegate.h>
#include <Poco/File.h>
#include <Poco/NotificationCenter.h>
#include <Poco/Path.h>
#include <Poco/RecursiveDirectoryIterator.h>
#include <Poco/String.h>
#include <Poco/StringTokenizer.h>

#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

const char* ProjectMWrapper::name() const
{
    return "ProjectM Wrapper";
}

void ProjectMWrapper::initialize(Poco::Util::Application& app)
{
    auto& projectMSDLApp = dynamic_cast<ProjectMSDLApplication&>(app);
    _projectMConfigView = projectMSDLApp.config().createView("projectM");
    _userConfig = projectMSDLApp.UserConfiguration();
    poco_information_f1(_logger, "Events enabled: %?d", _projectMConfigView->eventsEnabled());

    if (!_projectM)
    {
        auto& sdlWindow = app.getSubsystem<SDLRenderingWindow>();

        int canvasWidth{0};
        int canvasHeight{0};

        sdlWindow.GetDrawableSize(canvasWidth, canvasHeight);

        // Built-in defaults if no path is configured anywhere (e.g. no projectMSDL.properties next to the
        // executable): the "presets" and "textures" folders next to the executable, same as the shipped
        // config file. Previously, presets were searched in the whole executable folder, which also picked
        // up the favorites in a portable "userdata" folder, and no texture path was set at all.
        auto applicationSubDir = [&app](const std::string& name) {
            Poco::Path path(app.config().getString("application.dir", ""));
            path.makeDirectory().pushDirectory(name);
            return path.toString();
        };
        auto presetPaths = GetPathListWithDefault("presetPath", applicationSubDir("presets"));
        _presetPaths = presetPaths;
        for (const auto& presetPath : presetPaths)
        {
            poco_information_f1(_logger, "Preset folder: %s", presetPath);
        }
        auto texturePaths = GetPathListWithDefault("texturePath", applicationSubDir("textures"));

        _projectM = projectm_create();
        if (!_projectM)
        {
            poco_error(_logger, "Failed to initialize projectM. Possible reasons are a lack of required OpenGL features or GPU resources.");
            throw std::runtime_error("projectM initialization failed");
        }

        int fps = _projectMConfigView->getInt("fps", 60);
        if (fps <= 0)
        {
            // We don't know the target framerate, pass in a default of 60.
            fps = 60;
        }

        projectm_set_window_size(_projectM, canvasWidth, canvasHeight);
        projectm_set_fps(_projectM, fps);
        // Mesh size defaults (96x54) match the shipped projectMSDL.properties. Previously the code used
        // 48x32 here, 220x125 in RenderFrame() and the settings window showed 64x48.
        projectm_set_mesh_size(_projectM, _projectMConfigView->getInt("meshX", 96), _projectMConfigView->getInt("meshY", 54));
        projectm_set_aspect_correction(_projectM, _projectMConfigView->getBool("aspectCorrectionEnabled", true));
        projectm_set_preset_locked(_projectM, _projectMConfigView->getBool("presetLocked", false));

        // Preset display settings
        projectm_set_preset_duration(_projectM, _projectMConfigView->getDouble("displayDuration", 30.0));
        projectm_set_soft_cut_duration(_projectM, TransitionDuration());
        projectm_set_hard_cut_enabled(_projectM, _projectMConfigView->getBool("hardCutsEnabled", false));
        projectm_set_hard_cut_duration(_projectM, _projectMConfigView->getDouble("hardCutDuration", 20.0));
        projectm_set_hard_cut_sensitivity(_projectM, static_cast<float>(_projectMConfigView->getDouble("hardCutSensitivity", 1.0)));
        ApplyBeatSensitivity();
        ApplyAnimationSpeed();
        ApplyBandFrequencies();
        ApplyTransitionEffects();

        if (!texturePaths.empty())
        {
            std::vector<const char*> texturePathList;
            texturePathList.reserve(texturePaths.size());
            for (const auto& texturePath : texturePaths)
            {
                texturePathList.push_back(texturePath.data());
            }

            projectm_set_texture_search_paths(_projectM, texturePathList.data(), texturePaths.size());
        }

        // Playlist
        _playlist = std::make_unique<PresetPlaylist>();
        _playlist->ShuffleEnabled(_projectMConfigView->getBool("shuffleEnabled", true));

        FillAllPresets();

        _playlist->Connect(_projectM);
    }

    Poco::NotificationCenter::defaultCenter().addObserver(_playbackControlNotificationObserver);
    Notification::AudioDataAvailable::Center().addObserver(_audioDataAvailableNotificationObserver);

    // Observe user configuration changes (set via the settings window)
    _userConfig->propertyChanged += Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyChanged);
    _userConfig->propertyRemoved += Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyRemoved);
}

void ProjectMWrapper::uninitialize()
{
    _playlist.reset();

    _userConfig->propertyRemoved -= Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyRemoved);
    _userConfig->propertyChanged -= Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyChanged);
    Poco::NotificationCenter::defaultCenter().removeObserver(_playbackControlNotificationObserver);
    Notification::AudioDataAvailable::Center().removeObserver(_audioDataAvailableNotificationObserver);

    if (_projectM)
    {
        projectm_destroy(_projectM);
        _projectM = nullptr;
    }
}

projectm_handle ProjectMWrapper::ProjectM() const
{
    return _projectM;
}

const PresetPlaylist& ProjectMWrapper::Playlist() const
{
    if (_playlist)
    {
        return *_playlist;
    }

    throw std::runtime_error("Playlist not available (ProjectMWrapper subsystem not yet initialized?)");
}

PresetPlaylist& ProjectMWrapper::Playlist()
{
    if (_playlist)
    {
        return *_playlist;
    }

    throw std::runtime_error("Playlist not available (ProjectMWrapper subsystem not yet initialized?)");
}

int ProjectMWrapper::TargetFPS()
{
    return _projectMConfigView->getInt("fps", 60);
}

void ProjectMWrapper::UpdateRealFPS(float fps) const
{
    projectm_set_fps(_projectM, static_cast<uint32_t>(std::round(fps)));
}

void ProjectMWrapper::RenderFrame(unsigned int framebufferObject)
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    size_t currentMeshX{0};
    size_t currentMeshY{0};
    projectm_get_mesh_size(_projectM, &currentMeshX, &currentMeshY);
    if (currentMeshX != _projectMConfigView->getInt("meshX", 96) ||
        currentMeshY != _projectMConfigView->getInt("meshY", 54))
    {
        projectm_set_mesh_size(_projectM, _projectMConfigView->getInt("meshX", 96), _projectMConfigView->getInt("meshY", 54));
    }

    // The capture device (and with it the sample rate) can change at any time.
    if (Notification::AudioDataAvailable::SampleRate() != _bandFrequenciesSampleRate)
    {
        ApplyBandFrequencies();
    }

    // Add new audio data to the instance from the staging buffer
    {
        Poco::ScopedLock lock(_audioBufferMutex);
        if (!_audioStagingBuffer.empty())
        {
            poco_assert(_audioStagingBuffer.size() % _audioChannels == 0);

            // Beat sensitivity is applied inside libprojectM (patched, see vcpkg-overlay/ports/projectm).
            // v1.1 scaled these samples instead, but libprojectM compares the beat values with their average
            // over the last few seconds, so the effect faded after 5-10 seconds.
            projectm_pcm_add_float(_projectM, _audioStagingBuffer.data(), _audioStagingBuffer.size() / _audioChannels, static_cast<projectm_channels>(_audioChannels));
            _audioStagingBuffer.clear();
        }
    }

    // projectM may switch presets automatically (preset duration, hard cuts) while rendering.
    if (_playlist)
    {
        _playlist->BeginPresetSwitch();
    }
    if (framebufferObject != 0)
    {
        // Our libprojectM patch render-fbo.patch (backport from libprojectM 4.2).
        projectm_opengl_render_frame_fbo(_projectM, framebufferObject);
    }
    else
    {
        projectm_opengl_render_frame(_projectM);
    }
}

void ProjectMWrapper::DisplayInitialPreset()
{
    poco_assert(_playlist);

    // Default "false" matches the shipped projectMSDL.properties and the settings window.
    if (!_projectMConfigView->getBool("enableSplash", false))
    {
        if (_projectMConfigView->getBool("shuffleEnabled", true))
        {
            _playlist->Random(true);
        }
        else
        {
            _playlist->CurrentIndex(0, true);
        }
    }
}

bool ProjectMWrapper::BeatAveragingEnabled() const
{
    return _projectMConfigView->getBool("beatAveraging", false);
}

float ProjectMWrapper::BeatSensitivity() const
{
    return std::clamp(static_cast<float>(_projectMConfigView->getDouble("beatSensitivity", 1.0)), 0.0f, 2.0f);
}

void ProjectMWrapper::ApplyBeatSensitivity() const
{
    // With beat averaging on, presets react the standard Milkdrop way: the beat values are only compared
    // with their recent average (sensitivity 1), and the Beat Sensitivity setting isn't used.
    projectm_set_beat_sensitivity(_projectM, BeatAveragingEnabled() ? 1.0f : BeatSensitivity());

    // Per-band sensitivity (our libprojectM patch band-sensitivity.patch) multiplies Beat Sensitivity.
    // Beat Averaging also turns it off.
    if (BeatAveragingEnabled())
    {
        projectm_set_band_sensitivity(_projectM, 1.0f, 1.0f, 1.0f);
    }
    else
    {
        auto band = [this](const char* key) {
            return std::clamp(static_cast<float>(_projectMConfigView->getDouble(key, 1.0)), 0.0f, 2.0f);
        };
        projectm_set_band_sensitivity(_projectM, band("bassSensitivity"), band("midSensitivity"), band("trebleSensitivity"));
    }
}

void ProjectMWrapper::ChangeBeatSensitivity(float value)
{
    if (BeatAveragingEnabled())
    {
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Beat Sensitivity is off while Beat Averaging is on"));
        return;
    }

    // Stored in the settings like a change in the settings window, so both always show the same value.
    // The configuration change applies it (see OnConfigurationPropertyChanged()).
    float newValue = std::clamp(std::round((BeatSensitivity() + value) * 100.0f) / 100.0f, 0.0f, 2.0f);
    _userConfig->setDouble("projectM.beatSensitivity", newValue);
    Poco::NotificationCenter::defaultCenter().postNotification(
        new Notification::DisplayToast(Poco::format("Beat Sensitivity: %.2hf", newValue)));
}

std::string ProjectMWrapper::ProjectMBuildVersion()
{
    return PROJECTM_VERSION_STRING;
}

std::string ProjectMWrapper::ProjectMRuntimeVersion()
{
    auto* projectMVersion = projectm_get_version_string();
    std::string projectMRuntimeVersion(projectMVersion);
    projectm_free_string(projectMVersion);

    return projectMRuntimeVersion;
}

void ProjectMWrapper::PresetFileNameToClipboard() const
{
    if (_playlist && !_playlist->Empty())
    {
        SDL_SetClipboardText(_playlist->CurrentItem().Path().c_str());
    }
}

std::string ProjectMWrapper::FavoritesPath() const
{
    // Next to the user settings by default, so favorites also move into the userdata folder in portable mode.
    auto defaultPath = ProjectMSDLApplication::instance().UserDataDir() + "favorites" + Poco::Path::separator();
    return _projectMConfigView->getString("favoritesPath", defaultPath);
}

namespace {

/**
 * @brief Path with '/' separators, for comparing paths from different sources.
 */
std::string WithForwardSlashes(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

/**
 * @brief Key for comparing paths: '/' separators, and case-insensitive on Windows (like its file system).
 */
std::string PathKey(const std::string& path)
{
#ifdef _WIN32
    return Poco::toLower(WithForwardSlashes(path));
#else
    return WithForwardSlashes(path);
#endif
}

} // namespace

double ProjectMWrapper::TransitionDuration() const
{
    // libprojectM 4.1 divides by the transition duration to check if a transition has ended. With exactly 0,
    // that's 0/0, so the transition never ends and projectM never switches presets automatically again.
    // 0.05 s (about three frames) still looks like an instant switch.
    return std::max(_projectMConfigView->getDouble("transitionDuration", 3.0), 0.05);
}

std::string ProjectMWrapper::DisabledPath() const
{
    auto defaultPath = ProjectMSDLApplication::instance().UserDataDir() + "disabled" + Poco::Path::separator();
    return _projectMConfigView->getString("disabledPath", defaultPath);
}

std::string ProjectMWrapper::RelativePresetPath(const std::string& presetFile) const
{
    const auto file = WithForwardSlashes(presetFile);
    const auto fileKey = PathKey(presetFile);

    for (const auto& root : _presetPaths)
    {
        auto rootKey = PathKey(root);
        if (rootKey.empty())
        {
            continue;
        }
        if (rootKey.back() != '/')
        {
            rootKey += '/';
        }

        if (fileKey.size() > rootKey.size() && fileKey.compare(0, rootKey.size(), rootKey) == 0)
        {
            return file.substr(rootKey.size());
        }
    }

    return {};
}

void ProjectMWrapper::RemoveDisabledPresets()
{
    std::set<std::string> disabledKeys;

    try
    {
        Poco::File disabledDir(DisabledPath());
        if (!disabledDir.exists() || !disabledDir.isDirectory())
        {
            return;
        }

        const auto disabledRoot = PathKey(Poco::Path(DisabledPath()).makeDirectory().toString());
        Poco::SimpleRecursiveDirectoryIterator end;
        for (Poco::SimpleRecursiveDirectoryIterator it(disabledDir.path()); it != end; ++it)
        {
            if (it->isFile())
            {
                auto key = PathKey(it->path());
                if (key.compare(0, disabledRoot.size(), disabledRoot) == 0)
                {
                    disabledKeys.insert(key.substr(disabledRoot.size()));
                }
            }
        }
    }
    catch (const Poco::Exception& ex)
    {
        poco_warning_f1(_logger, "Could not read the disabled presets folder: %s", ex.displayText());
        return;
    }

    if (disabledKeys.empty())
    {
        return;
    }

    size_t removed{0};
    const auto& items = _playlist->Items();
    for (auto index = items.size(); index > 0; index--)
    {
        auto relativePath = RelativePresetPath(items[index - 1].Path());
        if (!relativePath.empty() && disabledKeys.count(PathKey(relativePath)) > 0)
        {
            _playlist->RemoveItem(index - 1);
            removed++;
        }
    }

    if (removed > 0)
    {
        poco_information_f1(_logger, "Disabled presets left out: %?d", removed);
    }
}

PresetPlaylist::ItemList::size_type ProjectMWrapper::FillPlaylist(const std::vector<std::string>& paths, bool skipDisabled)
{
    _playlist->BeginBatchEdit();
    _playlist->Clear();
    for (const auto& path : paths)
    {
        Poco::File file(path);
        if (file.exists() && file.isFile())
        {
            _playlist->AddItem(PresetPlaylist::Item(path), false);
        }
        else
        {
            // Symbolic links also fall under this. Without complex resolving, we can't
            // be sure what the link exactly points to, especially if a trailing slash is missing.
            _playlist->AddPath(path, true, false);
        }
    }
    if (skipDisabled)
    {
        RemoveDisabledPresets();
    }
    _playlist->Sort(PresetPlaylist::SortPredicate::PresetName, PresetPlaylist::SortOrder::Ascending);
    _playlist->EndBatchEdit();

    return _playlist->Size();
}

PresetPlaylist::ItemList::size_type ProjectMWrapper::FillAllPresets()
{
    return FillPlaylist(_presetPaths, true);
}

PresetPlaylist::ItemList::size_type ProjectMWrapper::FillFavorites()
{
    return FillPlaylist({FavoritesPath()}, false);
}

void ProjectMWrapper::PlayFromStart()
{
    if (_playlist->Empty())
    {
        return;
    }

    if (_playlist->ShuffleEnabled())
    {
        _playlist->Random(true);
    }
    else
    {
        _playlist->CurrentIndex(0, true);
    }
}

std::string ProjectMWrapper::CurrentPresetPath() const
{
    if (!_playlist || _playlist->Empty())
    {
        return {};
    }

    try
    {
        return _playlist->CurrentItem().Path();
    }
    catch (const std::exception&)
    {
        return {};
    }
}

std::string ProjectMWrapper::CurrentPresetName() const
{
    auto presetFile = CurrentPresetPath();
    return presetFile.empty() ? std::string() : Poco::Path(presetFile).getBaseName();
}

std::string ProjectMWrapper::PlaylistModeName() const
{
    switch (_mode)
    {
        case PlaylistMode::Favorites:
            return "favorites";
        case PlaylistMode::Disabled:
            return "disabled";
        case PlaylistMode::AllPresets:
        default:
            return "all";
    }
}

bool ProjectMWrapper::CurrentPresetIsFavorite() const
{
    auto presetFile = CurrentPresetPath();
    if (presetFile.empty())
    {
        return false;
    }

    try
    {
        Poco::Path favorite(FavoritesPath());
        favorite.setFileName(Poco::Path(presetFile).getFileName());
        return Poco::File(favorite).exists();
    }
    catch (const Poco::Exception&)
    {
        return false;
    }
}

bool ProjectMWrapper::PresetLocked() const
{
    return _projectM != nullptr && projectm_get_preset_locked(_projectM);
}

void ProjectMWrapper::SaveCurrentPresetToFavorites()
{
    auto presetFile = CurrentPresetPath();
    if (presetFile.empty())
    {
        return;
    }

    try
    {
        Poco::Path source(presetFile);
        Poco::File(FavoritesPath()).createDirectories();

        Poco::Path target(FavoritesPath());
        target.setFileName(source.getFileName());

        if (Poco::File(target).exists())
        {
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast("Already in favorites: " + source.getFileName()));
            return;
        }

        Poco::File(source).copyTo(target.toString());
        PushUndo({FavoriteChange::Kind::SavedFavorite, source.getFileName(), "", "", ""});
        poco_information_f1(_logger, "Saved favorite: %s", presetFile);
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Saved to favorites: " + source.getFileName()));
    }
    catch (const Poco::Exception& ex)
    {
        poco_error_f1(_logger, "Could not save favorite: %s", ex.displayText());
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Could not save favorite"));
    }
}

void ProjectMWrapper::RemoveCurrentPresetFromFavorites()
{
    auto presetFile = CurrentPresetPath();
    if (presetFile.empty())
    {
        return;
    }

    std::string fileName;

    try
    {
        Poco::Path source(presetFile);
        fileName = source.getFileName();

        Poco::Path target(FavoritesPath());
        target.setFileName(fileName);

        Poco::File favoriteFile(target);
        if (!favoriteFile.exists())
        {
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast("Not in favorites: " + fileName));
            return;
        }

        {
            // Keep a copy in memory so the removal can be undone.
            std::ifstream in(favoriteFile.path(), std::ios::binary);
            std::ostringstream buffer;
            buffer << in.rdbuf();
            PushUndo({FavoriteChange::Kind::RemovedFavorite, fileName, buffer.str(), "", ""});
        }

        favoriteFile.remove();
        poco_information_f1(_logger, "Removed favorite: %s", fileName);
    }
    catch (const Poco::Exception& ex)
    {
        poco_error_f1(_logger, "Could not remove favorite: %s", ex.displayText());
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Could not remove favorite"));
        return;
    }

    std::string message = "Removed from favorites: " + fileName;

    if (_mode == PlaylistMode::Favorites)
    {
        // Reload the favorites list so the removed preset leaves the rotation.
        if (FillFavorites() == 0)
        {
            FillAllPresets();
            _mode = PlaylistMode::AllPresets;
            message += " (no favorites left, playing all presets)";
        }

        PlayFromStart();
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(message));
}

void ProjectMWrapper::PushUndo(FavoriteChange change)
{
    _undoStack.push_back(std::move(change));
    if (_undoStack.size() > 20)
    {
        _undoStack.erase(_undoStack.begin());
    }
}

void ProjectMWrapper::UndoFavoriteChange()
{
    if (_undoStack.empty())
    {
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Nothing to undo"));
        return;
    }

    auto change = std::move(_undoStack.back());
    _undoStack.pop_back();

    if (change.kind == FavoriteChange::Kind::DisabledPreset)
    {
        UndoDisable(change);
        return;
    }

    if (change.kind == FavoriteChange::Kind::ReenabledPreset)
    {
        UndoReenable(change);
        return;
    }

    const bool wasSave = change.kind == FavoriteChange::Kind::SavedFavorite;

    try
    {
        Poco::File(FavoritesPath()).createDirectories();

        Poco::Path target(FavoritesPath());
        target.setFileName(change.fileName);

        if (wasSave)
        {
            // Undo a save: delete the copy again.
            Poco::File savedFile(target);
            if (savedFile.exists())
            {
                savedFile.remove();
            }
        }
        else
        {
            // Undo a removal: write the file back.
            std::ofstream out(target.toString(), std::ios::binary);
            out << change.contents;
            out.close();

            if (!out)
            {
                throw Poco::IOException("Could not write " + target.toString());
            }
        }
    }
    catch (const Poco::Exception& ex)
    {
        poco_error_f1(_logger, "Could not undo favorites change: %s", ex.displayText());
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Could not undo"));
        return;
    }

    poco_information_f1(_logger, "Undid favorites change: %s", change.fileName);

    std::string message = (wasSave ? "Undid save: " : "Restored favorite: ") + change.fileName;

    // In favorites-only mode, reload the list so it matches the folder again.
    if (_mode == PlaylistMode::Favorites)
    {
        if (FillFavorites() == 0)
        {
            FillAllPresets();
            _mode = PlaylistMode::AllPresets;
            message += " (no favorites left, playing all presets)";
        }

        if (wasSave)
        {
            // The undone preset may be the one playing, so move on to another.
            PlayFromStart();
        }
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(message));
}

void ProjectMWrapper::DeleteKeyPressed()
{
    switch (_mode)
    {
        case PlaylistMode::Favorites:
            RemoveCurrentPresetFromFavorites();
            break;

        case PlaylistMode::Disabled:
            ReenableCurrentPreset();
            break;

        case PlaylistMode::AllPresets:
            DisableCurrentPreset();
            break;
    }
}

bool ProjectMWrapper::MovePresetFile(const std::string& from, const std::string& to)
{
    try
    {
        Poco::File(Poco::Path(to).parent()).createDirectories();

        // Copy and delete instead of renaming, so it also works if the folders are on different drives.
        Poco::File(from).copyTo(to);
        try
        {
            Poco::File(from).remove();
        }
        catch (...)
        {
            Poco::File(to).remove();
            throw;
        }
    }
    catch (const Poco::Exception& ex)
    {
        poco_error_f3(_logger, "Could not move preset %s to %s: %s", from, to, ex.displayText());
        return false;
    }
    return true;
}

bool ProjectMWrapper::RemoveFromPlaylistAndContinue(const std::string& presetFile)
{
    auto index = _playlist->CurrentIndex();
    const auto& items = _playlist->Items();
    if (index >= items.size() || PathKey(items[index].Path()) != PathKey(presetFile))
    {
        index = items.size();
        for (size_t i = 0; i < items.size(); i++)
        {
            if (PathKey(items[i].Path()) == PathKey(presetFile))
            {
                index = i;
                break;
            }
        }
    }
    if (index < items.size())
    {
        _playlist->RemoveItem(index);
    }

    if (_playlist->Empty())
    {
        return false;
    }

    if (_playlist->ShuffleEnabled())
    {
        _playlist->Random(true);
    }
    else
    {
        // The next preset has moved up into the removed preset's place.
        _playlist->CurrentIndex(std::min<size_t>(index, _playlist->Size() - 1), true);
    }
    return true;
}

void ProjectMWrapper::PlayPresetFile(const std::string& presetFile)
{
    const auto& items = _playlist->Items();
    for (size_t i = 0; i < items.size(); i++)
    {
        if (PathKey(items[i].Path()) == PathKey(presetFile))
        {
            _playlist->CurrentIndex(i, true);
            return;
        }
    }
    PlayFromStart();
}

PresetPlaylist::ItemList::size_type ProjectMWrapper::FillDisabled()
{
    return FillPlaylist({DisabledPath()}, false);
}

PresetPlaylist::ItemList::size_type ProjectMWrapper::FillCurrentMode()
{
    switch (_mode)
    {
        case PlaylistMode::Favorites:
            return FillFavorites();
        case PlaylistMode::Disabled:
            return FillDisabled();
        case PlaylistMode::AllPresets:
        default:
            return FillAllPresets();
    }
}

std::string ProjectMWrapper::OriginalPresetPath(const std::string& disabledFile) const
{
    // The relative path inside the disabled folder, e.g. "Waveform/Wire Flat/x.milk".
    auto disabledRoot = PathKey(Poco::Path(DisabledPath()).makeDirectory().toString());
    auto fileKey = PathKey(disabledFile);
    if (fileKey.compare(0, disabledRoot.size(), disabledRoot) != 0 || _presetPaths.empty())
    {
        return {};
    }
    const auto relativePath = WithForwardSlashes(disabledFile).substr(disabledRoot.size());

    // Back into the preset folder that has its subfolder, or the first preset folder.
    std::string fallback;
    for (const auto& root : _presetPaths)
    {
        Poco::File rootDir(root);
        if (!rootDir.exists() || !rootDir.isDirectory())
        {
            continue;
        }

        Poco::Path target(root);
        target.makeDirectory();
        target.append(Poco::Path(relativePath, Poco::Path::PATH_UNIX));
        if (fallback.empty())
        {
            fallback = target.toString();
        }
        if (Poco::File(target.parent()).exists())
        {
            return target.toString();
        }
    }
    return fallback;
}

void ProjectMWrapper::DisableCurrentPreset()
{
    auto presetFile = CurrentPresetPath();
    if (presetFile.empty())
    {
        return;
    }

    Poco::Path source(presetFile);
    const auto fileName = source.getFileName();

    // Keep the folder structure (e.g. "disabled/Waveform/Wire Flat/x.milk"), so presets with the same name
    // from different folders don't collide, and a preset can be restored by moving it back by hand.
    auto relativePath = RelativePresetPath(presetFile);
    if (relativePath.empty())
    {
        relativePath = fileName;
    }

    Poco::Path target(DisabledPath());
    target.makeDirectory();
    target.append(Poco::Path(relativePath, Poco::Path::PATH_UNIX));

    if (!MovePresetFile(presetFile, target.toString()))
    {
        Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(
            "Could not disable preset: the presets folder may be read-only"));
        return;
    }

    PushUndo({FavoriteChange::Kind::DisabledPreset, fileName, "", presetFile, target.toString()});
    poco_information_f2(_logger, "Disabled preset %s (moved to %s)", presetFile, target.toString());

    // Remove it from the playlist and play another preset.
    std::string message = "Disabled: " + fileName + " (Ctrl+Z to undo)";
    if (!RemoveFromPlaylistAndContinue(presetFile))
    {
        message = "Disabled: " + fileName + " (no presets left)";
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(message));
}

void ProjectMWrapper::ReenableCurrentPreset()
{
    auto presetFile = CurrentPresetPath();
    if (presetFile.empty())
    {
        return;
    }

    const auto fileName = Poco::Path(presetFile).getFileName();
    const auto target = OriginalPresetPath(presetFile);
    if (target.empty())
    {
        Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(
            "Could not re-enable: no preset folder to move it back to"));
        return;
    }

    if (!MovePresetFile(presetFile, target))
    {
        Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(
            "Could not re-enable preset: the presets folder may be read-only"));
        return;
    }

    PushUndo({FavoriteChange::Kind::ReenabledPreset, fileName, "", target, presetFile});
    poco_information_f2(_logger, "Re-enabled preset %s (moved back to %s)", presetFile, target);

    std::string message = "Re-enabled: " + fileName + " (Ctrl+Z to undo)";
    if (!RemoveFromPlaylistAndContinue(presetFile))
    {
        // That was the last disabled preset.
        _mode = PlaylistMode::AllPresets;
        FillAllPresets();
        PlayPresetFile(target);
        message = "Re-enabled: " + fileName + " (no disabled presets left, playing all presets)";
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(message));
}

void ProjectMWrapper::UndoDisable(const FavoriteChange& change)
{
    if (!Poco::File(change.disabledPath).exists() || !MovePresetFile(change.disabledPath, change.originalPath))
    {
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Could not undo: " + change.fileName));
        return;
    }

    poco_information_f1(_logger, "Re-enabled preset %s", change.originalPath);

    std::string message = "Re-enabled: " + change.fileName;
    switch (_mode)
    {
        case PlaylistMode::AllPresets:
            // Reload all presets (the restored one is no longer left out) and show it again.
            FillAllPresets();
            PlayPresetFile(change.originalPath);
            break;

        case PlaylistMode::Disabled:
            // The restored preset leaves the disabled list.
            if (FillDisabled() == 0)
            {
                _mode = PlaylistMode::AllPresets;
                FillAllPresets();
                PlayPresetFile(change.originalPath);
                message += " (no disabled presets left, playing all presets)";
            }
            else if (PathKey(CurrentPresetPath()) == PathKey(change.disabledPath))
            {
                PlayFromStart();
            }
            break;

        case PlaylistMode::Favorites:
            break;
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(message));
}

void ProjectMWrapper::UndoReenable(const FavoriteChange& change)
{
    if (!Poco::File(change.originalPath).exists() || !MovePresetFile(change.originalPath, change.disabledPath))
    {
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Could not undo: " + change.fileName));
        return;
    }

    poco_information_f1(_logger, "Disabled preset again %s", change.disabledPath);

    switch (_mode)
    {
        case PlaylistMode::Disabled:
            // Back into the disabled list, and show it again.
            FillDisabled();
            PlayPresetFile(change.disabledPath);
            break;

        case PlaylistMode::AllPresets:
        {
            const bool wasPlaying = PathKey(CurrentPresetPath()) == PathKey(change.originalPath);
            FillAllPresets();
            if (wasPlaying)
            {
                PlayFromStart();
            }
            break;
        }

        case PlaylistMode::Favorites:
            break;
    }

    Poco::NotificationCenter::defaultCenter().postNotification(
        new Notification::DisplayToast("Disabled again: " + change.fileName));
}

void ProjectMWrapper::ToggleFavoritesOnly()
{
    if (_mode != PlaylistMode::Favorites)
    {
        auto count = FillFavorites();
        if (count == 0)
        {
            // Nothing saved yet: restore the previous list.
            FillCurrentMode();
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast("No favorites saved yet (press S to save one)"));
            return;
        }
        _mode = PlaylistMode::Favorites;
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Favorites only (" + std::to_string(count) + " presets)"));
    }
    else
    {
        _mode = PlaylistMode::AllPresets;
        FillAllPresets();
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Playing all presets"));
    }

    // Start from the newly loaded list.
    PlayFromStart();
}

void ProjectMWrapper::ToggleDisabledOnly()
{
    if (_mode != PlaylistMode::Disabled)
    {
        auto count = FillDisabled();
        if (count == 0)
        {
            // Nothing disabled yet: restore the previous list.
            FillCurrentMode();
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast("No disabled presets (press DEL to disable one)"));
            return;
        }
        _mode = PlaylistMode::Disabled;
        Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(
            "Disabled presets only (" + std::to_string(count) + " presets) - DEL re-enables"));
    }
    else
    {
        _mode = PlaylistMode::AllPresets;
        FillAllPresets();
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Playing all presets"));
    }

    // Start from the newly loaded list.
    PlayFromStart();
}

void ProjectMWrapper::PlaybackControlNotificationHandler(const Poco::AutoPtr<Notification::PlaybackControl>& notification)
{
    if (!_playlist)
    {
        return;
    }

    bool shuffleEnabled = _playlist->ShuffleEnabled();
    bool hardCut = !notification->SmoothTransition();

    switch (notification->ControlAction())
    {
        case Notification::PlaybackControl::Action::NextPreset:
            _playlist->Next(hardCut);
            break;

        case Notification::PlaybackControl::Action::PreviousPreset:
            _playlist->Previous(hardCut);
            break;

        case Notification::PlaybackControl::Action::LastPreset:
            _playlist->LastInHistory(hardCut);
            break;

        case Notification::PlaybackControl::Action::RandomPreset: {
            _playlist->Random(hardCut);
            break;
        }

        case Notification::PlaybackControl::Action::ToggleShuffle:
            _userConfig->setBool("projectM.shuffleEnabled", !shuffleEnabled);
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast(shuffleEnabled ? "Shuffle: Off" : "Shuffle: On"));
            break;

        case Notification::PlaybackControl::Action::TogglePresetLocked: {
            bool presetLocked = !projectm_get_preset_locked(_projectM);
            _userConfig->setBool("projectM.presetLocked", presetLocked);
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast(presetLocked ? "Preset locked" : "Preset unlocked"));
            break;
        }
    }
}

void ProjectMWrapper::AudioDataAvailableNotificationHandler(const Poco::AutoPtr<Notification::AudioDataAvailable>& notification)
{
    Poco::ScopedLock lock(_audioBufferMutex);

    // Clear existing buffer data if channel count differs, e.g. if audio device has changed
    if (_audioChannels != notification->Channels())
    {
        _audioChannels = notification->Channels();
        _audioStagingBuffer.clear();
    }

    std::copy(notification->Samples().cbegin(), notification->Samples().cend(), std::back_inserter(_audioStagingBuffer));
}

std::vector<std::string> ProjectMWrapper::GetPathListWithDefault(const std::string& baseKey, const std::string& defaultPath)
{
    using Poco::Util::AbstractConfiguration;

    std::vector<std::string> pathList;

    // Normalizes e.g. "${application.dir}/presets", which expands to ".../folder//presets" because
    // application.dir already ends with a separator, and uses the native separators.
    auto addPath = [&pathList](const std::string& path) {
        if (path.empty())
        {
            return;
        }
        try
        {
            pathList.push_back(Poco::Path(path).makeDirectory().toString());
        }
        catch (const Poco::Exception&)
        {
            pathList.push_back(path);
        }
    };

    addPath(_projectMConfigView->getString(baseKey, defaultPath));
    AbstractConfiguration::Keys subKeys;
    _projectMConfigView->keys(baseKey, subKeys);
    for (const auto& key : subKeys)
    {
        addPath(_projectMConfigView->getString(baseKey + "." + key, ""));
    }
    return pathList;
}

void ProjectMWrapper::OnConfigurationPropertyChanged(const Poco::Util::AbstractConfiguration::KeyValue& property)
{
    OnConfigurationPropertyRemoved(property.key());
}

void ProjectMWrapper::OnConfigurationPropertyRemoved(const std::string& key)
{
    if (_projectM == nullptr || _playlist == nullptr)
    {
        return;
    }

    if (key == "projectM.presetLocked")
    {
        projectm_set_preset_locked(_projectM, _projectMConfigView->getBool("presetLocked", false));
        Poco::NotificationCenter::defaultCenter().postNotification(new Notification::UpdateWindowTitle);
    }

    if (key == "projectM.shuffleEnabled")
    {
        _playlist->ShuffleEnabled(_projectMConfigView->getBool("shuffleEnabled", true));
    }

    if (key == "projectM.aspectCorrectionEnabled")
    {
        projectm_set_aspect_correction(_projectM, _projectMConfigView->getBool("aspectCorrectionEnabled", true));
    }

    if (key == "projectM.displayDuration")
    {
        projectm_set_preset_duration(_projectM, _projectMConfigView->getDouble("displayDuration", 30.0));
    }

    if (key == "projectM.transitionDuration")
    {
        projectm_set_soft_cut_duration(_projectM, TransitionDuration());
    }

    if (key == "projectM.hardCutsEnabled")
    {
        projectm_set_hard_cut_enabled(_projectM, _projectMConfigView->getBool("hardCutsEnabled", false));
    }

    if (key == "projectM.hardCutDuration")
    {
        projectm_set_hard_cut_duration(_projectM, _projectMConfigView->getDouble("hardCutDuration", 20.0));
    }

    if (key == "projectM.hardCutSensitivity")
    {
        projectm_set_hard_cut_sensitivity(_projectM, static_cast<float>(_projectMConfigView->getDouble("hardCutSensitivity", 1.0)));
    }

    if (key == "projectM.meshX" || key == "projectM.meshY")
    {
        projectm_set_mesh_size(_projectM, _projectMConfigView->getUInt64("meshX", 96), _projectMConfigView->getUInt64("meshY", 54));
    }

    // Previously only applied on startup, so changing it in the settings window had no visible effect.
    if (key == "projectM.beatSensitivity" || key == "projectM.beatAveraging" ||
        key == "projectM.bassSensitivity" || key == "projectM.midSensitivity" || key == "projectM.trebleSensitivity")
    {
        ApplyBeatSensitivity();
    }

    if (key == "projectM.ambientSpeed" || key == "projectM.fpsIndependentMotion")
    {
        ApplyAnimationSpeed();
    }

    if (key.rfind("projectM.band", 0) == 0)
    {
        ApplyBandFrequencies();
    }

    if (key == "projectM.transitionEffects")
    {
        ApplyTransitionEffects();
    }
}

const std::vector<std::pair<std::string, std::string>>& ProjectMWrapper::TransitionEffectNames()
{
    // Same order as the bits of projectm_set_transition_effects().
    static const std::vector<std::pair<std::string, std::string>> names{
        {"circle", "Circle"},
        {"plasma", "Plasma"},
        {"fade", "Fade"},
        {"sweep", "Sweep"},
        {"warp", "Warp"},
        {"zoomblur", "Zoom Blur"},
        {"beatpulse", "Beat Pulse"},
        {"datamosh", "Datamosh"}};
    return names;
}

namespace {

/**
 * @brief Splits "all" or a comma-separated list of transition effect keys into keys (all keys for "all").
 */
std::vector<std::string> TransitionEffectKeys(const std::string& value)
{
    std::vector<std::string> keys;
    const auto& names = ProjectMWrapper::TransitionEffectNames();
    auto trimmed = Poco::toLower(Poco::trim(value));
    if (trimmed.empty() || trimmed == "all")
    {
        for (const auto& name : names)
        {
            keys.push_back(name.first);
        }
        return keys;
    }

    Poco::StringTokenizer tokens(trimmed, ",", Poco::StringTokenizer::TOK_TRIM | Poco::StringTokenizer::TOK_IGNORE_EMPTY);
    for (const auto& token : tokens)
    {
        for (const auto& name : names)
        {
            if (name.first == token && std::find(keys.begin(), keys.end(), token) == keys.end())
            {
                keys.push_back(token);
            }
        }
    }
    return keys;
}

} // namespace

bool ProjectMWrapper::TransitionEffectEnabled(const std::string& key) const
{
    auto keys = TransitionEffectKeys(_projectMConfigView->getString("transitionEffects", "all"));
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

std::string ProjectMWrapper::TransitionEffectsWith(const std::string& current, const std::string& key, bool enabled)
{
    auto keys = TransitionEffectKeys(current);
    auto position = std::find(keys.begin(), keys.end(), key);
    if (enabled && position == keys.end())
    {
        keys.push_back(key);
    }
    else if (!enabled && position != keys.end())
    {
        if (keys.size() == 1)
        {
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast("At least one transition effect must stay on"));
            return current;
        }
        keys.erase(position);
    }

    const auto& names = TransitionEffectNames();
    if (keys.size() >= names.size())
    {
        return "all";
    }

    // Keep the list in the standard order, so the stored value doesn't depend on the click order.
    std::string result;
    for (const auto& name : names)
    {
        if (std::find(keys.begin(), keys.end(), name.first) != keys.end())
        {
            result += (result.empty() ? "" : ",") + name.first;
        }
    }
    return result;
}

void ProjectMWrapper::ApplyTransitionEffects()
{
    auto keys = TransitionEffectKeys(_projectMConfigView->getString("transitionEffects", "all"));
    const auto& names = TransitionEffectNames();

    uint32_t mask{0};
    for (size_t bit = 0; bit < names.size(); bit++)
    {
        if (std::find(keys.begin(), keys.end(), names[bit].first) != keys.end())
        {
            mask |= 1u << bit;
        }
    }

    // All effects (or none, from a hand-edited config) = 0, libprojectM's "use all".
    if (mask == (1u << names.size()) - 1)
    {
        mask = 0;
    }
    projectm_set_transition_effects(_projectM, mask);
}

void ProjectMWrapper::ApplyBandFrequencies()
{
    // Uses our libprojectM patch (vcpkg-overlay/ports/projectm/band-sensitivity.patch).
    _bandFrequenciesSampleRate = Notification::AudioDataAvailable::SampleRate();

    auto ranges = _projectMConfigView->getString("bandRanges", BandRangesMusic);
    if (ranges == BandRangesStandard)
    {
        // Stock libprojectM/Milkdrop split: the lower half of the spectrum in three equal parts.
        projectm_set_band_frequencies(_projectM, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        return;
    }

    float hz[6]{20.0f, 250.0f, 250.0f, 4000.0f, 4000.0f, 20000.0f};
    if (ranges == BandRangesCustom)
    {
        const char* keys[6]{"bandBassLow", "bandBassHigh", "bandMidLow", "bandMidHigh", "bandTrebleLow", "bandTrebleHigh"};
        for (int i = 0; i < 6; i++)
        {
            hz[i] = std::clamp(static_cast<float>(_projectMConfigView->getDouble(keys[i], hz[i])), 0.0f, 24000.0f);
        }
    }

    projectm_set_band_frequencies(_projectM, static_cast<float>(_bandFrequenciesSampleRate),
                                  hz[0], hz[1], hz[2], hz[3], hz[4], hz[5]);
}

float ProjectMWrapper::AmbientSpeed() const
{
    return std::clamp(static_cast<float>(_projectMConfigView->getDouble("ambientSpeed", 1.0)), MinAmbientSpeed, 1.0f);
}

void ProjectMWrapper::ChangeAmbientSpeed(float value)
{
    // Stored like a change in the settings window; the configuration change applies it.
    float newValue = std::clamp(std::round((AmbientSpeed() + value) * 100.0f) / 100.0f, MinAmbientSpeed, 1.0f);
    _userConfig->setDouble("projectM.ambientSpeed", newValue);
    Poco::NotificationCenter::defaultCenter().postNotification(
        new Notification::DisplayToast(Poco::format("Ambient Speed: %.2hf", newValue)));
}

void ProjectMWrapper::ApplyAnimationSpeed()
{
    // Both use our libprojectM patch (vcpkg-overlay/ports/projectm/animation-speed.patch).
    projectm_set_animation_speed(_projectM, AmbientSpeed());
    // Presets move as fast at any FPS as they do at 60 FPS (Milkdrop applies motion per frame).
    projectm_set_motion_reference_fps(_projectM, _projectMConfigView->getBool("fpsIndependentMotion", true) ? 60.0f : 0.0f);
}
