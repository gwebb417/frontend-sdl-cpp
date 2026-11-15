/**
 * @file PresetPlaylist.cpp
 * @brief
 *
 **/

#include "PresetPlaylist.h"

#include "notifications/DisplayToast.h"
#include "notifications/PlaylistChanged.h"
#include "notifications/UpdateWindowTitle.h"

#include <projectM-4/playlist.h>
#include <projectM-4/projectM.h>

#include <Poco/NotificationCenter.h>
#include <Poco/Path.h>

#include <SDL2/SDL_timer.h>

POCO_IMPLEMENT_EXCEPTION(PlaylistEmptyException, Poco::Exception, "PlaylistEmptyException")

PresetPlaylist::Item::Item(std::string path)
    : _path(std::move(path))
{
    poco_assert_dbg(!_path.empty());

    if (!_path.empty())
    {
        _presetName = Poco::Path(_path).getBaseName();
    }
}

PresetPlaylist::PresetPlaylist()
    : _projectMPlaylist(projectm_playlist_create(nullptr))
{
    if (!_projectMPlaylist)
    {
        poco_error(_logger, "Failed to create the projectM preset playlist manager instance.");
        throw std::runtime_error("projectM Playlist Manager initialization failed");
    }

    projectm_playlist_set_preset_switched_event_callback(_projectMPlaylist, &PresetPlaylist::PresetSwitchedEvent, this);
    projectm_playlist_set_preset_switch_failed_event_callback(_projectMPlaylist, &PresetPlaylist::PresetSwitchFailedEvent, this);
}
PresetPlaylist::~PresetPlaylist()
{
    poco_assert(_projectMPlaylist);
    projectm_playlist_set_preset_switched_event_callback(_projectMPlaylist, nullptr, nullptr);
    projectm_playlist_set_preset_switch_failed_event_callback(_projectMPlaylist, nullptr, nullptr);
    projectm_playlist_destroy(_projectMPlaylist);
    _projectMPlaylist = nullptr;
}

void PresetPlaylist::Connect(projectm* projectMHandle)
{
    projectm_playlist_connect(_projectMPlaylist, projectMHandle);

    if (projectMHandle)
    {
        // Handle automatic preset switches (display duration, hard cuts) here instead of in the playlist
        // library, so shuffle never picks the preset that is already playing (see Random()).
        projectm_set_preset_switch_requested_event_callback(projectMHandle, &PresetPlaylist::PresetSwitchRequestedEvent, this);
    }
}

void PresetPlaylist::Disconnect()
{
    projectm_playlist_connect(_projectMPlaylist, nullptr);
}

void PresetPlaylist::CurrentIndex(ItemList::size_type index, bool hardcut)
{
    if (_items.empty())
    {
        throw PlaylistEmptyException();
    }

    if (index >= _items.size())
    {
        throw std::out_of_range("Playlist index out of range");
    }

    // _currentIndex is updated by PresetSwitchedEvent() once the preset has actually loaded.
    poco_assert(_projectMPlaylist);
    BeginPresetSwitch();
    projectm_playlist_set_position(_projectMPlaylist, static_cast<uint32_t>(index), hardcut);
}

void PresetPlaylist::ShuffleEnabled(bool enabled)
{
    poco_assert(_projectMPlaylist);
    _shuffle = enabled; // Next()/Previous()/Random() restore this value after temporarily changing shuffle.
    projectm_playlist_set_shuffle(_projectMPlaylist, enabled);
}

bool PresetPlaylist::ShuffleEnabled() const
{
    poco_assert(_projectMPlaylist);
    return projectm_playlist_get_shuffle(_projectMPlaylist);
}

void PresetPlaylist::Next(bool hardCut)
{
    poco_assert(_projectMPlaylist);
    projectm_playlist_set_shuffle(_projectMPlaylist, false);
    BeginPresetSwitch();
    projectm_playlist_play_next(_projectMPlaylist, hardCut);
    projectm_playlist_set_shuffle(_projectMPlaylist, _shuffle);
}

void PresetPlaylist::Previous(bool hardCut)
{
    poco_assert(_projectMPlaylist);
    projectm_playlist_set_shuffle(_projectMPlaylist, false);
    BeginPresetSwitch();
    projectm_playlist_play_previous(_projectMPlaylist, hardCut);
    projectm_playlist_set_shuffle(_projectMPlaylist, _shuffle);
}

void PresetPlaylist::LastInHistory(bool hardCut)
{
    poco_assert(_projectMPlaylist);
    BeginPresetSwitch();
    projectm_playlist_play_last(_projectMPlaylist, hardCut);
}

