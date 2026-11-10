#include "RemoteControl.h"

#include "notifications/DisplayToast.h"

#include <Poco/Net/DNS.h>
#include <Poco/Net/HTTPBasicCredentials.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/IPAddress.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/NetworkInterface.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/NotificationCenter.h>
#include <Poco/String.h>
#include <Poco/URI.h>
#include <Poco/Util/Application.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <deque>
#include <mutex>

const char* RemoteControlPage(); // RemoteControlPage.cpp

struct RemoteControl::Shared {
    std::mutex mutex;
    std::deque<Command> queue;
    std::string statusJson{"{}"};
    bool requireAuthentication{false};
    std::string password;
};

namespace {

constexpr size_t MaxQueuedCommands{64};

/**
 * @brief Compares without stopping at the first difference, so response times don't reveal the password.
 */
bool SecureEquals(const std::string& a, const std::string& b)
{
    unsigned char difference = a.size() == b.size() ? 0 : 1;
    size_t length = std::max(a.size(), b.size());
    for (size_t i = 0; i < length; i++)
    {
        char charA = i < a.size() ? a[i] : 0;
        char charB = i < b.size() ? b[i] : 0;
        difference |= static_cast<unsigned char>(charA ^ charB);
    }
    return difference == 0;
}

/**
 * @brief Accepts IP addresses, "localhost" and this computer's name as the Host header. Blocks pages from
 *        other sites that make their own domain name point to this computer ("DNS rebinding").
 */
bool HostAllowed(const std::string& hostHeader)
{
    std::string host = hostHeader;
    if (!host.empty() && host[0] == '[')
    {
        // IPv6 literal, "[::1]:8765"
        auto end = host.find(']');
        host = end == std::string::npos ? host : host.substr(1, end - 1);
    }
    else
    {
        auto colon = host.rfind(':');
        if (colon != std::string::npos)
        {
            host = host.substr(0, colon);
        }
    }

    host = Poco::toLower(host);
    if (host.empty())
    {
        return false;
    }

    Poco::Net::IPAddress address;
    if (host == "localhost" || Poco::Net::IPAddress::tryParse(host, address))
    {
        return true;
    }

    try
    {
        auto computerName = Poco::toLower(Poco::Net::DNS::hostName());
        return host == computerName || host == computerName + ".local" || host == computerName + ".lan";
    }
    catch (const Poco::Exception&)
    {
        return false;
    }
}

void SendText(Poco::Net::HTTPServerResponse& response, Poco::Net::HTTPResponse::HTTPStatus status,
              const std::string& contentType, const std::string& body)
{
    response.setStatus(status);
    response.setContentType(contentType);
    response.set("Cache-Control", "no-store");
    response.set("X-Content-Type-Options", "nosniff");
    response.setContentLength(static_cast<std::streamsize>(body.size()));
    response.send() << body;
}

class RequestHandler : public Poco::Net::HTTPRequestHandler
{
public:
    explicit RequestHandler(std::shared_ptr<RemoteControl::Shared> shared)
        : _shared(std::move(shared))
    {
    }

