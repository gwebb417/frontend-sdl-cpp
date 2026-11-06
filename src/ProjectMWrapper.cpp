#include "ProjectMWrapper.h"

#include "ProjectMSDLApplication.h"
#include "SDLRenderingWindow.h"

#include "notifications/DisplayToastNotification.h"

#include <Poco/Delegate.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/NotificationCenter.h>

#include <SDL2/SDL_opengl.h>

#include <cmath>
#include <fstream>
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

        auto presetPaths = GetPathListWithDefault("presetPath", app.config().getString("application.dir", ""));
        _presetPaths = presetPaths;
        auto texturePaths = GetPathListWithDefault("texturePath", app.config().getString("", ""));

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
        projectm_set_mesh_size(_projectM, _projectMConfigView->getInt("meshX", 48), _projectMConfigView->getInt("meshY", 32));
        projectm_set_aspect_correction(_projectM, _projectMConfigView->getBool("aspectCorrectionEnabled", true));
        projectm_set_preset_locked(_projectM, _projectMConfigView->getBool("presetLocked", false));

        // Preset display settings
        projectm_set_preset_duration(_projectM, _projectMConfigView->getDouble("displayDuration", 30.0));
        projectm_set_soft_cut_duration(_projectM, _projectMConfigView->getDouble("transitionDuration", 3.0));
        projectm_set_hard_cut_enabled(_projectM, _projectMConfigView->getBool("hardCutsEnabled", false));
        projectm_set_hard_cut_duration(_projectM, _projectMConfigView->getDouble("hardCutDuration", 20.0));
        projectm_set_hard_cut_sensitivity(_projectM, static_cast<float>(_projectMConfigView->getDouble("hardCutSensitivity", 1.0)));
        projectm_set_beat_sensitivity(_projectM, static_cast<float>(_projectMConfigView->getDouble("beatSensitivity", 1.0)));

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
        _playlist = projectm_playlist_create(_projectM);
        if (!_playlist)
        {

            poco_error(_logger, "Failed to create the projectM preset playlist manager instance.");
            throw std::runtime_error("Playlist initialization failed");
        }

        projectm_playlist_set_shuffle(_playlist, _projectMConfigView->getBool("shuffleEnabled", true));

        for (const auto& presetPath : presetPaths)
        {
            Poco::File file(presetPath);
            if (file.exists() && file.isFile())
            {
                projectm_playlist_add_preset(_playlist, presetPath.c_str(), false);
            }
            else
            {
                // Symbolic links also fall under this. Without complex resolving, we can't
                // be sure what the link exactly points to, especially if a trailing slash is missing.
                projectm_playlist_add_path(_playlist, presetPath.c_str(), true, false);
            }
        }
        projectm_playlist_sort(_playlist, 0, projectm_playlist_size(_playlist), SORT_PREDICATE_FILENAME_ONLY, SORT_ORDER_ASCENDING);

        projectm_playlist_set_preset_switched_event_callback(_playlist, &ProjectMWrapper::PresetSwitchedEvent, static_cast<void*>(this));
    }

    Poco::NotificationCenter::defaultCenter().addObserver(_playbackControlNotificationObserver);
    Poco::NotificationCenter::defaultCenter().addObserver(_audioDataAvailableNotificationObserver);

    // Observe user configuration changes (set via the settings window)
    _userConfig->propertyChanged += Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyChanged);
    _userConfig->propertyRemoved += Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyRemoved);
}

void ProjectMWrapper::uninitialize()
{
    _userConfig->propertyRemoved -= Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyRemoved);
    _userConfig->propertyChanged -= Poco::delegate(this, &ProjectMWrapper::OnConfigurationPropertyChanged);
    Poco::NotificationCenter::defaultCenter().removeObserver(_playbackControlNotificationObserver);
    Poco::NotificationCenter::defaultCenter().removeObserver(_audioDataAvailableNotificationObserver);

    if (_projectM)
    {
        projectm_destroy(_projectM);
        _projectM = nullptr;
    }

    if (_playlist)
    {
        projectm_playlist_destroy(_playlist);
        _playlist = nullptr;
    }
}

projectm_handle ProjectMWrapper::ProjectM() const
{
    return _projectM;
}