void PresetPlaylist::Random(bool hardCut)
{
    poco_assert(_projectMPlaylist);

    auto size = projectm_playlist_size(_projectMPlaylist);
    if (size < 2)
    {
        // Nothing else to choose from.
        projectm_playlist_set_shuffle(_projectMPlaylist, true);
        BeginPresetSwitch();
        projectm_playlist_play_next(_projectMPlaylist, hardCut);
        projectm_playlist_set_shuffle(_projectMPlaylist, _shuffle);
        return;
    }

    // Use the preset chosen ahead and preloaded by UpdatePreload(), if it's still valid.
    auto current = projectm_playlist_get_position(_projectMPlaylist);
    uint32_t index;
    if (_upcomingRandomIndex >= 0 && _upcomingRandomIndex < size && static_cast<uint32_t>(_upcomingRandomIndex) != current)
    {
        index = static_cast<uint32_t>(_upcomingRandomIndex);
    }
    else
    {
        index = PickRandomIndex(size);
    }
    _upcomingRandomIndex = -1;

    BeginPresetSwitch();
    projectm_playlist_set_position(_projectMPlaylist, index, hardCut);
}

uint32_t PresetPlaylist::PickRandomIndex(uint32_t size)
{
    // The playlist library's shuffle picks from all presets, including the current one, so with only a
    // few presets (e.g. favorites) it often "switched" to the same preset again. Pick any other preset.
    auto current = projectm_playlist_get_position(_projectMPlaylist);
    std::uniform_int_distribution<uint32_t> distribution(0, size - 2);
    uint32_t index = distribution(_randomGenerator);
    if (index >= current)
    {
        index++;
    }
    return index;
}

void PresetPlaylist::UpdatePreload(projectm* projectMHandle, double delaySeconds)
{
    if (projectMHandle == nullptr || _projectMPlaylist == nullptr)
    {
        return;
    }

    auto now = SDL_GetTicks64();

    // Log once when the driver has finished compiling the preloaded preset's shaders.
    if (_preloadTicks != 0 && projectm_preloaded_preset_ready(projectMHandle))
    {
        _preloadReadyMs = static_cast<double>(now - _preloadTicks);
        _preloadTicks = 0;
    }

    if (_preloadDone || _lastSwitchTicks == 0 || now - _lastSwitchTicks < static_cast<uint64_t>(delaySeconds * 1000.0))
    {
        return;
    }
    _preloadDone = true;

    // No automatic switches while the preset is locked.
    if (projectm_get_preset_locked(projectMHandle))
    {
        _preloadDone = false;
        return;
    }

    auto size = projectm_playlist_size(_projectMPlaylist);
    if (size < 2)
    {
        return;
    }

    uint32_t index;
    if (_shuffle)
    {
        auto current = projectm_playlist_get_position(_projectMPlaylist);
        if (_upcomingRandomIndex < 0 || _upcomingRandomIndex >= size || static_cast<uint32_t>(_upcomingRandomIndex) == current)
        {
            _upcomingRandomIndex = PickRandomIndex(size);
        }
        index = static_cast<uint32_t>(_upcomingRandomIndex);
    }
    else
    {
        index = projectm_playlist_get_position(_projectMPlaylist) + 1;
        if (index >= size)
        {
            index = 0;
        }
    }

    // The filename exactly as the playlist library will load it.
    auto items = projectm_playlist_items(_projectMPlaylist, index, 1);
    if (items == nullptr || items[0] == nullptr)
    {
        projectm_playlist_free_string_array(items);
        return;
    }
    std::string filename(items[0]);
    projectm_playlist_free_string_array(items);

    // Keep the last switch's timings for the log, the preload replaces them only temporarily.
    _preloadReadyMs = -1.0;
    if (projectm_preload_preset_file(projectMHandle, filename.c_str()))
    {
        _preloadTicks = SDL_GetTicks64();
        if (_preloadTicks == 0)
        {
            _preloadTicks = 1;
        }
        poco_debug(_logger, "Preloaded next preset: " + Poco::Path(filename).getBaseName());
    }
    else
    {
        poco_debug(_logger, "Could not preload next preset: " + Poco::Path(filename).getBaseName());
    }
}

void PresetPlaylist::PresetSwitchRequestedEvent(bool isHardCut, void* context)
{
    auto* that = reinterpret_cast<PresetPlaylist*>(context);
    if (!that || !that->_projectMPlaylist)
    {
        return;
    }

    try
    {
        if (that->_shuffle)
        {
            that->Random(isHardCut);
        }
        else
        {
            that->BeginPresetSwitch();
            projectm_playlist_play_next(that->_projectMPlaylist, isHardCut);
        }
    }
    catch (...)
    {
        // Never let an exception escape into libprojectM.
    }
}

void PresetPlaylist::BeginBatchEdit()
{
    _isBatchEditing = true;
}

void PresetPlaylist::EndBatchEdit()
{
    if (_isBatchEditing)
    {
        _isBatchEditing = false;
        SendPlaylistChangedNotification();
    }
}

