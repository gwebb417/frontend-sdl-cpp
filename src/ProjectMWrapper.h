#pragma once

#include "PresetPlaylist.h"
#include "notifications/AudioDataAvailable.h"
#include "notifications/PlaybackControl.h"

#include <projectM-4/projectM.h>

#include <Poco/Logger.h>
#include <Poco/NObserver.h>

#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/Util/Subsystem.h>

#include <memory>

class ProjectMWrapper : public Poco::Util::Subsystem
{
public:
    const char* name() const override;

    void initialize(Poco::Util::Application& app) override;

    void uninitialize() override;

    /**
     * Returns the projectM instance handle.
     * @return The projectM instance handle used to call API functions.
     */
    projectm_handle ProjectM() const;

    /**
     * Returns the current playlist.
     * @return The currently active preset playlist.
     */
    const PresetPlaylist& Playlist() const;

    /**
     * Returns the current playlist.
     * @return The currently active preset playlist.
     */
    PresetPlaylist& Playlist();

    /**
     * Renders a single projectM frame.
     */
    void RenderFrame();

    /**
     * @brief Returns the targeted FPS value.
     * @return The user-configured target FPS. Can be 0, which means unlimited.
     */
    int TargetFPS();

    /**
     * @brief Updates projectM with the current, actual FPS value.
     * @param fps The current FPS value.
     */
    void UpdateRealFPS(float fps) const;

    /**
     * @brief If splash is disabled, shows the initial preset.
     * If shuffle is on, a random preset will be picked. Otherwise, the first playlist item is displayed.
     */
    void DisplayInitialPreset();

    /**
     * @brief Changes beat sensitivity by the given value.
     * @param value A positive or negative delta value.
     */
    void ChangeBeatSensitivity(float value) const;

    /**
     * @brief Returns the libprojectM version this application was built against.
     * @return A string with the libprojectM build version.
     */
    static std::string ProjectMBuildVersion();

    /**
     * @brief Returns the libprojectM version this applications currently runs with.
     * @return A string with the libprojectM runtime library version.
     */
    static std::string ProjectMRuntimeVersion();

    /**
     * Copies the full path of the current preset into the OS clipboard.
     */
    void PresetFileNameToClipboard() const;
    /**
     * @brief Copies the current preset file into the favorites folder.
     */
    void SaveCurrentPresetToFavorites();

    /**
     * @brief Removes the current preset from the favorites folder.
     */
    void RemoveCurrentPresetFromFavorites();

    /**
     * @brief Undoes the most recent favorites change (a save or a removal).
     */
    void UndoFavoriteChange();
    /**
     * @brief Switches between playing only favorites and playing all presets.
     */
    void ToggleFavoritesOnly();

private:
    void PlaybackControlNotificationHandler(const Poco::AutoPtr<Notification::PlaybackControl>& notification);

    /**
     * @brief Returns the favorites folder (projectM.favoritesPath or %APPDATA%\projectM\favorites\).
     */
    std::string FavoritesPath() const;

    /**
     * @brief Replaces the playlist contents with presets from the given files/folders, sorted by name.
     * @return The number of presets in the playlist afterwards.
     */
    PresetPlaylist::ItemList::size_type FillPlaylist(const std::vector<std::string>& paths);

    /**
     * @brief Plays a random preset if shuffle is on, otherwise the first one in the playlist.
     */
    void PlayFromStart();

    std::vector<std::string> _presetPaths; //!< The user's configured preset folders.
    bool _favoritesOnly{false}; //!< True while only the favorites folder is loaded.

    struct FavoriteChange
    {
        bool wasSave{false}; //!< True if the preset was saved (undo deletes it), false if it was removed (undo restores it).
        std::string fileName;
        std::string contents; //!< File contents, only used for removals.
    };
    std::vector<FavoriteChange> _undoStack; //!< Recent favorites changes for undo (most recent last).

    void PushUndo(FavoriteChange change);

    /**
     * @brief Returns the full path of the currently playing preset, or an empty string if none.
     */
    std::string CurrentPresetPath() const;

    void AudioDataAvailableNotificationHandler(const Poco::AutoPtr<Notification::AudioDataAvailable>& notification);

    std::vector<std::string> GetPathListWithDefault(const std::string& baseKey, const std::string& defaultPath);

    /**
     * @brief Event callback if a configuration value has changed.
     * @param property The key and value that has been changed.
     */
    void OnConfigurationPropertyChanged(const Poco::Util::AbstractConfiguration::KeyValue& property);

    /**
     * @brief Event callback if a configuration value has been removed.
     * @param key The key of the removed property.
     */
    void OnConfigurationPropertyRemoved(const std::string& key);

    Poco::AutoPtr<Poco::Util::AbstractConfiguration> _userConfig; //!< View of the "projectM" configuration subkey in the "user" configuration.
    Poco::AutoPtr<Poco::Util::AbstractConfiguration> _projectMConfigView; //!< View of the "projectM" configuration subkey in the "effective" configuration.

    projectm_handle _projectM{nullptr}; //!< Pointer to the projectM instance used by the application.
    std::unique_ptr<PresetPlaylist> _playlist; //!< The currently active playlist.

    uint32_t _audioChannels{0}; //!< Number of audio channels of the current capture device.
    std::vector<float> _audioStagingBuffer; //!< Buffer which receives audio data from the capture implementation.
    mutable Poco::Mutex _audioBufferMutex; //!< Mutex protecting access to the audio staging buffer.

    Poco::NObserver<ProjectMWrapper, Notification::PlaybackControl> _playbackControlNotificationObserver{*this, &ProjectMWrapper::PlaybackControlNotificationHandler};
    Poco::NObserver<ProjectMWrapper, Notification::AudioDataAvailable> _audioDataAvailableNotificationObserver{*this, &ProjectMWrapper::AudioDataAvailableNotificationHandler};

    Poco::Logger& _logger{Poco::Logger::get("SDLRenderingWindow")}; //!< The class logger.
};
