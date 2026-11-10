#pragma once

#include <Poco/Logger.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Poco {
namespace Net {
class HTTPServer;
}
} // namespace Poco

/**
 * @brief HTTP remote control with a web page for phones (Settings > Remote Control).
 *
 * Off by default. Listens on 127.0.0.1 only, unless "allow other devices" is on, which requires a password
 * (HTTP Basic authentication, plain HTTP: meant for a home network only).
 *
 * The server threads never touch projectM. They queue commands and read a status snapshot, both handled on
 * the main thread in Update(), called once per frame by the render loop.
 *
 * API:
 *   GET  /                      The remote control page.
 *   GET  /api/status            Status as JSON (current preset, modes, settings).
 *   POST /api/<action>?value=x&smooth=1
 *        Runs an action. Requires the header "X-ProjectM: 1", which web pages from other sites can't send.
 */
class RemoteControl
{
public:
    /**
     * @brief A command from the remote, executed on the main thread.
     */
    struct Command {
        std::string action; //!< e.g. "next", "lock", "beat-sensitivity".
        std::string value;  //!< Optional value, e.g. "1.25".
        bool smooth{false}; //!< For preset switches: smooth transition instead of a hard cut.
    };

    RemoteControl();
    ~RemoteControl();

    RemoteControl(const RemoteControl&) = delete;
    RemoteControl& operator=(const RemoteControl&) = delete;

    /**
     * @brief Starts/stops the server according to the settings, runs queued commands and refreshes the status.
     * Called once per frame on the main thread.
     * @param buildStatus Returns the current status as a JSON object string.
     * @param execute Runs one command.
     */
    void Update(const std::function<std::string()>& buildStatus, const std::function<void(const Command&)>& execute);

    /**
     * @brief Stops the server.
     */
    void Stop();

    /**
     * @brief Returns the URLs the remote page can be opened with, for display in the settings.
     * Only this PC's address unless other devices are allowed.
     */
    static std::vector<std::string> Urls();

    static constexpr int DefaultPort{8765}; //!< Default TCP port.

    struct Shared; //!< State shared with the request handler threads (defined in RemoteControl.cpp).

private:
    struct Settings {
        bool enabled{false};
        int port{DefaultPort};
        bool allowOtherDevices{false};
        std::string password;

        bool operator==(const Settings& other) const;
    };

    static Settings ReadSettings();

    void Start(const Settings& settings);

    std::shared_ptr<Shared> _shared;               //!< State shared with the request handler threads.
    std::unique_ptr<Poco::Net::HTTPServer> _server; //!< The running server, if any.
    Settings _applied;                              //!< Settings the server was last started (or tried) with.
    bool _firstUpdate{true};
    uint64_t _lastStatusTicks{0};

    Poco::Logger& _logger{Poco::Logger::get("RemoteControl")};
};