bool PresetPlaylist::AddItem(const Item& item, bool allowDuplicates)
{
    poco_assert(_projectMPlaylist);

    if (projectm_playlist_add_preset(_projectMPlaylist, item.Path().c_str(), allowDuplicates))
    {
        _items.push_back(item);

        if (!_isBatchEditing)
        {
            SendPlaylistChangedNotification();
        }

        return true;
    }

    return false;
}

PresetPlaylist::ItemList::size_type PresetPlaylist::AddPath(const std::string& path,
                                                            const bool recurseSubdirs,
                                                            const bool allowDuplicates)
{
    poco_assert(_projectMPlaylist);

    const uint32_t oldPlaylistSize = projectm_playlist_size(_projectMPlaylist);
    const uint32_t addedPresets = projectm_playlist_add_path(_projectMPlaylist, path.c_str(), recurseSubdirs, allowDuplicates);

    if (addedPresets > 0)
    {
        // For now, we gather the new items from the playlist manager and add them here.
        const auto newItems = projectm_playlist_items(_projectMPlaylist, oldPlaylistSize, addedPresets);
        auto curItem = newItems;
        while (*curItem)
        {
            if (**curItem != '\0')
            {
                _items.emplace_back(std::string(*curItem));
            }
            ++curItem;
        }
        projectm_playlist_free_string_array(newItems);

        if (!_isBatchEditing)
        {
            SendPlaylistChangedNotification();
        }
    }

    return addedPresets;
}

bool PresetPlaylist::InsertItem(const Item& item, ItemList::size_type index, bool allowDuplicates)
{
    poco_assert(_projectMPlaylist);

    if (projectm_playlist_insert_preset(_projectMPlaylist, item.Path().c_str(), index, allowDuplicates))
    {
        _items.insert(_items.begin() + static_cast<ItemList::difference_type>(index), item);

        if (!_isBatchEditing)
        {
            SendPlaylistChangedNotification();
        }

        return true;
    }

    return false;
}

PresetPlaylist::ItemList::size_type PresetPlaylist::InsertPath(const std::string& path,
                                                               const ItemList::size_type index,
                                                               const bool recurseSubdirs,
                                                               const bool allowDuplicates)
{
    poco_assert(_projectMPlaylist);

    const uint32_t addedPresets = projectm_playlist_insert_path(_projectMPlaylist, path.c_str(), index, recurseSubdirs, allowDuplicates);

    if (addedPresets > 0)
    {
        // For now, we gather the new items from the playlist manager and add them here.
        const auto newItems = projectm_playlist_items(_projectMPlaylist, index, addedPresets);
        auto curItem = newItems;
        while (*curItem)
        {
            if (**curItem != '\0')
            {
                _items.emplace_back(std::string(*curItem));
            }
            ++curItem;
        }
        projectm_playlist_free_string_array(newItems);

        if (!_isBatchEditing)
        {
            SendPlaylistChangedNotification();
        }
    }

    return addedPresets;
}

bool PresetPlaylist::RemoveItem(const ItemList::size_type index)
{
    if (index >= _items.size())
    {
        return false;
    }

    poco_assert(_projectMPlaylist);

    if (!projectm_playlist_remove_preset(_projectMPlaylist, index))
    {
        return false;
    }

    _items.erase(_items.begin() + static_cast<ItemList::difference_type>(index));

    if (!_isBatchEditing)
    {
        SendPlaylistChangedNotification();
    }

    return true;
}

bool PresetPlaylist::RemoveItems(const ItemList::size_type index, ItemList::size_type count)
{
    if (index >= _items.size())
    {
        return false;
    }

    if (index + count >= _items.size())
    {
        count = _items.size() - index;
    }

    poco_assert(_projectMPlaylist);

    if (!projectm_playlist_remove_presets(_projectMPlaylist, index, count))
    {
        return false;
    }

    _items.erase(_items.begin() + static_cast<ItemList::difference_type>(index),
                 _items.begin() + static_cast<ItemList::difference_type>(index) + static_cast<ItemList::difference_type>(count));

    if (!_isBatchEditing)
    {
        SendPlaylistChangedNotification();
    }

    return true;
}

void PresetPlaylist::Clear()
{
    poco_assert(_projectMPlaylist);

    if (_items.empty())
    {
        return;
    }

    _items.clear();
    projectm_playlist_clear(_projectMPlaylist);

    if (!_isBatchEditing)
    {
        SendPlaylistChangedNotification();
    }
}

void PresetPlaylist::Sort(SortPredicate predicate,
                          SortOrder order)
{
    SortRange(0, _items.size(), predicate, order);
}

