#pragma once

#include <memory>
#include <string>

/**
 * @brief Shares an OpenGL texture with other applications via Spout2 (Windows only).
 *
 * Receivers like OBS (with the obs-spout2-plugin) get every frame, even while the projectM window is
 * minimized or covered. On other platforms, or if the build has no Spout2, Available() returns false and
 * all functions do nothing.
 *
 * The Spout2 headers are only included in SpoutOutput.cpp, as they pull in windows.h and their own GL declarations.
 */
class SpoutOutput
{
public:
    SpoutOutput();
    ~SpoutOutput();

    SpoutOutput(const SpoutOutput&) = delete;
    SpoutOutput& operator=(const SpoutOutput&) = delete;

    /**
     * @brief Returns true if this build can send via Spout2.
     */
    static bool Available();

    /**
     * @brief Sends the texture to Spout receivers. Creates the sender on the first call.
     * Must be called with the OpenGL context current. Changes the framebuffer and texture bindings.
     * @param textureId OpenGL GL_TEXTURE_2D texture to send.
     * @param width Texture width.
     * @param height Texture height.
     * @param senderName Name receivers see. If it changes, the sender is recreated with the new name.
     */
    void Send(unsigned int textureId, int width, int height, const std::string& senderName);

    /**
     * @brief Closes the sender, receivers see it disappear. Does nothing if no sender exists.
     */
    void Release();

private:
    struct Impl;
    std::unique_ptr<Impl> _impl; //!< Spout2 sender, only with SPOUT_ENABLED.
};