    void handleRequest(Poco::Net::HTTPServerRequest& request, Poco::Net::HTTPServerResponse& response) override
    {
        using Status = Poco::Net::HTTPResponse::HTTPStatus;

        if (!HostAllowed(request.getHost()))
        {
            SendText(response, Status::HTTP_FORBIDDEN, "text/plain", "Forbidden");
            return;
        }

        if (!Authenticated(request))
        {
            response.requireAuthentication("projectM remote control");
            SendText(response, Status::HTTP_UNAUTHORIZED, "text/plain", "Password required");
            return;
        }

        Poco::URI uri(request.getURI());
        const auto& path = uri.getPath();
        const auto& method = request.getMethod();

        if (method == Poco::Net::HTTPRequest::HTTP_GET && (path == "/" || path == "/index.html"))
        {
            SendText(response, Status::HTTP_OK, "text/html; charset=utf-8", RemoteControlPage());
            return;
        }

        if (method == Poco::Net::HTTPRequest::HTTP_GET && path == "/api/status")
        {
            std::string status;
            {
                std::lock_guard<std::mutex> lock(_shared->mutex);
                status = _shared->statusJson;
            }
            SendText(response, Status::HTTP_OK, "application/json", status);
            return;
        }

        const std::string apiPrefix = "/api/";
        if (method == Poco::Net::HTTPRequest::HTTP_POST && path.rfind(apiPrefix, 0) == 0 && path.size() > apiPrefix.size())
        {
            // Browsers only send custom headers to other sites after asking permission (CORS preflight),
            // which this server never grants. So web pages from other sites can't send commands.
            if (request.get("X-ProjectM", "") != "1")
            {
                SendText(response, Status::HTTP_FORBIDDEN, "text/plain", "Missing X-ProjectM header");
                return;
            }

            RemoteControl::Command command;
            command.action = path.substr(apiPrefix.size());
            for (const auto& parameter : uri.getQueryParameters())
            {
                if (parameter.first == "value")
                {
                    command.value = parameter.second.substr(0, 256);
                }
                else if (parameter.first == "smooth")
                {
                    command.smooth = parameter.second == "1" || parameter.second == "true";
                }
            }

            {
                std::lock_guard<std::mutex> lock(_shared->mutex);
                if (_shared->queue.size() >= MaxQueuedCommands)
                {
                    SendText(response, Status::HTTP_SERVICE_UNAVAILABLE, "application/json", R"({"ok":false,"error":"busy"})");
                    return;
                }
                _shared->queue.push_back(std::move(command));
            }
            SendText(response, Status::HTTP_OK, "application/json", R"({"ok":true})");
            return;
        }

        SendText(response, Status::HTTP_NOT_FOUND, "text/plain", "Not found");
    }

private:
    bool Authenticated(const Poco::Net::HTTPServerRequest& request) const
    {
        std::string password;
        {
            std::lock_guard<std::mutex> lock(_shared->mutex);
            if (!_shared->requireAuthentication)
            {
                return true;
            }
            password = _shared->password;
        }

        if (!request.hasCredentials())
        {
            return false;
        }

        try
        {
            // Any user name, only the password counts.
            Poco::Net::HTTPBasicCredentials credentials(request);
            return SecureEquals(credentials.getPassword(), password);
        }
        catch (const Poco::Exception&)
        {
            return false;
        }
    }

    std::shared_ptr<RemoteControl::Shared> _shared;
};

class RequestHandlerFactory : public Poco::Net::HTTPRequestHandlerFactory
{
public:
    explicit RequestHandlerFactory(std::shared_ptr<RemoteControl::Shared> shared)
        : _shared(std::move(shared))
    {
    }

    Poco::Net::HTTPRequestHandler* createRequestHandler(const Poco::Net::HTTPServerRequest&) override
    {
        return new RequestHandler(_shared);
    }

private:
    std::shared_ptr<RemoteControl::Shared> _shared;
};

void Toast(const std::string& message)
{
    Poco::NotificationCenter::defaultCenter().postNotification(new Notification::DisplayToast(message));
}

} // namespace

bool RemoteControl::Settings::operator==(const Settings& other) const
{
    return enabled == other.enabled && port == other.port && allowOtherDevices == other.allowOtherDevices && password == other.password;
}

RemoteControl::RemoteControl()
    : _shared(std::make_shared<Shared>())
{
}

RemoteControl::~RemoteControl()
{
    Stop();
}

RemoteControl::Settings RemoteControl::ReadSettings()
{
    auto& config = Poco::Util::Application::instance().config();
    Settings settings;
    settings.enabled = config.getBool("remote.enabled", false);
    settings.port = std::clamp(config.getInt("remote.port", DefaultPort), 1, 65535);
    settings.allowOtherDevices = config.getBool("remote.allowOtherDevices", false);
    settings.password = config.getString("remote.password", "");
    return settings;
}

