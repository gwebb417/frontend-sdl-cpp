#include "SpoutOutput.h"

#include <Poco/Logger.h>

#ifdef SPOUT_ENABLED

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <SpoutGL/SpoutSender.h>

struct SpoutOutput::Impl {
    SpoutSender sender;
    std::string name;            //!< Name the sender was created with, empty if not created.
    bool failureLogged{false};   //!< Only log the first failed send in a row.
};

SpoutOutput::SpoutOutput()
    : _impl(std::make_unique<Impl>())
{
}

SpoutOutput::~SpoutOutput()
{
    Release();
}

bool SpoutOutput::Available()
{
    return true;
}

void SpoutOutput::Send(unsigned int textureId, int width, int height, const std::string& senderName)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    auto& logger = Poco::Logger::get("SpoutOutput");

    if (_impl->name != senderName)
    {
        Release();
        _impl->sender.SetSenderName(senderName.c_str());
        _impl->name = senderName;
        poco_information_f1(logger, R"(Spout output started as "%s".)", senderName);
    }

    // OpenGL textures are stored bottom-up, Spout shares them top-down, so invert.
    if (_impl->sender.SendTexture(textureId, GL_TEXTURE_2D, static_cast<unsigned int>(width), static_cast<unsigned int>(height), true, 0))
    {
        _impl->failureLogged = false;
    }
    else if (!_impl->failureLogged)
    {
        poco_warning(logger, "Spout output: sending a frame failed.");
        _impl->failureLogged = true;
    }
}

void SpoutOutput::Release()
{
    if (_impl && !_impl->name.empty())
    {
        _impl->sender.ReleaseSender();
        _impl->name.clear();
        poco_information(Poco::Logger::get("SpoutOutput"), "Spout output stopped.");
    }
}

#else // SPOUT_ENABLED

struct SpoutOutput::Impl {
};

SpoutOutput::SpoutOutput() = default;

SpoutOutput::~SpoutOutput() = default;

bool SpoutOutput::Available()
{
    return false;
}

void SpoutOutput::Send(unsigned int, int, int, const std::string&)
{
}

void SpoutOutput::Release()
{
}

#endif // SPOUT_ENABLED
