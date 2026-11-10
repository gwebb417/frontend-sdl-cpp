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
     * @param framebufferObject OpenGL framebuffer object to render into, 0 = the window.
     */
    void RenderFrame(unsigned int framebufferObject = 0);

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
    void ChangeBeatSensitivity(float value);

    /**
     * @brief Returns true if beat averaging is on (standard Milkdrop beat reaction, Beat Sensitivity not used).
     */
    bool BeatAveragingEnabled() const;

    /**
     * @brief Returns the configured Beat Sensitivity (0 to 2), even while beat averaging is on.
     */
    float BeatSensitivity() const;

    /**
     * @brief Passes the beat sensitivity to projectM: the configured value, or 1 if beat averaging is on.
     */
    void ApplyBeatSensitivity() const;

    static constexpr float MinAmbientSpeed{0.1f}; //!< Slowest Ambient Speed.

    /**
     * @brief Returns the configured Ambient Speed (0.1 to 1, 1 = normal speed).
     */
    float AmbientSpeed() const;

    /**
     * @brief Passes Ambient Speed and the FPS-independent motion setting to projectM.
     */
    void ApplyAnimationSpeed();

    /**
     * @brief Changes Ambient Speed by the given value and shows it in a toast (Left/Right arrow keys).
     * @param value A positive or negative delta value.
     */
    void ChangeAmbientSpeed(float value);

    static constexpr const char* BandRangesStandard{"standard"}; //!< Stock Milkdrop band split.
    static constexpr const char* BandRangesMusic{"music"};       //!< Bass 20-250 Hz, mid 250-4000 Hz, treble 4000-20000 Hz.
    static constexpr const char* BandRangesCustom{"custom"};     //!< Frequency ranges from the bandBass*/bandMid*/bandTreble* settings.

    /**
     * @brief Passes the bass/mid/treble frequency ranges for the current capture sample rate to projectM.
     */
    void ApplyBandFrequencies();

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
     * @brief Delete key: removes the current preset from favorites in favorites-only mode,
     *        otherwise disables it (see DisableCurrentPreset()).
     */
    void DeleteKeyPressed();

    /**
     * @brief Disables the current preset: moves its file into the "disabled" folder in the user data
     *        folder, removes it from the playlist and plays another preset. Can be undone.
     */
    void DisableCurrentPreset();

    /**
     * @brief Undoes the most recent change (favorite saved/removed, or preset disabled).
     */
    void UndoFavoriteChange();
    /**
     * @brief Switches between playing only favorites and playing all presets.
     */
    void ToggleFavoritesOnly();

    /**
     * @brief Switches between playing only the disabled presets and playing all presets (Ctrl+DEL).
     *        While only disabled presets play, DEL re-enables the current one.
     */
    void ToggleDisabledOnly();

    /**
     * @brief Re-enables the current preset (in disabled-only mode): moves it back into its preset folder,
     *        removes it from the playlist and plays another disabled preset. Can be undone.
     */
    void ReenableCurrentPreset();

    /**
     * @brief Name of the current preset (file name without extension), empty if none.
     */
    std::string CurrentPresetName() const;

    /**
     * @brief Which presets play: "all", "favorites" or "disabled".
     */
    std::string PlaylistModeName() const;

    /**
     * @brief Returns true if the current preset is in the favorites folder.
     */
    bool CurrentPresetIsFavorite() const;

    /**
     * @brief Returns true if the current preset is locked (no automatic switching).
     */
    bool PresetLocked() const;