projectm_playlist_handle ProjectMWrapper::Playlist() const
{
    return _playlist;
}

int ProjectMWrapper::TargetFPS()
{
    return _projectMConfigView->getInt("fps", 60);
}

void ProjectMWrapper::UpdateRealFPS(float fps)
{
    projectm_set_fps(_projectM, static_cast<uint32_t>(std::round(fps)));
}

void ProjectMWrapper::RenderFrame()
{
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    size_t currentMeshX{0};
    size_t currentMeshY{0};
    projectm_get_mesh_size(_projectM, &currentMeshX, &currentMeshY);
    if (currentMeshX != _projectMConfigView->getInt("meshX", 220) ||
        currentMeshY != _projectMConfigView->getInt("meshY", 125))
    {
        projectm_set_mesh_size(_projectM, _projectMConfigView->getInt("meshX", 220), _projectMConfigView->getInt("meshY", 125));
    }

    // Add new audio data to the instance from the staging buffer
    {
        Poco::ScopedLock lock(_audioBufferMutex);
        if (!_audioStagingBuffer.empty())
        {
            projectm_pcm_add_float(_projectM, _audioStagingBuffer.data(), _audioStagingBuffer.size() / _audioChannels, static_cast<projectm_channels>(_audioChannels));
            _audioStagingBuffer.clear();
        }
    }

    projectm_opengl_render_frame(_projectM);
}

void ProjectMWrapper::DisplayInitialPreset()
{
    if (!_projectMConfigView->getBool("enableSplash", true))
    {
        if (_projectMConfigView->getBool("shuffleEnabled", true))
        {
            projectm_playlist_play_next(_playlist, true);
        }
        else
        {
            projectm_playlist_set_position(_playlist, 0, true);
        }
    }
}

void ProjectMWrapper::ChangeBeatSensitivity(float value)
{
    projectm_set_beat_sensitivity(_projectM, projectm_get_beat_sensitivity(_projectM) + value);
    Poco::NotificationCenter::defaultCenter().postNotification(
        new DisplayToastNotification(Poco::format("Beat Sensitivity: %.2hf", projectm_get_beat_sensitivity(_projectM))));
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
    auto presetName = projectm_playlist_item(_playlist, projectm_playlist_get_position(_playlist));
    SDL_SetClipboardText(presetName);
    projectm_playlist_free_string(presetName);
}

std::string ProjectMWrapper::FavoritesPath() const
{
    auto defaultPath = Poco::Path::configHome() + "projectM" + Poco::Path::separator() + "favorites" + Poco::Path::separator();
    return _projectMConfigView->getString("favoritesPath", defaultPath);
}

uint32_t ProjectMWrapper::FillPlaylist(const std::vector<std::string>& paths)
{
    for (const auto& path : paths)
    {
        Poco::File file(path);
        if (file.exists() && file.isFile())
        {
            projectm_playlist_add_preset(_playlist, path.c_str(), false);
        }
        else
        {
            projectm_playlist_add_path(_playlist, path.c_str(), true, false);
        }
    }
    projectm_playlist_sort(_playlist, 0, projectm_playlist_size(_playlist), SORT_PREDICATE_FILENAME_ONLY, SORT_ORDER_ASCENDING);
    return projectm_playlist_size(_playlist);
}

