#include "OutputPipeline.h"

#include "SpoutOutput.h"

#include <Poco/Logger.h>
#include <Poco/Util/Application.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

// OpenGL types and constants used here. Defined locally so this file doesn't depend on a platform GL header.
using GLuint = unsigned int;
using GLint = int;
using GLenum = unsigned int;
using GLsizei = int;
using GLfloat = float;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLchar = char;

constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_COLOR_ATTACHMENT0 = 0x8CE0;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;
constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_RGBA = 0x1908;
constexpr GLenum GL_RGBA8 = 0x8058;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLint GL_LINEAR = 0x2601;
constexpr GLint GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLbitfield GL_COLOR_BUFFER_BIT = 0x4000;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_CULL_FACE = 0x0B44;

#ifdef _WIN32
#define PM_GL_API __stdcall
#else
#define PM_GL_API
#endif

struct GlFunctions {
    void(PM_GL_API* GenFramebuffers)(GLsizei, GLuint*){};
    void(PM_GL_API* DeleteFramebuffers)(GLsizei, const GLuint*){};
    void(PM_GL_API* BindFramebuffer)(GLenum, GLuint){};
    void(PM_GL_API* FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint){};
    GLenum(PM_GL_API* CheckFramebufferStatus)(GLenum){};
    void(PM_GL_API* GenTextures)(GLsizei, GLuint*){};
    void(PM_GL_API* DeleteTextures)(GLsizei, const GLuint*){};
    void(PM_GL_API* BindTexture)(GLenum, GLuint){};
    void(PM_GL_API* ActiveTexture)(GLenum){};
    void(PM_GL_API* TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*){};
    void(PM_GL_API* TexParameteri)(GLenum, GLenum, GLint){};
    void(PM_GL_API* BindSampler)(GLuint, GLuint){};
    GLuint(PM_GL_API* CreateShader)(GLenum){};
    void(PM_GL_API* ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*){};
    void(PM_GL_API* CompileShader)(GLuint){};
    void(PM_GL_API* GetShaderiv)(GLuint, GLenum, GLint*){};
    void(PM_GL_API* GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*){};
    void(PM_GL_API* DeleteShader)(GLuint){};
    GLuint(PM_GL_API* CreateProgram)(){};
    void(PM_GL_API* AttachShader)(GLuint, GLuint){};
    void(PM_GL_API* LinkProgram)(GLuint){};
    void(PM_GL_API* GetProgramiv)(GLuint, GLenum, GLint*){};
    void(PM_GL_API* GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*){};
    void(PM_GL_API* DeleteProgram)(GLuint){};
    void(PM_GL_API* UseProgram)(GLuint){};
    GLint(PM_GL_API* GetUniformLocation)(GLuint, const GLchar*){};
    void(PM_GL_API* Uniform1i)(GLint, GLint){};
    void(PM_GL_API* Uniform1f)(GLint, GLfloat){};
    void(PM_GL_API* GenVertexArrays)(GLsizei, GLuint*){};
    void(PM_GL_API* DeleteVertexArrays)(GLsizei, const GLuint*){};
    void(PM_GL_API* BindVertexArray)(GLuint){};
    void(PM_GL_API* DrawArrays)(GLenum, GLint, GLsizei){};
    void(PM_GL_API* Viewport)(GLint, GLint, GLsizei, GLsizei){};
    void(PM_GL_API* ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat){};
    void(PM_GL_API* Clear)(GLbitfield){};
    void(PM_GL_API* Disable)(GLenum){};
} gl;

template<typename T>
bool Load(T& function, const char* name)
{
    function = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    return function != nullptr;
}