private:
    void PlaybackControlNotificationHandler(const Poco::AutoPtr<Notification::PlaybackControl>& notification);

    /**
     * @brief Returns the favorites folder (projectM.favoritesPath or %APPDATA%\projectM\favorites\).
     */
    std::string FavoritesPath() const;

    /**
     * @brief Returns the configured transition (soft cut) duration in seconds, at least 0.05.
     */
    double TransitionDuration() const;

    /**
     * @brief Returns the folder disabled presets are moved to ("disabled" in the user data folder).
     */
    std::string DisabledPath() const;

    /**
     * @brief Returns the preset's path relative to the configured preset folder it is in, using '/'
     *        as separator, or an empty string if it isn't in any of them.
     */
    std::string RelativePresetPath(const std::string& presetFile) const;

    /**
     * @brief Removes presets from the playlist that have a copy in the disabled folder (at the same relative
     *        path). Keeps presets disabled after updating to a new version that ships all presets again.
     */
    void RemoveDisabledPresets();

    /**
     * @brief Replaces the playlist contents with presets from the given files/folders, sorted by name.
     * @param paths Files and folders to load.
     * @param skipDisabled If true, presets in the disabled folder are left out (see RemoveDisabledPresets()).
     * @return The number of presets in the playlist afterwards.
     */
    PresetPlaylist::ItemList::size_type FillPlaylist(const std::vector<std::string>& paths, bool skipDisabled);

    /**
     * @brief Loads all presets (without disabled ones) or only the favorites.
     */
    PresetPlaylist::ItemList::size_type FillAllPresets();
    PresetPlaylist::ItemList::size_type FillFavorites();
    PresetPlaylist::ItemList::size_type FillDisabled();

    /**
     * @brief Reloads the list for the current mode (all presets, favorites or disabled presets).
     */
    PresetPlaylist::ItemList::size_type FillCurrentMode();

    /**
     * @brief Moves a preset file (copy, then delete). Works across drives. Logs errors.
     * @return True if the file was moved.
     */
    bool MovePresetFile(const std::string& from, const std::string& to);

    /**
     * @brief Removes the given preset from the playlist and plays the next (or a random) one.
     * @return False if the playlist is empty afterwards.
     */
    bool RemoveFromPlaylistAndContinue(const std::string& presetFile);

    /**
     * @brief Plays the given preset if it's in the playlist, otherwise starts the playlist from the beginning.
     */
    void PlayPresetFile(const std::string& presetFile);

    /**
     * @brief Where a preset in the disabled folder came from: the same subfolder in the preset folder that
     *        has it (or the first preset folder). Empty if the file isn't in the disabled folder.
     */
    std::string OriginalPresetPath(const std::string& disabledFile) const;

    /**
     * @brief Plays a random preset if shuffle is on, otherwise the first one in the playlist.
     */
    void PlayFromStart();

    std::vector<std::string> _presetPaths; //!< The user's configured preset folders.
    /// Which presets are loaded: all (without disabled ones), only favorites (F) or only disabled presets (Ctrl+DEL).
    enum class PlaylistMode
    {
        AllPresets,
        Favorites,
        Disabled
    };
    PlaylistMode _mode{PlaylistMode::AllPresets};

    struct FavoriteChange
    {
        enum class Kind
        {
            SavedFavorite, //!< Preset was saved to favorites (undo deletes the copy).
            RemovedFavorite, //!< Preset was removed from favorites (undo writes it back).
            DisabledPreset, //!< Preset was moved to the disabled folder (undo moves it back).
            ReenabledPreset //!< Preset was moved back from the disabled folder (undo disables it again).
        };

        Kind kind{Kind::SavedFavorite};
        std::string fileName;
        std::string contents; //!< File contents, only used for removed favorites.
        std::string originalPath; //!< Where a disabled preset was, only used for disabled presets.
        std::string disabledPath; //!< Where a disabled preset was moved to, only used for disabled presets.
    };
    std::vector<FavoriteChange> _undoStack; //!< Recent favorites/disable changes for undo (most recent last).

    void PushUndo(FavoriteChange change);

    /**
     * @brief Moves a disabled preset back and plays it again.
     */
    void UndoDisable(const FavoriteChange& change);

    /**
     * @brief Moves a re-enabled preset back into the disabled folder.
     */
    void UndoReenable(const FavoriteChange& change);

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
    uint32_t _bandFrequenciesSampleRate{0}; //!< Sample rate the band frequency ranges were last applied for.
    std::vector<float> _audioStagingBuffer; //!< Buffer which receives audio data from the capture implementation.
    mutable Poco::Mutex _audioBufferMutex; //!< Mutex protecting access to the audio staging buffer.

    Poco::NObserver<ProjectMWrapper, Notification::PlaybackControl> _playbackControlNotificationObserver{*this, &ProjectMWrapper::PlaybackControlNotificationHandler};
    Poco::NObserver<ProjectMWrapper, Notification::AudioDataAvailable> _audioDataAvailableNotificationObserver{*this, &ProjectMWrapper::AudioDataAvailableNotificationHandler};

    Poco::Logger& _logger{Poco::Logger::get("SDLRenderingWindow")}; //!< The class logger.
};
