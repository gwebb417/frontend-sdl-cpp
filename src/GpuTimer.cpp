#include "GpuTimer.h"

#include <SDL2/SDL.h>

namespace {
// From the OpenGL 3.3 core specification / ARB_timer_query.
constexpr unsigned int GL_TIME_ELAPSED_VALUE = 0x88BF;
constexpr unsigned int GL_QUERY_RESULT_VALUE = 0x8866;
constexpr unsigned int GL_QUERY_RESULT_AVAILABLE_VALUE = 0x8867;
} // namespace

void GpuTimer::Initialize()
{
#ifdef USE_GLES
    // Timer queries are only an extension on OpenGL ES.
    _available = false;
#else
    // Not available with GL < 3.3 without ARB_timer_query.
    if (!SDL_GL_ExtensionSupported("GL_ARB_timer_query"))
    {
        int major{0};
        int minor{0};
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
        if (major < 3 || (major == 3 && minor < 3))
        {
            _available = false;
            return;
        }
    }

    _genQueries = reinterpret_cast<GenQueriesFn>(SDL_GL_GetProcAddress("glGenQueries"));
    _deleteQueries = reinterpret_cast<DeleteQueriesFn>(SDL_GL_GetProcAddress("glDeleteQueries"));
    _beginQuery = reinterpret_cast<BeginQueryFn>(SDL_GL_GetProcAddress("glBeginQuery"));
    _endQuery = reinterpret_cast<EndQueryFn>(SDL_GL_GetProcAddress("glEndQuery"));
    _getQueryObjectiv = reinterpret_cast<GetQueryObjectivFn>(SDL_GL_GetProcAddress("glGetQueryObjectiv"));
    _getQueryObjectui64v = reinterpret_cast<GetQueryObjectui64vFn>(SDL_GL_GetProcAddress("glGetQueryObjectui64v"));

    _available = _genQueries && _deleteQueries && _beginQuery && _endQuery && _getQueryObjectiv && _getQueryObjectui64v;
    if (_available)
    {
        _genQueries(QueryCount, _queries.data());
    }
#endif
}

void GpuTimer::Destroy()
{
    if (_available)
    {
        if (_running)
        {
            _endQuery(GL_TIME_ELAPSED_VALUE);
            _running = false;
        }
        _deleteQueries(QueryCount, _queries.data());
        _available = false;
    }
}

bool GpuTimer::Available() const
{
    return _available;
}

void GpuTimer::Begin()
{
    if (!_available || _running)
    {
        return;
    }

    // Collect finished results, oldest first, without waiting on the GPU.
    for (int i = 1; i <= QueryCount; i++)
    {
        int index = (_next + i) % QueryCount;
        if (!_pending[index])
        {
            continue;
        }

        int ready{0};
        _getQueryObjectiv(_queries[index], GL_QUERY_RESULT_AVAILABLE_VALUE, &ready);
        if (ready)
        {
            uint64_t nanoseconds{0};
            _getQueryObjectui64v(_queries[index], GL_QUERY_RESULT_VALUE, &nanoseconds);
            _lastResultMs = static_cast<float>(static_cast<double>(nanoseconds) / 1.0e6);
            _pending[index] = false;
        }
    }

    // If the GPU is so far behind that this query is still pending, skip this frame's measurement.
    if (_pending[_next])
    {
        return;
    }

    _beginQuery(GL_TIME_ELAPSED_VALUE, _queries[_next]);
    _running = true;
}

void GpuTimer::End()
{
    if (!_available || !_running)
    {
        return;
    }

    _endQuery(GL_TIME_ELAPSED_VALUE);
    _pending[_next] = true;
    _next = (_next + 1) % QueryCount;
    _running = false;
}

float GpuTimer::LastResultMs() const
{
    return _lastResultMs;
}