bool LoadGlFunctions()
{
    bool ok = true;
    ok &= Load(gl.GenFramebuffers, "glGenFramebuffers");
    ok &= Load(gl.DeleteFramebuffers, "glDeleteFramebuffers");
    ok &= Load(gl.BindFramebuffer, "glBindFramebuffer");
    ok &= Load(gl.FramebufferTexture2D, "glFramebufferTexture2D");
    ok &= Load(gl.CheckFramebufferStatus, "glCheckFramebufferStatus");
    ok &= Load(gl.GenTextures, "glGenTextures");
    ok &= Load(gl.DeleteTextures, "glDeleteTextures");
    ok &= Load(gl.BindTexture, "glBindTexture");
    ok &= Load(gl.ActiveTexture, "glActiveTexture");
    ok &= Load(gl.TexImage2D, "glTexImage2D");
    ok &= Load(gl.TexParameteri, "glTexParameteri");
    ok &= Load(gl.BindSampler, "glBindSampler");
    ok &= Load(gl.CreateShader, "glCreateShader");
    ok &= Load(gl.ShaderSource, "glShaderSource");
    ok &= Load(gl.CompileShader, "glCompileShader");
    ok &= Load(gl.GetShaderiv, "glGetShaderiv");
    ok &= Load(gl.GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= Load(gl.DeleteShader, "glDeleteShader");
    ok &= Load(gl.CreateProgram, "glCreateProgram");
    ok &= Load(gl.AttachShader, "glAttachShader");
    ok &= Load(gl.LinkProgram, "glLinkProgram");
    ok &= Load(gl.GetProgramiv, "glGetProgramiv");
    ok &= Load(gl.GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= Load(gl.DeleteProgram, "glDeleteProgram");
    ok &= Load(gl.UseProgram, "glUseProgram");
    ok &= Load(gl.GetUniformLocation, "glGetUniformLocation");
    ok &= Load(gl.Uniform1i, "glUniform1i");
    ok &= Load(gl.Uniform1f, "glUniform1f");
    ok &= Load(gl.GenVertexArrays, "glGenVertexArrays");
    ok &= Load(gl.DeleteVertexArrays, "glDeleteVertexArrays");
    ok &= Load(gl.BindVertexArray, "glBindVertexArray");
    ok &= Load(gl.DrawArrays, "glDrawArrays");
    ok &= Load(gl.Viewport, "glViewport");
    ok &= Load(gl.ClearColor, "glClearColor");
    ok &= Load(gl.Clear, "glClear");
    ok &= Load(gl.Disable, "glDisable");
    return ok;
}

#ifdef USE_GLES
constexpr const char* ShaderHeader = "#version 300 es\nprecision mediump float;\n";
#else
constexpr const char* ShaderHeader = "#version 330 core\n";
#endif

// Full-screen triangle without vertex buffers: vertices (0,0), (2,0), (0,2) in texture coordinates.
constexpr const char* VertexShader = R"(
out vec2 uv;
void main()
{
    vec2 position = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    uv = position;
    gl_Position = vec4(position * 2.0 - 1.0, 0.0, 1.0);
}
)";

// Mirror first (fold the output coordinates onto the kept half/quarter), then zoom into the center.
// Texture coordinates start at the bottom left, so "top half onto bottom" folds y < 0.5 up.
constexpr const char* FragmentShader = R"(
in vec2 uv;
out vec4 fragColor;
uniform sampler2D tex;
uniform int mirror;
uniform float zoom;
void main()
{
    vec2 coord = uv;
    if ((mirror == 1 || mirror == 3) && coord.x > 0.5)
    {
        coord.x = 1.0 - coord.x;
    }
    if ((mirror == 2 || mirror == 3) && coord.y < 0.5)
    {
        coord.y = 1.0 - coord.y;
    }
    coord = 0.5 + (coord - 0.5) / zoom;
    fragColor = vec4(texture(tex, coord).rgb, 1.0);
}
)";

