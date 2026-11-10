#pragma once

#include <string>
#include <utility>
#include <vector>

class SpoutOutput;

/**
 * @brief Image effects applied to the finished projectM picture (window and Spout output alike).
 */
struct OutputEffects {
    /**
     * Mirror modes. Stored in the config as the strings in MirrorModeNames().
     */
    enum class Mirror : int
    {
        Off = 0,       //!< No mirroring.
        LeftRight = 1, //!< The left half is mirrored onto the right half.
        TopBottom = 2, //!< The top half is mirrored onto the bottom half.
        FourWay = 3    //!< Both: the top left quarter is mirrored into all four quarters.
    };

    static constexpr float MinZoom{1.0f}; //!< 1 = no zoom.
    static constexpr float MaxZoom{4.0f}; //!< Strongest zoom.

    Mirror mirror{Mirror::Off};
    float zoom{1.0f}; //!< Zoom factor into the center of the picture.

    /**
     * @brief Returns true if any effect changes the picture.
     */
    bool Active() const;

    /**
     * @brief Pairs of (config value, display name) for all mirror modes, in Mirror enum order.
     */
    static const std::vector<std::pair<std::string, std::string>>& MirrorModeNames();

    /**
     * @brief Reads the current effect settings from the application configuration.
     */
    static OutputEffects FromConfig();
};

/**
 * @brief Renders projectM into an offscreen framebuffer, applies the output effects, sends the result
 *        via Spout and draws it into the window.
 *
 * Only used while an effect or the Spout output is active. Otherwise projectM renders directly into the
 * window, exactly as before. OpenGL functions are loaded via SDL, so no GL headers or GLEW are needed here.
 */
class OutputPipeline
{
public:
    OutputPipeline() = default;
    ~OutputPipeline();

    OutputPipeline(const OutputPipeline&) = delete;
    OutputPipeline& operator=(const OutputPipeline&) = delete;

    /**
     * @brief Loads the OpenGL functions and compiles the shader. Needs the GL context to be current.
     * @return false if the pipeline can't be used. Effects and Spout output are not available then.
     */
    bool Initialize();

    /**
     * @brief Frees all OpenGL objects. Needs the GL context to be current.
     */
    void Destroy();

    /**
     * @brief Returns true if Initialize() succeeded.
     */
    bool Available() const;

    /**
     * @brief Prepares the offscreen framebuffer projectM renders into.
     * @param width Render width, the size passed to projectm_set_window_size().
     * @param height Render height.
     * @return The framebuffer object to pass to projectm_opengl_render_frame_fbo(), or 0 on failure.
     */
    unsigned int BeginFrame(int width, int height);

    /**
     * @brief Applies the effects, sends the image via Spout (if given) and draws it into the window.
     * Leaves the default framebuffer bound with a viewport covering the whole window, for the UI.
     * @param windowWidth Drawable width of the window.
     * @param windowHeight Drawable height of the window.
     * @param effects The effects to apply.
     * @param spout Spout output to send the image to, or nullptr.
     * @param spoutName Spout sender name.
     */
    void EndFrame(int windowWidth, int windowHeight, const OutputEffects& effects, SpoutOutput* spout, const std::string& spoutName);

private:
    struct Target {
        unsigned int framebuffer{0};
        unsigned int texture{0};
        int width{0};
        int height{0};
    };

    bool Resize(Target& target, int width, int height);
    void FreeTarget(Target& target);
    void DrawTexture(unsigned int texture, const OutputEffects& effects);

    bool _available{false};
    unsigned int _program{0};
    unsigned int _vertexArray{0};
    int _textureLocation{-1};
    int _mirrorLocation{-1};
    int _zoomLocation{-1};

    Target _canvas; //!< projectM renders into this.
    Target _output; //!< Canvas with effects applied, sent via Spout.
};