std::vector<std::string> RemoteControl::Urls()
{
    auto settings = ReadSettings();
    auto port = std::to_string(settings.port);

    std::vector<std::string> urls{"http://127.0.0.1:" + port};
    if (!settings.allowOtherDevices)
    {
        return urls;
    }

    try
    {
        for (const auto& networkInterface : Poco::Net::NetworkInterface::list(true, true))
        {
            const auto& address = networkInterface.address();
            if (address.family() == Poco::Net::IPAddress::IPv4 && !address.isLoopback() && !address.isLinkLocal())
            {
                auto url = "http://" + address.toString() + ":" + port;
                if (std::find(urls.begin(), urls.end(), url) == urls.end())
                {
                    urls.push_back(url);
                }
            }
        }
    }
    catch (const Poco::Exception&)
    {
    }
    return urls;
}

void RemoteControl::Start(const Settings& settings)
{
    if (settings.allowOtherDevices && settings.password.empty())
    {
        poco_warning(_logger, "Remote control not started: \"Allow Other Devices\" needs a password.");
        Toast("Remote control: set a password to allow other devices");
        return;
    }

    try
    {
        // 127.0.0.1 only accepts connections from this computer. 0.0.0.0 accepts them from the network.
        Poco::Net::SocketAddress address(settings.allowOtherDevices ? "0.0.0.0" : "127.0.0.1", static_cast<Poco::UInt16>(settings.port));
        Poco::Net::ServerSocket socket;
        socket.bind(address, false);
        socket.listen(16);

        {
            std::lock_guard<std::mutex> lock(_shared->mutex);
            _shared->requireAuthentication = settings.allowOtherDevices;
            _shared->password = settings.password;
            _shared->queue.clear();
        }

        auto* params = new Poco::Net::HTTPServerParams;
        params->setMaxThreads(4);
        params->setMaxQueued(16);
        params->setTimeout(Poco::Timespan(10, 0));
        params->setServerName("projectM");

        _server = std::make_unique<Poco::Net::HTTPServer>(new RequestHandlerFactory(_shared), socket, params);
        _server->start();

        poco_information_f2(_logger, "Remote control running on port %d (%s).", settings.port,
                            std::string(settings.allowOtherDevices ? "other devices allowed, password required" : "this computer only"));
        auto urls = Urls();
        Toast("Remote control on: " + (urls.size() > 1 ? urls[1] : urls[0]));
    }
    catch (const Poco::Exception& ex)
    {
        _server.reset();
        poco_error_f2(_logger, "Could not start the remote control on port %d: %s", settings.port, ex.displayText());
        Toast("Remote control: could not use port " + std::to_string(settings.port) + " (in use by another program?)");
    }
}

void RemoteControl::Stop()
{
    if (_server)
    {
        _server->stopAll(true);
        _server.reset();
        poco_information(_logger, "Remote control stopped.");
    }
}

void RemoteControl::Update(const std::function<std::string()>& buildStatus, const std::function<void(const Command&)>& execute)
{
    auto settings = ReadSettings();
    if (_firstUpdate || !(settings == _applied))
    {
        bool wasRunning = _server != nullptr;
        Stop();
        _applied = settings;
        _firstUpdate = false;
        if (settings.enabled)
        {
            Start(settings);
        }
        else if (wasRunning)
        {
            Toast("Remote control off");
        }
    }

    if (!_server)
    {
        return;
    }

    std::deque<Command> commands;
    {
        std::lock_guard<std::mutex> lock(_shared->mutex);
        commands.swap(_shared->queue);
    }

    for (const auto& command : commands)
    {
        execute(command);
    }

    // The status is read by the page about once per second, so refreshing it 4 times per second is plenty.
    // Right after a command, refresh it immediately so the page shows the result.
    auto now = SDL_GetTicks64();
    if (!commands.empty() || now - _lastStatusTicks >= 250)
    {
        _lastStatusTicks = now;
        auto status = buildStatus();
        std::lock_guard<std::mutex> lock(_shared->mutex);
        _shared->statusJson = std::move(status);
    }
}