GLuint CompileShader(GLenum type, const char* source, Poco::Logger& logger)
{
    GLuint shader = gl.CreateShader(type);
    const GLchar* sources[2]{ShaderHeader, source};
    gl.ShaderSource(shader, 2, sources, nullptr);
    gl.CompileShader(shader);

    GLint status{0};
    gl.GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        GLchar log[1024]{};
        gl.GetShaderInfoLog(shader, sizeof(log), nullptr, log);
        poco_error_f1(logger, "Output effect shader failed to compile: %s", std::string(log));
        gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

bool OutputEffects::Active() const
{
    return mirror != Mirror::Off || zoom > MinZoom + 0.001f;
}

const std::vector<std::pair<std::string, std::string>>& OutputEffects::MirrorModeNames()
{
    static const std::vector<std::pair<std::string, std::string>> names{
        {"off", "Off"},
        {"leftright", "Left to Right"},
        {"topbottom", "Top to Bottom"},
        {"fourway", "4-Way (Both)"}};
    return names;
}

OutputEffects OutputEffects::FromConfig()
{
    auto& config = Poco::Util::Application::instance().config();

    OutputEffects effects;
    auto mirror = config.getString("effects.mirror", "off");
    const auto& names = MirrorModeNames();
    for (size_t index = 0; index < names.size(); index++)
    {
        if (names[index].first == mirror)
        {
            effects.mirror = static_cast<Mirror>(index);
        }
    }

    double zoom = config.getDouble("effects.zoom", 1.0);
    effects.zoom = std::isfinite(zoom) ? std::clamp(static_cast<float>(zoom), MinZoom, MaxZoom) : MinZoom;
    return effects;
}

OutputPipeline::~OutputPipeline()
{
    // GL objects need a current context, so they're freed in Destroy(), called by the render loop.
}

bool OutputPipeline::Initialize()
{
    auto& logger = Poco::Logger::get("OutputPipeline");

    if (!LoadGlFunctions())
    {
        poco_error(logger, "Could not load the OpenGL functions for output effects. Mirror, zoom and Spout output are not available.");
        return false;
    }

    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, VertexShader, logger);
    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, FragmentShader, logger);
    if (vertexShader == 0 || fragmentShader == 0)
    {
        if (vertexShader != 0)
        {
            gl.DeleteShader(vertexShader);
        }
        if (fragmentShader != 0)
        {
            gl.DeleteShader(fragmentShader);
        }
        return false;
    }

    _program = gl.CreateProgram();
    gl.AttachShader(_program, vertexShader);
    gl.AttachShader(_program, fragmentShader);
    gl.LinkProgram(_program);
    gl.DeleteShader(vertexShader);
    gl.DeleteShader(fragmentShader);

    GLint status{0};
    gl.GetProgramiv(_program, GL_LINK_STATUS, &status);
    if (!status)
    {
        GLchar log[1024]{};
        gl.GetProgramInfoLog(_program, sizeof(log), nullptr, log);
        poco_error_f1(logger, "Output effect shader failed to link: %s", std::string(log));
        gl.DeleteProgram(_program);
        _program = 0;
        return false;
    }

    _textureLocation = gl.GetUniformLocation(_program, "tex");
    _mirrorLocation = gl.GetUniformLocation(_program, "mirror");
    _zoomLocation = gl.GetUniformLocation(_program, "zoom");

    gl.GenVertexArrays(1, &_vertexArray);

    _available = true;
    return true;
}

void OutputPipeline::Destroy()
{
    if (!_available)
    {
        return;
    }

    FreeTarget(_canvas);
    FreeTarget(_output);
    gl.DeleteVertexArrays(1, &_vertexArray);
    gl.DeleteProgram(_program);
    _vertexArray = 0;
    _program = 0;
    _available = false;
}

bool OutputPipeline::Available() const
{
    return _available;
}

unsigned int OutputPipeline::BeginFrame(int width, int height)
{
    if (!_available || !Resize(_canvas, width, height))
    {
        return 0;
    }
    return _canvas.framebuffer;
}

