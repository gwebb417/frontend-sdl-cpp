#include "ProjectMWrapper.h"

#include "ProjectMSDLApplication.h"
#include "SDLRenderingWindow.h"

#include "notifications/DisplayToast.h"

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
        _playlist = std::make_unique<PresetPlaylist>();
        _playlist->ShuffleEnabled(_projectMConfigView->getBool("shuffleEnabled", true));

        _playlist->BeginBatchEdit();
        for (const auto& presetPath : presetPaths)
        {
            Poco::File file(presetPath);
            if (file.exists() && file.isFile())
            {
                _playlist->AddItem(PresetPlaylist::Item(presetPath), false);
            }
            else
            {
                // Symbolic links also fall under this. Without complex resolving, we can't
                // be sure what the link exactly points to, especially if a trailing slash is missing.
                _playlist->AddPath(presetPath, true, false);
            }
        }
        _playlist->Sort(PresetPlaylist::SortPredicate::PresetName, PresetPlaylist::SortOrder::Ascending);
        _playlist->EndBatchEdit();

        _playlist->Connect(_projectM);
    }

    Poco::NotificationCenter::defaultCenter().addObserver(_playbackControlNotificationObserver);
    Poco::NotificationCenter::defaultCenter().addObserver(_audioDataAvailableNotificationObserver);

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
    Poco::NotificationCenter::defaultCenter().removeObserver(_audioDataAvailableNotificationObserver);

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

void ProjectMWrapper::RenderFrame()
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

    // Add new audio data to the instance from the staging buffer
    {
        Poco::ScopedLock lock(_audioBufferMutex);
        if (!_audioStagingBuffer.empty())
        {
            poco_assert(_audioStagingBuffer.size() % _audioChannels == 0);

            // libprojectM 4.x stores the beat sensitivity (projectm_set_beat_sensitivity), but never uses it,
            // so the setting had no effect. Apply it here instead by scaling the samples, which scales the
            // bass/mid/treb values presets react to (and the waveforms drawn from the audio): 0 = no reaction
            // to the music, 1 = normal, 2 = twice as strong. This happens in the same frame the samples are
            // passed to projectM, so it adds no latency. At the default of 1.0 the samples are passed unchanged.
            auto beatSensitivity = projectm_get_beat_sensitivity(_projectM);
            if (beatSensitivity != 1.0f)
            {
                for (auto& sample : _audioStagingBuffer)
                {
                    sample *= beatSensitivity;
                }
            }

            projectm_pcm_add_float(_projectM, _audioStagingBuffer.data(), _audioStagingBuffer.size() / _audioChannels, static_cast<projectm_channels>(_audioChannels));
            _audioStagingBuffer.clear();
        }
    }

    projectm_opengl_render_frame(_projectM);
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

void ProjectMWrapper::ChangeBeatSensitivity(float value) const
{
    projectm_set_beat_sensitivity(_projectM, projectm_get_beat_sensitivity(_projectM) + value);
    Poco::NotificationCenter::defaultCenter().postNotification(
        new Notification::DisplayToast(Poco::format("Beat Sensitivity: %.2hf", projectm_get_beat_sensitivity(_projectM))));
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

PresetPlaylist::ItemList::size_type ProjectMWrapper::FillPlaylist(const std::vector<std::string>& paths)
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
            _playlist->AddPath(path, true, false);
        }
    }
    _playlist->Sort(PresetPlaylist::SortPredicate::PresetName, PresetPlaylist::SortOrder::Ascending);
    _playlist->EndBatchEdit();

    return _playlist->Size();
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
        PushUndo({true, source.getFileName(), ""});
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
            PushUndo({false, fileName, buffer.str()});
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

    if (_favoritesOnly)
    {
        // Reload the favorites list so the removed preset leaves the rotation.
        if (FillPlaylist({FavoritesPath()}) == 0)
        {
            FillPlaylist(_presetPaths);
            _favoritesOnly = false;
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
            new Notification::DisplayToast("Could not undo"));
        return;
    }

    poco_information_f1(_logger, "Undid favorites change: %s", change.fileName);

    std::string message = (change.wasSave ? "Undid save: " : "Restored favorite: ") + change.fileName;

    // In favorites-only mode, reload the list so it matches the folder again.
    if (_favoritesOnly)
    {
        if (FillPlaylist({FavoritesPath()}) == 0)
        {
            FillPlaylist(_presetPaths);
            _favoritesOnly = false;
            message += " (no favorites left, playing all presets)";
        }

        if (change.wasSave)
        {
            // The undone preset may be the one playing, so move on to another.
            PlayFromStart();
        }
    }

    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(message));
}

void ProjectMWrapper::ToggleFavoritesOnly()
{
    if (!_favoritesOnly)
    {
        auto count = FillPlaylist({FavoritesPath()});
        if (count == 0)
        {
            // Nothing saved yet: restore the full playlist.
            FillPlaylist(_presetPaths);
            Poco::NotificationCenter::defaultCenter().postNotification(
                new Notification::DisplayToast("No favorites saved yet (press S to save one)"));
            return;
        }
        _favoritesOnly = true;
        Poco::NotificationCenter::defaultCenter().postNotification(
            new Notification::DisplayToast("Favorites only (" + std::to_string(count) + " presets)"));
    }
    else
    {
        FillPlaylist(_presetPaths);
        _favoritesOnly = false;
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
        projectm_set_soft_cut_duration(_projectM, _projectMConfigView->getDouble("transitionDuration", 3.0));
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
    if (key == "projectM.beatSensitivity")
    {
        projectm_set_beat_sensitivity(_projectM, static_cast<float>(_projectMConfigView->getDouble("beatSensitivity", 1.0)));
    }
}