void ProjectMWrapper::SaveCurrentPresetToFavorites()
{
    auto presetItem = projectm_playlist_item(_playlist, projectm_playlist_get_position(_playlist));
    if (!presetItem)
    {
        return;
    }
    std::string presetFile(presetItem);
    projectm_playlist_free_string(presetItem);

    try
    {
        Poco::Path source(presetFile);
        Poco::File(FavoritesPath()).createDirectories();

        Poco::Path target(FavoritesPath());
        target.setFileName(source.getFileName());

        if (Poco::File(target).exists())
        {
            Poco::NotificationCenter::defaultCenter().postNotification(
                new DisplayToastNotification("Already in favorites: " + source.getFileName()));
            return;
        }

        Poco::File(source).copyTo(target.toString());
        PushUndo({true, source.getFileName(), ""});
        poco_information_f1(_logger, "Saved favorite: %s", presetFile);
        Poco::NotificationCenter::defaultCenter().postNotification(
            new DisplayToastNotification("Saved to favorites: " + source.getFileName()));
    }
    catch (const Poco::Exception& ex)
    {
        poco_error_f1(_logger, "Could not save favorite: %s", ex.displayText());
        Poco::NotificationCenter::defaultCenter().postNotification(
            new DisplayToastNotification("Could not save favorite"));
    }
}
void ProjectMWrapper::RemoveCurrentPresetFromFavorites()
{
    auto presetItem = projectm_playlist_item(_playlist, projectm_playlist_get_position(_playlist));
    if (!presetItem)
    {
        return;
    }
    std::string presetFile(presetItem);
    projectm_playlist_free_string(presetItem);

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
                new DisplayToastNotification("Not in favorites: " + fileName));
            return;
        }

        {
            // Keep a copy in memory so the removal can be undone.
            std::ifstream in(favoriteFile.path(), std::ios::binary);
            std::ostringstream buffer;
            buffer << in.rdbuf();
            PushUndo({false, fileName, buffer.str()});
        }

        favoriteFile.remove();
        poco_information_f1(_logger, "Removed favorite: %s", fileName);
    }
    catch (const Poco::Exception& ex)
    {
        poco_error_f1(_logger, "Could not remove favorite: %s", ex.displayText());
        Poco::NotificationCenter::defaultCenter().postNotification(
            new DisplayToastNotification("Could not remove favorite"));
        return;
    }

    std::string message = "Removed from favorites: " + fileName;

    if (_favoritesOnly)
    {
        // Reload the favorites list so the removed preset leaves the rotation.
        projectm_playlist_clear(_playlist);
        if (FillPlaylist({FavoritesPath()}) == 0)
        {
            FillPlaylist(_presetPaths);
            _favoritesOnly = false;
            message += " (no favorites left, playing all presets)";
        }

        if (projectm_playlist_get_shuffle(_playlist))
        {
            projectm_playlist_play_next(_playlist, true);
        }
        else
        {
            projectm_playlist_set_position(_playlist, 0, true);
        }
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new DisplayToastNotification(message));
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
            new DisplayToastNotification("Nothing to undo"));
        return;
    }

    auto change = std::move(_undoStack.back());
    _undoStack.pop_back();

    try
    {
        Poco::File(FavoritesPath()).createDirectories();

        Poco::Path target(FavoritesPath());
        target.setFileName(change.fileName);

        if (change.wasSave)
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
            new DisplayToastNotification("Could not undo"));
        return;
    }

    poco_information_f1(_logger, "Undid favorites change: %s", change.fileName);

    std::string message = (change.wasSave ? "Undid save: " : "Restored favorite: ") + change.fileName;

    // In favorites-only mode, reload the list so it matches the folder again.
    if (_favoritesOnly)
    {
        projectm_playlist_clear(_playlist);
        if (FillPlaylist({FavoritesPath()}) == 0)
        {
            FillPlaylist(_presetPaths);
            _favoritesOnly = false;
            message += " (no favorites left, playing all presets)";
        }

        if (change.wasSave)
        {
            // The undone preset may be the one playing, so move on to another.
            if (projectm_playlist_get_shuffle(_playlist))
            {
                projectm_playlist_play_next(_playlist, true);
            }
            else
            {
                projectm_playlist_set_position(_playlist, 0, true);
            }
        }
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new DisplayToastNotification(message));
}

void ProjectMWrapper::ToggleFavoritesOnly()
{
    projectm_playlist_clear(_playlist);

    if (!_favoritesOnly)
    {
        auto count = FillPlaylist({FavoritesPath()});
        if (count == 0)
        {
            // Nothing saved yet: restore the full playlist and keep the current preset.
            projectm_playlist_clear(_playlist);
            FillPlaylist(_presetPaths);
            Poco::NotificationCenter::defaultCenter().postNotification(
                new DisplayToastNotification("No favorites saved yet (press S to save one)"));
            return;
        }
        _favoritesOnly = true;
        Poco::NotificationCenter::defaultCenter().postNotification(
            new DisplayToastNotification("Favorites only (" + std::to_string(count) + " presets)"));
    }
    else
    {
        FillPlaylist(_presetPaths);
        _favoritesOnly = false;
        Poco::NotificationCenter::defaultCenter().postNotification(
            new DisplayToastNotification("Playing all presets"));
    }

    // Start from the newly loaded list.
    if (projectm_playlist_get_shuffle(_playlist))
    {
        projectm_playlist_play_next(_playlist, true);
    }
    else
    {
        projectm_playlist_set_position(_playlist, 0, true);
    }
}