void OutputPipeline::EndFrame(int windowWidth, int windowHeight, const OutputEffects& effects, SpoutOutput* spout, const std::string& spoutName)
{
    if (!_available || _canvas.framebuffer == 0)
    {
        return;
    }

    // Plain state for the full-screen passes. libprojectM sets up its own state each frame, and so does the UI.
    gl.Disable(GL_BLEND);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_SCISSOR_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindSampler(0, 0); // libprojectM binds sampler objects, which would override the texture's filtering.
    gl.UseProgram(_program);
    gl.BindVertexArray(_vertexArray);
    gl.Uniform1i(_textureLocation, 0);

    GLuint windowTexture = _canvas.texture;
    OutputEffects windowEffects = effects;

    if (spout != nullptr)
    {
        // Apply the effects once into the output texture, which is sent via Spout and shown in the window.
        // This pass also makes the image fully opaque, so receivers like OBS don't show it partly transparent.
        if (Resize(_output, _canvas.width, _canvas.height))
        {
            gl.BindFramebuffer(GL_FRAMEBUFFER, _output.framebuffer);
            gl.Viewport(0, 0, _output.width, _output.height);
            DrawTexture(_canvas.texture, effects);

            spout->Send(_output.texture, _output.width, _output.height, spoutName);

            // Spout may change bindings, so set them again.
            gl.ActiveTexture(GL_TEXTURE0);
            gl.UseProgram(_program);
            gl.BindVertexArray(_vertexArray);

            windowTexture = _output.texture;
            windowEffects = OutputEffects{};
        }
    }

    // Draw into the window. If the render size has a different shape than the window (fixed Spout resolution),
    // keep the picture's proportions and fill the rest with black.
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
    gl.Viewport(0, 0, windowWidth, windowHeight);
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);

    int viewportWidth = windowWidth;
    int viewportHeight = windowHeight;
    if (_canvas.width > 0 && _canvas.height > 0 && windowWidth > 0 && windowHeight > 0)
    {
        double scale = std::min(static_cast<double>(windowWidth) / _canvas.width, static_cast<double>(windowHeight) / _canvas.height);
        viewportWidth = std::max(1, static_cast<int>(std::lround(_canvas.width * scale)));
        viewportHeight = std::max(1, static_cast<int>(std::lround(_canvas.height * scale)));
    }
    gl.Viewport((windowWidth - viewportWidth) / 2, (windowHeight - viewportHeight) / 2, viewportWidth, viewportHeight);
    DrawTexture(windowTexture, windowEffects);

    // Leave a clean state for the UI.
    gl.Viewport(0, 0, windowWidth, windowHeight);
    gl.BindTexture(GL_TEXTURE_2D, 0);
    gl.BindVertexArray(0);
    gl.UseProgram(0);
}

void OutputPipeline::DrawTexture(unsigned int texture, const OutputEffects& effects)
{
    gl.BindTexture(GL_TEXTURE_2D, texture);
    gl.Uniform1i(_mirrorLocation, static_cast<int>(effects.mirror));
    gl.Uniform1f(_zoomLocation, effects.zoom);
    gl.DrawArrays(GL_TRIANGLES, 0, 3);
}

bool OutputPipeline::Resize(Target& target, int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return false;
    }

    if (target.framebuffer != 0 && target.width == width && target.height == height)
    {
        return true;
    }

    if (target.framebuffer == 0)
    {
        gl.GenFramebuffers(1, &target.framebuffer);
        gl.GenTextures(1, &target.texture);
    }

    gl.ActiveTexture(GL_TEXTURE0);
    gl.BindTexture(GL_TEXTURE_2D, target.texture);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl.BindTexture(GL_TEXTURE_2D, 0);

    gl.BindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
    gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    bool complete = gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!complete)
    {
        poco_error_f2(Poco::Logger::get("OutputPipeline"), "Could not create a %dx%d offscreen framebuffer.", width, height);
        FreeTarget(target);
        return false;
    }

    target.width = width;
    target.height = height;
    return true;
}

void OutputPipeline::FreeTarget(Target& target)
{
    if (target.framebuffer != 0)
    {
        gl.DeleteFramebuffers(1, &target.framebuffer);
        gl.DeleteTextures(1, &target.texture);
    }
    target = Target{};
}
