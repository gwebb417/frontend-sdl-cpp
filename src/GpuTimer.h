#pragma once

#include <array>
#include <cstdint>

/**
 * @brief Measures how long the GPU takes for a block of OpenGL work, using timer queries.
 *
 * Results arrive a few frames late (the GPU runs behind the CPU), so several queries are used in turn
 * and none of them is waited on. Functions are loaded via SDL_GL_GetProcAddress, so no extra GL loader
 * is needed. If timer queries aren't available (e.g. OpenGL ES), Available() returns false.
 */
class GpuTimer
{
public:
    /**
     * @brief Loads the GL functions and creates the queries. Requires a current GL context.
     */
    void Initialize();

    /**
     * @brief Deletes the queries. Requires the GL context to still be current.
     */
    void Destroy();

    bool Available() const;

    /**
     * @brief Starts timing. Collects any finished earlier result first.
     */
    void Begin();

    /**
     * @brief Stops timing the current block.
     */
    void End();

    /**
     * @brief The most recent finished GPU time in milliseconds, or a negative value if none yet.
     */
    float LastResultMs() const;

private:
    static constexpr int QueryCount = 4;

    using GenQueriesFn = void (*)(int, unsigned int*);
    using DeleteQueriesFn = void (*)(int, const unsigned int*);
    using BeginQueryFn = void (*)(unsigned int, unsigned int);
    using EndQueryFn = void (*)(unsigned int);
    using GetQueryObjectivFn = void (*)(unsigned int, unsigned int, int*);
    using GetQueryObjectui64vFn = void (*)(unsigned int, unsigned int, uint64_t*);

    GenQueriesFn _genQueries{nullptr};
    DeleteQueriesFn _deleteQueries{nullptr};
    BeginQueryFn _beginQuery{nullptr};
    EndQueryFn _endQuery{nullptr};
    GetQueryObjectivFn _getQueryObjectiv{nullptr};
    GetQueryObjectui64vFn _getQueryObjectui64v{nullptr};

    std::array<unsigned int, QueryCount> _queries{};
    std::array<bool, QueryCount> _pending{};
    int _next{0};
    bool _running{false};
    bool _available{false};
    float _lastResultMs{-1.0f};
};