void ProjectMWrapper::PresetSwitchedEvent(bool isHardCut, unsigned int index, void* context)
{
    auto that = reinterpret_cast<ProjectMWrapper*>(context);
    auto presetName = projectm_playlist_item(that->_playlist, index);
    poco_information_f1(that->_logger, "Displaying preset: %s", std::string(presetName));
    projectm_playlist_free_string(presetName);

    Poco::NotificationCenter::defaultCenter().postNotification(new UpdateWindowTitleNotification);
}

void ProjectMWrapper::PlaybackControlNotificationHandler(const Poco::AutoPtr<PlaybackControlNotification>& notification)
{
    bool shuffleEnabled = projectm_playlist_get_shuffle(_playlist);

    switch (notification->ControlAction())
    {
        case PlaybackControlNotification::Action::NextPreset:
            projectm_playlist_set_shuffle(_playlist, false);
            projectm_playlist_play_next(_playlist, !notification->SmoothTransition());
            projectm_playlist_set_shuffle(_playlist, shuffleEnabled);
            break;

        case PlaybackControlNotification::Action::PreviousPreset:
            projectm_playlist_set_shuffle(_playlist, false);
            projectm_playlist_play_previous(_playlist, !notification->SmoothTransition());
            projectm_playlist_set_shuffle(_playlist, shuffleEnabled);
            break;

        case PlaybackControlNotification::Action::LastPreset:
            projectm_playlist_play_last(_playlist, !notification->SmoothTransition());
            break;

        case PlaybackControlNotification::Action::RandomPreset: {
            projectm_playlist_set_shuffle(_playlist, true);
            projectm_playlist_play_next(_playlist, !notification->SmoothTransition());
            projectm_playlist_set_shuffle(_playlist, shuffleEnabled);
            break;
        }

        case PlaybackControlNotification::Action::ToggleShuffle:
            _userConfig->setBool("projectM.shuffleEnabled", !shuffleEnabled);
            Poco::NotificationCenter::defaultCenter().postNotification(
                new DisplayToastNotification(shuffleEnabled ? "Shuffle: Off" : "Shuffle: On"));
            break;

        case PlaybackControlNotification::Action::TogglePresetLocked: {
            bool presetLocked = !projectm_get_preset_locked(_projectM);
            _userConfig->setBool("projectM.presetLocked", presetLocked);
            Poco::NotificationCenter::defaultCenter().postNotification(
                new DisplayToastNotification(presetLocked ? "Preset locked" : "Preset unlocked"));
            break;
        }
    }
}

void ProjectMWrapper::AudioDataAvailableNotificationHandler(const Poco::AutoPtr<AudioDataAvailableNotification>& notification)
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
    auto defaultPresetPath = _projectMConfigView->getString(baseKey, defaultPath);
    if (!defaultPresetPath.empty())
    {
        pathList.push_back(defaultPresetPath);
    }
    AbstractConfiguration::Keys subKeys;
    _projectMConfigView->keys(baseKey, subKeys);
    for (const auto& key : subKeys)
    {
        auto path = _projectMConfigView->getString(baseKey + "." + key, "");
        if (!path.empty())
        {
            pathList.push_back(std::move(path));
        }
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
        Poco::NotificationCenter::defaultCenter().postNotification(new UpdateWindowTitleNotification);
    }

    if (key == "projectM.shuffleEnabled")
    {
        projectm_playlist_set_shuffle(_playlist, _projectMConfigView->getBool("shuffleEnabled", true));
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
        projectm_set_soft_cut_duration(_projectM, _projectMConfigView->getDouble("transitionDuration", 3.0));
    }

    if (key == "projectM.hardCutsEnabled")
    {
        projectm_set_aspect_correction(_projectM, _projectMConfigView->getBool("hardCutsEnabled", false));
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
        projectm_set_mesh_size(_projectM, _projectMConfigView->getUInt64("meshX", 48), _projectMConfigView->getUInt64("meshY", 32));
    }
}