void PresetPlaylist::SortRange(ItemList::size_type startIndex,
                               ItemList::size_type count,
                               SortPredicate predicate,
                               SortOrder order)
{
    poco_assert(_projectMPlaylist);

    if (_items.empty())
    {
        return;
    }

    projectm_playlist_sort_predicate prjmPredicate;
    switch (predicate)
    {
        case SortPredicate::FullPath:
        default:
            prjmPredicate = SORT_PREDICATE_FULL_PATH;

            break;
        case SortPredicate::PresetName:
            prjmPredicate = SORT_PREDICATE_FILENAME_ONLY;
            break;
    }

    projectm_playlist_sort_order prjmOrder;
    switch (order)
    {
        case SortOrder::Ascending:
        default:
            prjmOrder = SORT_ORDER_ASCENDING;
            break;
        case SortOrder::Descending:
            prjmOrder = SORT_ORDER_DESCENDING;
            break;
        case SortOrder::Random:
            // ToDo: Implement.
            throw std::runtime_error("Sort order Random is not supported yet.");
            break;
    }

    projectm_playlist_sort(_projectMPlaylist, startIndex, count, prjmPredicate, prjmOrder);

    // We just resync all items from the projectM playlist for now.
    auto playlistSize = projectm_playlist_size(_projectMPlaylist);
    _items.clear();
    _items.reserve(playlistSize);
    const auto newItems = projectm_playlist_items(_projectMPlaylist, 0, playlistSize);
    auto curItem = newItems;
    while (*curItem)
    {
        if (**curItem != '\0')
        {
            _items.emplace_back(std::string(*curItem));
        }
        ++curItem;
    }
    projectm_playlist_free_string_array(newItems);

    if (!_isBatchEditing)
    {
        SendPlaylistChangedNotification();
    }
}

void PresetPlaylist::PresetSwitchedEvent(bool /* isHardCut */, unsigned int index, void* context)
{
    auto that = static_cast<PresetPlaylist*>(context);

    // Only the first report after BeginPresetSwitch() is the preset that loaded, see BeginPresetSwitch().
    if (that->_presetSwitchResolved)
    {
        if (index < that->_items.size())
        {
            poco_warning_f1(that->_logger, "Skipped preset that failed to load: %s", that->_items.at(index).Path());
        }
        return;
    }
    that->_presetSwitchResolved = true;

    // The next preset is preloaded a few seconds after this switch, see UpdatePreload().
    that->_lastSwitchTicks = SDL_GetTicks64();
    that->_preloadDone = false;

    if (index < that->_items.size())
    {
        that->_currentIndex = index;

        // Where the time of the preset switch went (our libprojectM patches load-timings.patch and
        // preset-preload.patch).
        projectm_preset_load_timings timings{};
        projectm_get_last_preset_load_timings(nullptr, &timings);
        if (timings.preloaded)
        {
            std::string readyText = that->_preloadReadyMs >= 0.0
                                        ? Poco::format("ready after %.0f ms", that->_preloadReadyMs)
                                        : std::string("not ready yet");
            poco_information(that->_logger, Poco::format(
                                                "Displaying preset: %s (preloaded, switch took %.0f ms incl. %.0f ms waiting for the driver; "
                                                "preload took %.0f ms: file %.0f, expressions %.0f, shader translation %.0f, "
                                                "shader compile %.0f, %s)",
                                                that->CurrentItem().PresetName(), timings.switch_ms, timings.wait_ms,
                                                timings.total_ms, timings.file_ms, timings.expressions_ms,
                                                timings.shader_translate_ms, timings.shader_compile_ms, readyText));
        }
        else
        {
            poco_information(that->_logger, Poco::format(
                                                "Displaying preset: %s (loaded in %.0f ms: file %.0f, expressions %.0f, "
                                                "shader translation %.0f, shader compile %.0f, first image %.0f)",
                                                that->CurrentItem().PresetName(), timings.total_ms, timings.file_ms,
                                                timings.expressions_ms, timings.shader_translate_ms,
                                                timings.shader_compile_ms, timings.initial_image_ms));
        }
    }
    that->_preloadTicks = 0;
    that->_preloadReadyMs = -1.0;

    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::UpdateWindowTitle);
}

void PresetPlaylist::PresetSwitchFailedEvent(const char* presetFilename, const char* message, void* context)
{
    auto that = static_cast<PresetPlaylist*>(context);

    // All retries failed: the previous preset is still displayed, so keep it as the current item.
    that->_presetSwitchResolved = true;

    const std::string fileName(presetFilename ? presetFilename : "");
    poco_warning_f2(that->_logger, "Could not load preset %s: %s", fileName, std::string(message ? message : ""));
    Poco::NotificationCenter::defaultCenter().postNotification(
        new Notification::DisplayToast("Could not load preset: " + Poco::Path(fileName).getBaseName()));
}

void PresetPlaylist::SendPlaylistChangedNotification() const
{
    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::PlaylistChanged(*this));
}
